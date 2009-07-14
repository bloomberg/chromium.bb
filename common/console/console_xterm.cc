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

#include "native_client/common/console/console_xterm.h"
#include <stdio.h>


ConsoleXterm::ConsoleXterm(Console *console) {
  console_ = console;
  foreground_ = COLOR_GRAY;
  background_ = COLOR_BLACK;
  x_ = 0;
  y_ = 0;
  escape_stage_ = 0;
  escape_value1_ = 0;
  escape_value2_ = 0;
  console_->SetCursorPosition(x_, y_);
  scroll_top_ = 0;
  scroll_bottom_ = console_->GetHeight() - 1;
}

void ConsoleXterm::WriteString(const char *str, int len) {
  while (len > 0) {
    WriteChar(*str);
    str++;
    len--;
  }
}

void ConsoleXterm::WriteEscapedChar(char ch) {
  if (escape_stage_ == 1) {
    if (ch == '[') {
      escape_stage_++;
      return;
    } else {
      printf("Got unknown ^[(%d)\n", ch);
    }
  } else if (escape_stage_ == 2) {
    if (ch >= '0' && ch <='9') {
      escape_value1_ = escape_value1_ * 10 + ch - '0';
      return;
    } else if (ch == ';') {
      escape_stage_++;
      return;
    } else if (ch == 'A') {
      y_ -= escape_value1_;
      ReclipCursor();
    } else if (ch == 'B') {
      y_ += escape_value1_;
      ReclipCursor();
    } else if (ch == 'C') {
      x_ += escape_value1_;
      ReclipCursor();
    } else if (ch == 'D') {
      x_ -= escape_value1_;
      ReclipCursor();
    } else if (ch == 'L') {
      ReclipCursor();
      int count = escape_value1_;
      if (count <= 0) count = 1;
      while (count) {
        console_->Scroll(y_, scroll_bottom_, 1);
        count--;
      }
    } else if (ch == 'M') {
      ReclipCursor();
      int count = escape_value1_;
      if (count <= 0) count = 1;
      while (count) {
        console_->Scroll(y_, scroll_bottom_, 0);
        count--;
      }
    } else if (ch == 'm') {
      ReclipCursor();
      if (escape_value1_ == 0) {
        SetForeground(COLOR_GRAY);
        SetBackground(COLOR_BLACK);
      } else if (escape_value1_ == 1) {
        SetForeground(COLOR_WHITE);
        SetBackground(COLOR_BLACK);
      } else if (escape_value1_ == 7) {
        SetForeground(COLOR_BLACK);
        SetBackground(COLOR_GRAY);
      } else {
        SetForeground(COLOR_BLUE);
        SetBackground(COLOR_BLACK);
      }
    } else if (ch == 'J' || ch == 'K') {
      int width = console_->GetWidth();
      int height = console_->GetHeight();
      ReclipCursor();
      int start = 0, end = 0;
      if (ch == 'J') {
        if (escape_value1_ == 0) {
          start = x_ + y_ * width;
          end = width * height;
        } else if (escape_value1_ == 1) {
          start = 0;
          end = x_ + y_ * width + 1;
        } else if (escape_value1_ == 2) {
          start = 0;
          end = width * height;
        }
      } else if (ch == 'K') {
        if (escape_value1_ == 0) {
          start = x_ + y_ * width;
          end = (y_ + 1) * width;
        } else if (escape_value1_ == 1) {
          start = y_ * width;
          end = x_ + y_ * width + 1;
        } else if (escape_value1_ == 2) {
          start = y_ * width;
          end = (y_ + 1) * width;
        }
      }
      for (int z = start; z < end; ++z) {
        ConsoleChar& ch = console_->CharAt(z);
        ch.ch = ' ';
        ch.foreground = foreground_;
        ch.background = background_;
      }
    } else if (ch == 'H') {
      SetPosition(0, 0);
    } else {
      printf("Got unknown ^[%d(%d)\n", escape_value1_, ch);
    }
  } else if (escape_stage_ == 3) {
    if (ch >= '0' && ch <='9') {
      escape_value2_ = escape_value2_ * 10 + ch - '0';
      return;
    } else if (ch == 'H' || ch == 'f') {
      SetPosition(escape_value2_ - 1, escape_value1_ - 1);
      ReclipCursor();
    } else if (ch == 'r') {
      scroll_top_ = escape_value1_ - 1;
      scroll_bottom_ = escape_value2_ - 1;
    } else {
      printf("Got unknown ^[%d;%d(%d)\n", escape_value1_, escape_value2_, ch);
    }
  } else {
    printf("Unknown ^ (%d)\n", ch);
  }
  escape_stage_ = 0;
  escape_value1_ = 0;
  escape_value2_ = 0;
}

void ConsoleXterm::WriteChar(char ch) {
  if (escape_stage_) {
    WriteEscapedChar(ch);
    return;
  }
  if (ch == 10) {
    x_ = 0;
    y_++;
    ReclipCursor();
  } else if (ch == 8) {
    x_--;
    ReclipCursor();
  } else if (ch == 13) {
    x_ = 0;
    ReclipCursor();
  } else if (ch == 27) {
    escape_stage_++;
  } else if (ch < 32) {
    printf("Unknown code (%d)\n", ch);
  } else {
    ReclipCursor();
    ConsoleChar& dst = console_->CharAt(x_, y_);
    dst.ch = ch;
    dst.foreground = foreground_;
    dst.background = background_;
    x_++;
    ReclipCursor();
  }
}

void ConsoleXterm::ReclipCursor() {
  int width = console_->GetWidth();
  int z = x_ + y_ * width;
  if (z < scroll_top_ * width) z = scroll_top_ * width;
  x_ = z % width;
  y_ = z / width;
  if (y_ > scroll_bottom_) {
    y_ = scroll_bottom_;
    ScrollUp();
  }
  console_->SetCursorPosition(x_, y_);
}

void ConsoleXterm::SetForeground(ColorT col) {
  foreground_ = col;
}

void ConsoleXterm::SetBackground(ColorT col) {
  background_ = col;
}

void ConsoleXterm::SetPosition(int x, int y) {
  x_ = x;
  y_ = y;
  console_->SetCursorPosition(x_, y_);
}

void ConsoleXterm::ScrollUp() {
  console_->Scroll(scroll_top_, scroll_bottom_, 0);
}

