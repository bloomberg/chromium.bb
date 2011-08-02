// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ibus_keymap.h"

#if defined(HAVE_IBUS)
#include <ibus.h>
#endif

namespace chromeos {
namespace input_method {

#if defined(HAVE_IBUS)
std::string GetIBusKey(int keyval) {
  // TODO: Ensure all keys are supported.
  switch (keyval) {
    case IBUS_Escape:
      return "Esc";
    case IBUS_F1:
      return "HistoryBack";
    case IBUS_F2:
      return "HistoryForward";
    case IBUS_F3:
      return "BrowserRefresh";
    case IBUS_F4:
      return "ChromeOSFullscreen";  // TODO: Check this value
    case IBUS_F5:
      return "ChromeOSSwitchWindow";  // TODO: Check this value
    case IBUS_F6:
      return "BrightnessDown";
    case IBUS_F7:
      return "BrightnessUp";
    case IBUS_F8:
      return "AudioVolumeMute";
    case IBUS_F9:
      return "AudioVolumeDown";
    case IBUS_F10:
      return "AudioVolumeUp";
    case IBUS_BackSpace:
      return "Backspace";
    case IBUS_Tab:
      return "Tab";
    case IBUS_KP_Enter:
    case IBUS_Return:
      return "Enter";
    case IBUS_Meta_L:
      return "BrowserSearch";
    case IBUS_Up:
    case IBUS_KP_Up:
      return "Up";
    case IBUS_Down:
    case IBUS_KP_Down:
      return "Down";
    case IBUS_Left:
    case IBUS_KP_Left:
      return "Left";
    case IBUS_Right:
    case IBUS_KP_Right:
      return "Right";
    case IBUS_Page_Up:
      return "PageUp";
    case IBUS_Page_Down:
      return "PageDown";
    case IBUS_Home:
      return "Home";
    case IBUS_End:
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
#else
std::string GetIBusKey(int keyval) {
  return "";
}

std::string GetIBusKeyCode(int keycode) {
  return "";
}

#endif // HAVE_IBUS

}  // namespace input_method
}  // namespace chromeos
