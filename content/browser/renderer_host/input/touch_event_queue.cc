// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_event_queue.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/input/web_touch_event_traits.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/geometry/point_f.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::LatencyInfo;

namespace content {
namespace {

// Using a small epsilon when comparing slop distances allows pixel perfect
// slop determination when using fractional DIP coordinates (assuming the slop
// region and DPI scale are reasonably proportioned).
const float kSlopEpsilon = .05f;

typedef std::vector<TouchEventWithLatencyInfo> WebTouchEventWithLatencyList;

TouchEventWithLatencyInfo ObtainCancelEventForTouchEvent(
    const TouchEventWithLatencyInfo& event_to_cancel) {
  TouchEventWithLatencyInfo event = event_to_cancel;
  event.event.type = WebInputEvent::TouchCancel;
  for (size_t i = 0; i < event.event.touchesLength; i++)
    event.event.touches[i].state = WebTouchPoint::StateCancelled;
  return event;
}

bool ShouldTouchTypeTriggerTimeout(WebInputEvent::Type type) {
  return type == WebInputEvent::TouchStart ||
         type == WebInputEvent::TouchMove;
}

}  // namespace


// Cancels a touch sequence if a touchstart or touchmove ack response is
// sufficiently delayed.
class TouchEventQueue::TouchTimeoutHandler {
 public:
  TouchTimeoutHandler(TouchEventQueue* touch_queue,
                      base::TimeDelta timeout_delay)
      : touch_queue_(touch_queue),
        timeout_delay_(timeout_delay),
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

  bool FilterEvent(const WebTouchEvent& event) {
    return HasTimeoutEvent();
  }

  bool IsTimeoutTimerRunning() const {
    return timeout_monitor_.IsRunning();
  }

  void Reset() {
    pending_ack_state_ = PENDING_ACK_NONE;
    timeout_monitor_.Stop();
  }

  void set_timeout_delay(base::TimeDelta timeout_delay) {
    timeout_delay_ = timeout_delay;
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
    return !WebTouchEventTraits::IsTouchSequenceStart(timeout_event_.event);
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

  bool HasTimeoutEvent() const {
    return pending_ack_state_ != PENDING_ACK_NONE;
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

// Provides touchmove slop suppression for a single touch that remains within
// a given slop region, unless the touchstart is preventDefault'ed.
// TODO(jdduke): Use a flag bundled with each TouchEvent declaring whether it
// has exceeded the slop region, removing duplicated slop determination logic.
class TouchEventQueue::TouchMoveSlopSuppressor {
 public:
  TouchMoveSlopSuppressor(double slop_suppression_length_dips)
      : slop_suppression_length_dips_squared_(slop_suppression_length_dips *
                                              slop_suppression_length_dips),
        suppressing_touch_moves_(false) {}

  bool FilterEvent(const WebTouchEvent& event) {
    if (WebTouchEventTraits::IsTouchSequenceStart(event)) {
      touch_sequence_start_position_ =
          gfx::PointF(event.touches[0].position);
      suppressing_touch_moves_ = slop_suppression_length_dips_squared_ != 0;
    }

    if (event.type != WebInputEvent::TouchMove)
      return false;

    if (suppressing_touch_moves_) {
      // Movement with a secondary pointer should terminate suppression.
      if (event.touchesLength > 1) {
        suppressing_touch_moves_ = false;
      } else if (event.touchesLength == 1) {
        // Movement outside of the slop region should terminate suppression.
        gfx::PointF position(event.touches[0].position);
        if ((position - touch_sequence_start_position_).LengthSquared() >
            slop_suppression_length_dips_squared_)
          suppressing_touch_moves_ = false;
      }
    }
    return suppressing_touch_moves_;
  }

  void ConfirmTouchEvent(InputEventAckState ack_result) {
    if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
      suppressing_touch_moves_ = false;
  }

 private:
  double slop_suppression_length_dips_squared_;
  gfx::PointF touch_sequence_start_position_;
  bool suppressing_touch_moves_;

  DISALLOW_COPY_AND_ASSIGN(TouchMoveSlopSuppressor);
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

TouchEventQueue::TouchEventQueue(TouchEventQueueClient* client,
                                 TouchScrollingMode mode,
                                 double touchmove_suppression_length_dips)
    : client_(client),
      dispatching_touch_ack_(NULL),
      dispatching_touch_(false),
      touch_filtering_state_(TOUCH_FILTERING_STATE_DEFAULT),
      ack_timeout_enabled_(false),
      touchmove_slop_suppressor_(new TouchMoveSlopSuppressor(
          touchmove_suppression_length_dips + kSlopEpsilon)),
      absorbing_touch_moves_(false),
      touch_scrolling_mode_(mode) {
  DCHECK(client);
}

TouchEventQueue::~TouchEventQueue() {
  if (!touch_queue_.empty())
    STLDeleteElements(&touch_queue_);
}

void TouchEventQueue::QueueEvent(const TouchEventWithLatencyInfo& event) {
  TRACE_EVENT0("input", "TouchEventQueue::QueueEvent");

  // If the queueing of |event| was triggered by an ack dispatch, defer
  // processing the event until the dispatch has finished.
  if (touch_queue_.empty() && !dispatching_touch_ack_) {
    // Optimization of the case without touch handlers.  Removing this path
    // yields identical results, but this avoids unnecessary allocations.
    if (touch_filtering_state_ == DROP_ALL_TOUCHES ||
        (touch_filtering_state_ == DROP_TOUCHES_IN_SEQUENCE &&
         !WebTouchEventTraits::IsTouchSequenceStart(event.event))) {
      client_->OnTouchEventAck(event, INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
      return;
    }

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
                                      const LatencyInfo& latency_info) {
  TRACE_EVENT0("input", "TouchEventQueue::ProcessTouchAck");

  DCHECK(!dispatching_touch_ack_);
  dispatching_touch_ = false;

  if (timeout_handler_ && timeout_handler_->ConfirmTouchEvent(ack_result))
    return;

  touchmove_slop_suppressor_->ConfirmTouchEvent(ack_result);

  if (touch_queue_.empty())
    return;

  const WebTouchEvent& acked_event =
      touch_queue_.front()->coalesced_event().event;

  if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED &&
      touch_filtering_state_ == FORWARD_TOUCHES_UNTIL_TIMEOUT) {
    touch_filtering_state_ = FORWARD_ALL_TOUCHES;
  }

  if (ack_result == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS &&
      touch_filtering_state_ != DROP_ALL_TOUCHES &&
      WebTouchEventTraits::IsTouchSequenceStart(acked_event)) {
    touch_filtering_state_ = DROP_TOUCHES_IN_SEQUENCE;
  }

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
    PreFilterResult result = FilterBeforeForwarding(touch.event);
    switch (result) {
      case ACK_WITH_NO_CONSUMER_EXISTS:
        PopTouchEventToClient(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
                              LatencyInfo());
        break;
      case ACK_WITH_NOT_CONSUMED:
        PopTouchEventToClient(INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
                              LatencyInfo());
        break;
      case FORWARD_TO_RENDERER:
        ForwardToRenderer(touch);
        return;
    }
  }
}

void TouchEventQueue::ForwardToRenderer(
    const TouchEventWithLatencyInfo& touch) {
  TRACE_EVENT0("input", "TouchEventQueue::ForwardToRenderer");

  DCHECK(!dispatching_touch_);
  DCHECK_NE(touch_filtering_state_, DROP_ALL_TOUCHES);

  if (WebTouchEventTraits::IsTouchSequenceStart(touch.event)) {
    touch_filtering_state_ =
        ack_timeout_enabled_ ? FORWARD_TOUCHES_UNTIL_TIMEOUT
                             : FORWARD_ALL_TOUCHES;
    touch_ack_states_.clear();
    absorbing_touch_moves_ = false;
  }

  // A synchronous ack will reset |dispatching_touch_|, in which case
  // the touch timeout should not be started.
  base::AutoReset<bool> dispatching_touch(&dispatching_touch_, true);
  client_->SendTouchEventImmediately(touch);
  if (dispatching_touch_ &&
      touch_filtering_state_ == FORWARD_TOUCHES_UNTIL_TIMEOUT &&
      ShouldTouchTypeTriggerTimeout(touch.event.type)) {
    DCHECK(timeout_handler_);
    timeout_handler_->Start(touch);
  }
}

void TouchEventQueue::OnGestureScrollEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (gesture_event.event.type != blink::WebInputEvent::GestureScrollBegin)
    return;

  if (touch_scrolling_mode_ == TOUCH_SCROLLING_MODE_ABSORB_TOUCHMOVE)
    absorbing_touch_moves_ = true;

  if (touch_scrolling_mode_ != TOUCH_SCROLLING_MODE_TOUCHCANCEL)
    return;

  // We assume that scroll events are generated synchronously from
  // dispatching a touch event ack. This allows us to generate a synthetic
  // cancel event that has the same touch ids as the touch event that
  // is being acked. Otherwise, we don't perform the touch-cancel optimization.
  if (!dispatching_touch_ack_)
    return;

  if (touch_filtering_state_ == DROP_TOUCHES_IN_SEQUENCE)
    return;

  touch_filtering_state_ = DROP_TOUCHES_IN_SEQUENCE;

  // Fake a TouchCancel to cancel the touch points of the touch event
  // that is currently being acked.
  // Note: |dispatching_touch_ack_| is non-null when we reach here, meaning we
  // are in the scope of PopTouchEventToClient() and that no touch event
  // in the queue is waiting for ack from renderer. So we can just insert
  // the touch cancel at the beginning of the queue.
  touch_queue_.push_front(new CoalescedWebTouchEvent(
      ObtainCancelEventForTouchEvent(
          dispatching_touch_ack_->coalesced_event()), true));
}

void TouchEventQueue::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  if (touch_scrolling_mode_ != TOUCH_SCROLLING_MODE_ABSORB_TOUCHMOVE)
    return;

  if (event.event.type != blink::WebInputEvent::GestureScrollUpdate)
    return;

  // Suspend sending touchmove events as long as the scroll events are handled.
  // Note that there's no guarantee that this ACK is for the most recent
  // gesture event (or even part of the current sequence).  Worst case, the
  // delay in updating the absorption state should only result in minor UI
  // glitches.
  absorbing_touch_moves_ = (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED);
}

void TouchEventQueue::OnHasTouchEventHandlers(bool has_handlers) {
  DCHECK(!dispatching_touch_ack_);
  DCHECK(!dispatching_touch_);

  if (has_handlers) {
    if (touch_filtering_state_ == DROP_ALL_TOUCHES) {
      // If no touch handler was previously registered, ensure that we don't
      // send a partial touch sequence to the renderer.
      DCHECK(touch_queue_.empty());
      touch_filtering_state_ = DROP_TOUCHES_IN_SEQUENCE;
    }
  } else {
    // TODO(jdduke): Synthesize a TouchCancel if necessary to update Blink touch
    // state tracking (e.g., if the touch handler was removed mid-sequence).
    touch_filtering_state_ = DROP_ALL_TOUCHES;
    if (timeout_handler_)
      timeout_handler_->Reset();
    if (!touch_queue_.empty())
      ProcessTouchAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, LatencyInfo());
    // As there is no touch handler, ack'ing the event should flush the queue.
    DCHECK(touch_queue_.empty());
  }
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
                                           base::TimeDelta ack_timeout_delay) {
  if (!enabled) {
    ack_timeout_enabled_ = false;
    if (touch_filtering_state_ == FORWARD_TOUCHES_UNTIL_TIMEOUT)
      touch_filtering_state_ = FORWARD_ALL_TOUCHES;
    // Only reset the |timeout_handler_| if the timer is running and has not yet
    // timed out. This ensures that an already timed out sequence is properly
    // flushed by the handler.
    if (timeout_handler_ && timeout_handler_->IsTimeoutTimerRunning())
      timeout_handler_->Reset();
    return;
  }

  ack_timeout_enabled_ = true;
  if (!timeout_handler_)
    timeout_handler_.reset(new TouchTimeoutHandler(this, ack_timeout_delay));
  else
    timeout_handler_->set_timeout_delay(ack_timeout_delay);
}

bool TouchEventQueue::IsTimeoutRunningForTesting() const {
  return timeout_handler_ && timeout_handler_->IsTimeoutTimerRunning();
}

const TouchEventWithLatencyInfo&
TouchEventQueue::GetLatestEventForTesting() const {
  return touch_queue_.back()->coalesced_event();
}

void TouchEventQueue::FlushQueue() {
  DCHECK(!dispatching_touch_ack_);
  DCHECK(!dispatching_touch_);
  if (touch_filtering_state_ != DROP_ALL_TOUCHES)
    touch_filtering_state_ = DROP_TOUCHES_IN_SEQUENCE;
  while (!touch_queue_.empty()) {
    PopTouchEventToClient(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
                          LatencyInfo());
  }
}

void TouchEventQueue::PopTouchEventToClient(
    InputEventAckState ack_result,
    const LatencyInfo& renderer_latency_info) {
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

TouchEventQueue::PreFilterResult
TouchEventQueue::FilterBeforeForwarding(const WebTouchEvent& event) {
  if (timeout_handler_ && timeout_handler_->FilterEvent(event))
    return ACK_WITH_NO_CONSUMER_EXISTS;

  if (touchmove_slop_suppressor_->FilterEvent(event))
    return ACK_WITH_NOT_CONSUMED;

  if (touch_filtering_state_ == DROP_ALL_TOUCHES)
    return ACK_WITH_NO_CONSUMER_EXISTS;

  if (touch_filtering_state_ == DROP_TOUCHES_IN_SEQUENCE &&
      event.type != WebInputEvent::TouchCancel) {
    if (WebTouchEventTraits::IsTouchSequenceStart(event))
      return FORWARD_TO_RENDERER;
    return ACK_WITH_NOT_CONSUMED;
  }

  if (absorbing_touch_moves_ && event.type == WebInputEvent::TouchMove)
    return ACK_WITH_NOT_CONSUMED;

  // Touch press events should always be forwarded to the renderer.
  if (event.type == WebInputEvent::TouchStart)
    return FORWARD_TO_RENDERER;

  for (unsigned int i = 0; i < event.touchesLength; ++i) {
    const WebTouchPoint& point = event.touches[i];
    // If a point has been stationary, then don't take it into account.
    if (point.state == WebTouchPoint::StateStationary)
      continue;

    if (touch_ack_states_.count(point.id) > 0) {
      if (touch_ack_states_.find(point.id)->second !=
          INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS)
        return FORWARD_TO_RENDERER;
    } else {
      // If the ACK status of a point is unknown, then the event should be
      // forwarded to the renderer.
      return FORWARD_TO_RENDERER;
    }
  }

  return ACK_WITH_NO_CONSUMER_EXISTS;
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
