// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "content/browser/renderer_host/input/web_input_event_util.h"

#include <cmath>

#include "base/strings/string_util.h"
#include "content/common/input/web_touch_event_traits.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::MotionEvent;

namespace {

const char* GetKeyIdentifier(ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_MENU:
      return "Alt";
    case ui::VKEY_CONTROL:
      return "Control";
    case ui::VKEY_SHIFT:
      return "Shift";
    case ui::VKEY_CAPITAL:
      return "CapsLock";
    case ui::VKEY_LWIN:
    case ui::VKEY_RWIN:
      return "Win";
    case ui::VKEY_CLEAR:
      return "Clear";
    case ui::VKEY_DOWN:
      return "Down";
    case ui::VKEY_END:
      return "End";
    case ui::VKEY_RETURN:
      return "Enter";
    case ui::VKEY_EXECUTE:
      return "Execute";
    case ui::VKEY_F1:
      return "F1";
    case ui::VKEY_F2:
      return "F2";
    case ui::VKEY_F3:
      return "F3";
    case ui::VKEY_F4:
      return "F4";
    case ui::VKEY_F5:
      return "F5";
    case ui::VKEY_F6:
      return "F6";
    case ui::VKEY_F7:
      return "F7";
    case ui::VKEY_F8:
      return "F8";
    case ui::VKEY_F9:
      return "F9";
    case ui::VKEY_F10:
      return "F10";
    case ui::VKEY_F11:
      return "F11";
    case ui::VKEY_F12:
      return "F12";
    case ui::VKEY_F13:
      return "F13";
    case ui::VKEY_F14:
      return "F14";
    case ui::VKEY_F15:
      return "F15";
    case ui::VKEY_F16:
      return "F16";
    case ui::VKEY_F17:
      return "F17";
    case ui::VKEY_F18:
      return "F18";
    case ui::VKEY_F19:
      return "F19";
    case ui::VKEY_F20:
      return "F20";
    case ui::VKEY_F21:
      return "F21";
    case ui::VKEY_F22:
      return "F22";
    case ui::VKEY_F23:
      return "F23";
    case ui::VKEY_F24:
      return "F24";
    case ui::VKEY_HELP:
      return "Help";
    case ui::VKEY_HOME:
      return "Home";
    case ui::VKEY_INSERT:
      return "Insert";
    case ui::VKEY_LEFT:
      return "Left";
    case ui::VKEY_NEXT:
      return "PageDown";
    case ui::VKEY_PRIOR:
      return "PageUp";
    case ui::VKEY_PAUSE:
      return "Pause";
    case ui::VKEY_SNAPSHOT:
      return "PrintScreen";
    case ui::VKEY_RIGHT:
      return "Right";
    case ui::VKEY_SCROLL:
      return "Scroll";
    case ui::VKEY_SELECT:
      return "Select";
    case ui::VKEY_UP:
      return "Up";
    case ui::VKEY_DELETE:
      return "U+007F";  // Standard says that DEL becomes U+007F.
    case ui::VKEY_MEDIA_NEXT_TRACK:
      return "MediaNextTrack";
    case ui::VKEY_MEDIA_PREV_TRACK:
      return "MediaPreviousTrack";
    case ui::VKEY_MEDIA_STOP:
      return "MediaStop";
    case ui::VKEY_MEDIA_PLAY_PAUSE:
      return "MediaPlayPause";
    case ui::VKEY_VOLUME_MUTE:
      return "VolumeMute";
    case ui::VKEY_VOLUME_DOWN:
      return "VolumeDown";
    case ui::VKEY_VOLUME_UP:
      return "VolumeUp";
    default:
      return NULL;
  };
}

}  // namespace

namespace content {

void UpdateWindowsKeyCodeAndKeyIdentifier(blink::WebKeyboardEvent* event,
                                          ui::KeyboardCode windows_key_code) {
  event->windowsKeyCode = windows_key_code;

  const char* id = GetKeyIdentifier(windows_key_code);
  if (id) {
    base::strlcpy(event->keyIdentifier, id, sizeof(event->keyIdentifier) - 1);
  } else {
    base::snprintf(
        event->keyIdentifier,
        sizeof(event->keyIdentifier),
        "U+%04X",
        base::ToUpperASCII(static_cast<base::char16>(windows_key_code)));
  }
}

int WebEventModifiersToEventFlags(int modifiers) {
  int flags = 0;

  if (modifiers & blink::WebInputEvent::ShiftKey)
    flags |= ui::EF_SHIFT_DOWN;
  if (modifiers & blink::WebInputEvent::ControlKey)
    flags |= ui::EF_CONTROL_DOWN;
  if (modifiers & blink::WebInputEvent::AltKey)
    flags |= ui::EF_ALT_DOWN;
  if (modifiers & blink::WebInputEvent::MetaKey)
    flags |= ui::EF_COMMAND_DOWN;

  if (modifiers & blink::WebInputEvent::LeftButtonDown)
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::MiddleButtonDown)
    flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::RightButtonDown)
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::CapsLockOn)
    flags |= ui::EF_CAPS_LOCK_DOWN;
  if (modifiers & blink::WebInputEvent::IsAutoRepeat)
    flags |= ui::EF_IS_REPEAT;

  return flags;
}

}  // namespace content
