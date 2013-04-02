// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/basic_mouse_wheel_smooth_scroll_gesture.h"

#include "base/debug/trace_event.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"

namespace content {

BasicMouseWheelSmoothScrollGesture::BasicMouseWheelSmoothScrollGesture(
    bool scroll_down, int pixels_to_scroll,
    int mouse_event_x, int mouse_event_y)
    : scroll_down_(scroll_down),
      pixels_scrolled_(0),
      pixels_to_scroll_(pixels_to_scroll),
      mouse_event_x_(mouse_event_x),
      mouse_event_y_(mouse_event_y) { }

BasicMouseWheelSmoothScrollGesture::~BasicMouseWheelSmoothScrollGesture() { }

bool BasicMouseWheelSmoothScrollGesture::ForwardInputEvents(
    base::TimeTicks now, RenderWidgetHost* host) {

  if (pixels_scrolled_ >= pixels_to_scroll_)
    return false;

  double positionDelta = 10;
  if (!last_tick_time_.is_null()) {
    RenderWidgetHostImpl* hostImpl = RenderWidgetHostImpl::From(host);
    double desiredIntervalMs = hostImpl->SyntheticScrollMessageInterval();
    double velocity = 10 / desiredIntervalMs;
    double timeDelta = (now - last_tick_time_).InMillisecondsF();
    positionDelta = velocity * timeDelta;
  }

  last_tick_time_ = now;

  WebKit::WebMouseWheelEvent event;
  event.type = WebKit::WebInputEvent::MouseWheel;
  event.hasPreciseScrollingDeltas = 0;
  event.deltaY = scroll_down_ ? -positionDelta : positionDelta;
  // TODO(vollick): find a proper way to access
  // WebCore::WheelEvent::tickMultiplier.
  event.wheelTicksY = event.deltaY / 120;
  event.modifiers = 0;

  // TODO(nduca): Figure out plausible x and y values.
  event.globalX = 0;
  event.globalY = 0;
  event.x = mouse_event_x_;
  event.y = mouse_event_y_;
  event.windowX = event.x;
  event.windowY = event.y;
  host->ForwardWheelEvent(event);

  pixels_scrolled_ += abs(event.deltaY);

  TRACE_COUNTER_ID1(
      "gpu", "smooth_scroll_by_pixels_scrolled", this, pixels_scrolled_);

  return true;
}

}  // content

