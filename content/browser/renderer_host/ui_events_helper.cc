// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ui_events_helper.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_constants.h"

namespace {

int WebModifiersToUIFlags(int modifiers) {
  int flags = ui::EF_NONE;

  if (modifiers & WebKit::WebInputEvent::ShiftKey)
    flags |= ui::EF_SHIFT_DOWN;
  if (modifiers & WebKit::WebInputEvent::ControlKey)
    flags |= ui::EF_CONTROL_DOWN;
  if (modifiers & WebKit::WebInputEvent::AltKey)
    flags |= ui::EF_ALT_DOWN;

  if (modifiers & WebKit::WebInputEvent::LeftButtonDown)
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  if (modifiers & WebKit::WebInputEvent::RightButtonDown)
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;
  if (modifiers & WebKit::WebInputEvent::MiddleButtonDown)
    flags |= ui::EF_MIDDLE_MOUSE_BUTTON;

  if (modifiers & WebKit::WebInputEvent::CapsLockOn)
    flags |= ui::EF_CAPS_LOCK_DOWN;

  return flags;
}

ui::EventType WebTouchPointStateToEventType(
    WebKit::WebTouchPoint::State state) {
  switch (state) {
    case WebKit::WebTouchPoint::StateReleased:
      return ui::ET_TOUCH_RELEASED;

    case WebKit::WebTouchPoint::StatePressed:
      return ui::ET_TOUCH_PRESSED;

    case WebKit::WebTouchPoint::StateMoved:
      return ui::ET_TOUCH_MOVED;

    case WebKit::WebTouchPoint::StateCancelled:
      return ui::ET_TOUCH_CANCELLED;

    default:
      return ui::ET_UNKNOWN;
  }
}

WebKit::WebTouchPoint::State TouchPointStateFromEvent(
    const ui::TouchEvent& event) {
  switch (event.type()) {
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
    const ui::TouchEvent& event) {
  switch (event.type()) {
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

namespace content {

bool MakeUITouchEventsFromWebTouchEvents(const WebKit::WebTouchEvent& touch,
                                         ScopedVector<ui::TouchEvent>* list) {
  ui::EventType type = ui::ET_UNKNOWN;
  switch (touch.type) {
    case WebKit::WebInputEvent::TouchStart:
      type = ui::ET_TOUCH_PRESSED;
      break;
    case WebKit::WebInputEvent::TouchEnd:
      type = ui::ET_TOUCH_RELEASED;
      break;
    case WebKit::WebInputEvent::TouchMove:
      type = ui::ET_TOUCH_MOVED;
      break;
    case WebKit::WebInputEvent::TouchCancel:
      type = ui::ET_TOUCH_CANCELLED;
      break;
    default:
      NOTREACHED();
      return false;
  }

  int flags = WebModifiersToUIFlags(touch.modifiers);
  base::TimeDelta timestamp = base::TimeDelta::FromMicroseconds(
      static_cast<int64>(touch.timeStampSeconds * 1000000));
  for (unsigned i = 0; i < touch.touchesLength; ++i) {
    const WebKit::WebTouchPoint& point = touch.touches[i];
    if (WebTouchPointStateToEventType(point.state) != type)
      continue;
    // In aura, the touch-event needs to be in the screen coordinate, since the
    // touch-event is routed to RootWindow first. In Windows, on the other hand,
    // the touch-event is dispatched directly to the gesture-recognizer, so the
    // location needs to be in the local coordinate space.
#if defined(USE_AURA)
    gfx::Point location(point.screenPosition.x, point.screenPosition.y);
#else
    gfx::Point location(point.position.x, point.position.y);
#endif
    ui::TouchEvent* uievent = new ui::TouchEvent(type,
          location,
          flags,
          point.id,
          timestamp,
          point.radiusX,
          point.radiusY,
          point.rotationAngle,
          point.force);
    list->push_back(uievent);
  }
  return true;
}

WebKit::WebGestureEvent MakeWebGestureEventFromUIEvent(
    const ui::GestureEvent& event) {
  WebKit::WebGestureEvent gesture_event;

  switch (event.type()) {
    case ui::ET_GESTURE_TAP:
      gesture_event.type = WebKit::WebInputEvent::GestureTap;
      gesture_event.data.tap.tapCount = event.details().tap_count();
      gesture_event.data.tap.width = event.details().bounding_box().width();
      gesture_event.data.tap.height = event.details().bounding_box().height();
      break;
    case ui::ET_GESTURE_TAP_DOWN:
      gesture_event.type = WebKit::WebInputEvent::GestureTapDown;
      gesture_event.data.tapDown.width =
          event.details().bounding_box().width();
      gesture_event.data.tapDown.height =
          event.details().bounding_box().height();
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
      gesture_event.data.scrollUpdate.deltaX = event.details().scroll_x();
      gesture_event.data.scrollUpdate.deltaY = event.details().scroll_y();
      break;
    case ui::ET_GESTURE_SCROLL_END:
      gesture_event.type = WebKit::WebInputEvent::GestureScrollEnd;
      break;
    case ui::ET_GESTURE_PINCH_BEGIN:
      gesture_event.type = WebKit::WebInputEvent::GesturePinchBegin;
      break;
    case ui::ET_GESTURE_PINCH_UPDATE:
      gesture_event.type = WebKit::WebInputEvent::GesturePinchUpdate;
      gesture_event.data.pinchUpdate.scale = event.details().scale();
      break;
    case ui::ET_GESTURE_PINCH_END:
      gesture_event.type = WebKit::WebInputEvent::GesturePinchEnd;
      break;
    case ui::ET_SCROLL_FLING_START:
      gesture_event.type = WebKit::WebInputEvent::GestureFlingStart;
      gesture_event.data.flingStart.velocityX = event.details().velocity_x();
      gesture_event.data.flingStart.velocityY = event.details().velocity_y();
      break;
    case ui::ET_SCROLL_FLING_CANCEL:
      gesture_event.type = WebKit::WebInputEvent::GestureFlingCancel;
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      gesture_event.type = WebKit::WebInputEvent::GestureLongPress;
      gesture_event.data.longPress.width =
          event.details().bounding_box().width();
      gesture_event.data.longPress.height =
          event.details().bounding_box().height();
      break;
    case ui::ET_GESTURE_LONG_TAP:
      gesture_event.type = WebKit::WebInputEvent::GestureLongTap;
      gesture_event.data.longPress.width =
          event.details().bounding_box().width();
      gesture_event.data.longPress.height =
          event.details().bounding_box().height();
      break;
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      gesture_event.type = WebKit::WebInputEvent::GestureTwoFingerTap;
      gesture_event.data.twoFingerTap.firstFingerWidth =
          event.details().first_finger_width();
      gesture_event.data.twoFingerTap.firstFingerHeight =
          event.details().first_finger_height();
      break;
    case ui::ET_GESTURE_BEGIN:
    case ui::ET_GESTURE_END:
    case ui::ET_GESTURE_MULTIFINGER_SWIPE:
      gesture_event.type = WebKit::WebInputEvent::Undefined;
      break;
    default:
      NOTREACHED() << "Unknown gesture type: " << event.type();
  }

  gesture_event.sourceDevice = WebKit::WebGestureEvent::Touchscreen;
  gesture_event.modifiers = EventFlagsToWebEventModifiers(event.flags());
  gesture_event.timeStampSeconds = event.time_stamp().InSecondsF();

  return gesture_event;
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

WebKit::WebTouchPoint* UpdateWebTouchEventFromUIEvent(
    const ui::TouchEvent& event,
    WebKit::WebTouchEvent* web_event) {
  WebKit::WebTouchPoint* point = NULL;
  switch (event.type()) {
    case ui::ET_TOUCH_PRESSED:
      // Add a new touch point.
      if (web_event->touchesLength < WebKit::WebTouchEvent::touchesLengthCap) {
        point = &web_event->touches[web_event->touchesLength++];
        point->id = event.touch_id();
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
        if (point->id == event.touch_id())
          break;
        point = NULL;
      }
      break;
    }
    default:
      DLOG(WARNING) << "Unknown touch event " << event.type();
      break;
  }

  if (!point)
    return NULL;

  // The spec requires the radii values to be positive (and 1 when unknown).
  point->radiusX = std::max(1.f, event.radius_x());
  point->radiusY = std::max(1.f, event.radius_y());
  point->rotationAngle = event.rotation_angle();
  point->force = event.force();

  // Update the location and state of the point.
  point->state = TouchPointStateFromEvent(event);
  if (point->state == WebKit::WebTouchPoint::StateMoved) {
    // It is possible for badly written touch drivers to emit Move events even
    // when the touch location hasn't changed. In such cases, consume the event
    // and pretend nothing happened.
    if (point->position.x == event.x() && point->position.y == event.y())
      return NULL;
  }
  point->position.x = event.x();
  point->position.y = event.y();

  const gfx::Point root_point = event.root_location();
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
  web_event->timeStampSeconds = event.time_stamp().InSecondsF();
  web_event->modifiers = EventFlagsToWebEventModifiers(event.flags());

  return point;
}

}  // namespace content
