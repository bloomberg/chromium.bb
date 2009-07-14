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

#ifndef NATIVE_CLIENT_COMMON_CONSOLE_CONSOLE_COLOR_H_
#define NATIVE_CLIENT_COMMON_CONSOLE_CONSOLE_COLOR_H_


#include <stdint.h>


/* An RGBA value packed into a 32-bit value. */
/* (NOTE: assumes little endian + ARGB order) */
typedef uint32_t ColorT;

/* Package and unpackage RGBA values */
#define COLOR_RGBA(r, g, b, a) ((a << 24) | (r << 16) | (g << 8) | b)
#define COLOR_RGB(r, g, b) COLOR_RGBA(r, g, b, 255)
#define COLOR_A(col) ((col >> 24) & 0xff)
#define COLOR_R(col) ((col >> 16) & 0xff)
#define COLOR_G(col) ((col >> 8) & 0xff)
#define COLOR_B(col) (col & 0xff)

/* Common colors */
#define COLOR_RED COLOR_RGB(255, 0, 0)
#define COLOR_YELLOW COLOR_RGB(255, 255, 0)
#define COLOR_GREEN COLOR_RGB(0, 255, 0)
#define COLOR_CYAN COLOR_RGB(0, 255, 255)
#define COLOR_BLUE COLOR_RGB(0, 0, 255)
#define COLOR_MAGENTA COLOR_RGB(255, 0, 255)
#define COLOR_BLACK COLOR_RGB(0, 0, 0)
#define COLOR_GRAY COLOR_RGB(192, 192, 192)
#define COLOR_WHITE COLOR_RGB(255, 255, 255)


#endif  /* NATIVE_CLIENT_COMMON_CONSOLE_CONSOLE_COLOR_H_ */

