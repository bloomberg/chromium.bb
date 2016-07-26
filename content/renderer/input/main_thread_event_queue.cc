// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/main_thread_event_queue.h"
#include "content/common/input/event_with_latency_info.h"
#include "content/common/input_messages.h"

namespace content {

EventWithDispatchType::EventWithDispatchType(
    const blink::WebInputEvent& event,
    const ui::LatencyInfo& latency,
    InputEventDispatchType dispatch_type)
    : ScopedWebInputEventWithLatencyInfo(event, latency),
      dispatch_type_(dispatch_type) {}

EventWithDispatchType::~EventWithDispatchType() {}

bool EventWithDispatchType::CanCoalesceWith(
    const EventWithDispatchType& other) const {
  return other.dispatch_type_ == dispatch_type_ &&
         ScopedWebInputEventWithLatencyInfo::CanCoalesceWith(other);
}

void EventWithDispatchType::CoalesceWith(const EventWithDispatchType& other) {
  // If we are blocking and are coalescing touch, make sure to keep
  // the touch ids that need to be acked.
  if (dispatch_type_ == DISPATCH_TYPE_BLOCKING_NOTIFY_MAIN) {
    // We should only have blocking touch events that need coalescing.
    DCHECK(blink::WebInputEvent::isTouchEventType(other.event().type));
    eventsToAck_.push_back(
        WebInputEventTraits::GetUniqueTouchEventId(other.event()));
  }
  ScopedWebInputEventWithLatencyInfo::CoalesceWith(other);
}

MainThreadEventQueue::MainThreadEventQueue(int routing_id,
                                           MainThreadEventQueueClient* client)
    : routing_id_(routing_id),
      client_(client),
      is_flinging_(false),
      sent_notification_to_main_(false) {}

MainThreadEventQueue::~MainThreadEventQueue() {}

bool MainThreadEventQueue::HandleEvent(
    const blink::WebInputEvent* event,
    const ui::LatencyInfo& latency,
    InputEventDispatchType original_dispatch_type,
    InputEventAckState ack_result) {
  DCHECK(original_dispatch_type == DISPATCH_TYPE_BLOCKING ||
         original_dispatch_type == DISPATCH_TYPE_NON_BLOCKING);
  DCHECK(ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING ||
         ack_result == INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  bool non_blocking = original_dispatch_type == DISPATCH_TYPE_NON_BLOCKING ||
                      ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING;

  InputEventDispatchType dispatch_type =
      non_blocking ? DISPATCH_TYPE_NON_BLOCKING_NOTIFY_MAIN
                   : DISPATCH_TYPE_BLOCKING_NOTIFY_MAIN;

  bool is_wheel = event->type == blink::WebInputEvent::MouseWheel;
  bool is_touch = blink::WebInputEvent::isTouchEventType(event->type);

  if (is_wheel || is_touch) {
    std::unique_ptr<EventWithDispatchType> cloned_event(
        new EventWithDispatchType(*event, latency, dispatch_type));

    // Adjust the |dispatchType| on the event since the compositor
    // determined all event listeners are passive.
    if (non_blocking) {
      if (is_wheel) {
        static_cast<blink::WebMouseWheelEvent&>(cloned_event->event())
            .dispatchType = blink::WebInputEvent::ListenersNonBlockingPassive;
      } else if (is_touch) {
        static_cast<blink::WebTouchEvent&>(cloned_event->event()).dispatchType =
            blink::WebInputEvent::ListenersNonBlockingPassive;
      }
    }

    if (is_touch) {
      static_cast<blink::WebTouchEvent&>(cloned_event->event())
          .dispatchedDuringFling = is_flinging_;
    }

    if (sent_notification_to_main_) {
      events_.Queue(std::move(cloned_event));
    } else {
      if (non_blocking) {
        sent_notification_to_main_ = true;
        client_->SendEventToMainThread(routing_id_, &cloned_event->event(),
                                       latency, dispatch_type);
      } else {
        // If there is nothing in the event queue and the event is
        // blocking pass the |original_dispatch_type| to avoid
        // having the main thread call us back as an optimization.
        client_->SendEventToMainThread(routing_id_, &cloned_event->event(),
                                       latency, original_dispatch_type);
      }
    }
  } else {
    client_->SendEventToMainThread(routing_id_, event, latency,
                                   original_dispatch_type);
  }

  // send an ack when we are non-blocking.
  return non_blocking;
}

void MainThreadEventQueue::EventHandled(blink::WebInputEvent::Type type,
                                        InputEventAckState ack_result) {
  if (in_flight_event_) {
    // Send acks for blocking touch events.
    for (const auto id : in_flight_event_->eventsToAck())
      client_->SendInputEventAck(routing_id_, type, ack_result, id);
    }
    if (!events_.empty()) {
      in_flight_event_ = events_.Pop();
      client_->SendEventToMainThread(routing_id_, &in_flight_event_->event(),
                                     in_flight_event_->latencyInfo(),
                                     in_flight_event_->dispatchType());
    } else {
      in_flight_event_.reset();
      sent_notification_to_main_ = false;
    }
}

}  // namespace content
