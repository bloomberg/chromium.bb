// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_action_filter.h"

#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace content {

TouchActionFilter::TouchActionFilter() :
  drop_scroll_gesture_events_(false),
  allowed_touch_action_(TOUCH_ACTION_AUTO) {
}

bool TouchActionFilter::FilterGestureEvent(
    const WebGestureEvent& gesture_event) {
  // Filter for allowable touch actions first (eg. before the TouchEventQueue
  // can decide to send a touch cancel event).
  // TODO(rbyers): Add touch-action control over for pinch.  crbug.com/247566.
  switch(gesture_event.type) {
    case WebInputEvent::GestureScrollBegin:
      if (allowed_touch_action_ == TOUCH_ACTION_NONE)
        drop_scroll_gesture_events_ = true;
      // FALL THROUGH
    case WebInputEvent::GestureScrollUpdate:
      if (drop_scroll_gesture_events_)
        return true;
      break;

    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureFlingStart:
      allowed_touch_action_ = content::TOUCH_ACTION_AUTO;
      if (drop_scroll_gesture_events_) {
        drop_scroll_gesture_events_ = false;
        return true;
      }
      break;

    default:
      // Gesture events unrelated to touch actions (panning/zooming) are left
      // alone.
      break;
  }

  return false;
}

void TouchActionFilter::OnSetTouchAction(
    content::TouchAction touch_action) {
  // For multiple fingers, we take the intersection of the touch actions for
  // all fingers that have gone down during this action.
  // TODO(rbyers): What exact multi-finger semantic do we want?  This is left
  // as implementation-defined in the pointer events specification.
  // crbug.com/247566.
  if (touch_action == content::TOUCH_ACTION_NONE)
    allowed_touch_action_ = content::TOUCH_ACTION_NONE;
}

}
