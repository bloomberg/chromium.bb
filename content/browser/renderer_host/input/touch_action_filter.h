// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_ACTION_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_ACTION_FILTER_H_

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/common/input/touch_action.h"

namespace blink {
class WebGestureEvent;
}

namespace content {

// The TouchActionFilter is responsible for filtering scroll and pinch gesture
// events according to the CSS touch-action values the renderer has sent for
// each touch point.
// For details see the touch-action design doc at http://goo.gl/KcKbxQ.
class CONTENT_EXPORT TouchActionFilter {
public:
  TouchActionFilter();

  // Returns true if the supplied gesture event should be dropped based on
  // the current touch-action state.
  bool FilterGestureEvent(const blink::WebGestureEvent& gesture_event);

  // Called when a set-touch-action message is received from the renderer
  // for a touch start event that is currently in flight.
  void OnSetTouchAction(content::TouchAction touch_action);

private:
  // Whether GestureScroll events should be discarded due to touch-action.
  bool drop_scroll_gesture_events_;

  // What touch actions are currently permitted.
  content::TouchAction allowed_touch_action_;

  DISALLOW_COPY_AND_ASSIGN(TouchActionFilter);
};

}
#endif // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_ACTION_FILTER_H_
