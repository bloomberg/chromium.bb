// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BASIC_MOUSE_WHEEL_SMOOTH_SCROLL_GESTURE_
#define CONTENT_BROWSER_RENDERER_HOST_BASIC_MOUSE_WHEEL_SMOOTH_SCROLL_GESTURE_

#include "base/time.h"
#include "content/port/browser/smooth_scroll_gesture.h"

namespace content {

class RenderWidgetHost;

class BasicMouseWheelSmoothScrollGesture : public SmoothScrollGesture {
 public:
  BasicMouseWheelSmoothScrollGesture(bool scroll_down, int pixels_to_scroll,
                                     int mouse_event_x, int mouse_event_y);

  virtual bool ForwardInputEvents(base::TimeTicks now,
                                  RenderWidgetHost* host) OVERRIDE;
 private:
  virtual ~BasicMouseWheelSmoothScrollGesture();

  bool scroll_down_;
  int pixels_scrolled_;
  int pixels_to_scroll_;
  int mouse_event_x_;
  int mouse_event_y_;
  base::TimeTicks last_tick_time_;
};

}  // namespace content

#endif
