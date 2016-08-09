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
  if (dispatch_type_ == DISPATCH_TYPE_BLOCKING) {
    // We should only have blocking touch events that need coalescing.
    eventsToAck_.push_back(
        WebInputEventTraits::GetUniqueTouchEventId(other.event()));
  }
  ScopedWebInputEventWithLatencyInfo::CoalesceWith(other);
}

MainThreadEventQueue::MainThreadEventQueue(
    int routing_id,
    MainThreadEventQueueClient* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
    : routing_id_(routing_id),
      client_(client),
      is_flinging_(false),
      main_task_runner_(main_task_runner) {}

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
      non_blocking ? DISPATCH_TYPE_NON_BLOCKING : DISPATCH_TYPE_BLOCKING;

  bool is_wheel = event->type == blink::WebInputEvent::MouseWheel;
  bool is_touch = blink::WebInputEvent::isTouchEventType(event->type);

  std::unique_ptr<EventWithDispatchType> cloned_event(
      new EventWithDispatchType(*event, latency, dispatch_type));

  if (is_touch) {
    blink::WebTouchEvent& touch_event =
        static_cast<blink::WebTouchEvent&>(cloned_event->event());
    touch_event.dispatchedDuringFling = is_flinging_;
    // Adjust the |dispatchType| on the event since the compositor
    // determined all event listeners are passive.
    if (non_blocking) {
      touch_event.dispatchType =
          blink::WebInputEvent::ListenersNonBlockingPassive;
    }
  }
  if (is_wheel && non_blocking) {
    // Adjust the |dispatchType| on the event since the compositor
    // determined all event listeners are passive.
    static_cast<blink::WebMouseWheelEvent&>(cloned_event->event())
        .dispatchType = blink::WebInputEvent::ListenersNonBlockingPassive;
  }

  QueueEvent(std::move(cloned_event));

  // send an ack when we are non-blocking.
  return non_blocking;
}

void MainThreadEventQueue::PopEventOnMainThread() {
  {
    base::AutoLock lock(event_queue_lock_);
    if (!events_.empty())
      in_flight_event_ = events_.Pop();
  }

  if (in_flight_event_) {
    InputEventDispatchType dispatch_type = in_flight_event_->dispatchType();
    if (!in_flight_event_->eventsToAck().empty() &&
        dispatch_type == DISPATCH_TYPE_BLOCKING) {
      dispatch_type = DISPATCH_TYPE_BLOCKING_NOTIFY_MAIN;
    }

    client_->HandleEventOnMainThread(routing_id_, &in_flight_event_->event(),
                                     in_flight_event_->latencyInfo(),
                                     dispatch_type);
  }

  in_flight_event_.reset();
}

void MainThreadEventQueue::EventHandled(blink::WebInputEvent::Type type,
                                        InputEventAckState ack_result) {
  if (in_flight_event_) {
    // Send acks for blocking touch events.
    for (const auto id : in_flight_event_->eventsToAck())
      client_->SendInputEventAck(routing_id_, type, ack_result, id);
  }
}

void MainThreadEventQueue::SendEventNotificationToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MainThreadEventQueue::PopEventOnMainThread,
                            this));
}

void MainThreadEventQueue::QueueEvent(
    std::unique_ptr<EventWithDispatchType> event) {
  bool send_notification = false;
  {
    base::AutoLock lock(event_queue_lock_);
    size_t size_before = events_.size();
    events_.Queue(std::move(event));
    send_notification = events_.size() != size_before;
  }
  if (send_notification)
    SendEventNotificationToMainThread();
}

}  // namespace content
