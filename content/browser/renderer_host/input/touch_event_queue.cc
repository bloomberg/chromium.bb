// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_event_queue.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/public/common/content_switches.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {
namespace {

const InputEventAckState kDefaultNotForwardedAck =
    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;

typedef std::vector<TouchEventWithLatencyInfo> WebTouchEventWithLatencyList;

TouchEventWithLatencyInfo ObtainCancelEventForTouchEvent(
    const TouchEventWithLatencyInfo& event_to_cancel) {
  TouchEventWithLatencyInfo event = event_to_cancel;
  event.event.type = WebInputEvent::TouchCancel;
  for (size_t i = 0; i < event.event.touchesLength; i++)
    event.event.touches[i].state = WebTouchPoint::StateCancelled;
  return event;
}

bool IsNewTouchGesture(const WebTouchEvent& event) {
  if (event.type != WebInputEvent::TouchStart)
    return false;
  if (!event.touchesLength)
    return false;
  for (size_t i = 0; i < event.touchesLength; i++) {
    if (event.touches[i].state != WebTouchPoint::StatePressed)
      return false;
  }
  return true;
}

bool ShouldTouchTypeTriggerTimeout(WebInputEvent::Type type) {
  return type == WebInputEvent::TouchStart ||
         type == WebInputEvent::TouchMove;
}

}  // namespace

class TouchEventQueue::TouchTimeoutHandler {
 public:
  TouchTimeoutHandler(TouchEventQueue* touch_queue, size_t timeout_delay_ms)
      : touch_queue_(touch_queue),
        timeout_delay_(base::TimeDelta::FromMilliseconds(timeout_delay_ms)),
        pending_ack_state_(PENDING_ACK_NONE),
        timeout_monitor_(base::Bind(&TouchTimeoutHandler::OnTimeOut,
                                    base::Unretained(this))) {}

  ~TouchTimeoutHandler() {}

  void Start(const TouchEventWithLatencyInfo& event) {
    DCHECK_EQ(pending_ack_state_, PENDING_ACK_NONE);
    DCHECK(ShouldTouchTypeTriggerTimeout(event.event.type));
    timeout_event_ = event;
    timeout_monitor_.Restart(timeout_delay_);
  }

  bool ConfirmTouchEvent(InputEventAckState ack_result) {
    switch (pending_ack_state_) {
      case PENDING_ACK_NONE:
        timeout_monitor_.Stop();
        return false;
      case PENDING_ACK_ORIGINAL_EVENT:
        if (AckedTimeoutEventRequiresCancel(ack_result)) {
          SetPendingAckState(PENDING_ACK_CANCEL_EVENT);
          TouchEventWithLatencyInfo cancel_event =
              ObtainCancelEventForTouchEvent(timeout_event_);
          touch_queue_->UpdateTouchAckStates(
              cancel_event.event, kDefaultNotForwardedAck);
          touch_queue_->client_->SendTouchEventImmediately(cancel_event);
        } else {
          SetPendingAckState(PENDING_ACK_NONE);
          touch_queue_->UpdateTouchAckStates(timeout_event_.event, ack_result);
        }
        return true;
      case PENDING_ACK_CANCEL_EVENT:
        SetPendingAckState(PENDING_ACK_NONE);
        return true;
    }
    return false;
  }

  bool HasTimeoutEvent() const {
    return pending_ack_state_ != PENDING_ACK_NONE;
  }

  bool IsTimeoutTimerRunning() const {
    return timeout_monitor_.IsRunning();
  }

 private:
  enum PendingAckState {
    PENDING_ACK_NONE,
    PENDING_ACK_ORIGINAL_EVENT,
    PENDING_ACK_CANCEL_EVENT,
  };

  void OnTimeOut() {
    SetPendingAckState(PENDING_ACK_ORIGINAL_EVENT);
    touch_queue_->FlushQueue();
  }

  // Skip a cancel event if the timed-out event had no consumer and was the
  // initial event in the gesture.
  bool AckedTimeoutEventRequiresCancel(InputEventAckState ack_result) const {
    DCHECK(HasTimeoutEvent());
    if (ack_result != INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS)
      return true;
    return !IsNewTouchGesture(timeout_event_.event);
  }

  void SetPendingAckState(PendingAckState new_pending_ack_state) {
    DCHECK_NE(pending_ack_state_, new_pending_ack_state);
    switch (new_pending_ack_state) {
      case PENDING_ACK_ORIGINAL_EVENT:
        DCHECK_EQ(pending_ack_state_, PENDING_ACK_NONE);
        TRACE_EVENT_ASYNC_BEGIN0("input", "TouchEventTimeout", this);
        break;
      case PENDING_ACK_CANCEL_EVENT:
        DCHECK_EQ(pending_ack_state_, PENDING_ACK_ORIGINAL_EVENT);
        DCHECK(!timeout_monitor_.IsRunning());
        DCHECK(touch_queue_->empty());
        TRACE_EVENT_ASYNC_STEP_INTO0(
            "input", "TouchEventTimeout", this, "CancelEvent");
        break;
      case PENDING_ACK_NONE:
        DCHECK(!timeout_monitor_.IsRunning());
        DCHECK(touch_queue_->empty());
        TRACE_EVENT_ASYNC_END0("input", "TouchEventTimeout", this);
        break;
    }
    pending_ack_state_ = new_pending_ack_state;
  }


  TouchEventQueue* touch_queue_;

  // How long to wait on a touch ack before cancelling the touch sequence.
  base::TimeDelta timeout_delay_;

  // The touch event source for which we expect the next ack.
  PendingAckState pending_ack_state_;

  // The event for which the ack timeout is triggered.
  TouchEventWithLatencyInfo timeout_event_;

  // Provides timeout-based callback behavior.
  TimeoutMonitor timeout_monitor_;
};


// This class represents a single coalesced touch event. However, it also keeps
// track of all the original touch-events that were coalesced into a single
// event. The coalesced event is forwarded to the renderer, while the original
// touch-events are sent to the Client (on ACK for the coalesced event) so that
// the Client receives the event with their original timestamp.
class CoalescedWebTouchEvent {
 public:
  CoalescedWebTouchEvent(const TouchEventWithLatencyInfo& event,
                         bool ignore_ack)
      : coalesced_event_(event),
        ignore_ack_(ignore_ack) {
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
      dispatching_touch_(false),
      no_touch_to_renderer_(false),
      renderer_is_consuming_touch_gesture_(false),
      ack_timeout_enabled_(false) {
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
    touch_queue_.push_back(new CoalescedWebTouchEvent(event, false));
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
  touch_queue_.push_back(new CoalescedWebTouchEvent(event, false));
}

void TouchEventQueue::ProcessTouchAck(InputEventAckState ack_result,
                                      const ui::LatencyInfo& latency_info) {
  DCHECK(!dispatching_touch_ack_);
  dispatching_touch_ = false;

  if (timeout_handler_ && timeout_handler_->ConfirmTouchEvent(ack_result))
    return;

  if (touch_queue_.empty())
    return;

  if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
    renderer_is_consuming_touch_gesture_ = true;

  const WebTouchEvent& acked_event =
      touch_queue_.front()->coalesced_event().event;
  UpdateTouchAckStates(acked_event, ack_result);
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
    if (IsNewTouchGesture(touch.event))
      renderer_is_consuming_touch_gesture_ = false;
    if (ShouldForwardToRenderer(touch.event)) {
      ForwardToRenderer(touch);
      break;
    }
    PopTouchEventToClient(kDefaultNotForwardedAck, ui::LatencyInfo());
  }
}

void TouchEventQueue::ForwardToRenderer(
    const TouchEventWithLatencyInfo& touch) {
  DCHECK(!dispatching_touch_);
  // A synchronous ack will reset |dispatching_touch_|, in which case
  // the touch timeout should not be started.
  base::AutoReset<bool> dispatching_touch(&dispatching_touch_, true);
  client_->SendTouchEventImmediately(touch);
  if (ack_timeout_enabled_ &&
      dispatching_touch_ &&
      !renderer_is_consuming_touch_gesture_ &&
      ShouldTouchTypeTriggerTimeout(touch.event.type)) {
    DCHECK(timeout_handler_);
    timeout_handler_->Start(touch);
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

    // If we have a timeout event, a cancel has already been dispatched
    // for the current touch stream.
    if (HasTimeoutEvent())
      return;

    // Fake a TouchCancel to cancel the touch points of the touch event
    // that is currently being acked.
    // Note: |dispatching_touch_ack_| is non-null when we reach here, meaning we
    // are in the scope of PopTouchEventToClient() and that no touch event
    // in the queue is waiting for ack from renderer. So we can just insert
    // the touch cancel at the beginning of the queue.
    touch_queue_.push_front(new CoalescedWebTouchEvent(
        ObtainCancelEventForTouchEvent(
            dispatching_touch_ack_->coalesced_event()), true));
  } else if (type == blink::WebInputEvent::GestureScrollEnd ||
             type == blink::WebInputEvent::GestureFlingStart) {
    no_touch_to_renderer_ = false;
  }
}

void TouchEventQueue::FlushQueue() {
  DCHECK(!dispatching_touch_ack_);
  DCHECK(!dispatching_touch_);
  while (!touch_queue_.empty())
    PopTouchEventToClient(kDefaultNotForwardedAck, ui::LatencyInfo());
}

bool TouchEventQueue::IsPendingAckTouchStart() const {
  DCHECK(!dispatching_touch_ack_);
  if (touch_queue_.empty())
    return false;

  const blink::WebTouchEvent& event =
      touch_queue_.front()->coalesced_event().event;
  return (event.type == WebInputEvent::TouchStart);
}

void TouchEventQueue::SetAckTimeoutEnabled(bool enabled,
                                           size_t ack_timeout_delay_ms) {
  if (!enabled) {
    // Avoid resetting |timeout_handler_|, as an outstanding timeout may
    // be active and must be completed for ack handling consistency.
    ack_timeout_enabled_ = false;
    return;
  }

  ack_timeout_enabled_ = true;
  if (!timeout_handler_)
    timeout_handler_.reset(new TouchTimeoutHandler(this, ack_timeout_delay_ms));
}

bool TouchEventQueue::HasTimeoutEvent() const {
  return timeout_handler_ && timeout_handler_->HasTimeoutEvent();
}

bool TouchEventQueue::IsTimeoutRunningForTesting() const {
  return timeout_handler_ && timeout_handler_->IsTimeoutTimerRunning();
}

const TouchEventWithLatencyInfo&
TouchEventQueue::GetLatestEventForTesting() const {
  return touch_queue_.back()->coalesced_event();
}

void TouchEventQueue::PopTouchEventToClient(
    InputEventAckState ack_result,
    const ui::LatencyInfo& renderer_latency_info) {
  DCHECK(!dispatching_touch_ack_);
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
    const WebTouchEvent& event) const {
  if (HasTimeoutEvent())
    return false;

  if (no_touch_to_renderer_ &&
      event.type != blink::WebInputEvent::TouchCancel)
    return false;

  // Touch press events should always be forwarded to the renderer.
  if (event.type == WebInputEvent::TouchStart)
    return true;

  for (unsigned int i = 0; i < event.touchesLength; ++i) {
    const WebTouchPoint& point = event.touches[i];
    // If a point has been stationary, then don't take it into account.
    if (point.state == WebTouchPoint::StateStationary)
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

void TouchEventQueue::UpdateTouchAckStates(const WebTouchEvent& event,
                                           InputEventAckState ack_result) {
  // Update the ACK status for each touch point in the ACKed event.
  if (event.type == WebInputEvent::TouchEnd ||
      event.type == WebInputEvent::TouchCancel) {
    // The points have been released. Erase the ACK states.
    for (unsigned i = 0; i < event.touchesLength; ++i) {
      const WebTouchPoint& point = event.touches[i];
      if (point.state == WebTouchPoint::StateReleased ||
          point.state == WebTouchPoint::StateCancelled)
        touch_ack_states_.erase(point.id);
    }
  } else if (event.type == WebInputEvent::TouchStart) {
    for (unsigned i = 0; i < event.touchesLength; ++i) {
      const WebTouchPoint& point = event.touches[i];
      if (point.state == WebTouchPoint::StatePressed)
        touch_ack_states_[point.id] = ack_result;
    }
  }
}

}  // namespace content
