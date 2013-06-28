// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TOUCH_SMOOTH_SCROLL_GESTURE_
#define CONTENT_BROWSER_RENDERER_HOST_TOUCH_SMOOTH_SCROLL_GESTURE_

#include "base/time/time.h"
#include "content/browser/renderer_host/smooth_scroll_calculator.h"
#include "content/port/browser/smooth_scroll_gesture.h"
#include "ui/gfx/point.h"

namespace aura {
class Window;
}

namespace content {

class TouchSmoothScrollGestureAura : public SmoothScrollGesture {
 public:
  TouchSmoothScrollGestureAura(bool scroll_down,
                               int pixels_to_scroll,
                               int mouse_event_x,
                               int mouse_event_y,
                               aura::Window* window);
 private:
  virtual ~TouchSmoothScrollGestureAura();

  // Overridden from SmoothScrollGesture.
  virtual bool ForwardInputEvents(base::TimeTicks now,
                                  RenderWidgetHost* host) OVERRIDE;

  bool scroll_down_;
  int pixels_to_scroll_;
  int pixels_scrolled_;
  gfx::Point location_;
  aura::Window* window_;
  SmoothScrollCalculator smooth_scroll_calculator_;

  DISALLOW_COPY_AND_ASSIGN(TouchSmoothScrollGestureAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TOUCH_SMOOTH_SCROLL_GESTURE_
