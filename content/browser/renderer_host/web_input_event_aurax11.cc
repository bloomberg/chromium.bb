// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions based heavily on:
// third_party/WebKit/Source/WebKit/chromium/public/gtk/WebInputEventFactory.cpp
//
/*
 * Copyright (C) 2006-2011 Google Inc. All rights reserved.
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

#include "content/browser/renderer_host/web_input_event_aura.h"

#include <cstdlib>
#include <X11/Xlib.h>

#include "base/event_types.h"
#include "base/logging.h"
#include "ui/aura/event.h"
#include "ui/base/events.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"

namespace content {

// chromium WebKit does not provide a WebInputEventFactory for X11, so we have
// to do the work here ourselves.

namespace {

double XEventTimeToWebEventTime(Time time) {
  // Convert from time in ms to time in s.
  return time / 1000.0;
}

int EventFlagsToWebEventModifiers(int flags) {
  int modifiers = 0;
  if (flags & ui::EF_SHIFT_DOWN)
    modifiers |= WebKit::WebInputEvent::ShiftKey;
  if (flags & ui::EF_CONTROL_DOWN)
    modifiers |= WebKit::WebInputEvent::ControlKey;
  if (flags & ui::EF_ALT_DOWN)
    modifiers |= WebKit::WebInputEvent::AltKey;
  // TODO(beng): MetaKey/META_MASK
  if (flags & ui::EF_LEFT_BUTTON_DOWN)
    modifiers |= WebKit::WebInputEvent::LeftButtonDown;
  if (flags & ui::EF_MIDDLE_BUTTON_DOWN)
    modifiers |= WebKit::WebInputEvent::MiddleButtonDown;
  if (flags & ui::EF_RIGHT_BUTTON_DOWN)
    modifiers |= WebKit::WebInputEvent::RightButtonDown;
  if (flags & ui::EF_CAPS_LOCK_DOWN)
    modifiers |= WebKit::WebInputEvent::CapsLockOn;
  return modifiers;
}

int XStateToWebEventModifiers(unsigned int state) {
  int modifiers = 0;
  if (state & ShiftMask)
    modifiers |= WebKit::WebInputEvent::ShiftKey;
  if (state & ControlMask)
    modifiers |= WebKit::WebInputEvent::ControlKey;
  if (state & Mod1Mask)
    modifiers |= WebKit::WebInputEvent::AltKey;
  // TODO(beng): MetaKey/META_MASK
  if (state & Button1Mask)
    modifiers |= WebKit::WebInputEvent::LeftButtonDown;
  if (state & Button2Mask)
    modifiers |= WebKit::WebInputEvent::MiddleButtonDown;
  if (state & Button3Mask)
    modifiers |= WebKit::WebInputEvent::RightButtonDown;
  if (state & LockMask)
    modifiers |= WebKit::WebInputEvent::CapsLockOn;
  if (state & Mod2Mask)
    modifiers |= WebKit::WebInputEvent::NumLockOn;
  return modifiers;
}

int XKeyEventToWindowsKeyCode(XKeyEvent* event) {
  return ui::KeyboardCodeFromXKeyEvent((XEvent*)event);
}

// From
// third_party/WebKit/Source/WebKit/chromium/src/gtk/WebInputEventFactory.cpp:
WebKit::WebUChar GetControlCharacter(int windows_key_code, bool shift) {
  if (windows_key_code >= ui::VKEY_A &&
    windows_key_code <= ui::VKEY_Z) {
    // ctrl-A ~ ctrl-Z map to \x01 ~ \x1A
    return windows_key_code - ui::VKEY_A + 1;
  }
  if (shift) {
    // following graphics chars require shift key to input.
    switch (windows_key_code) {
      // ctrl-@ maps to \x00 (Null byte)
      case ui::VKEY_2:
        return 0;
      // ctrl-^ maps to \x1E (Record separator, Information separator two)
      case ui::VKEY_6:
        return 0x1E;
      // ctrl-_ maps to \x1F (Unit separator, Information separator one)
      case ui::VKEY_OEM_MINUS:
        return 0x1F;
      // Returns 0 for all other keys to avoid inputting unexpected chars.
      default:
        break;
    }
  } else {
    switch (windows_key_code) {
      // ctrl-[ maps to \x1B (Escape)
      case ui::VKEY_OEM_4:
        return 0x1B;
      // ctrl-\ maps to \x1C (File separator, Information separator four)
      case ui::VKEY_OEM_5:
        return 0x1C;
      // ctrl-] maps to \x1D (Group separator, Information separator three)
      case ui::VKEY_OEM_6:
        return 0x1D;
      // ctrl-Enter maps to \x0A (Line feed)
      case ui::VKEY_RETURN:
        return 0x0A;
      // Returns 0 for all other keys to avoid inputting unexpected chars.
      default:
        break;
    }
  }
  return 0;
}

WebKit::WebMouseEvent::Button ButtonFromXButton(int button) {
  switch (button) {
    case 1:
      return WebKit::WebMouseEvent::ButtonLeft;
    case 2:
      return WebKit::WebMouseEvent::ButtonMiddle;
    case 3:
      return WebKit::WebMouseEvent::ButtonRight;
    default:
      break;
  }
  return WebKit::WebMouseEvent::ButtonNone;
}

WebKit::WebMouseEvent::Button ButtonFromXState(int state) {
  if (state & Button1MotionMask)
    return WebKit::WebMouseEvent::ButtonLeft;
  if (state & Button2MotionMask)
    return WebKit::WebMouseEvent::ButtonMiddle;
  if (state & Button3MotionMask)
    return WebKit::WebMouseEvent::ButtonRight;
  return WebKit::WebMouseEvent::ButtonNone;
}

// We have to count clicks (for double-clicks) manually.
unsigned int g_num_clicks = 0;
double g_last_click_time = 0.0;
int g_last_click_x = 0;
int g_last_click_y = 0;
WebKit::WebMouseEvent::Button g_last_click_button =
    WebKit::WebMouseEvent::ButtonNone;

bool ShouldForgetPreviousClick(double time, int x, int y) {
  const double double_click_time = 0.250;  // in seconds
  const int double_click_distance = 5;
  return (time - g_last_click_time) > double_click_time ||
      std::abs(x - g_last_click_x) > double_click_distance ||
      std::abs(y - g_last_click_y) > double_click_distance;
}

void ResetClickCountState() {
  g_num_clicks = 0;
  g_last_click_time = 0.0;
  g_last_click_x = 0;
  g_last_click_y = 0;
  g_last_click_button = WebKit::WebMouseEvent::ButtonNone;
}

WebKit::WebTouchPoint::State TouchPointStateFromEvent(
    const aura::TouchEvent* event) {
  switch (event->type()) {
    case ui::ET_TOUCH_PRESSED:
      return WebKit::WebTouchPoint::StatePressed;
    case ui::ET_TOUCH_RELEASED:
      return WebKit::WebTouchPoint::StateReleased;
    case ui::ET_TOUCH_MOVED:
      return WebKit::WebTouchPoint::StateMoved;
    case ui::ET_TOUCH_CANCELLED:
      return WebKit::WebTouchPoint::StateCancelled;
    default:
      return WebKit::WebTouchPoint::StateUndefined;
  }
}

WebKit::WebInputEvent::Type TouchEventTypeFromEvent(
    const aura::TouchEvent* event) {
  switch (event->type()) {
    case ui::ET_TOUCH_PRESSED:
      return WebKit::WebInputEvent::TouchStart;
    case ui::ET_TOUCH_RELEASED:
      return WebKit::WebInputEvent::TouchEnd;
    case ui::ET_TOUCH_MOVED:
      return WebKit::WebInputEvent::TouchMove;
    case ui::ET_TOUCH_CANCELLED:
      return WebKit::WebInputEvent::TouchCancel;
    default:
      return WebKit::WebInputEvent::Undefined;
  }
}

}  // namespace

WebKit::WebMouseEvent MakeWebMouseEventFromAuraEvent(aura::MouseEvent* event) {
  WebKit::WebMouseEvent webkit_event;

  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  webkit_event.timeStampSeconds = event->time_stamp().ToDoubleT();

  webkit_event.button = WebKit::WebMouseEvent::ButtonNone;
  if (event->flags() & ui::EF_LEFT_BUTTON_DOWN)
    webkit_event.button = WebKit::WebMouseEvent::ButtonLeft;
  if (event->flags() & ui::EF_MIDDLE_BUTTON_DOWN)
    webkit_event.button = WebKit::WebMouseEvent::ButtonMiddle;
  if (event->flags() & ui::EF_RIGHT_BUTTON_DOWN)
    webkit_event.button = WebKit::WebMouseEvent::ButtonRight;

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      webkit_event.type = WebKit::WebInputEvent::MouseDown;
      if (!ShouldForgetPreviousClick(event->time_stamp().ToDoubleT(),
            event->location().x(), event->location().y()) &&
          webkit_event.button == g_last_click_button) {
        ++g_num_clicks;
      } else {
        g_num_clicks = 1;
        g_last_click_time = event->time_stamp().ToDoubleT();
        g_last_click_x = event->location().x();
        g_last_click_y = event->location().y();
        g_last_click_button = webkit_event.button;
      }
      webkit_event.clickCount = g_num_clicks;
      break;
    case ui::ET_MOUSE_RELEASED:
      webkit_event.type = WebKit::WebInputEvent::MouseUp;
      break;
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
      webkit_event.type = WebKit::WebInputEvent::MouseMove;
      if (ShouldForgetPreviousClick(event->time_stamp().ToDoubleT(),
            event->location().x(), event->location().y()))
        ResetClickCountState();
      break;
    default:
      NOTIMPLEMENTED() << "Received unexpected event: " << event->type();
      break;
  }

  return webkit_event;
}

WebKit::WebMouseWheelEvent MakeWebMouseWheelEventFromAuraEvent(
    aura::MouseEvent* event) {
  WebKit::WebMouseWheelEvent webkit_event;

  webkit_event.type = WebKit::WebInputEvent::MouseWheel;
  webkit_event.button = WebKit::WebMouseEvent::ButtonNone;
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  webkit_event.timeStampSeconds = event->time_stamp().ToDoubleT();
  webkit_event.deltaY = ui::GetMouseWheelOffset(event->native_event());
  webkit_event.wheelTicksY = webkit_event.deltaY > 0 ? 1 : -1;

  return webkit_event;
}

WebKit::WebKeyboardEvent MakeWebKeyboardEventFromAuraEvent(
    aura::KeyEvent* event) {
  base::NativeEvent native_event = event->native_event();
  WebKit::WebKeyboardEvent webkit_event;
  XKeyEvent* native_key_event = &native_event->xkey;

  webkit_event.timeStampSeconds =
      XEventTimeToWebEventTime(native_key_event->time);
  webkit_event.modifiers = XStateToWebEventModifiers(native_key_event->state);

  switch (native_event->type) {
    case KeyPress:
      webkit_event.type = event->is_char() ? WebKit::WebInputEvent::Char :
          WebKit::WebInputEvent::RawKeyDown;
      break;
    case KeyRelease:
      webkit_event.type = WebKit::WebInputEvent::KeyUp;
      break;
    default:
      NOTREACHED();
  }

  if (webkit_event.modifiers & WebKit::WebInputEvent::AltKey)
    webkit_event.isSystemKey = true;

  webkit_event.windowsKeyCode = XKeyEventToWindowsKeyCode(native_key_event);
  webkit_event.nativeKeyCode = native_key_event->keycode;

  if (webkit_event.windowsKeyCode == ui::VKEY_RETURN)
    webkit_event.unmodifiedText[0] = '\r';
  else
    webkit_event.unmodifiedText[0] = ui::DefaultSymbolFromXEvent(native_event);

  if (webkit_event.modifiers & WebKit::WebInputEvent::ControlKey) {
    webkit_event.text[0] =
        GetControlCharacter(
            webkit_event.windowsKeyCode,
            webkit_event.modifiers & WebKit::WebInputEvent::ShiftKey);
  } else {
    webkit_event.text[0] = webkit_event.unmodifiedText[0];
  }

  webkit_event.setKeyIdentifierFromWindowsKeyCode();

  // TODO: IsAutoRepeat/IsKeyPad?

  return webkit_event;
}

WebKit::WebTouchPoint* UpdateWebTouchEventFromAuraEvent(
    aura::TouchEvent* event, WebKit::WebTouchEvent* web_event) {
  WebKit::WebTouchPoint* point = NULL;
  switch (event->type()) {
    case ui::ET_TOUCH_PRESSED:
      // Add a new touch point.
      if (web_event->touchesLength < WebKit::WebTouchEvent::touchesLengthCap) {
        point = &web_event->touches[web_event->touchesLength++];
        point->id = event->touch_id();
      }
      break;
    case ui::ET_TOUCH_RELEASED:
    case ui::ET_TOUCH_CANCELLED:
    case ui::ET_TOUCH_MOVED: {
      // The touch point should have been added to the event from an earlier
      // _PRESSED event. So find that.
      // At the moment, only a maximum of 4 touch-points are allowed. So a
      // simple loop should be sufficient.
      for (unsigned i = 0; i < web_event->touchesLength; ++i) {
        point = web_event->touches + i;
        if (point->id == event->touch_id())
          break;
        point = NULL;
      }
      break;
    }
    default:
      DLOG(WARNING) << "Unknown touch event " << event->type();
      break;
  }

  if (!point)
    return NULL;

  point->radiusX = event->radius_x();
  point->radiusY = event->radius_y();
  point->rotationAngle = event->rotation_angle();
  point->force = event->force();

  // Update the location and state of the point.
  point->state = TouchPointStateFromEvent(event);
  if (point->state == WebKit::WebTouchPoint::StateMoved) {
    // It is possible for badly written touch drivers to emit Move events even
    // when the touch location hasn't changed. In such cases, consume the event
    // and pretend nothing happened.
    if (point->position.x == event->x() && point->position.y == event->y())
      return NULL;
  }
  point->position.x = event->x();
  point->position.y = event->y();

  // TODO(sad): Convert to screen coordinates.
  point->screenPosition.x = point->position.x;
  point->screenPosition.y = point->position.y;

  // Mark the rest of the points as stationary.
  for (unsigned i = 0; i < web_event->touchesLength; ++i) {
    WebKit::WebTouchPoint* iter = web_event->touches + i;
    if (iter != point)
      iter->state = WebKit::WebTouchPoint::StateStationary;
  }

  // Update the type of the touch event.
  web_event->type = TouchEventTypeFromEvent(event);
  web_event->timeStampSeconds = event->time_stamp().ToDoubleT();

  return point;
}

}  // namespace content
