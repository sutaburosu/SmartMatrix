#include "Layer_Background.h"

// TODO: get color correction working

// TODO: get these from screenconfig?
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 32


// needed for lutInterpolate, and must be included after Arduino.h (contains definition for PI - conflict with wiring.h)
#include "arm_math.h"

static color_chan_t backgroundColorCorrectionLUT[256];

// buffers are pointers to a 2-dimensional array of unknown size * MATRIX_WIDTH
static rgb24 *currentDrawBufferPtr;
static rgb24 *currentRefreshBufferPtr;
#ifdef SMARTMATRIX_TRIPLEBUFFER
rgb24 *previousRefreshBufferPtr;
unsigned char SMLayerBackground::previousRefreshBuffer = 2;
#endif
unsigned char SMLayerBackground::currentDrawBuffer = 0;
unsigned char SMLayerBackground::currentRefreshBuffer = 1;
volatile bool SMLayerBackground::swapPending = false;
static bitmap_font *font = (bitmap_font *) &apple3x5;


SMLayerBackground::SMLayerBackground(rgb24 * buffer) {
    backgroundBuffer = buffer;

    currentDrawBufferPtr = &backgroundBuffer[0 * (MATRIX_WIDTH * MATRIX_HEIGHT)];
    currentRefreshBufferPtr = &backgroundBuffer[1 * (MATRIX_WIDTH * MATRIX_HEIGHT)];
#ifdef SMARTMATRIX_TRIPLEBUFFER
    previousRefreshBufferPtr = &backgroundBuffer[2 * (MATRIX_WIDTH * MATRIX_HEIGHT)];
#endif
}

void SMLayerBackground::frameRefreshCallback(void) {
    handleBufferSwap();
    calculateBackgroundLUT(backgroundColorCorrectionLUT, backgroundBrightness);

#ifdef SMARTMATRIX_TRIPLEBUFFER
    fcCoefficient = calculateFcInterpCoefficient();
    icPrev = 257 * (0x10000 - fcCoefficient);
    icNext = 257 * fcCoefficient;
#endif
}

void SMLayerBackground::getRefreshPixel(uint8_t hardwareX, uint8_t hardwareY, rgb48 &xyPixel) {
    rgb24 currentPixel = currentRefreshBufferPtr[(hardwareY * MATRIX_HEIGHT) + hardwareX];

#ifdef SMARTMATRIX_TRIPLEBUFFER
    rgb24 prevPixel = previousRefreshBufferPtr[(hardwareY * MATRIX_HEIGHT) + hardwareX];
    refreshPixel.red = lutInterpolate(lightPowerMap16bit2, ((prevPixel.red * icPrev + currentPixel.red * icNext) >> 16));
    refreshPixel.green = lutInterpolate(lightPowerMap16bit2, ((prevPixel.green * icPrev + currentPixel.green * icNext) >> 16));
    refreshPixel.blue = lutInterpolate(lightPowerMap16bit2, ((prevPixel.blue * icPrev + currentPixel.blue * icNext) >> 16));
#else
    // do once per refresh
    bool bHasCC = ccmode != ccNone;

    if(bHasCC) {
        // load background pixel with color correction
        xyPixel.red = backgroundColorCorrection(currentPixel.red);
        xyPixel.green = backgroundColorCorrection(currentPixel.green);
        xyPixel.blue = backgroundColorCorrection(currentPixel.blue);
    } else {
        // load background pixel without color correction
        xyPixel.red = currentPixel.red << 8;
        xyPixel.green = currentPixel.green << 8;
        xyPixel.blue = currentPixel.blue << 8;
    }
#endif
}

void SMLayerBackground::getRefreshPixel(uint8_t hardwareX, uint8_t hardwareY, rgb24 &xyPixel) {
    rgb24 currentPixel = currentRefreshBufferPtr[(hardwareY * MATRIX_HEIGHT) + hardwareX];

    // do once per refresh
    bool bHasCC = ccmode != ccNone;

    if(bHasCC) {
        // load background pixel with color correction
        xyPixel.red = backgroundColorCorrection(currentPixel.red);
        xyPixel.green = backgroundColorCorrection(currentPixel.green);
        xyPixel.blue = backgroundColorCorrection(currentPixel.blue);
    } else {
        // load background pixel without color correction
        xyPixel.red = currentPixel.red;
        xyPixel.green = currentPixel.green;
        xyPixel.blue = currentPixel.blue;
    }
}



color_chan_t SMLayerBackground::backgroundColorCorrection(uint8_t inputcolor) {
    return backgroundColorCorrectionLUT[inputcolor];
}


// coordinates based on screen position, which is between 0-localWidth/localHeight
void SMLayerBackground::getPixel(uint8_t x, uint8_t y, rgb24 *xyPixel) {
    copyRgb24(*xyPixel, currentRefreshBufferPtr[(y * MATRIX_HEIGHT) + x]);
}

volatile int totalFramesToInterpolate;
volatile int framesInterpolated;

#ifdef SMARTMATRIX_TRIPLEBUFFER
// function from fadecandy
uint32_t SMLayerBackground::calculateFcInterpCoefficient()
{
    /*
     * Calculate our interpolation coefficient. This is a value between
     * 0x0000 and 0x10000, representing some point in between fbPrev and fbNext.
     *
     * We timestamp each frame at the moment its final packet has been received.
     * In other words, fbNew has no valid timestamp yet, and fbPrev/fbNext both
     * have timestamps in the recent past.
     *
     * fbNext's timestamp indicates when both fbPrev and fbNext entered their current
     * position in the keyframe queue. The difference between fbPrev and fbNext indicate
     * how long the interpolation between those keyframes should take.
     */

    if(framesInterpolated >= totalFramesToInterpolate)
        return 0x10000;

    return (0x10000 * framesInterpolated) / totalFramesToInterpolate;
}

rgb24 *SMLayerBackground::getPreviousRefreshRow(uint8_t y) {
  return &previousRefreshBufferPtr[y*MATRIX_HEIGHT];
}
#endif

rgb24 *SMLayerBackground::getCurrentRefreshRow(uint8_t y) {
  return &currentRefreshBufferPtr[y*MATRIX_HEIGHT];
}

#ifdef SMARTMATRIX_TRIPLEBUFFER
uint32_t fcCoefficient;
uint32_t icPrev;
uint32_t icNext;

// generated by adafruit utility included with matrix library
// options: planes = 16 and GAMMA = 2.5
// shifted over by 1 to work with fadecandy interpolation, extra 0xffff added to end
static const uint16_t lightPowerMap16bit2[] = {
    0x00 >> 1, 0x00 >> 1, 0x00 >> 1, 0x01 >> 1, 0x02 >> 1, 0x04 >> 1, 0x06 >> 1, 0x08 >> 1,
    0x0b >> 1, 0x0f >> 1, 0x14 >> 1, 0x19 >> 1, 0x1f >> 1, 0x26 >> 1, 0x2e >> 1, 0x37 >> 1,
    0x41 >> 1, 0x4b >> 1, 0x57 >> 1, 0x63 >> 1, 0x71 >> 1, 0x80 >> 1, 0x8f >> 1, 0xa0 >> 1,
    0xb2 >> 1, 0xc5 >> 1, 0xda >> 1, 0xef >> 1, 0x106 >> 1, 0x11e >> 1, 0x137 >> 1, 0x152 >> 1,
    0x16e >> 1, 0x18b >> 1, 0x1a9 >> 1, 0x1c9 >> 1, 0x1eb >> 1, 0x20e >> 1, 0x232 >> 1, 0x257 >> 1,
    0x27f >> 1, 0x2a7 >> 1, 0x2d2 >> 1, 0x2fd >> 1, 0x32b >> 1, 0x359 >> 1, 0x38a >> 1, 0x3bc >> 1,
    0x3ef >> 1, 0x425 >> 1, 0x45c >> 1, 0x494 >> 1, 0x4cf >> 1, 0x50b >> 1, 0x548 >> 1, 0x588 >> 1,
    0x5c9 >> 1, 0x60c >> 1, 0x651 >> 1, 0x698 >> 1, 0x6e0 >> 1, 0x72a >> 1, 0x776 >> 1, 0x7c4 >> 1,
    0x814 >> 1, 0x866 >> 1, 0x8b9 >> 1, 0x90f >> 1, 0x967 >> 1, 0x9c0 >> 1, 0xa1b >> 1, 0xa79 >> 1,
    0xad8 >> 1, 0xb3a >> 1, 0xb9d >> 1, 0xc03 >> 1, 0xc6a >> 1, 0xcd4 >> 1, 0xd3f >> 1, 0xdad >> 1,
    0xe1d >> 1, 0xe8f >> 1, 0xf03 >> 1, 0xf79 >> 1, 0xff2 >> 1, 0x106c >> 1, 0x10e9 >> 1, 0x1168 >> 1,
    0x11e9 >> 1, 0x126c >> 1, 0x12f2 >> 1, 0x137a >> 1, 0x1404 >> 1, 0x1490 >> 1, 0x151f >> 1, 0x15b0 >> 1,
    0x1643 >> 1, 0x16d9 >> 1, 0x1771 >> 1, 0x180b >> 1, 0x18a7 >> 1, 0x1946 >> 1, 0x19e8 >> 1, 0x1a8b >> 1,
    0x1b32 >> 1, 0x1bda >> 1, 0x1c85 >> 1, 0x1d33 >> 1, 0x1de2 >> 1, 0x1e95 >> 1, 0x1f49 >> 1, 0x2001 >> 1,
    0x20bb >> 1, 0x2177 >> 1, 0x2236 >> 1, 0x22f7 >> 1, 0x23bb >> 1, 0x2481 >> 1, 0x254a >> 1, 0x2616 >> 1,
    0x26e4 >> 1, 0x27b5 >> 1, 0x2888 >> 1, 0x295e >> 1, 0x2a36 >> 1, 0x2b11 >> 1, 0x2bef >> 1, 0x2cd0 >> 1,
    0x2db3 >> 1, 0x2e99 >> 1, 0x2f81 >> 1, 0x306d >> 1, 0x315a >> 1, 0x324b >> 1, 0x333f >> 1, 0x3435 >> 1,
    0x352e >> 1, 0x3629 >> 1, 0x3728 >> 1, 0x3829 >> 1, 0x392d >> 1, 0x3a33 >> 1, 0x3b3d >> 1, 0x3c49 >> 1,
    0x3d59 >> 1, 0x3e6b >> 1, 0x3f80 >> 1, 0x4097 >> 1, 0x41b2 >> 1, 0x42d0 >> 1, 0x43f0 >> 1, 0x4513 >> 1,
    0x463a >> 1, 0x4763 >> 1, 0x488f >> 1, 0x49be >> 1, 0x4af0 >> 1, 0x4c25 >> 1, 0x4d5d >> 1, 0x4e97 >> 1,
    0x4fd5 >> 1, 0x5116 >> 1, 0x525a >> 1, 0x53a1 >> 1, 0x54eb >> 1, 0x5638 >> 1, 0x5787 >> 1, 0x58da >> 1,
    0x5a31 >> 1, 0x5b8a >> 1, 0x5ce6 >> 1, 0x5e45 >> 1, 0x5fa7 >> 1, 0x610d >> 1, 0x6276 >> 1, 0x63e1 >> 1,
    0x6550 >> 1, 0x66c2 >> 1, 0x6837 >> 1, 0x69af >> 1, 0x6b2b >> 1, 0x6caa >> 1, 0x6e2b >> 1, 0x6fb0 >> 1,
    0x7139 >> 1, 0x72c4 >> 1, 0x7453 >> 1, 0x75e5 >> 1, 0x777a >> 1, 0x7912 >> 1, 0x7aae >> 1, 0x7c4c >> 1,
    0x7def >> 1, 0x7f94 >> 1, 0x813d >> 1, 0x82e9 >> 1, 0x8498 >> 1, 0x864b >> 1, 0x8801 >> 1, 0x89ba >> 1,
    0x8b76 >> 1, 0x8d36 >> 1, 0x8efa >> 1, 0x90c0 >> 1, 0x928a >> 1, 0x9458 >> 1, 0x9629 >> 1, 0x97fd >> 1,
    0x99d4 >> 1, 0x9bb0 >> 1, 0x9d8e >> 1, 0x9f70 >> 1, 0xa155 >> 1, 0xa33e >> 1, 0xa52a >> 1, 0xa71a >> 1,
    0xa90d >> 1, 0xab04 >> 1, 0xacfe >> 1, 0xaefb >> 1, 0xb0fc >> 1, 0xb301 >> 1, 0xb509 >> 1, 0xb715 >> 1,
    0xb924 >> 1, 0xbb37 >> 1, 0xbd4d >> 1, 0xbf67 >> 1, 0xc184 >> 1, 0xc3a5 >> 1, 0xc5ca >> 1, 0xc7f2 >> 1,
    0xca1e >> 1, 0xcc4d >> 1, 0xce80 >> 1, 0xd0b7 >> 1, 0xd2f1 >> 1, 0xd52f >> 1, 0xd771 >> 1, 0xd9b6 >> 1,
    0xdbfe >> 1, 0xde4b >> 1, 0xe09b >> 1, 0xe2ef >> 1, 0xe547 >> 1, 0xe7a2 >> 1, 0xea01 >> 1, 0xec63 >> 1,
    0xeeca >> 1, 0xf134 >> 1, 0xf3a2 >> 1, 0xf613 >> 1, 0xf888 >> 1, 0xfb02 >> 1, 0xfd7e >> 1, 0xffff >> 1,
    0xffff >> 1
};

//ALWAYS_INLINE
static inline
uint32_t lutInterpolate(const uint16_t *lut, uint32_t arg)
{
    /*
     * Using our color LUT for the indicated channel, convert the
     * 16-bit intensity "arg" in our input colorspace to a corresponding
     * 16-bit intensity in the device colorspace.
     *
     * Remember that our LUT is 257 entries long. The final entry corresponds to an
     * input of 0x10000, which can't quite be reached.
     *
     * 'arg' is in the range [0, 0xFFFF]
     *
     * This operation is equivalent to the following:
     *
     *      unsigned index = arg >> 8;          // Range [0, 0xFF]
     *      unsigned alpha = arg & 0xFF;        // Range [0, 0xFF]
     *      unsigned invAlpha = 0x100 - alpha;  // Range [1, 0x100]
     *
     *      // Result in range [0, 0xFFFF]
     *      return (lut[index] * invAlpha + lut[index + 1] * alpha) >> 8;
     *
     * This is easy to understand, but it turns out to be a serious bottleneck
     * in terms of speed and memory bandwidth, as well as register pressure that
     * affects the compilation of updatePixel().
     *
     * To speed this up, we try and do the lut[index] and lut[index+1] portions
     * in parallel using the SMUAD instruction. This is a pair of 16x16 multiplies,
     * and the results are added together. We can combine this with an unaligned load
     * to grab two adjacent entries from the LUT. The remaining complications are:
     *
     *   1. We wanted unsigned, not signed
     *   2. We still need to generate the input values efficiently.
     *
     * (1) is easy to solve if we're okay with 15-bit precision for the LUT instead
     * of 16-bit, which is fine. During LUT preparation, we right-shift each entry
     * by 1, keeping them within the positive range of a signed 16-bit int.
     *
     * For (2), we need to quickly put 'alpha' in the high halfword and invAlpha in
     * the low halfword, or vice versa. One fast way to do this is (0x01000000 + x - (x << 16).
     */

#if (ENABLE_FADECANDY_GAMMA_CORRECTION == 1)
    uint32_t index = arg >> 8;          // Range [0, 0xFF]

    // Load lut[index] into low halfword, lut[index+1] into high halfword.
    uint32_t pair = *(const uint32_t*)(lut + index);

    unsigned alpha = arg & 0xFF;        // Range [0, 0xFF]

    // Reversed halfword order
    uint32_t pairAlpha = (0x01000000 + alpha - (alpha << 16));

    return __SMUADX(pairAlpha, pair) >> 7;
#endif

    return arg;
}
#endif



// reads pixel from drawing buffer, not refresh buffer
const rgb24 SMLayerBackground::readPixel(int16_t x, int16_t y) {
    int hwx, hwy;

    // check for out of bounds coordinates
    if (x < 0 || y < 0 || x >= screenConfig.localWidth || y >= screenConfig.localHeight)
        return (rgb24){0, 0, 0};

    // map pixel into hardware buffer before writing
    if (screenConfig.rotation == rotation0) {
        hwx = x;
        hwy = y;
    } else if (screenConfig.rotation == rotation180) {
        hwx = (MATRIX_WIDTH - 1) - x;
        hwy = (MATRIX_HEIGHT - 1) - y;
    } else if (screenConfig.rotation == rotation90) {
        hwx = (MATRIX_WIDTH - 1) - y;
        hwy = x;
    } else { /* if (screenConfig.rotation == rotation270)*/
        hwx = y;
        hwy = (MATRIX_HEIGHT - 1) - x;
    }

    return currentDrawBufferPtr[(hwy * MATRIX_HEIGHT) + hwx];
}

void SMLayerBackground::drawPixel(int16_t x, int16_t y, const rgb24& color) {
    int hwx, hwy;

    // check for out of bounds coordinates
    if (x < 0 || y < 0 || x >= screenConfig.localWidth || y >= screenConfig.localHeight)
        return;

    // map pixel into hardware buffer before writing
    if (screenConfig.rotation == rotation0) {
        hwx = x;
        hwy = y;
    } else if (screenConfig.rotation == rotation180) {
        hwx = (MATRIX_WIDTH - 1) - x;
        hwy = (MATRIX_HEIGHT - 1) - y;
    } else if (screenConfig.rotation == rotation90) {
        hwx = (MATRIX_WIDTH - 1) - y;
        hwy = x;
    } else { /* if (screenConfig.rotation == rotation270)*/
        hwx = y;
        hwy = (MATRIX_HEIGHT - 1) - x;
    }

    copyRgb24(currentDrawBufferPtr[(hwy * MATRIX_HEIGHT) + hwx], color);
}

#define SWAPint(X,Y) { \
        int temp = X ; \
        X = Y ; \
        Y = temp ; \
    }

// x0, x1, and y must be in bounds (0-localWidth/Height), x1 > x0
void SMLayerBackground::drawHardwareHLine(uint8_t x0, uint8_t x1, uint8_t y, const rgb24& color) {
    int i;

    for (i = x0; i <= x1; i++) {
        copyRgb24(currentDrawBufferPtr[(y * MATRIX_HEIGHT) + i], color);
    }
}

// x, y0, and y1 must be in bounds (0-localWidth/Height), y1 > y0
void SMLayerBackground::drawHardwareVLine(uint8_t x, uint8_t y0, uint8_t y1, const rgb24& color) {
    int i;

    for (i = y0; i <= y1; i++) {
        copyRgb24(currentDrawBufferPtr[(i * MATRIX_HEIGHT) + x], color);
    }
}

void SMLayerBackground::drawFastHLine(int16_t x0, int16_t x1, int16_t y, const rgb24& color) {
    // make sure line goes from x0 to x1
    if (x1 < x0)
        SWAPint(x1, x0);

    // check for completely out of bounds line
    if (x1 < 0 || x0 >= screenConfig.localWidth || y < 0 || y >= screenConfig.localHeight)
        return;

    // truncate if partially out of bounds
    if (x0 < 0)
        x0 = 0;

    if (x1 >= screenConfig.localWidth)
        x1 = screenConfig.localWidth - 1;

    // map to hardware drawline function
    if (screenConfig.rotation == rotation0) {
        drawHardwareHLine(x0, x1, y, color);
    } else if (screenConfig.rotation == rotation180) {
        drawHardwareHLine((MATRIX_WIDTH - 1) - x1, (MATRIX_WIDTH - 1) - x0, (MATRIX_HEIGHT - 1) - y, color);
    } else if (screenConfig.rotation == rotation90) {
        drawHardwareVLine((MATRIX_WIDTH - 1) - y, x0, x1, color);
    } else { /* if (screenConfig.rotation == rotation270)*/
        drawHardwareVLine(y, (MATRIX_HEIGHT - 1) - x1, (MATRIX_HEIGHT - 1) - x0, color);
    }
}

void SMLayerBackground::drawFastVLine(int16_t x, int16_t y0, int16_t y1, const rgb24& color) {
    // make sure line goes from y0 to y1
    if (y1 < y0)
        SWAPint(y1, y0);

    // check for completely out of bounds line
    if (y1 < 0 || y0 >= screenConfig.localHeight || x < 0 || x >= screenConfig.localWidth)
        return;

    // truncate if partially out of bounds
    if (y0 < 0)
        y0 = 0;

    if (y1 >= screenConfig.localHeight)
        y1 = screenConfig.localHeight - 1;

    // map to hardware drawline function
    if (screenConfig.rotation == rotation0) {
        drawHardwareVLine(x, y0, y1, color);
    } else if (screenConfig.rotation == rotation180) {
        drawHardwareVLine((MATRIX_WIDTH - 1) - x, (MATRIX_HEIGHT - 1) - y1, (MATRIX_HEIGHT - 1) - y0, color);
    } else if (screenConfig.rotation == rotation90) {
        drawHardwareHLine((MATRIX_WIDTH - 1) - y1, (MATRIX_WIDTH - 1) - y0, x, color);
    } else { /* if (screenConfig.rotation == rotation270)*/
        drawHardwareHLine(y0, y1, (MATRIX_HEIGHT - 1) - x, color);
    }
}

void SMLayerBackground::bresteepline(int16_t x3, int16_t y3, int16_t x4, int16_t y4, const rgb24& color) {
    // if point x3, y3 is on the right side of point x4, y4, change them
    if ((x3 - x4) > 0) {
        bresteepline(x4, y4, x3, y3, color);
        return;
    }

    int x = x3, y = y3, sum = x4 - x3,  Dx = 2 * (x4 - x3), Dy = abs(2 * (y4 - y3));
    int prirastokDy = ((y4 - y3) > 0) ? 1 : -1;

    for (int i = 0; i <= x4 - x3; i++) {
        drawPixel(y, x, color);
        x++;
        sum -= Dy;
        if (sum < 0) {
            y = y + prirastokDy;
            sum += Dx;
        }
    }
}

// algorithm from http://www.netgraphics.sk/bresenham-algorithm-for-a-line
void SMLayerBackground::drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, const rgb24& color) {
    // if point x1, y1 is on the right side of point x2, y2, change them
    if ((x1 - x2) > 0) {
        drawLine(x2, y2, x1, y1, color);
        return;
    }
    // test inclination of line
    // function Math.abs(y) defines absolute value y
    if (abs(y2 - y1) > abs(x2 - x1)) {
        // line and y axis angle is less then 45 degrees
        // thats why go on the next procedure
        bresteepline(y1, x1, y2, x2, color); return;
    }
    // line and x axis angle is less then 45 degrees, so x is guiding
    // auxiliary variables
    int x = x1, y = y1, sum = x2 - x1, Dx = 2 * (x2 - x1), Dy = abs(2 * (y2 - y1));
    int prirastokDy = ((y2 - y1) > 0) ? 1 : -1;
    // draw line
    for (int i = 0; i <= x2 - x1; i++) {
        drawPixel(x, y, color);
        x++;
        sum -= Dy;
        if (sum < 0) {
            y = y + prirastokDy;
            sum += Dx;
        }
    }
}

// algorithm from http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
void SMLayerBackground::drawCircle(int16_t x0, int16_t y0, uint16_t radius, const rgb24& color)
{
    int a = radius, b = 0;
    int radiusError = 1 - a;

    if (radius == 0) {
        drawPixel(x0, y0, color);
        return;
    }

    while (a >= b)
    {
        drawPixel(a + x0, b + y0, color);
        drawPixel(b + x0, a + y0, color);
        drawPixel(-a + x0, b + y0, color);
        drawPixel(-b + x0, a + y0, color);
        drawPixel(-a + x0, -b + y0, color);
        drawPixel(-b + x0, -a + y0, color);
        drawPixel(a + x0, -b + y0, color);
        drawPixel(b + x0, -a + y0, color);

        b++;
        if (radiusError < 0)
            radiusError += 2 * b + 1;
        else
        {
            a--;
            radiusError += 2 * (b - a + 1);
        }
    }
}

// algorithm from drawCircle rearranged with hlines drawn between points on the radius
void SMLayerBackground::fillCircle(int16_t x0, int16_t y0, uint16_t radius, const rgb24& outlineColor, const rgb24& fillColor)
{
    int a = radius, b = 0;
    int radiusError = 1 - a;

    if (radius == 0)
        return;

    // only draw one line per row, skipping the top and bottom
    bool hlineDrawn = true;

    while (a >= b)
    {
        // this pair sweeps from horizontal center down
        drawPixel(a + x0, b + y0, outlineColor);
        drawPixel(-a + x0, b + y0, outlineColor);
        drawFastHLine((a - 1) + x0, (-a + 1) + x0, b + y0, fillColor);

        // this pair sweeps from bottom up
        drawPixel(b + x0, a + y0, outlineColor);
        drawPixel(-b + x0, a + y0, outlineColor);

        // this pair sweeps from horizontal center up
        drawPixel(-a + x0, -b + y0, outlineColor);
        drawPixel(a + x0, -b + y0, outlineColor);
        drawFastHLine((a - 1) + x0, (-a + 1) + x0, -b + y0, fillColor);

        // this pair sweeps from top down
        drawPixel(-b + x0, -a + y0, outlineColor);
        drawPixel(b + x0, -a + y0, outlineColor);

        if (b > 1 && !hlineDrawn) {
            drawFastHLine((b - 1) + x0, (-b + 1) + x0, a + y0, fillColor);
            drawFastHLine((b - 1) + x0, (-b + 1) + x0, -a + y0, fillColor);
            hlineDrawn = true;
        }

        b++;
        if (radiusError < 0) {
            radiusError += 2 * b + 1;
        } else {
            a--;
            hlineDrawn = false;
            radiusError += 2 * (b - a + 1);
        }
    }
}

// algorithm from drawCircle rearranged with hlines drawn between points on the raidus
void SMLayerBackground::fillCircle(int16_t x0, int16_t y0, uint16_t radius, const rgb24& fillColor)
{
    int a = radius, b = 0;
    int radiusError = 1 - a;

    if (radius == 0)
        return;

    // only draw one line per row, skipping the top and bottom
    bool hlineDrawn = true;

    while (a >= b)
    {
        // this pair sweeps from horizontal center down
        drawFastHLine((a - 1) + x0, (-a + 1) + x0, b + y0, fillColor);

        // this pair sweeps from horizontal center up
        drawFastHLine((a - 1) + x0, (-a + 1) + x0, -b + y0, fillColor);

        if (b > 1 && !hlineDrawn) {
            drawFastHLine((b - 1) + x0, (-b + 1) + x0, a + y0, fillColor);
            drawFastHLine((b - 1) + x0, (-b + 1) + x0, -a + y0, fillColor);
            hlineDrawn = true;
        }

        b++;
        if (radiusError < 0) {
            radiusError += 2 * b + 1;
        } else {
            a--;
            hlineDrawn = false;
            radiusError += 2 * (b - a + 1);
        }
    }
}

// from https://web.archive.org/web/20120225095359/http://homepage.smc.edu/kennedy_john/belipse.pdf
void SMLayerBackground::drawEllipse(int16_t x0, int16_t y0, uint16_t radiusX, uint16_t radiusY, const rgb24& color) {
    int16_t twoASquare = 2 * radiusX * radiusX;
    int16_t twoBSquare = 2 * radiusY * radiusY;
    
    int16_t x = radiusX;
    int16_t y = 0;
    int16_t changeX = radiusY * radiusY * (1 - (2 * radiusX));
    int16_t changeY = radiusX * radiusX;
    int16_t ellipseError = 0;
    int16_t stoppingX = twoBSquare * radiusX;
    int16_t stoppingY = 0;
    
    while (stoppingX >= stoppingY) {    // first set of points, y' > -1
        drawPixel(x0 + x, y0 + y, color);
        drawPixel(x0 - x, y0 + y, color);
        drawPixel(x0 - x, y0 - y, color);
        drawPixel(x0 + x, y0 - y, color);
        
        y++;
        stoppingY += twoASquare;
        ellipseError += changeY;
        changeY += twoASquare;
        
        if (((2 * ellipseError) + changeX) > 0) {
            x--;
            stoppingX -= twoBSquare;
            ellipseError += changeX;
            changeX += twoBSquare;
        }
    }
    
    // first point set is done, start the second set of points
    
    x = 0;
    y = radiusY;
    changeX = radiusY * radiusY;
    changeY = radiusX * radiusX * (1 - 2 * radiusY);
    ellipseError = 0;
    stoppingX = 0;
    stoppingY = twoASquare * radiusY;
    
    while (stoppingX <= stoppingY) {    // second set of points, y' < -1
        drawPixel(x0 + x, y0 + y, color);
        drawPixel(x0 - x, y0 + y, color);
        drawPixel(x0 - x, y0 - y, color);
        drawPixel(x0 + x, y0 - y, color);
        
        x++;
        stoppingX += twoBSquare;
        ellipseError += changeX;
        changeX += twoBSquare;
        
        if (((2 * ellipseError) + changeY) > 0) {
            y--;
            stoppingY -= twoASquare;
            ellipseError += changeY;
            changeY += twoASquare;
        }
    }
}

void SMLayerBackground::fillRoundRectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
  uint16_t radius, const rgb24& fillColor) {
    fillRoundRectangle(x0, y0, x1, y1, radius, fillColor, fillColor);
}

void SMLayerBackground::fillRoundRectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
  uint16_t radius, const rgb24& outlineColor, const rgb24& fillColor) {
    if (x1 < x0)
        SWAPint(x1, x0);

    if (y1 < y0)
        SWAPint(y1, y0);

    // decrease large radius that would break shape
    if(radius > (x1-x0)/2)
        radius = (x1-x0)/2;
    if(radius > (y1-y0)/2)
        radius = (y1-y0)/2;

    int a = radius, b = 0;
    int radiusError = 1 - a;

    if (radius == 0) {
        fillRectangle(x0, y0, x1, y1, outlineColor, fillColor);
    }

    // draw straight part of outline
    drawFastHLine(x0 + radius, x1 - radius, y0, outlineColor);
    drawFastHLine(x0 + radius, x1 - radius, y1, outlineColor);
    drawFastVLine(x0, y0 + radius, y1 - radius, outlineColor);
    drawFastVLine(x1, y0 + radius, y1 - radius, outlineColor);

    // convert coordinates to point at center of rounded sections
    x0 += radius;
    x1 -= radius;
    y0 += radius;
    y1 -= radius;

    // only draw one line per row/column, skipping the sides
    bool hlineDrawn = true;
    bool vlineDrawn = true;

    while (a >= b)
    {
        // this pair sweeps from far left towards right
        drawPixel(-a + x0, -b + y0, outlineColor);
        drawPixel(-a + x0, b + y1, outlineColor);

        // this pair sweeps from far right towards left
        drawPixel(a + x1, -b + y0, outlineColor);
        drawPixel(a + x1, b + y1, outlineColor);

        if (!vlineDrawn) {
            drawFastVLine(-a + x0, (-b + 1) + y0, (b - 1) + y1, fillColor);
            drawFastVLine(a + x1, (-b + 1) + y0, (b - 1) + y1, fillColor);
            vlineDrawn = true;
        }

        // this pair sweeps from very top towards bottom
        drawPixel(-b + x0, -a + y0, outlineColor);
        drawPixel(b + x1, -a + y0, outlineColor);

        // this pair sweeps from bottom up
        drawPixel(-b + x0, a + y1, outlineColor);
        drawPixel(b + x1, a + y1, outlineColor);

        if (!hlineDrawn) {
            drawFastHLine((-b + 1) + x0, (b - 1) + x1, -a + y0, fillColor);
            drawFastHLine((-b + 1) + x0, (b - 1) + x1, a + y1, fillColor);
            hlineDrawn = true;
        }

        b++;
        if (radiusError < 0) {
            radiusError += 2 * b + 1;
        } else {
            a--;
            hlineDrawn = false;
            vlineDrawn = false;
            radiusError += 2 * (b - a + 1);
        }
    }

    // draw rectangle in center
    fillRectangle(x0 - a, y0 - a, x1 + a, y1 + a, fillColor);
}

void SMLayerBackground::drawRoundRectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
  uint16_t radius, const rgb24& outlineColor) {
    if (x1 < x0)
        SWAPint(x1, x0);

    if (y1 < y0)
        SWAPint(y1, y0);

    // decrease large radius that would break shape
    if(radius > (x1-x0)/2)
        radius = (x1-x0)/2;
    if(radius > (y1-y0)/2)
        radius = (y1-y0)/2;

    int a = radius, b = 0;
    int radiusError = 1 - a;

    // draw straight part of outline
    drawFastHLine(x0 + radius, x1 - radius, y0, outlineColor);
    drawFastHLine(x0 + radius, x1 - radius, y1, outlineColor);
    drawFastVLine(x0, y0 + radius, y1 - radius, outlineColor);
    drawFastVLine(x1, y0 + radius, y1 - radius, outlineColor);

    // convert coordinates to point at center of rounded sections
    x0 += radius;
    x1 -= radius;
    y0 += radius;
    y1 -= radius;

    while (a >= b)
    {
        // this pair sweeps from far left towards right
        drawPixel(-a + x0, -b + y0, outlineColor);
        drawPixel(-a + x0, b + y1, outlineColor);

        // this pair sweeps from far right towards left
        drawPixel(a + x1, -b + y0, outlineColor);
        drawPixel(a + x1, b + y1, outlineColor);

        // this pair sweeps from very top towards bottom
        drawPixel(-b + x0, -a + y0, outlineColor);
        drawPixel(b + x1, -a + y0, outlineColor);

        // this pair sweeps from bottom up
        drawPixel(-b + x0, a + y1, outlineColor);
        drawPixel(b + x1, a + y1, outlineColor);

        b++;
        if (radiusError < 0) {
            radiusError += 2 * b + 1;
        } else {
            a--;
            radiusError += 2 * (b - a + 1);
        }
    }
}

// Code from http://www.sunshine2k.de/coding/java/TriangleRasterization/TriangleRasterization.html
void SMLayerBackground::fillFlatSideTriangleInt(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
  int16_t x3, int16_t y3, const rgb24& color) {
    int16_t t1x, t2x, t1y, t2y;
    bool changed1 = false;
    bool changed2 = false;
    int8_t signx1, signx2, signy1, signy2, dx1, dy1, dx2, dy2;
    int i;
    int8_t e1, e2;

    t1x = t2x = x1; t1y = t2y = y1; // Starting points

    dx1 = abs(x2 - x1);
    dy1 = abs(y2 - y1);
    dx2 = abs(x3 - x1);
    dy2 = abs(y3 - y1);

    if (x2 - x1 < 0) {
        signx1 = -1;
    } else signx1 = 1;
    if (x3 - x1 < 0) {
        signx2 = -1;
    } else signx2 = 1;
    if (y2 - y1 < 0) {
        signy1 = -1;
    } else signy1 = 1;
    if (y3 - y1 < 0) {
        signy2 = -1;
    } else signy2 = 1;

    if (dy1 > dx1) {   // swap values
        SWAPint(dx1, dy1);
        changed1 = true;
    }
    if (dy2 > dx2) {   // swap values
        SWAPint(dy2, dx2);
        changed2 = true;
    }

    e1 = 2 * dy1 - dx1;
    e2 = 2 * dy2 - dx2;

    for (i = 0; i <= dx1; i++)
    {
        drawFastHLine(t1x, t2x, t1y, color);

        while (dx1 > 0 && e1 >= 0)
        {
            if (changed1)
                t1x += signx1;
            else
                t1y += signy1;
            e1 = e1 - 2 * dx1;
        }

        if (changed1)
            t1y += signy1;
        else
            t1x += signx1;

        e1 = e1 + 2 * dy1;

        /* here we rendered the next point on line 1 so follow now line 2
         * until we are on the same y-value as line 1.
         */
        while (t2y != t1y)
        {
            while (dx2 > 0 && e2 >= 0)
            {
                if (changed2)
                    t2x += signx2;
                else
                    t2y += signy2;
                e2 = e2 - 2 * dx2;
            }

            if (changed2)
                t2y += signy2;
            else
                t2x += signx2;

            e2 = e2 + 2 * dy2;
        }
    }
}

// Code from http://www.sunshine2k.de/coding/java/TriangleRasterization/TriangleRasterization.html
void SMLayerBackground::fillTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, const rgb24& fillColor) {
    // Sort vertices
    if (y1 > y2) {
        SWAPint(y1, y2);
        SWAPint(x1, x2);
    }
    if (y1 > y3) {
        SWAPint(y1, y3);
        SWAPint(x1, x3);
    }
    if (y2 > y3) {
        SWAPint(y2, y3);
        SWAPint(x2, x3);
    }

    if (y2 == y3)
    {
        fillFlatSideTriangleInt(x1, y1, x2, y2, x3, y3, fillColor);
    }
    /* check for trivial case of top-flat triangle */
    else if (y1 == y2)
    {
        fillFlatSideTriangleInt(x3, y3, x1, y1, x2, y2, fillColor);
    }
    else
    {
        /* general case - split the triangle in a topflat and bottom-flat one */
        int16_t xtmp, ytmp;
        xtmp = (int)(x1 + ((float)(y2 - y1) / (float)(y3 - y1)) * (x3 - x1));
        ytmp = y2;
        fillFlatSideTriangleInt(x1, y1, x2, y2, xtmp, ytmp, fillColor);
        fillFlatSideTriangleInt(x3, y3, x2, y2, xtmp, ytmp, fillColor);
    }
}

void SMLayerBackground::fillTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3,
  const rgb24& outlineColor, const rgb24& fillColor) {
    fillTriangle(x1, y1, x2, y2, x3, y3, fillColor);
    drawTriangle(x1, y1, x2, y2, x3, y3, outlineColor);
}

void SMLayerBackground::drawTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, const rgb24& color) {
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x3, y3, color);
    drawLine(x1, y1, x3, y3, color);
}

void SMLayerBackground::drawRectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const rgb24& color) {
    drawFastHLine(x0, x1, y0, color);
    drawFastHLine(x0, x1, y1, color);
    drawFastVLine(x0, y0, y1, color);
    drawFastVLine(x1, y0, y1, color);
}

void SMLayerBackground::fillRectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const rgb24& color) {
    int i;
// Loop only works if y1 > y0
    if (y0 > y1) {
        SWAPint(y0, y1);
    };
// Putting the x coordinates in order saves multiple swaps in drawFastHLine
    if (x0 > x1) {
        SWAPint(x0, x1);
    };

    for (i = y0; i <= y1; i++) {
        drawFastHLine(x0, x1, i, color);
    }
}

void SMLayerBackground::fillScreen(const rgb24& color) {
    fillRectangle(0, 0, screenConfig.localWidth, screenConfig.localHeight, color);
}

void SMLayerBackground::fillRectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const rgb24& outlineColor, const rgb24& fillColor) {
    fillRectangle(x0, y0, x1, y1, fillColor);
    drawRectangle(x0, y0, x1, y1, outlineColor);
}

bool SMLayerBackground::getBitmapPixelAtXY(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t *bitmap) {
    int cell = (y * ((width / 8) + 1)) + (x / 8);

    uint8_t mask = 0x80 >> (x % 8);
    return (mask & bitmap[cell]);
}

void SMLayerBackground::setFont(fontChoices newFont) {
    font = (bitmap_font *)fontLookup(newFont);
}

void SMLayerBackground::drawChar(int16_t x, int16_t y, const rgb24& charColor, char character) {
    int xcnt, ycnt;

    for (ycnt = 0; ycnt < font->Height; ycnt++) {
        for (xcnt = 0; xcnt < font->Width; xcnt++) {
            if (getBitmapFontPixelAtXY(character, xcnt, ycnt, font)) {
                drawPixel(x + xcnt, y + ycnt, charColor);
            }
        }
    }
}

void SMLayerBackground::drawString(int16_t x, int16_t y, const rgb24& charColor, const char text[]) {
    int xcnt, ycnt, i = 0, offset = 0;
    char character;

    // limit text to 10 chars, why?
    for (i = 0; i < 10; i++) {
        character = text[offset++];
        if (character == '\0')
            return;

        for (ycnt = 0; ycnt < font->Height; ycnt++) {
            for (xcnt = 0; xcnt < font->Width; xcnt++) {
                if (getBitmapFontPixelAtXY(character, xcnt, ycnt, font)) {
                    drawPixel(x + xcnt, y + ycnt, charColor);
                }
            }
        }
        x += font->Width;
    }
}

// draw string while clearing background
void SMLayerBackground::drawString(int16_t x, int16_t y, const rgb24& charColor, const rgb24& backColor, const char text[]) {
    int xcnt, ycnt, i = 0, offset = 0;
    char character;

    // limit text to 10 chars, why?
    for (i = 0; i < 10; i++) {
        character = text[offset++];
        if (character == '\0')
            return;

        for (ycnt = 0; ycnt < font->Height; ycnt++) {
            for (xcnt = 0; xcnt < font->Width; xcnt++) {
                if (getBitmapFontPixelAtXY(character, xcnt, ycnt, font)) {
                    drawPixel(x + xcnt, y + ycnt, charColor);
                } else {
                    drawPixel(x + xcnt, y + ycnt, backColor);
                }
            }
        }
        x += font->Width;
    }
}

void SMLayerBackground::drawMonoBitmap(int16_t x, int16_t y, uint8_t width, uint8_t height,
  const rgb24& bitmapColor, const uint8_t *bitmap) {
    int xcnt, ycnt;

    for (ycnt = 0; ycnt < height; ycnt++) {
        for (xcnt = 0; xcnt < width; xcnt++) {
            if (getBitmapPixelAtXY(xcnt, ycnt, width, height, bitmap)) {
                drawPixel(x + xcnt, y + ycnt, bitmapColor);
            }
        }
    }
}

#ifdef SMARTMATRIX_TRIPLEBUFFER
int newFramesToInterpolate;
#endif

void SMLayerBackground::handleBufferSwap(void) {
#ifdef SMARTMATRIX_TRIPLEBUFFER
    if(framesInterpolated < totalFramesToInterpolate)
        framesInterpolated++;
#endif

    if (!swapPending)
        return;

#ifdef SMARTMATRIX_TRIPLEBUFFER
    framesInterpolated = 0;
    totalFramesToInterpolate = newFramesToInterpolate;

    unsigned char newDrawBuffer = previousRefreshBuffer;

    previousRefreshBuffer = currentRefreshBuffer;
    currentRefreshBuffer = currentDrawBuffer;
    currentDrawBuffer = newDrawBuffer;

    currentRefreshBufferPtr = &backgroundBuffer[currentRefreshBuffer * (MATRIX_WIDTH * MATRIX_HEIGHT)];
    previousRefreshBufferPtr = &backgroundBuffer[previousRefreshBuffer * (MATRIX_WIDTH * MATRIX_HEIGHT)];
    currentDrawBufferPtr = &backgroundBuffer[currentDrawBuffer * (MATRIX_WIDTH * MATRIX_HEIGHT)];
#else
    unsigned char newDrawBuffer = currentRefreshBuffer;

    currentRefreshBuffer = currentDrawBuffer;
    currentDrawBuffer = newDrawBuffer;

    currentRefreshBufferPtr = &backgroundBuffer[currentRefreshBuffer * (MATRIX_WIDTH * MATRIX_HEIGHT)];
    currentDrawBufferPtr = &backgroundBuffer[currentDrawBuffer * (MATRIX_WIDTH * MATRIX_HEIGHT)];
#endif

    swapPending = false;
}

// waits until previous swap is complete
// waits until current swap is complete if copy is enabled
void SMLayerBackground::swapBuffers(bool copy) {
    while (swapPending);

#ifdef SMARTMATRIX_TRIPLEBUFFER
    newFramesToInterpolate = 0;
#endif

    swapPending = true;

    if (copy) {
        while (swapPending);
        memcpy(currentDrawBufferPtr, currentRefreshBufferPtr, sizeof(rgb24) * (MATRIX_WIDTH * MATRIX_HEIGHT));
    }
}

#ifdef SMARTMATRIX_TRIPLEBUFFER
// waits until previous swap and previous interpolation span is complete
void SMLayerBackground::swapBuffersWithInterpolation_frames(int framesToInterpolate, bool copy) {
    while (swapPending);
    while (framesInterpolated < totalFramesToInterpolate);

    newFramesToInterpolate = framesToInterpolate;

    swapPending = true;
    if (copy) {
        while (swapPending);
        memcpy(currentDrawBufferPtr, currentRefreshBufferPtr, sizeof(rgb24) * (MATRIX_WIDTH * MATRIX_HEIGHT));
    }
}

// waits until previous swap and previous interpolation span is complete
void SMLayerBackground::swapBuffersWithInterpolation_ms(int interpolationSpan_ms, bool copy) {
    while (swapPending);
    while (framesInterpolated < totalFramesToInterpolate);

    newFramesToInterpolate = interpolationSpan_ms * MATRIX_REFRESH_RATE / 1000;

    swapPending = true;
    if (copy) {
        while (swapPending);
        memcpy(currentDrawBufferPtr, currentRefreshBufferPtr, sizeof(rgb24) * (MATRIX_WIDTH * MATRIX_HEIGHT));
    }
}
#endif

// return pointer to start of currentDrawBuffer, so application can do efficient loading of bitmaps
rgb24 *SMLayerBackground::backBuffer(void) {
    return currentDrawBufferPtr;
}

void SMLayerBackground::setBackBuffer(rgb24 *newBuffer) {
  currentDrawBufferPtr = newBuffer;
}

rgb24 *SMLayerBackground::getRealBackBuffer() {
  return &backgroundBuffer[currentDrawBuffer * (MATRIX_WIDTH * MATRIX_HEIGHT)];
}

uint8_t SMLayerBackground::backgroundBrightness = 255;

void SMLayerBackground::setBrightness(uint8_t brightness) {
    backgroundBrightness = brightness;
}

void SMLayerBackground::setColorCorrection(colorCorrectionModes mode) {
    ccmode = mode;
}

