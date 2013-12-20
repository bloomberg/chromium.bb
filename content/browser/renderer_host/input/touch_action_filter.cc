// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_action_filter.h"

#include <math.h>

#include "base/logging.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace content {

TouchActionFilter::TouchActionFilter() :
  drop_scroll_gesture_events_(false),
  allowed_touch_action_(TOUCH_ACTION_AUTO) {
}

bool TouchActionFilter::FilterGestureEvent(WebGestureEvent* gesture_event) {
  // Filter for allowable touch actions first (eg. before the TouchEventQueue
  // can decide to send a touch cancel event).
  // TODO(rbyers): Add touch-action control over for pinch.  crbug.com/247566.
  switch(gesture_event->type) {
    case WebInputEvent::GestureScrollBegin:
      DCHECK(!drop_scroll_gesture_events_);
      drop_scroll_gesture_events_ = ShouldSuppressScroll(*gesture_event);
      return drop_scroll_gesture_events_;

    case WebInputEvent::GestureScrollUpdate:
      if (drop_scroll_gesture_events_)
        return true;
      else {
        if (allowed_touch_action_ == TOUCH_ACTION_PAN_X) {
          gesture_event->data.scrollUpdate.deltaY = 0;
          gesture_event->data.scrollUpdate.velocityY = 0;
        } else if (allowed_touch_action_ == TOUCH_ACTION_PAN_Y) {
          gesture_event->data.scrollUpdate.deltaX = 0;
          gesture_event->data.scrollUpdate.velocityX = 0;
        }
      }
      break;

    case WebInputEvent::GestureFlingStart:
      if (gesture_event->sourceDevice != WebGestureEvent::Touchscreen)
        break;
      if (!drop_scroll_gesture_events_) {
        if (allowed_touch_action_ == TOUCH_ACTION_PAN_X)
          gesture_event->data.flingStart.velocityY = 0;
        if (allowed_touch_action_ == TOUCH_ACTION_PAN_Y)
          gesture_event->data.flingStart.velocityX = 0;
      }
      return FilterGestureEnd();

    case WebInputEvent::GestureScrollEnd:
      return FilterGestureEnd();

    default:
      // Gesture events unrelated to touch actions (panning/zooming) are left
      // alone.
      break;
  }

  return false;
}

bool TouchActionFilter::FilterGestureEnd() {
  allowed_touch_action_ = TOUCH_ACTION_AUTO;
  if (drop_scroll_gesture_events_) {
    drop_scroll_gesture_events_ = false;
    return true;
  }
  return false;
}

void TouchActionFilter::OnSetTouchAction(TouchAction touch_action) {
  // For multiple fingers, we take the intersection of the touch actions for
  // all fingers that have gone down during this action.
  // TODO(rbyers): What exact multi-finger semantic do we want?  This is left
  // as implementation-defined in the pointer events specification.
  // crbug.com/247566.
  allowed_touch_action_ = Intersect(allowed_touch_action_, touch_action);
}

bool TouchActionFilter::ShouldSuppressScroll(
    const blink::WebGestureEvent& gesture_event) {
  DCHECK_EQ(gesture_event.type, WebInputEvent::GestureScrollBegin);
  if (allowed_touch_action_ == TOUCH_ACTION_AUTO)
    return false;
  if (allowed_touch_action_ == TOUCH_ACTION_NONE)
    return true;

  // If there's no hint or it's perfectly diagonal, then allow the scroll.
  if (fabs(gesture_event.data.scrollBegin.deltaXHint) ==
      fabs(gesture_event.data.scrollBegin.deltaYHint))
    return false;

  // Determine the primary initial axis of the scroll, and check whether
  // panning along that axis is permitted.
  if (fabs(gesture_event.data.scrollBegin.deltaXHint) >
      fabs(gesture_event.data.scrollBegin.deltaYHint))
    return !(allowed_touch_action_ & TOUCH_ACTION_PAN_X);
  return !(allowed_touch_action_ & TOUCH_ACTION_PAN_Y);
}

TouchAction TouchActionFilter::Intersect(TouchAction ta1, TouchAction ta2) {
  if (ta1 == TOUCH_ACTION_NONE || ta2 == TOUCH_ACTION_NONE)
    return TOUCH_ACTION_NONE;
  if (ta1 == TOUCH_ACTION_AUTO)
    return ta2;
  if (ta2 == TOUCH_ACTION_AUTO)
    return ta1;

  // Only the true flags are left - take their intersection.
  if (!(ta1 & ta2))
    return TOUCH_ACTION_NONE;
  return static_cast<TouchAction>(ta1 & ta2);
}

}
