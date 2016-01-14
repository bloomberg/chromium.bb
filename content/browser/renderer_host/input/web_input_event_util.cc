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
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::MotionEvent;

namespace content {

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
  if (modifiers & blink::WebInputEvent::CapsLockOn)
    flags |= ui::EF_CAPS_LOCK_ON;
  if (modifiers & blink::WebInputEvent::NumLockOn)
    flags |= ui::EF_NUM_LOCK_ON;
  if (modifiers & blink::WebInputEvent::ScrollLockOn)
    flags |= ui::EF_SCROLL_LOCK_ON;
  if (modifiers & blink::WebInputEvent::LeftButtonDown)
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::MiddleButtonDown)
    flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::RightButtonDown)
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::IsAutoRepeat)
    flags |= ui::EF_IS_REPEAT;

  return flags;
}

blink::WebInputEvent::Modifiers DomCodeToWebInputEventModifiers(
    ui::DomCode code) {
  switch (ui::KeycodeConverter::DomCodeToLocation(code)) {
    case ui::DomKeyLocation::LEFT:
      return blink::WebInputEvent::IsLeft;
    case ui::DomKeyLocation::RIGHT:
      return blink::WebInputEvent::IsRight;
    case ui::DomKeyLocation::NUMPAD:
      return blink::WebInputEvent::IsKeyPad;
    case ui::DomKeyLocation::STANDARD:
      break;
  }
  return static_cast<blink::WebInputEvent::Modifiers>(0);
}

// This coversino is temporary. WebInputEvent should be generated
// directly fromui::Event with the viewport coordinates. See
// crbug.com/563730.
scoped_ptr<blink::WebInputEvent> ConvertWebInputEventToViewport(
    const blink::WebInputEvent& event,
    float scale) {
  scoped_ptr<blink::WebInputEvent> scaled_event;
  if (scale == 1.f)
    return scaled_event;
  if (event.type == blink::WebMouseEvent::MouseWheel) {
    blink::WebMouseWheelEvent* wheel_event = new blink::WebMouseWheelEvent;
    scaled_event.reset(wheel_event);
    *wheel_event = static_cast<const blink::WebMouseWheelEvent&>(event);
    wheel_event->x *= scale;
    wheel_event->y *= scale;
    wheel_event->deltaX *= scale;
    wheel_event->deltaY *= scale;
    wheel_event->wheelTicksX *= scale;
    wheel_event->wheelTicksY *= scale;
  } else if (blink::WebInputEvent::isMouseEventType(event.type)) {
    blink::WebMouseEvent* mouse_event = new blink::WebMouseEvent;
    scaled_event.reset(mouse_event);
    *mouse_event = static_cast<const blink::WebMouseEvent&>(event);
    mouse_event->x *= scale;
    mouse_event->y *= scale;
    mouse_event->windowX = mouse_event->x;
    mouse_event->windowY = mouse_event->y;
    mouse_event->movementX *= scale;
    mouse_event->movementY *= scale;
  } else if (blink::WebInputEvent::isTouchEventType(event.type)) {
    blink::WebTouchEvent* touch_event = new blink::WebTouchEvent;
    scaled_event.reset(touch_event);
    *touch_event = static_cast<const blink::WebTouchEvent&>(event);
    for (unsigned i = 0; i < touch_event->touchesLength; i++) {
      touch_event->touches[i].position.x *= scale;
      touch_event->touches[i].position.y *= scale;
      touch_event->touches[i].radiusX *= scale;
      touch_event->touches[i].radiusY *= scale;
    }
  } else if (blink::WebInputEvent::isGestureEventType(event.type)) {
    blink::WebGestureEvent* gesture_event = new blink::WebGestureEvent;
    scaled_event.reset(gesture_event);
    *gesture_event = static_cast<const blink::WebGestureEvent&>(event);
    gesture_event->x *= scale;
    gesture_event->y *= scale;
    switch (gesture_event->type) {
      case blink::WebInputEvent::GestureScrollUpdate:
        gesture_event->data.scrollUpdate.deltaX *= scale;
        gesture_event->data.scrollUpdate.deltaY *= scale;
        break;
      case blink::WebInputEvent::GestureScrollBegin:
        gesture_event->data.scrollBegin.deltaXHint *= scale;
        gesture_event->data.scrollBegin.deltaYHint *= scale;
        break;

      case blink::WebInputEvent::GesturePinchUpdate:
        // Scale in pinch gesture is DSF agnostic.
        break;

      case blink::WebInputEvent::GestureDoubleTap:
      case blink::WebInputEvent::GestureTap:
      case blink::WebInputEvent::GestureTapUnconfirmed:
        gesture_event->data.tap.width *= scale;
        gesture_event->data.tap.height *= scale;
        break;

      case blink::WebInputEvent::GestureTapDown:
        gesture_event->data.tapDown.width *= scale;
        gesture_event->data.tapDown.height *= scale;
        break;

      case blink::WebInputEvent::GestureShowPress:
        gesture_event->data.showPress.width *= scale;
        gesture_event->data.showPress.height *= scale;
        break;

      case blink::WebInputEvent::GestureLongPress:
      case blink::WebInputEvent::GestureLongTap:
        gesture_event->data.longPress.width *= scale;
        gesture_event->data.longPress.height *= scale;
        break;

      case blink::WebInputEvent::GestureTwoFingerTap:
        gesture_event->data.twoFingerTap.firstFingerWidth *= scale;
        gesture_event->data.twoFingerTap.firstFingerHeight *= scale;
        break;

      case blink::WebInputEvent::GestureFlingStart:
        gesture_event->data.flingStart.velocityX *= scale;
        gesture_event->data.flingStart.velocityY *= scale;
        break;

      // These event does not have location data.
      case blink::WebInputEvent::GesturePinchBegin:
      case blink::WebInputEvent::GesturePinchEnd:
      case blink::WebInputEvent::GestureTapCancel:
      case blink::WebInputEvent::GestureFlingCancel:
      case blink::WebInputEvent::GestureScrollEnd:
        break;

      // TODO(oshima): Find out if ContextMenu needs to be scaled.
      default:
        break;
    }
  }
  return scaled_event;
}

}  // namespace content
