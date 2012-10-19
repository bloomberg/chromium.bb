// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/touch_event_queue.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/port/browser/render_widget_host_view_port.h"

namespace content {

TouchEventQueue::TouchEventQueue(RenderWidgetHostImpl* host)
    : render_widget_host_(host) {
}

TouchEventQueue::~TouchEventQueue() {
}

void TouchEventQueue::QueueEvent(const WebKit::WebTouchEvent& event) {
  if (touch_queue_.empty()) {
    // There is no touch event in the queue. Forward it to the renderer
    // immediately.
    touch_queue_.push_back(event);
    render_widget_host_->ForwardTouchEventImmediately(event);
    return;
  }

  WebKit::WebTouchEvent copy = event;

  // If the last queued touch-event was a touch-move, and the current event is
  // also a touch-move, then the events can be coalesced into a single event.
  if (touch_queue_.size() > 1) {
    const WebKit::WebTouchEvent& last_event = touch_queue_.back();
    if (copy.type == WebKit::WebInputEvent::TouchMove &&
        last_event.type == WebKit::WebInputEvent::TouchMove &&
        copy.modifiers == last_event.modifiers &&
        copy.touchesLength == last_event.touchesLength) {
      // The WebTouchPoints include absolute position information. So it is
      // sufficient to simply replace the previous event with the new event.
      // However, it is necessary to make sure that all the points have the
      // correct state, i.e. the touch-points that moved in the last event, but
      // didn't change in the current event, will have Stationary state. It is
      // necessary to change them back to Moved state.
      for (unsigned i = 0; i < last_event.touchesLength; ++i) {
        if (last_event.touches[i].state == WebKit::WebTouchPoint::StateMoved)
          copy.touches[i].state = WebKit::WebTouchPoint::StateMoved;
      }
      touch_queue_.pop_back();
    }
  }
  touch_queue_.push_back(copy);
}

void TouchEventQueue::ProcessTouchAck(bool processed) {
  PopTouchEventToView(processed);
  // If there's a queued touch-event, then forward it to the renderer now.
  if (!touch_queue_.empty())
    render_widget_host_->ForwardTouchEventImmediately(touch_queue_.front());
}

void TouchEventQueue::FlushQueue() {
  while (!touch_queue_.empty())
    PopTouchEventToView(false);
}

void TouchEventQueue::Reset() {
  touch_queue_.clear();
}

void TouchEventQueue::PopTouchEventToView(bool processed) {
  CHECK(!touch_queue_.empty());
  WebKit::WebTouchEvent acked_event = touch_queue_.front();
  touch_queue_.pop_front();

  // Note that acking the touch-event may result in multiple gestures being sent
  // to the renderer.
  RenderWidgetHostViewPort* view = RenderWidgetHostViewPort::FromRWHV(
      render_widget_host_->GetView());
  view->ProcessAckedTouchEvent(acked_event, processed);
}

}  // namespace content
