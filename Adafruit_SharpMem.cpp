/*********************************************************************
This is an Arduino library for our Monochrome SHARP Memory Displays

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/products/1393

These displays use SPI to communicate, 3 pins are required to
interface

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.
BSD license, check license.txt for more information
All text above, and the splash screen must be included in any redistribution
*********************************************************************/

#include "Adafruit_SharpMem.h"

#ifndef _swap_int16_t
#define _swap_int16_t(a, b)                                                    \
  {                                                                            \
    int16_t t = a;                                                             \
    a = b;                                                                     \
    b = t;                                                                     \
  }
#endif
#ifndef _swap_uint16_t
#define _swap_uint16_t(a, b)                                                   \
  {                                                                            \
    uint16_t t = a;                                                            \
    a = b;                                                                     \
    b = t;                                                                     \
  }
#endif

/**************************************************************************
    Sharp Memory Display Connector
    -----------------------------------------------------------------------
    Pin   Function        Notes
    ===   ==============  ===============================
      1   VIN             3.3-5.0V (into LDO supply)
      2   3V3             3.3V out
      3   GND
      4   SCLK            Serial Clock
      5   MOSI            Serial Data Input
      6   CS              Serial Chip Select
      9   EXTMODE         COM Inversion Select (Low = SW clock/serial)
      7   EXTCOMIN        External COM Inversion Signal
      8   DISP            Display On(High)/Off(Low)

 **************************************************************************/

#define TOGGLE_VCOM                                                            \
  do {                                                                         \
    _sharpmem_vcom = _sharpmem_vcom ? 0x00 : SHARPMEM_BIT_VCOM;                \
  } while (0);

/**
 * @brief Construct a new Adafruit_SharpMem object with software SPI
 *
 * @param clk The clock pin
 * @param mosi The MOSI pin
 * @param cs The display chip select pin - **NOTE** this is ACTIVE HIGH!
 * @param width The display width
 * @param height The display height
 * @param freq The SPI clock frequency desired (unlikely to be that fast in soft
 * spi mode!)
 */
Adafruit_SharpMem::Adafruit_SharpMem(uint8_t clk, uint8_t mosi, uint8_t cs,
                                     uint16_t width, uint16_t height,
                                     uint32_t freq)
    : Adafruit_GFX(width, height) {
  _cs = cs;
  if (spidev) {
    delete spidev;
  }
  spidev =
      new Adafruit_SPIDevice(cs, clk, -1, mosi, freq, SPI_BITORDER_LSBFIRST);
}

/**
 * @brief Construct a new Adafruit_SharpMem object with hardware SPI
 *
 * @param theSPI Pointer to hardware SPI device you want to use
 * @param cs The display chip select pin - **NOTE** this is ACTIVE HIGH!
 * @param width The display width
 * @param height The display height
 * @param freq The SPI clock frequency desired
 */
Adafruit_SharpMem::Adafruit_SharpMem(SPIClass *theSPI, uint8_t cs,
                                     uint16_t width, uint16_t height,
                                     uint32_t freq)
    : Adafruit_GFX(width, height) {
  _cs = cs;
  if (spidev) {
    delete spidev;
  }
  spidev = new Adafruit_SPIDevice(cs, freq, SPI_BITORDER_LSBFIRST, SPI_MODE0,
                                  theSPI);
}

/**
 * @brief Start the driver object, setting up pins and configuring a buffer for
 * the screen contents
 *
 * @return boolean true: success false: failure
 */
bool Adafruit_SharpMem::begin(void) {
  if (!spidev->begin()) {
    return false;
  }
  // this display is weird in that _cs is active HIGH not LOW like every other
  // SPI device
  digitalWrite(_cs, LOW);

  // Set the vcom bit to a defined state
  _sharpmem_vcom = SHARPMEM_BIT_VCOM;

  sharpmem_buffer = (uint8_t *)malloc((WIDTH * HEIGHT) / 8);

  if (!sharpmem_buffer)
    return false;

  setRotation(0);

  return true;
}

// 1<<n is a costly operation on AVR -- table usu. smaller & faster
static const uint8_t set[] = {1, 2, 4, 8, 16, 32, 64, 128},
                     clr[] = {(uint8_t)~1,  (uint8_t)~2,  (uint8_t)~4,
                              (uint8_t)~8,  (uint8_t)~16, (uint8_t)~32,
                              (uint8_t)~64, (uint8_t)~128};

/**************************************************************************/
/*!
    @brief Draws a single pixel in image buffer

    @param[in]  x
                The x position (0 based)
    @param[in]  y
                The y position (0 based)
    @param color The color to set:
    * **0**: Black
    * **1**: White
*/
/**************************************************************************/
void Adafruit_SharpMem::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height))
    return;

  switch (rotation) {
  case 1:
    _swap_int16_t(x, y);
    x = WIDTH - 1 - x;
    break;
  case 2:
    x = WIDTH - 1 - x;
    y = HEIGHT - 1 - y;
    break;
  case 3:
    _swap_int16_t(x, y);
    y = HEIGHT - 1 - y;
    break;
  }

  switch (color) {
  case 1: // WHITE white
    // sharpmem_buffer[(y * WIDTH + x) / 8] |= set[x & 7];
    sharpmem_buffer[(x / 8) + y * ((WIDTH + 7) / 8)] |= set[x & 7];
    break;
  default:
  case 0: // BLACK black
    // sharpmem_buffer[(y * WIDTH + x) / 8] &= clr[x & 7];
    sharpmem_buffer[(x / 8) + y * ((WIDTH + 7) / 8)] &= clr[x & 7];
    break;
  case 7:
    // line pattern reversed
    if (y % 3 == 2 - (x % 3)) {
      sharpmem_buffer[(y * WIDTH + x) / 8] &= clr[x & 7];
    } else {
      sharpmem_buffer[(y * WIDTH + x) / 8] |= set[x & 7];
    }
    break;
  case 6:
    // line pattern
    if (y % 3 == x % 3) {
      sharpmem_buffer[(y * WIDTH + x) / 8] &= clr[x & 7];
    } else {
      sharpmem_buffer[(y * WIDTH + x) / 8] |= set[x & 7];
    }
    break;
  case 5: // PATTERN
    // V1
    if ((y % 4 == 0 && x % 4 == 2) || // line 0
        (x % 2 == 1 && y % 2 == 1) || // line 1 & 3
        (y % 4 == 2 && x % 4 == 0)    // line 2
    ) {
      // black
      sharpmem_buffer[(y * WIDTH + x) / 8] &= clr[x & 7];
    } else {
      sharpmem_buffer[(y * WIDTH + x) / 8] |= set[x & 7];
    }

    break;
  case 4: // LIGHT lighter gray
    if (y % 2 != 0) {
      sharpmem_buffer[(y * WIDTH + x) / 8] |= set[x & 7];
    } else if ((x + 2 * ((y / 2) % 2)) % 4 == 0) { // on
      sharpmem_buffer[(y * WIDTH + x) / 8] &= clr[x & 7];
    } else {
      sharpmem_buffer[(y * WIDTH + x) / 8] |= set[x & 7];
    }
    break;
  case 3:             // DARK darker gray
    if (y % 2 != 0) { // off
      sharpmem_buffer[(y * WIDTH + x) / 8] &= clr[x & 7];
    } else if ((x + 2 * ((y / 2) % 2)) % 4 == 0) { // on
      sharpmem_buffer[(y * WIDTH + x) / 8] |= set[x & 7];
    } else {
      sharpmem_buffer[(y * WIDTH + x) / 8] &= clr[x & 7];
    }
    break;
  case 2: // GRAY medium gray
    if (((x + y) % 2 == 0)) {
      sharpmem_buffer[(y * WIDTH + x) / 8] |= set[x & 7];
    } else {
      sharpmem_buffer[(y * WIDTH + x) / 8] &= clr[x & 7];
    }
    break;
  }
}

/**************************************************************************/
/*!
    @brief Gets the value (1 or 0) of the specified pixel from the buffer

    @param[in]  x
                The x position (0 based)
    @param[in]  y
                The y position (0 based)

    @return     1 if the pixel is enabled, 0 if disabled
*/
/**************************************************************************/
uint8_t Adafruit_SharpMem::getPixel(uint16_t x, uint16_t y) {
  if ((x >= _width) || (y >= _height))
    return 0; // <0 test not needed, unsigned

  switch (rotation) {
  case 1:
    _swap_uint16_t(x, y);
    x = WIDTH - 1 - x;
    break;
  case 2:
    x = WIDTH - 1 - x;
    y = HEIGHT - 1 - y;
    break;
  case 3:
    _swap_uint16_t(x, y);
    y = HEIGHT - 1 - y;
    break;
  }

  return sharpmem_buffer[(y * WIDTH + x) / 8] & set[x & 7] ? 1 : 0;
}

/**************************************************************************/
/*!
    @brief Draw a fat line
*/
/**************************************************************************/
void Adafruit_SharpMem::drawFatLine(int16_t x0, int16_t y0, // first point
                                    int16_t x1, int16_t y1, // second point
                                    int16_t strokeWidth,    // stroke width
                                    uint16_t color) {
  if (strokeWidth < 1) {
    return;
  }
  // create perpendicular vector
  float px = y1 - y0;
  float py = -(x1 - x0);
  // calculate length to normalize perpendicular vector
  float l = sqrt(px * px + py * py);
  if (l < 1) {
    // do not divide by zero
    // do not draw a line too short
    return;
  }
  // normalize and scale to strokewidth
  px = (float)strokeWidth * px / l;
  py = (float)strokeWidth * py / l;

  // finally draw our line!
  fillTriangle(x0 + (int)px, y0 + (int)py, // a
               x1 + (int)px, y1 + (int)py, // b
               x1 - (int)px, y1 - (int)py, // c
               color);
  fillTriangle(x0 + (int)px, y0 + (int)py, // a
               x1 - (int)px, y1 - (int)py, // c
               x0 - (int)px, y0 - (int)py, // d
               color);
}

/**************************************************************************/
/*!
    @brief Clears the screen
*/
/**************************************************************************/
void Adafruit_SharpMem::clearDisplay() {
  memset(sharpmem_buffer, 0xff, (WIDTH * HEIGHT) / 8);

  spidev->beginTransaction();
  // Send the clear screen command rather than doing a HW refresh (quicker)
  digitalWrite(_cs, HIGH);

  uint8_t clear_data[2] = {(uint8_t)(_sharpmem_vcom | SHARPMEM_BIT_CLEAR),
                           0x00};
  spidev->transfer(clear_data, 2);

  TOGGLE_VCOM;
  digitalWrite(_cs, LOW);
  spidev->endTransaction();
}

/**************************************************************************/
/*!
    @brief Renders the contents of the pixel buffer on the LCD
*/
/**************************************************************************/
void Adafruit_SharpMem::refresh(void) {
  uint16_t i, currentline;

  spidev->beginTransaction();
  // Send the write command
  digitalWrite(_cs, HIGH);

  spidev->transfer(_sharpmem_vcom | SHARPMEM_BIT_WRITECMD);
  TOGGLE_VCOM;

  uint8_t bytes_per_line = WIDTH / 8;
  uint16_t totalbytes = (WIDTH * HEIGHT) / 8;

  for (i = 0; i < totalbytes; i += bytes_per_line) {
    uint8_t line[bytes_per_line + 2];

    // Send address byte
    currentline = ((i + 1) / (WIDTH / 8)) + 1;
    line[0] = currentline;
    // copy over this line
    memcpy(line + 1, sharpmem_buffer + i, bytes_per_line);
    // Send end of line
    line[bytes_per_line + 1] = 0x00;
    // send it!
    spidev->transfer(line, bytes_per_line + 2);
  }

  // Send another trailing 8 bits for the last line
  spidev->transfer(0x00);
  digitalWrite(_cs, LOW);
  spidev->endTransaction();
}

/**************************************************************************/
/*!
    @brief Clears the display buffer without outputting to the display
*/
/**************************************************************************/
void Adafruit_SharpMem::clearDisplayBuffer() {
  memset(sharpmem_buffer, 0xff, (WIDTH * HEIGHT) / 8);
}

/**************************************************************************/
/*!
    @brief access to the raw display buffer
*/
/**************************************************************************/
void Adafruit_SharpMem::copyPixelBuffer(uint8_t *bitmap) {
  memcpy(bitmap, sharpmem_buffer, (WIDTH * HEIGHT) / 8);
}
/**************************************************************************/
/*!
    @brief fills the display buffer with contents of bitmap without outputting
   to the display
*/
/**************************************************************************/

void Adafruit_SharpMem::setBitmap(uint8_t *bitmap) {
  memcpy(sharpmem_buffer, bitmap, (WIDTH * HEIGHT) / 8);
}

/**************************************************************************/
void Adafruit_SharpMem::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                 uint16_t color) {
  for (int16_t i = y; i < y + h; i++) {
    drawFastHLine(x, i, w, color);
  }
}

/**************************************************************************/
void Adafruit_SharpMem::drawFastHLine(int16_t x, int16_t y, int16_t w,
                                      uint16_t color) {
  if (w < 0) { // Convert negative widths to positive equivalent
    w *= -1;
    x -= w - 1;
    if (x < 0) {
      w += x;
      x = 0;
    }
  }

  // Edge rejection (no-draw if totally off canvas)
  if ((y < 0) || (y >= height()) || (x >= width()) || ((x + w - 1) < 0)) {
    return;
  }

  if (x < 0) { // Clip left
    w += x;
    x = 0;
  }
  if (x + w >= width()) { // Clip right
    w = width() - x;
  }

  if (getRotation() == 0) {
    drawFastRawHLine(x, y, w, color);
  } else if (getRotation() == 1) {
    int16_t t = x;
    x = WIDTH - 1 - y;
    y = t;
    drawFastRawVLine(x, y, w, color);
  } else if (getRotation() == 2) {
    x = WIDTH - 1 - x;
    y = HEIGHT - 1 - y;

    x -= w - 1;
    drawFastRawHLine(x, y, w, color);
  } else if (getRotation() == 3) {
    int16_t t = x;
    x = y;
    y = HEIGHT - 1 - t;
    y -= w - 1;
    drawFastRawVLine(x, y, w, color);
  }
}

/**************************************************************************/
void Adafruit_SharpMem::drawFastRawVLine(int16_t x, int16_t y, int16_t h,
                                         uint16_t color) {
  // x & y already in raw (rotation 0) coordinates, no need to transform.
  int16_t row_bytes = ((WIDTH + 7) / 8);
  uint8_t *ptr = &sharpmem_buffer[(x / 8) + y * row_bytes];

  if (color > 0) {
    uint8_t bit_mask = set[x & 7]; // CHANGED
    if (color == 2) {              // GRAY
      if (y % 2 == 0) {
        bit_mask &= 0xAA;
      } else {
        bit_mask &= 0x55;
      }
    }

    for (int16_t i = 0; i < h; i++) {
      *ptr |= bit_mask;
      ptr += row_bytes;
    }
  } else {                         // BLACK
    uint8_t bit_mask = clr[x & 7]; // CHANGED

    for (int16_t i = 0; i < h; i++) {
      *ptr &= bit_mask;
      ptr += row_bytes;
    }
  }
}

/**************************************************************************/
void Adafruit_SharpMem::drawFastRawHLine(int16_t x, int16_t y, int16_t w,
                                         uint16_t color) {
  // x & y already in raw (rotation 0) coordinates, no need to transform.
  int16_t rowBytes = ((WIDTH + 7) / 8);
  uint8_t *ptr = &sharpmem_buffer[(x / 8) + y * rowBytes];
  size_t remainingWidthBits = w;

  // check to see if first byte needs to be partially filled
  if ((x & 7) > 0) {
    // create bit mask for first byte
    uint8_t startByteBitMask = 0x00;
    for (int8_t i = (x & 7); ((i < 8) && (remainingWidthBits > 0)); i++) {
      startByteBitMask |= set[i]; // CHANGED
      remainingWidthBits--;
    }

    if (color == 7) {
      uint8_t pattern = 0x00;
      switch (y % 4) {
      case 0:
        pattern = 0x77;
        break;
      case 1:
        pattern = 0xBB;
        break;
      case 2:
        pattern = 0xDD;
        break;
      case 3:
        pattern = 0xEE;
        break;
      }
      *ptr &= ~(startByteBitMask & ~pattern);
      *ptr |= startByteBitMask & pattern;
    } else if (color == 6) {
      uint8_t pattern = 0x00;
      switch (y % 4) {
      case 0:
        pattern = 0xEE;
        break;
      case 1:
        pattern = 0xDD;
        break;
      case 2:
        pattern = 0xBB;
        break;
      case 3:
        pattern = 0x77;
        break;
      }
      *ptr &= ~(startByteBitMask & ~pattern);
      *ptr |= startByteBitMask & pattern;
    } else if (color == 5) {
      uint8_t pattern = 0x00;
      switch (y % 4) {
      case 0: // 0x11
        pattern = 0xEE;
        break;
      case 1: // 0x22
        pattern = 0x55;
        break;
      case 2: // 0xBB
        pattern = 0xBB;
        break;
      case 3: // 0x88
        pattern = 0x55;
        break;
      }
      *ptr &= ~(startByteBitMask & ~pattern);
      *ptr |= startByteBitMask & pattern;
    } else if (color == 4) { // LIGHT GRAY
      if (y % 2 != 0) {      // off
        *ptr |= startByteBitMask;
      } else { // on every other pixel
        *ptr &= ~(startByteBitMask & ~0x55);
        *ptr |= startByteBitMask & 0x55;
      }
    } else if (color == 3) { // DARK GRAY
      if (y % 2 != 0) {      // off
        *ptr &= ~startByteBitMask;
      } else { // on every other pixel
        *ptr &= ~(startByteBitMask & ~0xAA);
        *ptr |= startByteBitMask & 0xAA;
      }
    } else if (color == 2) { // GRAY
      if (y % 2 == 0) {
        *ptr &= ~(startByteBitMask & ~0xAA);
        *ptr |= startByteBitMask & 0xAA;
      } else {
        *ptr &= ~(startByteBitMask & ~0x55);
        *ptr |= startByteBitMask & 0x55;
      }
    } else if (color == 1) { // white
      *ptr |= startByteBitMask;
    } else { // black
      *ptr &= ~startByteBitMask;
    }

    ptr++;
  }

  // do the next remainingWidthBits bits
  if (remainingWidthBits > 0) {
    size_t remainingWholeBytes = remainingWidthBits / 8;
    size_t lastByteBits = remainingWidthBits % 8;
    uint8_t wholeByteColor = color > 0 ? 0xFF : 0x00;

    if (color == 7) {
      switch (y % 4) {
      case 0:
        wholeByteColor = 0x77;
        break;
      case 1:
        wholeByteColor = 0xBB;
        break;
      case 2:
        wholeByteColor = 0xDD;
        break;
      case 3:
        wholeByteColor = 0xEE;
        break;
      }
    } else if (color == 6) {
      switch (y % 4) {
      case 0:
        wholeByteColor = 0xEE;
        break;
      case 1:
        wholeByteColor = 0xDD;
        break;
      case 2:
        wholeByteColor = 0xBB;
        break;
      case 3:
        wholeByteColor = 0x77;
        break;
      }
    } else if (color == 5) {
      switch (y % 4) {
      case 0: // 0x11
        wholeByteColor = 0xEE;
        break;
      case 1: // 0x22
        wholeByteColor = 0x55;
        break;
      case 2: // 0xBB
        wholeByteColor = 0xBB;
        break;
      case 3: // 0x88
        wholeByteColor = 0x55;
        break;
      }
    } else if (color == 4) {
      if (y % 2 != 0) { // off
        wholeByteColor = 0xFF;
      } else { // on every other pixel
        wholeByteColor = 0x55;
      }
    } else if (color == 3) { // DARK GRAY
      if (y % 2 != 0) {      // off
        wholeByteColor = 0x00;
      } else { // on every other pixel
        wholeByteColor = 0xAA;
      }
    } else if (color == 2) { // GRAY
      if (y % 2 == 0) {
        wholeByteColor = 0xAA;
      } else {
        wholeByteColor = 0x55;
      }
    }

    memset(ptr, wholeByteColor, remainingWholeBytes);

    if (lastByteBits > 0) {
      uint8_t lastByteBitMask = 0x00;
      for (size_t i = 0; i < lastByteBits; i++) {
        lastByteBitMask |= set[i]; // CHANGED
      }
      ptr += remainingWholeBytes;

      if (color == 7) {
        uint8_t pattern = 0x00;
        switch (y % 4) {
        case 0:
          pattern = 0x77;
          break;
        case 1:
          pattern = 0xBB;
          break;
        case 2:
          pattern = 0xDD;
          break;
        case 3:
          pattern = 0xEE;
          break;
        }
        *ptr &= ~(lastByteBitMask & ~pattern);
        *ptr |= lastByteBitMask & pattern;
      } else if (color == 6) {
        uint8_t pattern = 0x00;
        switch (y % 4) {
        case 0:
          pattern = 0xEE;
          break;
        case 1:
          pattern = 0xDD;
          break;
        case 2:
          pattern = 0xBB;
          break;
        case 3:
          pattern = 0x77;
          break;
        }
        *ptr &= ~(lastByteBitMask & ~pattern);
        *ptr |= lastByteBitMask & pattern;
      } else if (color == 5) {
        uint8_t pattern = 0x00;
        switch (y % 4) {
        case 0: // 0x11
          pattern = 0xEE;
          break;
        case 1: // 0x22
          pattern = 0x55;
          break;
        case 2: // 0xBB
          pattern = 0xBB;
          break;
        case 3: // 0x88
          pattern = 0x55;
          break;
        }
        *ptr &= ~(lastByteBitMask & ~pattern);
        *ptr |= lastByteBitMask & pattern;
      } else if (color == 4) { // LIGHT GRAY
        if (y % 2 != 0) {      // off
          *ptr |= lastByteBitMask;
        } else { // on every other pixel
          *ptr &= ~(lastByteBitMask & ~0x55);
          *ptr |= lastByteBitMask & 0x55;
        }
      } else if (color == 3) { // DARK GRAY
        if (y % 2 != 0) {      // off
          *ptr &= ~lastByteBitMask;
        } else { // on every other pixel
          *ptr &= ~(lastByteBitMask & ~0xAA);
          *ptr |= lastByteBitMask & 0xAA;
        }
      } else if (color == 2) {
        if (y % 2 == 0) {
          *ptr &= ~(lastByteBitMask & ~0xAA);
          *ptr |= lastByteBitMask & 0xAA;
        } else {
          *ptr &= ~(lastByteBitMask & ~0x55); // black pixels
          *ptr |= lastByteBitMask & 0x55;     // white pixels
        }
      } else if (color == 1) {
        *ptr |= lastByteBitMask;
      } else {
        *ptr &= ~lastByteBitMask;
      }
    }
  }
}