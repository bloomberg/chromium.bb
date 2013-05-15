// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/touch_smooth_scroll_gesture_aura.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

namespace {

WebKit::WebTouchEvent CreateWebTouchPointAt(const gfx::Point& location,
                                            WebKit::WebInputEvent::Type type) {
  WebKit::WebTouchEvent touch;
  touch.type = type;
  touch.touchesLength = 1;
  touch.changedTouchesLength = 1;

  WebKit::WebTouchPoint point;
  switch (type) {
    case WebKit::WebInputEvent::TouchStart:
      point.state = WebKit::WebTouchPoint::StatePressed;
      break;

    case WebKit::WebInputEvent::TouchMove:
      point.state = WebKit::WebTouchPoint::StateMoved;
      break;

    case WebKit::WebInputEvent::TouchEnd:
      point.state = WebKit::WebTouchPoint::StateReleased;
      break;

    case WebKit::WebInputEvent::TouchCancel:
      point.state = WebKit::WebTouchPoint::StateCancelled;
      break;

    default:
      NOTREACHED();
  }
  point.position.x = location.x();
  point.position.y = location.y();

  point.screenPosition = point.position;

  touch.touches[0] = touch.changedTouches[0] = point;

  return touch;
}

}  // namespace

namespace content {

TouchSmoothScrollGestureAura::TouchSmoothScrollGestureAura(bool scroll_down,
                                                           int pixels_to_scroll,
                                                           int mouse_event_x,
                                                           int mouse_event_y)
    : scroll_down_(scroll_down),
      pixels_to_scroll_(pixels_to_scroll),
      pixels_scrolled_(0),
      location_(mouse_event_x, mouse_event_y) {
}

TouchSmoothScrollGestureAura::~TouchSmoothScrollGestureAura() {}

bool TouchSmoothScrollGestureAura::ForwardInputEvents(
    base::TimeTicks now,
    RenderWidgetHost* host) {
  if (pixels_scrolled_ >= pixels_to_scroll_)
    return false;

  RenderWidgetHostImpl* host_impl = RenderWidgetHostImpl::From(host);
  double position_delta = smooth_scroll_calculator_.GetScrollDelta(now,
      host_impl->GetSyntheticScrollMessageInterval());

  if (pixels_scrolled_ == 0) {
    host_impl->ForwardTouchEvent(CreateWebTouchPointAt(location_,
        WebKit::WebInputEvent::TouchStart));
  }

  location_.Offset(0, scroll_down_ ? -position_delta : position_delta);
  host_impl->ForwardTouchEvent(CreateWebTouchPointAt(location_,
        WebKit::WebInputEvent::TouchMove));

  pixels_scrolled_ += abs(position_delta);
  if (pixels_scrolled_ >= pixels_to_scroll_) {
    host_impl->ForwardTouchEvent(CreateWebTouchPointAt(location_,
        WebKit::WebInputEvent::TouchEnd));
  }

  return true;
}

}  // namespace content
