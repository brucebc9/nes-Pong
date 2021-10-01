#include <stdlib.h>
#include <string.h>
#include "neslib.h"
#include <nes.h>
#include "vrambuf.h"
//#link "vrambuf.c"
#include "pong_titlescreen.h"
//#link "pong_titlescreen.s"


// link the pattern table into CHR ROM
//#link "chr_generic.s"

#define COLS 32
#define ROWS 32
#define MAX_SCORE 5
#define FP_BITS 4

// setup Famitone library
//#link "famitone2.s"
void __fastcall__ famitone_update(void);
//#link "music_aftertherain.s"
extern char after_the_rain_music_data[];
//#link "demosounds.s"
extern char demo_sounds[];

static byte score1 =0;
static byte score2 =0;
static unsigned char bright;
static unsigned char frame_cnt;
static unsigned char wait;
static int iy,dy;
static unsigned char i;

/*{pal:"nes",layout:"nes"}*/
const char PALETTE[32] = { 
  0x0D,			// screen color

  0x0D,0x00,0x30,0x00,	// background palette 0
  0x1C,0x20,0x2C,0x00,	// background palette 1
  0x00,0x10,0x20,0x00,	// background palette 2
  0x06,0x16,0x26,0x00,	// background palette 3

  0x00,0x30,0x2D,0x00,	// sprite palette 0
  0x00,0x37,0x25,0x00,	// sprite palette 1
  0x0D,0x2D,0x3A,0x00,	// sprite palette 2
  0x0D,0x27,0x2A	// sprite palette 3
};

// define a 1x4 metasprite
#define DEF_METASPRITE_1x4(name,code,pal)\
const unsigned char name[]={\
        0,      0,      (code),   pal, \
        0,      8,      (code),   pal, \
        0,      16,     (code),   pal, \
        0,      24,     (code),   pal, \
        128};

DEF_METASPRITE_1x4(paddle, 0x8f, 0);

// number of metasprites/sprites
// 2 paddles/ 1 ball
#define NUM_ACTORS 3

// sprite x/y positions
byte actor_x[NUM_ACTORS];
byte actor_y[NUM_ACTORS];
// actor x/y deltas per frame (signed)
sbyte actor_dx[NUM_ACTORS];
sbyte actor_dy[NUM_ACTORS];

// returns absolute value of x
byte iabs(int x) {
  return x >= 0 ? x : -x;
}

void cputcxy(byte x, byte y, char ch) {
  vrambuf_put(NTADR_A(x,y), &ch, 1);
}

void cputsxy(byte x, byte y, const char* str) {
  vrambuf_put(NTADR_A(x,y), str, strlen(str));
}

void clrscr() {
  vrambuf_clear();
  ppu_off();
  vram_adr(0x2000);
  vram_fill(0, 32*32);
  vram_adr(0x0);
  ppu_on_bg();
}

//characters for drawing a border around the screen
const char BOX_CHARS[8] = { '+','+','+','+','-','-','!','!' };

//draws a border around the main screen and winner screen
void draw_box(byte x, byte y, byte x2, byte y2, const char* chars) {
  byte x1 = x;
  cputcxy(x, y, chars[2]);
  cputcxy(x2, y, chars[3]);
  cputcxy(x, y2, chars[0]);
  cputcxy(x2, y2, chars[1]);
  while (++x < x2) {
    cputcxy(x, y, chars[5]);
    cputcxy(x, y2, chars[4]);
  }
  while (++y < y2) {
    cputcxy(x1, y, chars[6]);
    cputcxy(x2, y, chars[7]);
  }
}

void draw_playfield() {
  draw_box(0,2,COLS-1,ROWS-5,BOX_CHARS);
  cputcxy(13,1,score1+'0');
  cputcxy(27,1,score2+'0');
  cputsxy(5,1,"PLAYER1:");
  cputsxy(19,1,"PLAYER2:");
}

void declare_winner(byte winner) {
  byte i;
  clrscr();
  music_play(2);
  for (i=0; i<ROWS/2-5; i++) {
    draw_box(i,i,COLS-i,ROWS-6-i,BOX_CHARS);
    vrambuf_flush();
  }
  cputsxy(13,11,"WINNER:");
  cputsxy(13,13,"PLAYER ");
  cputcxy(12+7, 13, '1'+winner);
  vrambuf_flush();
  delay(160);
  music_stop();
}

void reset_players() {
    actor_x[0] = 14;
    actor_y[0] = 100;
    actor_dx[0] = 0;
    actor_dy[0] = 0;
  
    actor_x[1] = 235;
    actor_y[1] = 100;
    actor_dx[1] = 0;
    actor_dy[1] = 0;
  
    actor_x[2] = 128;
    actor_y[2] = 8;
    actor_dx[2] = 1;
    actor_dy[2] = 1;
}

// setup PPU and tables
void setup_graphics() {
  // clear sprites
  oam_hide_rest(0);
  // set palette colors
  pal_all(PALETTE);
  // turn on PPU
  ppu_on_all();
  reset_players();
}

void pal_fade_to(unsigned to)
{
 if(!to) music_stop();

  while(bright!=to)
  {
    delay(4);
    if(bright<to) ++bright; else --bright;
    pal_bright(bright);
  }

  if(!bright)
  {
    ppu_off();
    set_vram_update(NULL);
    scroll(0,0);
  }
}

void title_screen(void)
{
  scroll(0,240);//title is aligned to the color attributes, so shift it a bit to the right

  vram_adr(NTADR_A(0,0));
  vram_unrle(pong_titlescreen);
 
 // vram_adr(NAMETABLE_C);//clear second nametable, as it is visible in the jumping effect
  //vram_fill(0,1024);

  pal_bg(PALETTE);
  pal_bright(4);
  ppu_on_bg();
  delay(20);//delay just to make it look better

  iy=240<<FP_BITS;
  dy=-8<<FP_BITS;
  frame_cnt=0;
  wait=0;
  bright=4;
  
  while(1)
  {
    ppu_wait_frame();
    scroll(0,iy>>FP_BITS);
    if(pad_trigger(0)&PAD_START) break;
    iy+=dy;
    if(iy<0)
    {
      iy=0;
      dy=-dy>>1;
    }
    if(dy>(-8<<FP_BITS)) dy-=2;
    if(wait)
    {
      --wait;
    }
    else
    {
      pal_col(2,(frame_cnt&32)?0x0f:0x20);//blinking press start text
      ++frame_cnt;
    }
  }

  scroll(1,0);//if start is pressed, show the title at whole
  sfx_play(1,0);//titlescreen sound effect
  for(i=0;i<16;++i)//and blink the text faster
  {
    pal_col(2,i&1?0x0f:0x01);
    delay(4);
  }
  pal_fade_to(4);
  vrambuf_clear();
  ppu_off();
  vram_adr(NTADR_A(0,0));
  vram_fill(0, 1024);
  vrambuf_flush();
  set_vram_update(updbuf);
}

// main program
void main() {
  char i;	// actor index
  char oam_id;	// sprite ID
  char pad;	// controller flags
  
  famitone_init(after_the_rain_music_data);
  sfx_init(demo_sounds);
  // set music callback function for NMI
  nmi_set_callback(famitone_update);
  // play music
  music_play(4);
  // enable PPU rendering (turn on screen)
  title_screen();
	
  draw_playfield();
  setup_graphics();
  
  draw_playfield();

   
  // loop forever
  while (1) 
  {
    // start with OAMid/sprite 0
    oam_id = 0;
    // set player 0/1 velocity based on controller
    for (i=0; i<2; i++) 
    {
      // poll controller i (0-1)
      pad = pad_poll(i);
      if (pad&PAD_UP && actor_y[i]>22) actor_dy[i]=-5;
      else if (pad&PAD_DOWN && actor_y[i]<183) actor_dy[i]=5;
      else actor_dy[i]=0;
    }

    // draw and move paddles
    for (i=0; i<2; i++) 
    {
      byte runseq = actor_x[i] & 7;
      if (actor_dx[i] >= 0)
      runseq += 8;
      oam_id = oam_meta_spr(actor_x[i], actor_y[i], oam_id, paddle);
      actor_x[i] += actor_dx[i];
      actor_y[i] += actor_dy[i];
    }
    
    //draw ball
    oam_id = oam_spr(actor_x[2], actor_y[2], 01, 0, oam_id);
    actor_x[2] += actor_dx[2];
    actor_y[2] += actor_dy[2];
    
   //bounce ball
    if (iabs(actor_x[i]-actor_x[0])<8 && iabs(actor_y[i]-(actor_y[0]-6)<37)){ 
      actor_dx[i]=4;
      sfx_play(3,1);}
    else if (iabs(actor_x[i]-(actor_x[1]-6)<14) && iabs(actor_y[i]-(actor_y[1]-6)<37)){
      actor_dx[i]=-4;
      sfx_play(3,2);}
    if (actor_y[i]<21) actor_dy[i]=2;
    else if (actor_y[i]>212) actor_dy[i]=-2;
    
    //Check if point scored
    if(actor_x[2]<4 && actor_dx[2]<0)
    {
      sfx_play(2,0);
      delay(15);
      score2++;
      if(score2>=MAX_SCORE){
      declare_winner(1);
      score2=0;
      break;
      }
      ppu_off();
      reset_players();
      vrambuf_flush();
      cputcxy(27,1,score2+'0');
      cputcxy(27,1,score2+'0');
      ppu_on_all();
    }
    
    else if(actor_x[2]>244 && actor_dx[2]>0)
    {
      sfx_play(2,0);
      delay(15);
      score1++;
      if(score1>=MAX_SCORE){
      declare_winner(0);
      score1=0;
      break;
      }
      ppu_off();
      reset_players();
      vrambuf_flush();
      cputcxy(13,1,score1+'0');
      cputcxy(13,1,score1+'0');
      ppu_on_all();
    }
    // wait for next frame
    ppu_wait_frame();
  }
}
