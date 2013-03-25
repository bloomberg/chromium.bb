// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/ibus_keymap.h"

#define XK_MISCELLANY
#include <X11/keysymdef.h>
#include <X11/XF86keysym.h>

namespace chromeos {
namespace input_method {

std::string GetIBusKey(int keyval) {
  // TODO: Ensure all keys are supported.
  switch (keyval) {
    case XK_Escape:
      return "Esc";
    case XK_F1:
    case XF86XK_Back:
      return "HistoryBack";
    case XK_F2:
    case XF86XK_Forward:
      return "HistoryForward";
    case XK_F3:
    case XF86XK_Reload:
      return "BrowserRefresh";
    case XK_F4:
    case XF86XK_LaunchB:
      return "ChromeOSFullscreen";  // TODO: Check this value
    case XK_F5:
    case XF86XK_LaunchA:
      return "ChromeOSSwitchWindow";  // TODO: Check this value
    case XK_F6:
    case XF86XK_MonBrightnessDown:
      return "BrightnessDown";
    case XK_F7:
    case XF86XK_KbdBrightnessUp:
      return "BrightnessUp";
    case XK_F8:
    case XF86XK_AudioMute:
      return "AudioVolumeMute";
    case XK_F9:
    case XF86XK_AudioLowerVolume:
      return "AudioVolumeDown";
    case XK_F10:
    case XF86XK_AudioRaiseVolume:
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
    case XK_Shift_L:
    case XK_Shift_R:
      return "Shift";
    case XK_Alt_L:
    case XK_Alt_R:
      return "Alt";
    case XK_Control_L:
    case XK_Control_R:
      return "Ctrl";
    default: {
      // TODO: Properly support unicode characters.
      char value[2];
      value[0] = keyval;
      value[1] = '\0';
      return value;
    }
  }
}

// We should send KeyCode as string to meet DOM Level 4 event specification
// proposal. https://dvcs.w3.org/hg/d4e/raw-file/tip/source_respec.htm
// The original spec is 1:1 mapping with USB keycode, but for the Extension IME,
// mapping with XKB keycode is sufficient, because it works only on Chrome OS.
// We have to have own mapping because We can't use WebKit component in
// Extension IME,
// TODO(nona): Use if the original spec is introduced.
std::string GetIBusKeyCode(uint16 keycode) {
  switch (keycode) {
    // Function keys
    case 0x0009: return "Esc";
    case 0x0043: return "F1";
    case 0x0044: return "F2";
    case 0x0045: return "F3";
    case 0x0046: return "F4";
    case 0x0047: return "F5";
    case 0x0048: return "F6";
    case 0x0049: return "F7";
    case 0x004a: return "F8";
    case 0x004b: return "F9";
    case 0x004c: return "F10";
    case 0x005f: return "F11";
    case 0x0060: return "F12";

    // Alphanumeric keys
    case 0x000a: return "Digit1";
    case 0x000b: return "Digit2";
    case 0x000c: return "Digit3";
    case 0x000d: return "Digit4";
    case 0x000e: return "Digit5";
    case 0x000f: return "Digit6";
    case 0x0010: return "Digit7";
    case 0x0011: return "Digit8";
    case 0x0012: return "Digit9";
    case 0x0013: return "Digit0";
    case 0x0014: return "Minus";
    case 0x0015: return "Equal";
    case 0x0016: return "Backspace";
    case 0x0017: return "Tab";
    case 0x0018: return "KeyQ";
    case 0x0019: return "KeyW";
    case 0x001a: return "KeyE";
    case 0x001b: return "KeyR";
    case 0x001c: return "KeyT";
    case 0x001d: return "KeyY";
    case 0x001e: return "KeyU";
    case 0x001f: return "KeyI";
    case 0x0020: return "KeyO";
    case 0x0021: return "KeyP";
    case 0x0022: return "BracketLeft";
    case 0x0023: return "BlacketRight";
    case 0x0024: return "Enter";
    case 0x0025: return "ControlLeft";
    case 0x0026: return "KeyA";
    case 0x0027: return "KeyS";
    case 0x0028: return "KeyD";
    case 0x0029: return "KeyF";
    case 0x002a: return "KeyG";
    case 0x002b: return "KeyH";
    case 0x002c: return "KeyJ";
    case 0x002d: return "KeyK";
    case 0x002e: return "KeyL";
    case 0x002f: return "Semicolon";
    case 0x0030: return "Quote";
    case 0x0031: return "BackQuote";
    case 0x0032: return "ShiftLeft";
    case 0x0033: return "Backslash";
    case 0x0034: return "KeyZ";
    case 0x0035: return "KeyX";
    case 0x0036: return "KeyC";
    case 0x0037: return "KeyV";
    case 0x0038: return "KeyB";
    case 0x0039: return "KeyN";
    case 0x003a: return "KeyM";
    case 0x003b: return "Comma";
    case 0x003c: return "Period";
    case 0x003d: return "Slash";
    case 0x003e: return "ShiftRight";
    case 0x003f: return "NumpadMultiply";
    case 0x0040: return "AltLeft";
    case 0x0041: return "Space";
    case 0x0042: return "CapsLock";
    case 0x004d: return "NumLock";
    case 0x004e: return "ScrollLock";
    case 0x005e: return "IntlBackslash";
    case 0x0064: return "Convert";
    case 0x0065: return "KanaMode";
    case 0x0066: return "NoConvert";
    case 0x0069: return "ControlRight";
    case 0x006c: return "AltRight";
    case 0x0082: return "HangulMode";
    case 0x0083: return "Hanja";
    case 0x0085: return "OSLeft";
    case 0x0086: return "OSRight";
    case 0x0087: return "ContextMenu";
    case 0x0061: return "IntlRo";
    case 0x0084: return "IntlYen";

    // Control pad keys
    case 0x006b: return "PrintScreen";
    case 0x0070: return "PageUp";
    case 0x0073: return "End";
    case 0x0076: return "Insert";
    case 0x0077: return "Delete";
    case 0x006e: return "Home";
    case 0x0075: return "PageDown";
    case 0x0079: return "VolumeMute";
    case 0x007a: return "VolumeDown";
    case 0x007b: return "VolumeUp";
    case 0x007c: return "Power";
    case 0x007f: return "Pause";
    case 0x0092: return "Help";

    // Arrow pad keys
    case 0x006f: return "ArrowUp";
    case 0x0071: return "ArrowLeft";
    case 0x0072: return "ArrowRight";
    case 0x0074: return "ArrowDown";

    // Numpad keys
    case 0x005a: return "Numpad0";
    case 0x0057: return "Numpad1";
    case 0x0058: return "Numpad2";
    case 0x0059: return "Numpad3";
    case 0x0053: return "Numpad4";
    case 0x0054: return "Numpad5";
    case 0x0055: return "Numpad6";
    case 0x004f: return "Numpad7";
    case 0x0050: return "Numpad8";
    case 0x0051: return "Numpad9";
    case 0x0052: return "NumpadSubtract";
    case 0x0056: return "NumpadAdd";
    case 0x005b: return "NumpadDecimal";
    case 0x0068: return "NumpadEnter";
    case 0x006a: return "NumpadDivide";
    case 0x00bb: return "NumpadParentLeft";
    case 0x00bc: return "NumpadParentRight";

    // Unsupported keys

    // No entry in specification.
    case 0x0062: return "";  // UsbKeyCode: 0x070092(LANG3)
    case 0x0063: return "";  // UsbKeyCode: 0x070093(LANG4)
    case 0x007d: return "";  // UsbKeyCode: 0x070067(Num_=)
    case 0x007e: return "";  // UsbkeyCode: 0x0700d7(Num_+-)
    case 0x0081: return "";  // UsbKeyCode: 0x0700dc(NumpadDecimal)
    case 0x0088: return "";  // UsbKeyCode: 0x07009b(Cancel)
    case 0x0089: return "";  // UsbKeyCode: 0x070079(Again)
    case 0x008b: return "";  // UsbKeyCode: 0x07007a(Undo)
    case 0x008d: return "";  // UsbKeyCode: 0x07007c(Copy)
    case 0x008f: return "";  // UsbKeyCode: 0x07007d(Paste)
    case 0x0090: return "";  // UsbKeyCode: 0x07007e(Find)
    case 0x0091: return "";  // UsbKeyCode: 0x07007b(Cut)
    case 0x0093: return "";  // UsbKeyCode: 0x070076(Menu)

    // USB Usage Page 0x01: Generic Desktop Page
    case 0x0094: return "";  // 0x0C page: AL_Calculator
    case 0x0096: return "";  // 0x01 page: SystemSleep
    case 0x0097: return "";  // 0x01 page: SystemWakeUp

    // USB Usage Page 0x0c: Consumer Page
    case 0x0098: return "";  // AL_FileBrowser (Explorer)";
    case 0x00a4: return "";  // AC_Bookmarks (Favorites)";
    case 0x00a5: return "";  // AL_LocalMachineBrowser";
    case 0x00a6: return "";  // AC_Back";
    case 0x00a7: return "";  // AC_Forward";
    case 0x00b5: return "";  // AC_Refresh (Reload)";
    case 0x00ef: return "";  // AC_Send";
    case 0x00f0: return "";  // AC_Reply";
    case 0x00f1: return "";  // AC_ForwardMsg (MailForward)";
    case 0x00f3: return "";  // AL_Documents";

    default: return "";
  }
}

}  // namespace input_method
}  // namespace chromeos
