// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/keyboard_code_conversion_x.h"

#include <X11/keysym.h>
#include <X11/Xlib.h>

#include "base/logging.h"

namespace app {

// Get an app::KeyboardCode from an X keyevent
KeyboardCode KeyboardCodeFromXKeyEvent(XEvent* xev) {
  KeySym keysym = XLookupKeysym(&xev->xkey, 0);

  // TODO(sad): Have |keysym| go through the X map list?

  switch (keysym) {
    case XK_BackSpace:
      return VKEY_BACK;
    case XK_Tab:
      return VKEY_TAB;
    case XK_Linefeed:
    case XK_Return:
      return VKEY_RETURN;
    case XK_Clear:
      return VKEY_CLEAR;
    case XK_KP_Space:
    case XK_space:
      return VKEY_SPACE;
    case XK_Home:
    case XK_KP_Home:
      return VKEY_HOME;
    case XK_End:
    case XK_KP_End:
      return VKEY_END;
    case XK_Left:
    case XK_KP_Left:
      return VKEY_LEFT;
    case XK_Right:
    case XK_KP_Right:
      return VKEY_RIGHT;
    case XK_Down:
    case XK_KP_Down:
      return VKEY_DOWN;
    case XK_Up:
    case XK_KP_Up:
      return VKEY_UP;
    case XK_Control_L:
      return VKEY_LCONTROL;
    case XK_Control_R:
      return VKEY_RCONTROL;
    case XK_Escape:
      return VKEY_ESCAPE;
    case XK_A:
    case XK_a:
      return VKEY_A;
    case XK_B:
    case XK_b:
      return VKEY_B;
    case XK_C:
    case XK_c:
      return VKEY_C;
    case XK_D:
    case XK_d:
      return VKEY_D;
    case XK_E:
    case XK_e:
      return VKEY_E;
    case XK_F:
    case XK_f:
      return VKEY_F;
    case XK_G:
    case XK_g:
      return VKEY_G;
    case XK_H:
    case XK_h:
      return VKEY_H;
    case XK_I:
    case XK_i:
      return VKEY_I;
    case XK_J:
    case XK_j:
      return VKEY_J;
    case XK_K:
    case XK_k:
      return VKEY_K;
    case XK_L:
    case XK_l:
      return VKEY_L;
    case XK_M:
    case XK_m:
      return VKEY_M;
    case XK_N:
    case XK_n:
      return VKEY_N;
    case XK_O:
    case XK_o:
      return VKEY_O;
    case XK_P:
    case XK_p:
      return VKEY_P;
    case XK_Q:
    case XK_q:
      return VKEY_Q;
    case XK_R:
    case XK_r:
      return VKEY_R;
    case XK_S:
    case XK_s:
      return VKEY_S;
    case XK_T:
    case XK_t:
      return VKEY_T;
    case XK_U:
    case XK_u:
      return VKEY_U;
    case XK_V:
    case XK_v:
      return VKEY_V;
    case XK_W:
    case XK_w:
      return VKEY_W;
    case XK_X:
    case XK_x:
      return VKEY_X;
    case XK_Y:
    case XK_y:
      return VKEY_Y;
    case XK_Z:
    case XK_z:
      return VKEY_Z;
    case XK_0:
      return VKEY_0;
    case XK_1:
      return VKEY_1;
    case XK_2:
      return VKEY_2;
    case XK_3:
      return VKEY_3;
    case XK_4:
      return VKEY_4;
    case XK_5:
      return VKEY_5;
    case XK_6:
      return VKEY_6;
    case XK_7:
      return VKEY_7;
    case XK_8:
      return VKEY_8;
    case XK_9:
      return VKEY_9;

    // TODO(sad): A lot of keycodes are still missing.
  }

  DLOG(WARNING) << "Unknown keycode: " << keysym;
  return VKEY_UNKNOWN;
}

}  // namespace app
