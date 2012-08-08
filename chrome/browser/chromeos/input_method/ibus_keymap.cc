// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ibus_keymap.h"

#define XK_MISCELLANY
#include <X11/keysymdef.h>

namespace chromeos {
namespace input_method {

std::string GetIBusKey(int keyval) {
  // TODO: Ensure all keys are supported.
  switch (keyval) {
    case XK_Escape:
      return "Esc";
    case XK_F1:
      return "HistoryBack";
    case XK_F2:
      return "HistoryForward";
    case XK_F3:
      return "BrowserRefresh";
    case XK_F4:
      return "ChromeOSFullscreen";  // TODO: Check this value
    case XK_F5:
      return "ChromeOSSwitchWindow";  // TODO: Check this value
    case XK_F6:
      return "BrightnessDown";
    case XK_F7:
      return "BrightnessUp";
    case XK_F8:
      return "AudioVolumeMute";
    case XK_F9:
      return "AudioVolumeDown";
    case XK_F10:
      return "AudioVolumeUp";
    case XK_BackSpace:
      return "Backspace";
    case XK_Delete:
    case XK_KP_Delete:
      return "Delete";
    case XK_Tab:
      return "Tab";
    case XK_KP_Enter:
    case XK_Return:
      return "Enter";
    case XK_Meta_L:
      return "BrowserSearch";
    case XK_Up:
    case XK_KP_Up:
      return "Up";
    case XK_Down:
    case XK_KP_Down:
      return "Down";
    case XK_Left:
    case XK_KP_Left:
      return "Left";
    case XK_Right:
    case XK_KP_Right:
      return "Right";
    case XK_Page_Up:
      return "PageUp";
    case XK_Page_Down:
      return "PageDown";
    case XK_Home:
      return "Home";
    case XK_End:
      return "End";
    default: {
      // TODO: Properly support unicode characters.
      char value[2];
      value[0] = keyval;
      value[1] = '\0';
      return value;
    }
  }
}

std::string GetIBusKeyCode(int keycode) {
  // TODO: Support keyboard layouts properly.
  return GetIBusKey(keycode);
}

}  // namespace input_method
}  // namespace chromeos
