// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BASIC_MOUSE_WHEEL_SMOOTH_SCROLL_GESTURE_
#define CONTENT_BROWSER_RENDERER_HOST_BASIC_MOUSE_WHEEL_SMOOTH_SCROLL_GESTURE_

#include "base/time/time.h"
#include "content/browser/renderer_host/synthetic_gesture_calculator.h"
#include "content/port/browser/synthetic_gesture.h"

namespace content {

class RenderWidgetHost;

class BasicMouseWheelSmoothScrollGesture : public SyntheticGesture {
 public:
  BasicMouseWheelSmoothScrollGesture(bool scroll_down, int pixels_to_scroll,
                                     int mouse_event_x, int mouse_event_y);

  virtual bool ForwardInputEvents(base::TimeTicks now,
                                  RenderWidgetHost* host) OVERRIDE;
 private:
  virtual ~BasicMouseWheelSmoothScrollGesture();

  SyntheticGestureCalculator synthetic_gesture_calculator_;

  bool scroll_down_;
  int pixels_scrolled_;
  int pixels_to_scroll_;
  int mouse_event_x_;
  int mouse_event_y_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BASIC_MOUSE_WHEEL_SMOOTH_SCROLL_GESTURE_
