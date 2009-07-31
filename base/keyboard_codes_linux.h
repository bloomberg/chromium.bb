// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com. All rights reserved.
 * Copyright (C) 2008, 2009 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, LOSS OF USE, DATA, OR
 * PROFITS, OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BASE_KEYBOARD_CODES_LINUX_H_
#define BASE_KEYBOARD_CODES_LINUX_H_

#include <gdk/gdkkeysyms.h>
#include <X11/XF86keysym.h>

namespace base {

enum {
  VKEY_BACK = GDK_BackSpace,
  VKEY_TAB = GDK_Tab,
  VKEY_CLEAR = GDK_Clear,
  VKEY_RETURN = GDK_Return,
  VKEY_SHIFT = GDK_Shift_L,  // TODO(jcampan): what about GDK_Shift_R?
  VKEY_CONTROL = GDK_Control_L,  // TODO(jcampan): what about GDK_Control_R?
  VKEY_MENU = GDK_Menu,
  VKEY_PAUSE = GDK_Pause,
  VKEY_CAPITAL = GDK_Shift_Lock,
  VKEY_KANA = GDK_Kana_Shift,
  VKEY_HANGUL = GDK_Hangul,
  // TODO(jcampan): not sure what the next 2 are.
  VKEY_JUNJA = GDK_Hangul,
  VKEY_FINAL = GDK_Hangul,
  VKEY_HANJA = GDK_Hangul_Hanja,
  VKEY_KANJI = GDK_Kanji,
  VKEY_ESCAPE = GDK_Escape,
  // TODO(jcampan): not sure what the next 4 are.
  VKEY_CONVERT = 0x1C,
  VKEY_NONCONVERT = 0x1D,
  VKEY_ACCEPT = 0x1E,
  VKEY_MODECHANGE = 0x1F,
  VKEY_SPACE = GDK_space,
  VKEY_PRIOR = GDK_Prior,
  VKEY_NEXT = GDK_Next,
  VKEY_END = GDK_End,
  VKEY_HOME = GDK_Home,
  VKEY_LEFT = GDK_Left,
  VKEY_UP = GDK_Up,
  VKEY_RIGHT = GDK_Right,
  VKEY_DOWN = GDK_Down,
  VKEY_SELECT = GDK_Select,
  VKEY_PRINT = GDK_Print,
  VKEY_EXECUTE = GDK_Execute,
  VKEY_SNAPSHOT = 0x2C,    // TODO(jcampan): not sure what this one is.
  VKEY_INSERT = GDK_Insert,
  VKEY_DELETE = GDK_Delete,
  VKEY_HELP = GDK_Help,
  VKEY_0 = GDK_0,
  VKEY_1 = GDK_1,
  VKEY_2 = GDK_2,
  VKEY_3 = GDK_3,
  VKEY_4 = GDK_4,
  VKEY_5 = GDK_5,
  VKEY_6 = GDK_6,
  VKEY_7 = GDK_7,
  VKEY_8 = GDK_8,
  VKEY_9 = GDK_9,
  VKEY_A = GDK_A,
  VKEY_B = GDK_B,
  VKEY_C = GDK_C,
  VKEY_D = GDK_D,
  VKEY_E = GDK_E,
  VKEY_F = GDK_F,
  VKEY_G = GDK_G,
  VKEY_H = GDK_H,
  VKEY_I = GDK_I,
  VKEY_J = GDK_J,
  VKEY_K = GDK_K,
  VKEY_L = GDK_L,
  VKEY_M = GDK_M,
  VKEY_N = GDK_N,
  VKEY_O = GDK_O,
  VKEY_P = GDK_P,
  VKEY_Q = GDK_Q,
  VKEY_R = GDK_R,
  VKEY_S = GDK_S,
  VKEY_T = GDK_T,
  VKEY_U = GDK_U,
  VKEY_V = GDK_V,
  VKEY_W = GDK_W,
  VKEY_X = GDK_X,
  VKEY_Y = GDK_Y,
  VKEY_Z = GDK_Z,
  VKEY_LWIN = GDK_Meta_L,
  VKEY_RWIN = GDK_Meta_R,
  VKEY_APPS = 0x5D,  // TODO(jcampan): not sure what this one is.
  VKEY_SLEEP = XF86XK_Sleep,
  VKEY_NUMPAD0 = GDK_KP_0,
  VKEY_NUMPAD1 = GDK_KP_1,
  VKEY_NUMPAD2 = GDK_KP_2,
  VKEY_NUMPAD3 = GDK_KP_3,
  VKEY_NUMPAD4 = GDK_KP_4,
  VKEY_NUMPAD5 = GDK_KP_5,
  VKEY_NUMPAD6 = GDK_KP_6,
  VKEY_NUMPAD7 = GDK_KP_7,
  VKEY_NUMPAD8 = GDK_KP_8,
  VKEY_NUMPAD9 = GDK_KP_9,
  VKEY_MULTIPLY = GDK_KP_Multiply,
  VKEY_ADD = GDK_KP_Add,
  VKEY_SEPARATOR = GDK_KP_Separator,
  VKEY_SUBTRACT = GDK_KP_Subtract,
  VKEY_DECIMAL = GDK_KP_Decimal,
  VKEY_DIVIDE = GDK_KP_Divide,
  VKEY_F1 = GDK_F1,
  VKEY_F2 = GDK_F2,
  VKEY_F3 = GDK_F3,
  VKEY_F4 = GDK_F4,
  VKEY_F5 = GDK_F5,
  VKEY_F6 = GDK_F6,
  VKEY_F7 = GDK_F7,
  VKEY_F8 = GDK_F8,
  VKEY_F9 = GDK_F9,
  VKEY_F10 = GDK_F10,
  VKEY_F11 = GDK_F11,
  VKEY_F12 = GDK_F12,
  VKEY_F13 = GDK_F13,
  VKEY_F14 = GDK_F14,
  VKEY_F15 = GDK_F15,
  VKEY_F16 = GDK_F16,
  VKEY_F17 = GDK_F17,
  VKEY_F18 = GDK_F18,
  VKEY_F19 = GDK_F19,
  VKEY_F20 = GDK_F20,
  VKEY_F21 = GDK_F21,
  VKEY_F22 = GDK_F22,
  VKEY_F23 = GDK_F23,
  VKEY_F24 = GDK_F24,
  VKEY_NUMLOCK = GDK_Num_Lock,
  VKEY_SCROLL = GDK_Scroll_Lock,
  VKEY_LSHIFT = GDK_Shift_L,
  VKEY_RSHIFT = GDK_Shift_R,
  VKEY_LCONTROL = GDK_Control_L,
  VKEY_RCONTROL = GDK_Control_R,
  VKEY_LMENU = GDK_Alt_L,
  VKEY_RMENU = GDK_Alt_R,
  VKEY_BROWSER_BACK = XF86XK_Back,
  VKEY_BROWSER_FORWARD = XF86XK_Forward,
  VKEY_BROWSER_REFRESH = XF86XK_Refresh,
  VKEY_BROWSER_STOP = XF86XK_Stop,
  VKEY_BROWSER_SEARCH = XF86XK_Search,
  VKEY_BROWSER_FAVORITES = XF86XK_Favorites,
  VKEY_BROWSER_HOME = XF86XK_HomePage,
  VKEY_VOLUME_MUTE = XF86XK_AudioMute,
  VKEY_VOLUME_DOWN = XF86XK_AudioLowerVolume,
  VKEY_VOLUME_UP = XF86XK_AudioRaiseVolume,
  VKEY_MEDIA_NEXT_TRACK = XF86XK_AudioNext,
  VKEY_MEDIA_PREV_TRACK = XF86XK_AudioPrev,
  VKEY_MEDIA_STOP = XF86XK_AudioStop,
  VKEY_MEDIA_PLAY_PAUSE = XF86XK_AudioPause,
  VKEY_MEDIA_LAUNCH_MAIL = XF86XK_Mail,
  VKEY_MEDIA_LAUNCH_MEDIA_SELECT = XF86XK_AudioMedia,
  VKEY_MEDIA_LAUNCH_APP1 = XF86XK_Launch1,
  VKEY_MEDIA_LAUNCH_APP2 = XF86XK_Launch2,
  // TODO(jcampan): Figure-out values below.
  VKEY_OEM_1 = 0xBA,
  VKEY_OEM_PLUS = 0xBB,
  VKEY_OEM_COMMA = 0xBC,
  VKEY_OEM_MINUS = 0xBD,
  VKEY_OEM_PERIOD = 0xBE,
  VKEY_OEM_2 = 0xBF,
  VKEY_OEM_3 = 0xC0,
  VKEY_OEM_4 = 0xDB,
  VKEY_OEM_5 = 0xDC,
  VKEY_OEM_6 = 0xDD,
  VKEY_OEM_7 = 0xDE,
  VKEY_OEM_8 = 0xDF,
  VKEY_OEM_102 = 0xE2,
  VKEY_PROCESSKEY = 0xE5,
  VKEY_PACKET = 0xE7,
  VKEY_ATTN = 0xF6,
  VKEY_CRSEL = 0xF7,
  VKEY_EXSEL = 0xF8,
  VKEY_EREOF = 0xF9,
  VKEY_PLAY = 0xFA,
  VKEY_ZOOM = XF86XK_ZoomIn,
  VKEY_NONAME = 0xFC,
  VKEY_PA1 = 0xFD,
  VKEY_OEM_CLEAR = 0xFE,
  VKEY_UNKNOWN = 0
};

}  // namespace views

#endif  // BASE_KEYBOARD_CODES_LINUX_H_
