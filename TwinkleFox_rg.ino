#include "FastLED.h"

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif


#define NUM_LEDS      200
#define LED_TYPE   WS2812
#define COLOR_ORDER   GRB
#define DATA_PIN        3
//#define CLK_PIN       4
#define VOLTS          12
#define MAX_MA       4000
#define POT_BRIGHTNESS A0
#define POT_LIVELINESS A1

#define NUM_LIGHTS     10
#define NUM_LEDS_LIGHT 20

//  TwinkleFOX: Twinkling 'holiday' lights that fade in and out.
//  Colors are chosen from a palette; a few palettes are provided.
//
//  This December 2015 implementation improves on the December 2014 version
//  in several ways:
//  - smoother fading, compatible with any colors and any palettes
//  - easier control of twinkle speed and twinkle density
//  - supports an optional 'background color'
//  - takes even less RAM: zero RAM overhead per pixel
//  - illustrates a couple of interesting techniques (uh oh...)
//
//  The idea behind this (new) implementation is that there's one
//  basic, repeating pattern that each pixel follows like a waveform:
//  The brightness rises from 0..255 and then falls back down to 0.
//  The brightness at any given point in time can be determined as
//  as a function of time, for example:
//    brightness = sine( time ); // a sine wave of brightness over time
//
//  So the way this implementation works is that every pixel follows
//  the exact same wave function over time.  In this particular case,
//  I chose a sawtooth triangle wave (triwave8) rather than a sine wave,
//  but the idea is the same: brightness = triwave8( time ).  
//  
//  Of course, if all the pixels used the exact same wave form, and 
//  if they all used the exact same 'clock' for their 'time base', all
//  the pixels would brighten and dim at once -- which does not look
//  like twinkling at all.
//
//  So to achieve random-looking twinkling, each pixel is given a 
//  slightly different 'clock' signal.  Some of the clocks run faster, 
//  some run slower, and each 'clock' also has a random offset from zero.
//  The net result is that the 'clocks' for all the pixels are always out 
//  of sync from each other, producing a nice random distribution
//  of twinkles.
//
//  The 'clock speed adjustment' and 'time offset' for each pixel
//  are generated randomly.  One (normal) approach to implementing that
//  would be to randomly generate the clock parameters for each pixel 
//  at startup, and store them in some arrays.  However, that consumes
//  a great deal of precious RAM, and it turns out to be totally
//  unnessary!  If the random number generate is 'seeded' with the
//  same starting value every time, it will generate the same sequence
//  of values every time.  So the clock adjustment parameters for each
//  pixel are 'stored' in a pseudo-random number generator!  The PRNG 
//  is reset, and then the first numbers out of it are the clock 
//  adjustment parameters for the first pixel, the second numbers out
//  of it are the parameters for the second pixel, and so on.
//  In this way, we can 'store' a stable sequence of thousands of
//  random clock adjustment parameters in literally two bytes of RAM.
//
//  There's a little bit of fixed-point math involved in applying the
//  clock speed adjustments, which are expressed in eighths.  Each pixel's
//  clock speed ranges from 8/8ths of the system clock (i.e. 1x) to
//  23/8ths of the system clock (i.e. nearly 3x).
//
//  On a basic Arduino Uno or Leonardo, this code can twinkle 300+ pixels
//  smoothly at over 50 updates per seond.
//
//  -Mark Kriegsman, December 2015

CRGBArray<NUM_LEDS> leds;

//should set up an array of these? - an array of groups of leds, which would be a 2 dim array
//CRGBArray //LightsArray<NUM_LIGHTS> CRGBArray;
//CRGBArray<NUM_LEDS_LIGHT> leds_1;
//then maintain a palette for each of these, address each of the groups independently
//using the functions provided in this example. 
//don't really know what I'm doing :-)


// Overall twinkle speed.
// 0 (VERY slow) to 8 (VERY fast).  
// 4, 5, and 6 are recommended, default is 4.
//#define TWINKLE_SPEED 2
uint8_t twinkleSpeed = 2;

// Overall twinkle density.
// 0 (NONE lit) to 8 (ALL lit at once).  
// Default is 5.
//#define TWINKLE_DENSITY 5
uint8_t twinkleDensity = 5;

// How often to change color palettes.
//#define SECONDS_PER_PALETTE  5
uint8_t secondsPerPalette = 5;
// Also: toward the bottom of the file is an array 
// called "ActivePaletteList" which controls which color
// palettes are used; you can add or remove color palettes
// from there freely.

// Background color for 'unlit' pixels
// Can be set to CRGB::Black if desired.
CRGB gBackgroundColor = CRGB::Black; 
// Example of dim incandescent fairy light background color
// CRGB gBackgroundColor = CRGB(CRGB::FairyLight).nscale8_video(16);

// If AUTO_SELECT_BACKGROUND_COLOR is set to 1,
// then for any palette where the first two entries 
// are the same, a dimmed version of that color will
// automatically be used as the background color.
#define AUTO_SELECT_BACKGROUND_COLOR 0

// If COOL_LIKE_INCANDESCENT is set to 1, colors will 
// fade out slighted 'reddened', similar to how
// incandescent bulbs change color as they get dim down.
#define COOL_LIKE_INCANDESCENT 0


//CRGBPalette16 gCurrentPalette;
CRGBPalette16 gTargetPalette;
CRGBPalette16 gLightPalette[NUM_LIGHTS];
uint8_t SeedLight;
uint8_t BlendOut;
uint8_t light;
uint8_t seed = random8(); 
int brightness=1023;
int liveliness=1023;

void setup() {
  delay( 3000 ); //safety startup delay
  FastLED.setMaxPowerInVoltsAndMilliamps( VOLTS, MAX_MA);
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  
  chooseNextColorPalette(gTargetPalette);
  
  //setup all palettes with an initial value
  for (int i=0; i<NUM_LIGHTS; i++) {
     gLightPalette[i] = gTargetPalette;    
  }
  BlendOut=0;
  SeedLight=7;

  pinMode(POT_BRIGHTNESS, INPUT);
  brightness=analogRead(POT_BRIGHTNESS);
  pinMode(POT_LIVELINESS, INPUT);
  liveliness=analogRead(POT_LIVELINESS);
}


void loop()
{
/*
  EVERY_N_MILLISECONDS(500){
    chooseNextColorPalette( gTargetPalette ); 
    for (int k=0; k<16; k++){
      leds[k] = ColorFromPalette(gTargetPalette, k*16);
    }
  }
*/
///*  
  EVERY_N_SECONDS( secondsPerPalette ) {
    if (BlendOut >= 10) { 
      chooseNextColorPalette( gTargetPalette ); 
      //choose which light to applyt it to
      SeedLight=random(NUM_LIGHTS);
      BlendOut=0;
    } else {
      BlendOut++;
    }
    //palettes in the array will need to be blended to gradually
    
    brightness = analogRead(POT_BRIGHTNESS);
    liveliness = analogRead(POT_LIVELINESS);
    
    //recalculate secondsPerPalette based on liveliness setting - range = 3 - 15 sec
    secondsPerPalette = (liveliness / 85) + 3;
  }

  //now I'll need to blend the current palette to the target palette
  //this needs to happen for all palettes adjacent to the seedLight
  //need a counter so I can move outwards
  EVERY_N_MILLISECONDS( 100 ) {
    for (int i=0; i<=BlendOut; i++){
      nblendPaletteTowardPalette( gLightPalette[SeedLight], gTargetPalette, 12);
      if (i>0){
        if (SeedLight-i >= 0) {
          nblendPaletteTowardPalette( gLightPalette[SeedLight-i], gTargetPalette, 12);
        }
        if (SeedLight+i < NUM_LIGHTS) {
          nblendPaletteTowardPalette( gLightPalette[SeedLight+i], gTargetPalette, 12);
        }
      }
    }
  }
  
  drawTwinkles( leds);
//*/  
  FastLED.show();
}


//  This function loops over each pixel, calculates the 
//  adjusted 'clock' that this pixel should use, and calls 
//  "CalculateOneTwinkle" on each pixel.  It then displays
//  either the twinkle color of the background color, 
//  whichever is brighter.
void drawTwinkles( CRGBSet& L)
{
  // "PRNG16" is the pseudorandom number generator
  // It MUST be reset to the same starting value each time
  // this function is called, so that the sequence of 'random'
  // numbers that it generates is (paradoxically) stable.
  uint16_t PRNG16 = 11337;
  
  uint32_t clock32 = millis();

  // Set up the background color, "bg".
  // if AUTO_SELECT_BACKGROUND_COLOR == 1, and the first two colors of
  // the current palette are identical, then a deeply faded version of
  // that color is used for the background color
  CRGB bg;
  bg = gBackgroundColor; // just use the explicitly defined background color

  uint8_t backgroundBrightness = bg.getAverageLight();

//  CRGB& pixel;
//  for( CRGB& pixel: L) {
//  for(int i=256; i<277; i++) {
//    CRGB& pixel = L[i];

  //loop through light pods
  for(light=0; light<NUM_LIGHTS; light++){

    int light_offset = light * NUM_LEDS_LIGHT;
    
    for(int i=light_offset; i<=light_offset+NUM_LEDS_LIGHT; i++){
      CRGB& pixel = L[i];
      
      PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
      uint16_t myclockoffset16= PRNG16; // use that number as clock offset
      PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
      // use that number as clock speed adjustment factor (in 8ths, from 8/8ths to 23/8ths)
      uint8_t myspeedmultiplierQ5_3 =  ((((PRNG16 & 0xFF)>>4) + (PRNG16 & 0x0F)) & 0x0F) + 0x08;
      uint32_t myclock30 = (uint32_t)((clock32 * myspeedmultiplierQ5_3) >> 3) + myclockoffset16;
      uint8_t  myunique8 = PRNG16 >> 8; // get 'salt' value for this pixel
  
      // We now have the adjusted 'clock' for this pixel, now we call
      // the function that computes what color the pixel should be based
      // on the "brightness = f( time )" idea.
      CRGB c = computeOneTwinkle( myclock30, myunique8);
  
      uint8_t cbright = c.getAverageLight();
      int16_t deltabright = cbright - backgroundBrightness;
      if( deltabright >= 32 || (!bg)) {
        // If the new pixel is significantly brighter than the background color, 
        // use the new color.
        pixel = c;
      } else if( deltabright > 0 ) {
        // If the new pixel is just slightly brighter than the background color,
        // mix a blend of the new color and the background color
        pixel = blend( bg, c, deltabright * 8);
      } else { 
        // if the new pixel is not at all brighter than the background color,
        // just use the background color.
        pixel = bg;
      }
      
    }
  }
    
}


//  This function takes a time in pseudo-milliseconds,
//  figures out brightness = f( time ), and also hue = f( time )
//  The 'low digits' of the millisecond time are used as 
//  input to the brightness wave function.  
//  The 'high digits' are used to select a color, so that the color
//  does not change over the course of the fade-in, fade-out
//  of one cycle of the brightness wave function.
//  The 'high digits' are also used to determine whether this pixel
//  should light at all during this cycle, based on the TWINKLE_DENSITY.
CRGB computeOneTwinkle( uint32_t ms, uint8_t salt)
{
  uint16_t ticks = ms >> (8-twinkleSpeed);
  uint8_t fastcycle8 = ticks;
  uint16_t slowcycle16 = (ticks >> 8) + salt;
  slowcycle16 += sin8( slowcycle16);
  slowcycle16 =  (slowcycle16 * 2053) + 1384;
  uint8_t slowcycle8 = (slowcycle16 & 0xFF) + (slowcycle16 >> 8);
  
  uint8_t bright = 0;
  if( ((slowcycle8 & 0x0E)/2) < twinkleDensity) {
    bright = attackDecayWave8( fastcycle8);
  }

  uint8_t hue = slowcycle8 - salt;
  CRGB c;
  if( bright > 0) {
    c = ColorFromPalette( gLightPalette[light], hue, bright, LINEARBLEND);
    if( COOL_LIKE_INCANDESCENT == 1 ) {
      coolLikeIncandescent( c, fastcycle8);
    }
  } else {
    c = CRGB::Black;
  }
  return c;
}


// This function is like 'triwave8', which produces a 
// symmetrical up-and-down triangle sawtooth waveform, except that this
// function produces a triangle wave with a faster attack and a slower decay:
//
//     / \ 
//    /     \ 
//   /         \ 
//  /             \ 
//

uint8_t attackDecayWave8( uint8_t i)
{
  if( i < 86) {
    return i * 3;
  } else {
    i -= 86;
    return 255 - (i + (i/2));
  }
}

// This function takes a pixel, and if its in the 'fading down'
// part of the cycle, it adjusts the color a little bit like the 
// way that incandescent bulbs fade toward 'red' as they dim.
void coolLikeIncandescent( CRGB& c, uint8_t phase)
{
  if( phase < 128) return;

  uint8_t cooling = (phase - 128) >> 4;
  c.g = qsub8( c.g, cooling);
  c.b = qsub8( c.b, cooling * 2);
}



DEFINE_GRADIENT_PALETTE( heatmap_gp ) {
  0,     0,  0,  0,   //black
128,   255,  0,  0,   //red
224,   255,255,  0,   //bright yellow
255,   255,255,255 }; //full white

// Advance to the next color palette in the list (above).
void chooseNextColorPalette( CRGBPalette16& pal)
{
  //seed = seed + random8(-64,64); 
  //greater liveliness = larger jumps between palettes, from min. 32 to max 96
  uint8_t jump = (liveliness / 16) + 32;
  uint8_t negjump = jump * -1;
  seed = seed + random8(negjump, jump);
  
 // pal = heatmap_gp;
//  pal = CRGBPalette16(CHSV( seed - 64, 255, 255),
//                      CHSV( seed, 255, 255)); 

  //calc b to be between 64 and 255
  uint8_t b = (brightness * 0.1875) + 64;
  uint8_t bd = b/2;
  //cald twinkleDensity to be between 2 and 7
  twinkleDensity = (brightness / 204) + 2;

  //work paletteSpread min: 32 total, -16,2,2,16 to 96 total, -48, 6, 6, 48 so calc to 2-6
  //liveliness/220 (0-4 with a reasonable bit of 4 at the high end 
  uint8_t paletteSpread = (liveliness/220)+2;
  pal = CRGBPalette16(CHSV( seed - (paletteSpread * 8), 255, bd),
                      CHSV( seed - paletteSpread, 255, b),
                      CHSV( seed + paletteSpread, 255, b),
                      CHSV( seed + (paletteSpread * 8), 255, bd)); 
  
}
