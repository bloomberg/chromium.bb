// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/non_blocking_event_queue.h"

namespace content {

NonBlockingEventQueue::NonBlockingEventQueue(
    int routing_id,
    NonBlockingEventQueueClient* client)
    : routing_id_(routing_id), client_(client) {}

NonBlockingEventQueue::~NonBlockingEventQueue() {}

void NonBlockingEventQueue::HandleEvent(const blink::WebInputEvent* event,
                                        const ui::LatencyInfo& latency) {
  if (event->type == blink::WebInputEvent::MouseWheel) {
    if (wheel_events_.state() == WebInputEventQueueState::ITEM_PENDING) {
      wheel_events_.Queue(MouseWheelEventWithLatencyInfo(
          *static_cast<const blink::WebMouseWheelEvent*>(event), latency));
    } else {
      wheel_events_.set_state(WebInputEventQueueState::ITEM_PENDING);
      client_->SendNonBlockingEvent(routing_id_, event, latency);
    }
  } else if (blink::WebInputEvent::isTouchEventType(event->type)) {
    if (touch_events_.state() == WebInputEventQueueState::ITEM_PENDING) {
      touch_events_.Queue(TouchEventWithLatencyInfo(
          *static_cast<const blink::WebTouchEvent*>(event), latency));
    } else {
      touch_events_.set_state(WebInputEventQueueState::ITEM_PENDING);
      client_->SendNonBlockingEvent(routing_id_, event, latency);
    }
  } else {
    NOTREACHED() << "Invalid passive event type";
  }
}

void NonBlockingEventQueue::EventHandled(blink::WebInputEvent::Type type) {
  if (type == blink::WebInputEvent::MouseWheel) {
    if (!wheel_events_.empty()) {
      scoped_ptr<MouseWheelEventWithLatencyInfo> event = wheel_events_.Pop();

      client_->SendNonBlockingEvent(routing_id_, &event->event, event->latency);
    } else {
      wheel_events_.set_state(WebInputEventQueueState::ITEM_NOT_PENDING);
    }
  } else if (blink::WebInputEvent::isTouchEventType(type)) {
    if (!touch_events_.empty()) {
      scoped_ptr<TouchEventWithLatencyInfo> event = touch_events_.Pop();
      client_->SendNonBlockingEvent(routing_id_, &event->event, event->latency);
    } else {
      touch_events_.set_state(WebInputEventQueueState::ITEM_NOT_PENDING);
    }
  } else {
    NOTREACHED() << "Invalid passive event type";
  }
}

}  // namespace content
