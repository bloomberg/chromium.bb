/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "native_client/common/console/console.h"
#include <string.h>
#include <stdlib.h>


extern unsigned char console_font1[];


void Console::DrawChar(ColorT *dst, int stride,
                       char ch, ColorT color) {
  int width = console_font1[0];
  int height = console_font1[1];
  unsigned char *src = &console_font1[2 + (ch - 33) * width * height];

  if (ch < 33 || ch > 126) return;
  for (int j = 0; j < height; ++j) {
    for (int i = 0; i < width; ++i) {
      if (src[i]) {
        dst[i] = color;
      }
    }
    src += width;
    dst += stride;
  }
}

void Console::DrawBlank(ColorT *dst, int stride,
                        int width, int height, ColorT color) {
  for (int j = 0; j < height; ++j) {
    for (int i = 0; i < width; ++i) {
      dst[i] = color;
    }
    dst += stride;
  }
}

void Console::DrawInvert(ColorT *dst, int stride,
                         int width, int height) {
  for (int j = 0; j < height; ++j) {
    for (int i = 0; i < width; ++i) {
      ColorT col = dst[i];
      dst[i] = COLOR_RGBA(255 - COLOR_R(col),
                          255 - COLOR_G(col),
                          255 - COLOR_B(col),
                          COLOR_A(col));
    }
    dst += stride;
  }
}

Console::Console() {
  cursor_x_ = 0;
  cursor_y_ = 0;
  width_ = 80;
  height_ = 24;
  buffer_ = new ConsoleChar[width_ * height_];
  memset(buffer_, 0, width_ * height_ * sizeof(ConsoleChar));
}

Console::~Console() {
  delete[] buffer_;
}

void Console::SetCursorPosition(int x, int y) {
  cursor_x_ = x;
  cursor_y_ = y;
}

void Console::SetSize(int width, int height) {
  delete[] buffer_;
  width_ = width;
  height_ = height;
  buffer_ = new ConsoleChar[width_ * height_];
  memset(buffer_, 0, width_ * height_ * sizeof(ConsoleChar));
}

void Console::Scroll(int start, int end, int down) {
  if (start < 0) start = 0;
  if (start > height_ - 1) start = height_ - 1;
  if (end < start) end = start;
  if (end < 0) end = 0;
  if (end > height_ - 1) end = height_ - 1;

  if (down) {
    memmove(buffer_ + (start + 1) * width_, buffer_ + start * width_,
            width_ * (end - start) * sizeof(ConsoleChar));
    memset(buffer_ + width_ * start, 0, width_ * sizeof(ConsoleChar));
  } else {
    memmove(buffer_ + start * width_, buffer_ + (start + 1) * width_,
            width_ * (end - start) * sizeof(ConsoleChar));
    memset(buffer_ + width_ * end, 0, width_ * sizeof(ConsoleChar));
  }
}

void Console::Draw(ColorT *dst, int stride) {
  int font_width = console_font1[0];
  int font_height = console_font1[1];

  for (int y = 0; y < height_; ++y) {
    ColorT *dstr = dst + stride * font_height * y;
    for (int x = 0; x < width_; ++x) {
      ConsoleChar& ch = CharAt(x, y);
      DrawBlank(dstr + x * font_width, stride,
                font_width, font_height,
                ch.background);
      DrawChar(dstr + x * font_width, stride,
               ch.ch, ch.foreground);
    }
  }
  if (cursor_x_ >= 0 && cursor_y_ >= 0 &&
      cursor_x_ < width_ && cursor_y_ < height_) {
    DrawInvert(dst + cursor_x_ * font_width +
               cursor_y_ * stride * font_height, stride,
               font_width, font_height);
  }
}

