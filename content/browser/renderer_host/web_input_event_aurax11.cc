// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdlib>

#include "base/event_types.h"
#include "base/logging.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace content {

// chromium WebKit does not provide a WebInputEventFactory for X11, so we have
// to do the work here ourselves.

namespace {

// This matches Firefox behavior.
const int kPixelsPerTick = 53;

int EventFlagsToWebEventModifiers(int flags) {
  int modifiers = 0;
  if (flags & ui::EF_SHIFT_DOWN)
    modifiers |= WebKit::WebInputEvent::ShiftKey;
  if (flags & ui::EF_CONTROL_DOWN)
    modifiers |= WebKit::WebInputEvent::ControlKey;
  if (flags & ui::EF_ALT_DOWN)
    modifiers |= WebKit::WebInputEvent::AltKey;
  // TODO(beng): MetaKey/META_MASK
  if (flags & ui::EF_LEFT_MOUSE_BUTTON)
    modifiers |= WebKit::WebInputEvent::LeftButtonDown;
  if (flags & ui::EF_MIDDLE_MOUSE_BUTTON)
    modifiers |= WebKit::WebInputEvent::MiddleButtonDown;
  if (flags & ui::EF_RIGHT_MOUSE_BUTTON)
    modifiers |= WebKit::WebInputEvent::RightButtonDown;
  if (flags & ui::EF_CAPS_LOCK_DOWN)
    modifiers |= WebKit::WebInputEvent::CapsLockOn;
  return modifiers;
}

int XKeyEventToWindowsKeyCode(XKeyEvent* event) {
  int windows_key_code =
      ui::KeyboardCodeFromXKeyEvent(reinterpret_cast<XEvent*>(event));
  if (windows_key_code == ui::VKEY_SHIFT ||
      windows_key_code == ui::VKEY_CONTROL ||
      windows_key_code == ui::VKEY_MENU) {
    // To support DOM3 'location' attribute, we need to lookup an X KeySym and
    // set ui::VKEY_[LR]XXX instead of ui::VKEY_XXX.
    KeySym keysym = XK_VoidSymbol;
    XLookupString(event, NULL, 0, &keysym, NULL);
    switch (keysym) {
      case XK_Shift_L:
        return ui::VKEY_LSHIFT;
      case XK_Shift_R:
        return ui::VKEY_RSHIFT;
      case XK_Control_L:
        return ui::VKEY_LCONTROL;
      case XK_Control_R:
        return ui::VKEY_RCONTROL;
      case XK_Meta_L:
      case XK_Alt_L:
        return ui::VKEY_LMENU;
      case XK_Meta_R:
      case XK_Alt_R:
        return ui::VKEY_RMENU;
    }
  }
  return windows_key_code;
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

WebKit::WebTouchPoint::State TouchPointStateFromEvent(
    const ui::TouchEvent* event) {
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
    const ui::TouchEvent* event) {
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

WebKit::WebMouseEvent MakeWebMouseEventFromAuraEvent(ui::MouseEvent* event) {
  WebKit::WebMouseEvent webkit_event;

  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();

  webkit_event.button = WebKit::WebMouseEvent::ButtonNone;
  if (event->flags() & ui::EF_LEFT_MOUSE_BUTTON)
    webkit_event.button = WebKit::WebMouseEvent::ButtonLeft;
  if (event->flags() & ui::EF_MIDDLE_MOUSE_BUTTON)
    webkit_event.button = WebKit::WebMouseEvent::ButtonMiddle;
  if (event->flags() & ui::EF_RIGHT_MOUSE_BUTTON)
    webkit_event.button = WebKit::WebMouseEvent::ButtonRight;

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      webkit_event.type = WebKit::WebInputEvent::MouseDown;
      webkit_event.clickCount = event->GetClickCount();
      break;
    case ui::ET_MOUSE_RELEASED:
      webkit_event.type = WebKit::WebInputEvent::MouseUp;
      break;
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
      webkit_event.type = WebKit::WebInputEvent::MouseMove;
      break;
    default:
      NOTIMPLEMENTED() << "Received unexpected event: " << event->type();
      break;
  }

  return webkit_event;
}

WebKit::WebMouseWheelEvent MakeWebMouseWheelEventFromAuraEvent(
    ui::MouseWheelEvent* event) {
  WebKit::WebMouseWheelEvent webkit_event;

  webkit_event.type = WebKit::WebInputEvent::MouseWheel;
  webkit_event.button = WebKit::WebMouseEvent::ButtonNone;
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();
  webkit_event.deltaY = event->offset();
  webkit_event.wheelTicksY = webkit_event.deltaY / kPixelsPerTick;

  return webkit_event;
}

WebKit::WebMouseWheelEvent MakeWebMouseWheelEventFromAuraEvent(
    ui::ScrollEvent* event) {
  WebKit::WebMouseWheelEvent webkit_event;

  webkit_event.type = WebKit::WebInputEvent::MouseWheel;
  webkit_event.button = WebKit::WebMouseEvent::ButtonNone;
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();
  webkit_event.hasPreciseScrollingDeltas = true;
  webkit_event.deltaX = event->x_offset();
  webkit_event.wheelTicksX = webkit_event.deltaX / kPixelsPerTick;
  webkit_event.deltaY = event->y_offset();
  webkit_event.wheelTicksY = webkit_event.deltaY / kPixelsPerTick;

  return webkit_event;
}

// NOTE: ui::ScrollEvent instances come from the touchpad.
WebKit::WebGestureEvent MakeWebGestureEventFromAuraEvent(
    ui::ScrollEvent* event) {
  WebKit::WebGestureEvent webkit_event;

  switch (event->type()) {
    case ui::ET_SCROLL:
      // TODO(sadrul || rjkroege): This will do touchscreen style scrolling in
      // response to touchpad events. Currently, touchscreen and touchpad
      // scrolls are the same. However, if the planned changes to touchscreen
      // scrolling take place, this will no longer be so. If so, this needs to
      // be adjusted.
      webkit_event.type = WebKit::WebInputEvent::GestureScrollUpdate;
      webkit_event.data.scrollUpdate.deltaX = event->x_offset();
      webkit_event.data.scrollUpdate.deltaY = event->y_offset();
      break;
    case ui::ET_SCROLL_FLING_START:
      webkit_event.type = WebKit::WebInputEvent::GestureFlingStart;
      webkit_event.data.flingStart.velocityX = event->x_offset();
      webkit_event.data.flingStart.velocityY = event->y_offset();
      webkit_event.data.flingStart.sourceDevice =
          WebKit::WebGestureEvent::Touchpad;
      break;
    case ui::ET_SCROLL_FLING_CANCEL:
      webkit_event.type = WebKit::WebInputEvent::GestureFlingCancel;
      break;
    default:
      NOTREACHED() << "Unknown gesture type: " << event->type();
  }

  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();

  return webkit_event;
}

WebKit::WebKeyboardEvent MakeWebKeyboardEventFromAuraEvent(
    ui::KeyEvent* event) {
  base::NativeEvent native_event = event->native_event();
  WebKit::WebKeyboardEvent webkit_event;
  XKeyEvent* native_key_event = &native_event->xkey;

  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());

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
    webkit_event.unmodifiedText[0] = ui::GetCharacterFromXEvent(native_event);

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

// NOTE: ui::GestureEvent instances come from the touchscreen.
WebKit::WebGestureEvent MakeWebGestureEventFromAuraEvent(
    ui::GestureEvent* event) {
  WebKit::WebGestureEvent gesture_event;

  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
      gesture_event.type = WebKit::WebInputEvent::GestureTap;
      gesture_event.data.tap.tapCount = event->details().tap_count();
      gesture_event.data.tap.width = event->details().bounding_box().width();
      gesture_event.data.tap.height = event->details().bounding_box().height();
      break;
    case ui::ET_GESTURE_TAP_DOWN:
      gesture_event.type = WebKit::WebInputEvent::GestureTapDown;
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
      gesture_event.type = WebKit::WebInputEvent::GestureTapCancel;
      break;
    case ui::ET_GESTURE_DOUBLE_TAP:
      gesture_event.type = WebKit::WebInputEvent::GestureDoubleTap;
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      gesture_event.type = WebKit::WebInputEvent::GestureScrollBegin;
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      gesture_event.type = WebKit::WebInputEvent::GestureScrollUpdate;
      gesture_event.data.scrollUpdate.deltaX = event->details().scroll_x();
      gesture_event.data.scrollUpdate.deltaY = event->details().scroll_y();
      break;
    case ui::ET_GESTURE_SCROLL_END:
      gesture_event.type = WebKit::WebInputEvent::GestureScrollEnd;
      break;
    case ui::ET_GESTURE_PINCH_BEGIN:
      gesture_event.type = WebKit::WebInputEvent::GesturePinchBegin;
      break;
    case ui::ET_GESTURE_PINCH_UPDATE:
      gesture_event.type = WebKit::WebInputEvent::GesturePinchUpdate;
      gesture_event.data.pinchUpdate.scale = event->details().scale();
      break;
    case ui::ET_GESTURE_PINCH_END:
      gesture_event.type = WebKit::WebInputEvent::GesturePinchEnd;
      break;
    case ui::ET_SCROLL_FLING_START:
      gesture_event.type = WebKit::WebInputEvent::GestureFlingStart;
      gesture_event.data.flingStart.velocityX = event->details().velocity_x();
      gesture_event.data.flingStart.velocityY = event->details().velocity_y();
      gesture_event.data.flingStart.sourceDevice =
          WebKit::WebGestureEvent::Touchscreen;
      break;
    case ui::ET_SCROLL_FLING_CANCEL:
      gesture_event.type = WebKit::WebInputEvent::GestureFlingCancel;
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      gesture_event.type = WebKit::WebInputEvent::GestureLongPress;
      gesture_event.data.longPress.width =
          event->details().bounding_box().width();
      gesture_event.data.longPress.height =
          event->details().bounding_box().height();
      break;
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      gesture_event.type = WebKit::WebInputEvent::GestureTwoFingerTap;
      break;
    case ui::ET_GESTURE_BEGIN:
    case ui::ET_GESTURE_END:
    case ui::ET_GESTURE_MULTIFINGER_SWIPE:
      gesture_event.type = WebKit::WebInputEvent::Undefined;
      break;
    default:
      NOTREACHED() << "Unknown gesture type: " << event->type();
  }

  gesture_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  gesture_event.timeStampSeconds = event->time_stamp().InSecondsF();

  return gesture_event;
}

WebKit::WebTouchPoint* UpdateWebTouchEventFromAuraEvent(
    ui::TouchEvent* event, WebKit::WebTouchEvent* web_event) {
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

  // The spec requires the radii values to be positive (and 1 when unknown).
  point->radiusX = std::max(1.f, event->radius_x());
  point->radiusY = std::max(1.f, event->radius_y());
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

  const gfx::Point root_point = event->root_location();
  point->screenPosition.x = root_point.x();
  point->screenPosition.y = root_point.y();

  // Mark the rest of the points as stationary.
  for (unsigned i = 0; i < web_event->touchesLength; ++i) {
    WebKit::WebTouchPoint* iter = web_event->touches + i;
    if (iter != point)
      iter->state = WebKit::WebTouchPoint::StateStationary;
  }

  // Update the type of the touch event.
  web_event->type = TouchEventTypeFromEvent(event);
  web_event->timeStampSeconds = event->time_stamp().InSecondsF();
  web_event->modifiers = EventFlagsToWebEventModifiers(event->flags());

  return point;
}

}  // namespace content
