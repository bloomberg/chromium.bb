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

}  // namespace content
