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

// Standalone helper functions
// Mimic barebones nacl multimedia interface for standalone
// non-nacl applications.  Mainly used as a debugging aid.
// * Only supported under Mac & Linux for now.

#ifndef NATIVE_CLIENT_TESTS_COMMON_STANDALONE_H_
#define NATIVE_CLIENT_TESTS_COMMON_STANDALONE_H_

#if defined(STANDALONE)
#include <SDL.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kNaClAudioBufferLength (16 * 1024)
#define kNaClVideoMinWindowSize (32)
#define kNaClVideoMaxWindowSize (4096)

enum {
  /* subsystems represented as bitset, ie 0x01, 0x02, 0x04 */
  NACL_SUBSYSTEM_VIDEO = 0x01,
  NACL_SUBSYSTEM_AUDIO = 0x02,
};

/* NACL_SUBSYSTEM_EMBED is not supported for standalone */
enum {
  NACL_SUBSYSTEM_EMBED = 0
};

enum NaClAudioFormat {
  NACL_AUDIO_FORMAT_STEREO_44K = 0,
  NACL_AUDIO_FORMAT_STEREO_48K,
};

enum NaClEvent {
  NACL_EVENT_NOT_USED = 0,
  NACL_EVENT_ACTIVE,
  NACL_EVENT_EXPOSE,
  NACL_EVENT_KEY_DOWN,
  NACL_EVENT_KEY_UP,
  NACL_EVENT_MOUSE_MOTION,
  NACL_EVENT_MOUSE_BUTTON_DOWN,
  NACL_EVENT_MOUSE_BUTTON_UP,
  NACL_EVENT_QUIT,
  NACL_EVENT_UNSUPPORTED
};

enum NaClKey {
  NACL_KEY_UNKNOWN      = 0,
  NACL_KEY_FIRST        = 0,
  NACL_KEY_BACKSPACE    = 8,
  NACL_KEY_TAB          = 9,
  NACL_KEY_CLEAR        = 12,
  NACL_KEY_RETURN       = 13,
  NACL_KEY_PAUSE        = 19,
  NACL_KEY_ESCAPE       = 27,
  NACL_KEY_SPACE        = 32,
  NACL_KEY_EXCLAIM      = 33,
  NACL_KEY_QUOTEDBL     = 34,
  NACL_KEY_HASH         = 35,
  NACL_KEY_DOLLAR       = 36,
  NACL_KEY_AMPERSAND    = 38,
  NACL_KEY_QUOTE        = 39,
  NACL_KEY_LEFTPAREN    = 40,
  NACL_KEY_RIGHTPAREN   = 41,
  NACL_KEY_ASTERISK     = 42,
  NACL_KEY_PLUS         = 43,
  NACL_KEY_COMMA        = 44,
  NACL_KEY_MINUS        = 45,
  NACL_KEY_PERIOD       = 46,
  NACL_KEY_SLASH        = 47,
  NACL_KEY_0            = 48,
  NACL_KEY_1            = 49,
  NACL_KEY_2            = 50,
  NACL_KEY_3            = 51,
  NACL_KEY_4            = 52,
  NACL_KEY_5            = 53,
  NACL_KEY_6            = 54,
  NACL_KEY_7            = 55,
  NACL_KEY_8            = 56,
  NACL_KEY_9            = 57,
  NACL_KEY_COLON        = 58,
  NACL_KEY_SEMICOLON    = 59,
  NACL_KEY_LESS         = 60,
  NACL_KEY_EQUALS       = 61,
  NACL_KEY_GREATER      = 62,
  NACL_KEY_QUESTION     = 63,
  NACL_KEY_AT           = 64,
  NACL_KEY_LEFTBRACKET  = 91,
  NACL_KEY_BACKSLASH    = 92,
  NACL_KEY_RIGHTBRACKET = 93,
  NACL_KEY_CARET        = 94,
  NACL_KEY_UNDERSCORE   = 95,
  NACL_KEY_BACKQUOTE    = 96,
  NACL_KEY_a            = 97,
  NACL_KEY_b            = 98,
  NACL_KEY_c            = 99,
  NACL_KEY_d            = 100,
  NACL_KEY_e            = 101,
  NACL_KEY_f            = 102,
  NACL_KEY_g            = 103,
  NACL_KEY_h            = 104,
  NACL_KEY_i            = 105,
  NACL_KEY_j            = 106,
  NACL_KEY_k            = 107,
  NACL_KEY_l            = 108,
  NACL_KEY_m            = 109,
  NACL_KEY_n            = 110,
  NACL_KEY_o            = 111,
  NACL_KEY_p            = 112,
  NACL_KEY_q            = 113,
  NACL_KEY_r            = 114,
  NACL_KEY_s            = 115,
  NACL_KEY_t            = 116,
  NACL_KEY_u            = 117,
  NACL_KEY_v            = 118,
  NACL_KEY_w            = 119,
  NACL_KEY_x            = 120,
  NACL_KEY_y            = 121,
  NACL_KEY_z            = 122,
  NACL_KEY_DELETE       = 127,
  NACL_KEY_WORLD_0      = 160,
  NACL_KEY_WORLD_1      = 161,
  NACL_KEY_WORLD_2      = 162,
  NACL_KEY_WORLD_3      = 163,
  NACL_KEY_WORLD_4      = 164,
  NACL_KEY_WORLD_5      = 165,
  NACL_KEY_WORLD_6      = 166,
  NACL_KEY_WORLD_7      = 167,
  NACL_KEY_WORLD_8      = 168,
  NACL_KEY_WORLD_9      = 169,
  NACL_KEY_WORLD_10     = 170,
  NACL_KEY_WORLD_11     = 171,
  NACL_KEY_WORLD_12     = 172,
  NACL_KEY_WORLD_13     = 173,
  NACL_KEY_WORLD_14     = 174,
  NACL_KEY_WORLD_15     = 175,
  NACL_KEY_WORLD_16     = 176,
  NACL_KEY_WORLD_17     = 177,
  NACL_KEY_WORLD_18     = 178,
  NACL_KEY_WORLD_19     = 179,
  NACL_KEY_WORLD_20     = 180,
  NACL_KEY_WORLD_21     = 181,
  NACL_KEY_WORLD_22     = 182,
  NACL_KEY_WORLD_23     = 183,
  NACL_KEY_WORLD_24     = 184,
  NACL_KEY_WORLD_25     = 185,
  NACL_KEY_WORLD_26     = 186,
  NACL_KEY_WORLD_27     = 187,
  NACL_KEY_WORLD_28     = 188,
  NACL_KEY_WORLD_29     = 189,
  NACL_KEY_WORLD_30     = 190,
  NACL_KEY_WORLD_31     = 191,
  NACL_KEY_WORLD_32     = 192,
  NACL_KEY_WORLD_33     = 193,
  NACL_KEY_WORLD_34     = 194,
  NACL_KEY_WORLD_35     = 195,
  NACL_KEY_WORLD_36     = 196,
  NACL_KEY_WORLD_37     = 197,
  NACL_KEY_WORLD_38     = 198,
  NACL_KEY_WORLD_39     = 199,
  NACL_KEY_WORLD_40     = 200,
  NACL_KEY_WORLD_41     = 201,
  NACL_KEY_WORLD_42     = 202,
  NACL_KEY_WORLD_43     = 203,
  NACL_KEY_WORLD_44     = 204,
  NACL_KEY_WORLD_45     = 205,
  NACL_KEY_WORLD_46     = 206,
  NACL_KEY_WORLD_47     = 207,
  NACL_KEY_WORLD_48     = 208,
  NACL_KEY_WORLD_49     = 209,
  NACL_KEY_WORLD_50     = 210,
  NACL_KEY_WORLD_51     = 211,
  NACL_KEY_WORLD_52     = 212,
  NACL_KEY_WORLD_53     = 213,
  NACL_KEY_WORLD_54     = 214,
  NACL_KEY_WORLD_55     = 215,
  NACL_KEY_WORLD_56     = 216,
  NACL_KEY_WORLD_57     = 217,
  NACL_KEY_WORLD_58     = 218,
  NACL_KEY_WORLD_59     = 219,
  NACL_KEY_WORLD_60     = 220,
  NACL_KEY_WORLD_61     = 221,
  NACL_KEY_WORLD_62     = 222,
  NACL_KEY_WORLD_63     = 223,
  NACL_KEY_WORLD_64     = 224,
  NACL_KEY_WORLD_65     = 225,
  NACL_KEY_WORLD_66     = 226,
  NACL_KEY_WORLD_67     = 227,
  NACL_KEY_WORLD_68     = 228,
  NACL_KEY_WORLD_69     = 229,
  NACL_KEY_WORLD_70     = 230,
  NACL_KEY_WORLD_71     = 231,
  NACL_KEY_WORLD_72     = 232,
  NACL_KEY_WORLD_73     = 233,
  NACL_KEY_WORLD_74     = 234,
  NACL_KEY_WORLD_75     = 235,
  NACL_KEY_WORLD_76     = 236,
  NACL_KEY_WORLD_77     = 237,
  NACL_KEY_WORLD_78     = 238,
  NACL_KEY_WORLD_79     = 239,
  NACL_KEY_WORLD_80     = 240,
  NACL_KEY_WORLD_81     = 241,
  NACL_KEY_WORLD_82     = 242,
  NACL_KEY_WORLD_83     = 243,
  NACL_KEY_WORLD_84     = 244,
  NACL_KEY_WORLD_85     = 245,
  NACL_KEY_WORLD_86     = 246,
  NACL_KEY_WORLD_87     = 247,
  NACL_KEY_WORLD_88     = 248,
  NACL_KEY_WORLD_89     = 249,
  NACL_KEY_WORLD_90     = 250,
  NACL_KEY_WORLD_91     = 251,
  NACL_KEY_WORLD_92     = 252,
  NACL_KEY_WORLD_93     = 253,
  NACL_KEY_WORLD_94     = 254,
  NACL_KEY_WORLD_95     = 255,

  /* numeric keypad */
  NACL_KEY_KP0          = 256,
  NACL_KEY_KP1          = 257,
  NACL_KEY_KP2          = 258,
  NACL_KEY_KP3          = 259,
  NACL_KEY_KP4          = 260,
  NACL_KEY_KP5          = 261,
  NACL_KEY_KP6          = 262,
  NACL_KEY_KP7          = 263,
  NACL_KEY_KP8          = 264,
  NACL_KEY_KP9          = 265,
  NACL_KEY_KP_PERIOD    = 266,
  NACL_KEY_KP_DIVIDE    = 267,
  NACL_KEY_KP_MULTIPLY  = 268,
  NACL_KEY_KP_MINUS     = 269,
  NACL_KEY_KP_PLUS      = 270,
  NACL_KEY_KP_ENTER     = 271,
  NACL_KEY_KP_EQUALS    = 272,

  /* arrow & insert/delete pad */
  NACL_KEY_UP           = 273,
  NACL_KEY_DOWN         = 274,
  NACL_KEY_RIGHT        = 275,
  NACL_KEY_LEFT         = 276,
  NACL_KEY_INSERT       = 277,
  NACL_KEY_HOME         = 278,
  NACL_KEY_END          = 279,
  NACL_KEY_PAGEUP       = 280,
  NACL_KEY_PAGEDOWN     = 281,

  /* function keys */
  NACL_KEY_F1           = 282,
  NACL_KEY_F2           = 283,
  NACL_KEY_F3           = 284,
  NACL_KEY_F4           = 285,
  NACL_KEY_F5           = 286,
  NACL_KEY_F6           = 287,
  NACL_KEY_F7           = 288,
  NACL_KEY_F8           = 289,
  NACL_KEY_F9           = 290,
  NACL_KEY_F10          = 291,
  NACL_KEY_F11          = 292,
  NACL_KEY_F12          = 293,
  NACL_KEY_F13          = 294,
  NACL_KEY_F14          = 295,
  NACL_KEY_F15          = 296,

  /* modifier keys */
  NACL_KEY_NUMLOCK      = 300,
  NACL_KEY_CAPSLOCK     = 301,
  NACL_KEY_SCROLLOCK    = 302,
  NACL_KEY_RSHIFT       = 303,
  NACL_KEY_LSHIFT       = 304,
  NACL_KEY_RCTRL        = 305,
  NACL_KEY_LCTRL        = 306,
  NACL_KEY_RALT         = 307,
  NACL_KEY_LALT         = 308,
  NACL_KEY_RMETA        = 309,
  NACL_KEY_LMETA        = 310,
  NACL_KEY_LSUPER       = 311,
  NACL_KEY_RSUPER       = 312,
  NACL_KEY_MODE         = 313,
  NACL_KEY_COMPOSE      = 314,

  /* misc keys */
  NACL_KEY_HELP         = 315,
  NACL_KEY_PRINT        = 316,
  NACL_KEY_SYSREQ       = 317,
  NACL_KEY_BREAK        = 318,
  NACL_KEY_MENU         = 319,
  NACL_KEY_POWER        = 320,
  NACL_KEY_EURO         = 321,
  NACL_KEY_UNDO         = 322,

  /* Add any other keys here */
  NACL_KEY_LAST
};

enum NaClKeyMod {
  /* mods represented as bitset */
  NACL_KEYMOD_NONE  = 0x0000,
  NACL_KEYMOD_LSHIFT= 0x0001,
  NACL_KEYMOD_RSHIFT= 0x0002,
  NACL_KEYMOD_LCTRL = 0x0040,
  NACL_KEYMOD_RCTRL = 0x0080,
  NACL_KEYMOD_LALT  = 0x0100,
  NACL_KEYMOD_RALT  = 0x0200,
  NACL_KEYMOD_LMETA = 0x0400,
  NACL_KEYMOD_RMETA = 0x0800,
  NACL_KEYMOD_NUM   = 0x1000,
  NACL_KEYMOD_CAPS  = 0x2000,
  NACL_KEYMOD_MODE  = 0x4000,
  NACL_KEYMOD_RESERVED = 0x8000
};

enum NaClMouseButton {
  NACL_MOUSE_BUTTON_LEFT = 1,
  NACL_MOUSE_BUTTON_MIDDLE = 2,
  NACL_MOUSE_BUTTON_RIGHT = 3,
  NACL_MOUSE_SCROLL_UP = 4,
  NACL_MOUSE_SCROLL_DOWN = 5
};

enum NaClMouseState {
  NACL_MOUSE_STATE_LEFT_BUTTON_PRESSED = 1,
  NACL_MOUSE_STATE_MIDDLE_BUTTON_PRESSED = 2,
  NACL_MOUSE_STATE_RIGHT_BUTTON_PRESSED = 4
};

enum NaClActive {
  NACL_ACTIVE_MOUSE = 1,
  NACL_ACTIVE_INPUT_FOCUS = 2,
  NACL_ACTIVE_APPLICATION = 4
};

struct NaClMultimediaKeySymbol {
  uint8_t scancode;
  int16_t sym;
  int16_t mod;
  uint16_t unicode;
};

struct NaClMultimediaActiveEvent {
  uint8_t type;
  uint8_t gain;
  uint8_t state;
};

struct NaClMultimediaKeyboardEvent {
  uint8_t type;
  uint8_t which;
  uint8_t state;
  struct NaClMultimediaKeySymbol keysym;
};

struct NaClMultimediaMouseMotionEvent {
  uint8_t type;
  uint8_t which;
  uint8_t state;
  uint16_t x;
  uint16_t y;
  int16_t xrel;
  int16_t yrel;
};

struct NaClMultimediaMouseButtonEvent {
  uint8_t type;
  uint8_t which;
  uint8_t button;
  uint8_t state;
  uint16_t x;
  uint16_t y;
};

struct NaClMultimediaQuitEvent {
  uint8_t type;
};

union NaClMultimediaEvent {
  uint8_t type;
  struct NaClMultimediaActiveEvent active;
  struct NaClMultimediaKeyboardEvent key;
  struct NaClMultimediaMouseMotionEvent motion;
  struct NaClMultimediaMouseButtonEvent button;
  struct NaClMultimediaQuitEvent quit;
};

extern int nacl_multimedia_init(int subsystems);
extern int nacl_multimedia_shutdown();

extern int nacl_multimedia_is_embedded(int *embedded);
extern int nacl_multimedia_get_embed_size(int *width, int *height);
extern int nacl_video_init(int width, int height);
extern int nacl_video_shutdown();
extern int nacl_video_update(const void *data);
extern int nacl_video_poll_event(union NaClMultimediaEvent *event);
extern int nacl_audio_init(enum NaClAudioFormat format,
                           int desired_samples, int *obtained_samples);
extern int nacl_audio_shutdown();
extern int nacl_audio_stream(const void *data, size_t *size);

#ifdef __cplusplus
}
#endif

#endif  // STANDALONE
#endif  // NATIVE_CLIENT_TESTS_COMMON_STANDALONE_H_
