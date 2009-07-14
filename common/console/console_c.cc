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

#include "native_client/common/console/console_c.h"
#include "native_client/common/console/console_xterm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <nacl/nacl_av.h>
#include <nacl/nacl_srpc.h>


// Keep some global state for apps that assume a single console.
const int kKeyBufferLimit = 100;
static Console *global_console = 0;
static ConsoleXterm *global_console_xterm = 0;
static ColorT *global_console_image = 0;
static int global_console_stride = 0;
static char global_console_key_buffer[kKeyBufferLimit] = "";


void ConsoleInit(int width, int height) {
  if (nacl_multimedia_init(NACL_SUBSYSTEM_VIDEO |
                           NACL_SUBSYSTEM_EMBED) == -1) {
    fprintf(stderr, "Multimedia system failed to initialize!  errno: %d\n",
            errno);
    exit(-1);
  }
  if (nacl_video_init(width, height) == -1) {
    fprintf(stderr, "Video subsystem failed to initialize!  errno; %d\n",
            errno);
    exit(-1);
  }
  global_console = new Console();
  global_console->SetSize(width / 8, height / 8);
  global_console_image = new ColorT[width * height];
  global_console_stride = width;
  global_console_xterm = new ConsoleXterm(global_console);
}

void ConsoleShutdown(void) {
  delete[] global_console_image;
  delete global_console_xterm;
  delete global_console;
  nacl_video_shutdown();
  nacl_multimedia_shutdown();
}

void ConsoleWrite(const char *buf, int len) {
  global_console_xterm->WriteString(buf, len);
}

static int ConsoleReadOne(int wtime) {
  NaClMultimediaEvent event;

  // Use things from the buffer first.
  if (*global_console_key_buffer) {
    int ret = *global_console_key_buffer;
    memmove(global_console_key_buffer,
            global_console_key_buffer + 1,
            strlen(global_console_key_buffer));
    return ret;
  }
  // Update the display.
  global_console->Draw(global_console_image,
                       global_console_stride);
  if (nacl_video_update(global_console_image) == -1) {
    fprintf(stderr, "nacl_video_update() returned %d\n", errno);
  }
  // Wait for keys.
  do {
    while (!nacl_video_poll_event(&event)) {
      if (event.type == NACL_EVENT_QUIT) {
        exit(0);
      } else if (event.type == NACL_EVENT_KEY_DOWN) {
        int sym = event.key.keysym.sym;
        int mod = event.key.keysym.mod;
        int shift = (mod & NACL_KEYMOD_LSHIFT) ||
                    (mod & NACL_KEYMOD_RSHIFT);
        int control = (mod & NACL_KEYMOD_LCTRL) ||
                      (mod & NACL_KEYMOD_RCTRL);
        int caps = mod & NACL_KEYMOD_CAPS;
        switch (sym) {
          case NACL_KEY_UP:
            snprintf(global_console_key_buffer, kKeyBufferLimit, "OA");
            return '\x1b';
          case NACL_KEY_DOWN:
            snprintf(global_console_key_buffer, kKeyBufferLimit, "OB");
            return '\x1b';
          case NACL_KEY_RIGHT:
            snprintf(global_console_key_buffer, kKeyBufferLimit, "OC");
            return '\x1b';
          case NACL_KEY_LEFT:
            snprintf(global_console_key_buffer, kKeyBufferLimit, "OD");
            return '\x1b';
          case NACL_KEY_INSERT:
            snprintf(global_console_key_buffer, kKeyBufferLimit, "[2~");
            return '\x1b';
          case NACL_KEY_DELETE:
            snprintf(global_console_key_buffer, kKeyBufferLimit, "[3~");
            return '\x1b';
          case NACL_KEY_PAGEUP:
            snprintf(global_console_key_buffer, kKeyBufferLimit, "[5~");
            return '\x1b';
          case NACL_KEY_PAGEDOWN:
            snprintf(global_console_key_buffer, kKeyBufferLimit, "[6~");
            return '\x1b';
          case NACL_KEY_HOME:
            snprintf(global_console_key_buffer, kKeyBufferLimit, "OH");
            return '\x1b';
          case NACL_KEY_END:
            snprintf(global_console_key_buffer, kKeyBufferLimit, "OF");
            return '\x1b';
        }
        if (sym >= 'a' && sym <= 'z') {
          if (control) {
            return sym + (1 - 'a');
          } else if (shift || caps) {
            return sym + ('A' - 'a');
          } else {
            return sym;
          }
        } else if ((sym >= '0' && sym <= '9') ||
                   sym == '`' ||
                   sym == '-' ||
                   sym == '=' ||
                   sym == '[' ||
                   sym == ']' ||
                   sym == '\\' ||
                   sym == ';' ||
                   sym == '\'' ||
                   sym == ',' ||
                   sym == '.' ||
                   sym == '/' ||
                   sym == '=') {
          if (control && shift) {
            switch (sym) {
              case '2': return 0;  // Ctrl @
              case '6': return 30;  // Ctrl ^
              case '-': return 31;  // Ctrl _
              case '?': return 127;  // Ctrl ?
            }
          } else if (control && !shift) {
            switch (sym) {
              case '[': return 27;
              case '\\': return 28;
              case ']': return 29;
            }
          } else if (shift) {
            switch (sym) {
              case '`': return '~';
              case '1': return '!';
              case '2': return '@';
              case '3': return '#';
              case '4': return '$';
              case '5': return '%';
              case '6': return '^';
              case '7': return '&';
              case '8': return '*';
              case '9': return '(';
              case '0': return ')';
              case '-': return '_';
              case '=': return '+';
              case '[': return '{';
              case ']': return '}';
              case '\\': return '|';
              case ';': return ':';
              case '\'': return '"';
              case ',': return '<';
              case '.': return '>';
              case '/': return '?';
            }
          } else {
            return sym;
          }
        } else {
          if (sym >= 0 && sym <= 126) {
            return sym;
          }
        }
      }
    }
  } while (wtime < 0);
  return -1;
}

int ConsoleRead(int wtime, char *dst, int len) {
  int count;
  for (count = 0; count < len; count++) {
    int ch = ConsoleReadOne(wtime);
    if (ch < 0) break;
    dst[count] = ch;
    wtime = 0;
  }
  return count;
}

int ConsoleGetWidth(void) {
  return global_console->GetWidth();
}

int ConsoleGetHeight(void) {
  return global_console->GetHeight();
}

