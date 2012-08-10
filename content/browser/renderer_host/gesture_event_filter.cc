// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/gesture_event_filter.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/tap_suppression_controller.h"

using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;

namespace content {
namespace {
// Returns |true| if two gesture events should be coalesced.
bool ShouldCoalesceGestureEvents(const WebKit::WebGestureEvent& last_event,
                                 const WebKit::WebGestureEvent& new_event) {
  return new_event.type == WebInputEvent::GestureScrollUpdate &&
      last_event.type == new_event.type &&
      last_event.modifiers == new_event.modifiers;
}
} // namespace

GestureEventFilter::GestureEventFilter(RenderWidgetHostImpl* rwhv)
     : render_widget_host_(rwhv),
       fling_in_progress_(false),
       gesture_event_pending_(false),
       tap_suppression_controller_(new TapSuppressionController(rwhv)) {
}

GestureEventFilter::~GestureEventFilter() { }

bool GestureEventFilter::ShouldDiscardFlingCancelEvent(
    const WebKit::WebGestureEvent& gesture_event) {
  DCHECK(gesture_event.type == WebInputEvent::GestureFlingCancel);
  if (coalesced_gesture_events_.empty() && fling_in_progress_)
    return false;
  GestureEventQueue::reverse_iterator it =
      coalesced_gesture_events_.rbegin();
  while (it != coalesced_gesture_events_.rend()) {
    if (it->type == WebInputEvent::GestureFlingStart)
      return false;
    if (it->type == WebInputEvent::GestureFlingCancel)
      return true;
    it++;
  }
  return true;
}

bool GestureEventFilter::ShouldForward(const WebGestureEvent& gesture_event) {
  if (gesture_event.type == WebInputEvent::GestureFlingCancel) {
    if (ShouldDiscardFlingCancelEvent(gesture_event))
      return false;
    fling_in_progress_ = false;
  }

  if (gesture_event_pending_) {
    if (coalesced_gesture_events_.empty() ||
        !ShouldCoalesceGestureEvents(coalesced_gesture_events_.back(),
                                     gesture_event)) {
     coalesced_gesture_events_.push_back(gesture_event);
    } else {
      WebGestureEvent* last_gesture_event =
          &coalesced_gesture_events_.back();
      last_gesture_event->deltaX += gesture_event.deltaX;
      last_gesture_event->deltaY += gesture_event.deltaY;
      DCHECK_GE(gesture_event.timeStampSeconds,
                last_gesture_event->timeStampSeconds);
      last_gesture_event->timeStampSeconds = gesture_event.timeStampSeconds;
    }
    return false;
  }
  gesture_event_pending_ = true;

  if (gesture_event.type == WebInputEvent::GestureFlingCancel) {
    tap_suppression_controller_->GestureFlingCancel(
        gesture_event.timeStampSeconds);
  } else if (gesture_event.type == WebInputEvent::GestureFlingStart) {
    fling_in_progress_ = true;
  }

  return true;
}

void GestureEventFilter::Reset() {
  fling_in_progress_ = false;
  coalesced_gesture_events_.clear();
  gesture_event_pending_ = false;
}

void GestureEventFilter::ProcessGestureAck(bool processed, int type) {
  if (type == WebInputEvent::GestureFlingCancel)
    tap_suppression_controller_->GestureFlingCancelAck(processed);

  gesture_event_pending_ = false;

  // Now send the next (coalesced) gesture event.
  if (!coalesced_gesture_events_.empty()) {
    WebGestureEvent next_gesture_event =
        coalesced_gesture_events_.front();
    coalesced_gesture_events_.pop_front();
    render_widget_host_->ForwardGestureEvent(next_gesture_event);
  }
}

TapSuppressionController*  GestureEventFilter::GetTapSuppressionController() {
  return tap_suppression_controller_.get();
}

void GestureEventFilter::FlingHasBeenHalted() {
  fling_in_progress_ = false;
}

} // namespace content
