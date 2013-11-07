// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_event_queue.h"

#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "base/stl_util.h"

namespace content {

typedef std::vector<TouchEventWithLatencyInfo> WebTouchEventWithLatencyList;

// This class represents a single coalesced touch event. However, it also keeps
// track of all the original touch-events that were coalesced into a single
// event. The coalesced event is forwarded to the renderer, while the original
// touch-events are sent to the Client (on ACK for the coalesced event) so that
// the Client receives the event with their original timestamp.
class CoalescedWebTouchEvent {
 public:
  explicit CoalescedWebTouchEvent(const TouchEventWithLatencyInfo& event)
      : coalesced_event_(event),
        ignore_ack_(false) {
    events_.push_back(event);
    TRACE_EVENT_ASYNC_BEGIN0(
        "input", "TouchEventQueue::QueueEvent", this);
  }

  ~CoalescedWebTouchEvent() {
    TRACE_EVENT_ASYNC_END0(
        "input", "TouchEventQueue::QueueEvent", this);
  }

  // Coalesces the event with the existing event if possible. Returns whether
  // the event was coalesced.
  bool CoalesceEventIfPossible(
      const TouchEventWithLatencyInfo& event_with_latency) {
    if (ignore_ack_)
      return false;

    if (!coalesced_event_.CanCoalesceWith(event_with_latency))
      return false;

    TRACE_EVENT_INSTANT0(
        "input", "TouchEventQueue::MoveCoalesced", TRACE_EVENT_SCOPE_THREAD);
    coalesced_event_.CoalesceWith(event_with_latency);
    events_.push_back(event_with_latency);
    return true;
  }

  const TouchEventWithLatencyInfo& coalesced_event() const {
    return coalesced_event_;
  }

  WebTouchEventWithLatencyList::iterator begin() {
    return events_.begin();
  }

  WebTouchEventWithLatencyList::iterator end() {
    return events_.end();
  }

  size_t size() const { return events_.size(); }

  bool ignore_ack() const { return ignore_ack_; }
  void set_ignore_ack(bool value) { ignore_ack_ = value; }

 private:
  // This is the event that is forwarded to the renderer.
  TouchEventWithLatencyInfo coalesced_event_;

  // This is the list of the original events that were coalesced.
  WebTouchEventWithLatencyList events_;

  // If |ignore_ack_| is true, don't send this touch event to client
  // when the event is acked.
  bool ignore_ack_;

  DISALLOW_COPY_AND_ASSIGN(CoalescedWebTouchEvent);
};

TouchEventQueue::TouchEventQueue(TouchEventQueueClient* client)
    : client_(client),
      dispatching_touch_ack_(NULL),
      no_touch_to_renderer_(false) {
  DCHECK(client);
}

TouchEventQueue::~TouchEventQueue() {
  if (!touch_queue_.empty())
    STLDeleteElements(&touch_queue_);
}

void TouchEventQueue::QueueEvent(const TouchEventWithLatencyInfo& event) {
  // If the queueing of |event| was triggered by an ack dispatch, defer
  // processing the event until the dispatch has finished.
  if (touch_queue_.empty() && !dispatching_touch_ack_) {
    // There is no touch event in the queue. Forward it to the renderer
    // immediately.
    touch_queue_.push_back(new CoalescedWebTouchEvent(event));
    TryForwardNextEventToRenderer();
    return;
  }

  // If the last queued touch-event was a touch-move, and the current event is
  // also a touch-move, then the events can be coalesced into a single event.
  if (touch_queue_.size() > 1) {
    CoalescedWebTouchEvent* last_event = touch_queue_.back();
    if (last_event->CoalesceEventIfPossible(event))
      return;
  }
  touch_queue_.push_back(new CoalescedWebTouchEvent(event));
}

void TouchEventQueue::ProcessTouchAck(InputEventAckState ack_result,
                                      const ui::LatencyInfo& latency_info) {
  DCHECK(!dispatching_touch_ack_);
  if (touch_queue_.empty())
    return;

  // Update the ACK status for each touch point in the ACKed event.
  const blink::WebTouchEvent& event =
      touch_queue_.front()->coalesced_event().event;
  if (event.type == blink::WebInputEvent::TouchEnd ||
      event.type == blink::WebInputEvent::TouchCancel) {
    // The points have been released. Erase the ACK states.
    for (unsigned i = 0; i < event.touchesLength; ++i) {
      const blink::WebTouchPoint& point = event.touches[i];
      if (point.state == blink::WebTouchPoint::StateReleased ||
          point.state == blink::WebTouchPoint::StateCancelled)
        touch_ack_states_.erase(point.id);
    }
  } else if (event.type == blink::WebInputEvent::TouchStart) {
    for (unsigned i = 0; i < event.touchesLength; ++i) {
      const blink::WebTouchPoint& point = event.touches[i];
      if (point.state == blink::WebTouchPoint::StatePressed)
        touch_ack_states_[point.id] = ack_result;
    }
  }

  PopTouchEventToClient(ack_result, latency_info);
  TryForwardNextEventToRenderer();
}

void TouchEventQueue::TryForwardNextEventToRenderer() {
  DCHECK(!dispatching_touch_ack_);
  // If there are queued touch events, then try to forward them to the renderer
  // immediately, or ACK the events back to the client if appropriate.
  while (!touch_queue_.empty()) {
    const TouchEventWithLatencyInfo& touch =
        touch_queue_.front()->coalesced_event();
    if (ShouldForwardToRenderer(touch.event)) {
      client_->SendTouchEventImmediately(touch);
      break;
    }
    PopTouchEventToClient(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
                          ui::LatencyInfo());
  }
}

void TouchEventQueue::OnGestureScrollEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  blink::WebInputEvent::Type type = gesture_event.event.type;
  if (type == blink::WebInputEvent::GestureScrollBegin) {
    // We assume the scroll event are generated synchronously from
    // dispatching a touch event ack, so that we can fake a cancel
    // event that has the correct touch ids as the touch event that
    // is being acked. If not, we don't do the touch-cancel optimization.
    if (no_touch_to_renderer_ || !dispatching_touch_ack_)
      return;
    no_touch_to_renderer_ = true;
    // Fake a TouchCancel to cancel the touch points of the touch event
    // that is currently being acked.
    TouchEventWithLatencyInfo cancel_event =
        dispatching_touch_ack_->coalesced_event();
    cancel_event.event.type = blink::WebInputEvent::TouchCancel;
    for (size_t i = 0; i < cancel_event.event.touchesLength; i++)
      cancel_event.event.touches[i].state =
          blink::WebTouchPoint::StateCancelled;
    CoalescedWebTouchEvent* coalesced_cancel_event =
        new CoalescedWebTouchEvent(cancel_event);
    // Ignore the ack of the touch cancel so when it is acked, it won't get
    // sent to gesture recognizer.
    coalesced_cancel_event->set_ignore_ack(true);
    // |dispatching_touch_ack_| is non-null when we reach here, meaning we
    // are in the scope of PopTouchEventToClient() and that no touch event
    // in the queue is waiting for ack from renderer. So we can just insert
    // the touch cancel at the beginning of the queue.
    touch_queue_.push_front(coalesced_cancel_event);
  } else if (type == blink::WebInputEvent::GestureScrollEnd ||
             type == blink::WebInputEvent::GestureFlingStart) {
    no_touch_to_renderer_ = false;
  }
}

void TouchEventQueue::FlushQueue() {
  DCHECK(!dispatching_touch_ack_);
  while (!touch_queue_.empty())
    PopTouchEventToClient(INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
                          ui::LatencyInfo());
}

size_t TouchEventQueue::GetQueueSize() const {
  return touch_queue_.size();
}

const TouchEventWithLatencyInfo& TouchEventQueue::GetLatestEvent() const {
  return touch_queue_.back()->coalesced_event();
}

void TouchEventQueue::PopTouchEventToClient(
    InputEventAckState ack_result,
    const ui::LatencyInfo& renderer_latency_info) {
  if (touch_queue_.empty())
    return;
  scoped_ptr<CoalescedWebTouchEvent> acked_event(touch_queue_.front());
  touch_queue_.pop_front();

  if (acked_event->ignore_ack())
    return;

  // Note that acking the touch-event may result in multiple gestures being sent
  // to the renderer, or touch-events being queued.
  base::AutoReset<CoalescedWebTouchEvent*>
      dispatching_touch_ack(&dispatching_touch_ack_, acked_event.get());

  for (WebTouchEventWithLatencyList::iterator iter = acked_event->begin(),
       end = acked_event->end();
       iter != end; ++iter) {
    iter->latency.AddNewLatencyFrom(renderer_latency_info);
    client_->OnTouchEventAck((*iter), ack_result);
  }
}

bool TouchEventQueue::ShouldForwardToRenderer(
    const blink::WebTouchEvent& event) const {
  if (no_touch_to_renderer_ &&
      event.type != blink::WebInputEvent::TouchCancel)
    return false;

  // Touch press events should always be forwarded to the renderer.
  if (event.type == blink::WebInputEvent::TouchStart)
    return true;

  for (unsigned int i = 0; i < event.touchesLength; ++i) {
    const blink::WebTouchPoint& point = event.touches[i];
    // If a point has been stationary, then don't take it into account.
    if (point.state == blink::WebTouchPoint::StateStationary)
      continue;

    if (touch_ack_states_.count(point.id) > 0) {
      if (touch_ack_states_.find(point.id)->second !=
          INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS)
        return true;
    } else {
      // If the ACK status of a point is unknown, then the event should be
      // forwarded to the renderer.
      return true;
    }
  }

  return false;
}

}  // namespace content
