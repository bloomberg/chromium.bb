// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_event_queue.h"

#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/common/input/web_touch_event_traits.h"
#include "ui/gfx/geometry/point_f.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::LatencyInfo;

namespace content {
namespace {

// Time interval at which touchmove events will be forwarded to the client while
// scrolling is active and possible.
const double kAsyncTouchMoveIntervalSec = .2;

// A slop region just larger than that used by many web applications. When
// touchmove's are being sent asynchronously, movement outside this region will
// trigger an immediate async touchmove to cancel potential tap-related logic.
const double kApplicationSlopRegionLengthDipsSqared = 15. * 15.;

// Using a small epsilon when comparing slop distances allows pixel perfect
// slop determination when using fractional DIP coordinates (assuming the slop
// region and DPI scale are reasonably proportioned).
const float kSlopEpsilon = .05f;

TouchEventWithLatencyInfo ObtainCancelEventForTouchEvent(
    const TouchEventWithLatencyInfo& event_to_cancel) {
  TouchEventWithLatencyInfo event = event_to_cancel;
  WebTouchEventTraits::ResetTypeAndTouchStates(
      WebInputEvent::TouchCancel,
      // TODO(rbyers): Shouldn't we use a fresh timestamp?
      event.event.timeStampSeconds,
      &event.event);
  return event;
}

bool ShouldTouchTriggerTimeout(const WebTouchEvent& event) {
  return (event.type == WebInputEvent::TouchStart ||
          event.type == WebInputEvent::TouchMove) &&
         !WebInputEventTraits::IgnoresAckDisposition(event);
}

bool OutsideApplicationSlopRegion(const WebTouchEvent& event,
                                  const gfx::PointF& anchor) {
  return (gfx::PointF(event.touches[0].position) - anchor).LengthSquared() >
         kApplicationSlopRegionLengthDipsSqared;
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
                                    base::Unretained(this))),
        enabled_(true),
        enabled_for_current_sequence_(false) {
    DCHECK(timeout_delay != base::TimeDelta());
  }

  ~TouchTimeoutHandler() {}

  void StartIfNecessary(const TouchEventWithLatencyInfo& event) {
    DCHECK_EQ(pending_ack_state_, PENDING_ACK_NONE);
    if (!enabled_)
      return;

    if (!ShouldTouchTriggerTimeout(event.event))
      return;

    if (WebTouchEventTraits::IsTouchSequenceStart(event.event))
      enabled_for_current_sequence_ = true;

    if (!enabled_for_current_sequence_)
      return;

    timeout_event_ = event;
    timeout_monitor_.Restart(timeout_delay_);
  }

  bool ConfirmTouchEvent(InputEventAckState ack_result) {
    switch (pending_ack_state_) {
      case PENDING_ACK_NONE:
        if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
          enabled_for_current_sequence_ = false;
        timeout_monitor_.Stop();
        return false;
      case PENDING_ACK_ORIGINAL_EVENT:
        if (AckedTimeoutEventRequiresCancel(ack_result)) {
          SetPendingAckState(PENDING_ACK_CANCEL_EVENT);
          TouchEventWithLatencyInfo cancel_event =
              ObtainCancelEventForTouchEvent(timeout_event_);
          touch_queue_->SendTouchEventImmediately(cancel_event);
        } else {
          SetPendingAckState(PENDING_ACK_NONE);
          touch_queue_->UpdateTouchConsumerStates(timeout_event_.event,
                                                  ack_result);
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

  void SetEnabled(bool enabled) {
    if (enabled_ == enabled)
      return;

    enabled_ = enabled;

    if (enabled_)
      return;

    enabled_for_current_sequence_ = false;
    // Only reset the |timeout_handler_| if the timer is running and has not
    // yet timed out. This ensures that an already timed out sequence is
    // properly flushed by the handler.
    if (IsTimeoutTimerRunning()) {
      pending_ack_state_ = PENDING_ACK_NONE;
      timeout_monitor_.Stop();
    }
  }

  bool IsTimeoutTimerRunning() const { return timeout_monitor_.IsRunning(); }

  bool enabled() const { return enabled_; }

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

  bool enabled_;
  bool enabled_for_current_sequence_;
};

// Provides touchmove slop suppression for a single touch that remains within
// a given slop region, unless the touchstart is preventDefault'ed.
// TODO(jdduke): Use a flag bundled with each TouchEvent declaring whether it
// has exceeded the slop region, removing duplicated slop determination logic.
class TouchEventQueue::TouchMoveSlopSuppressor {
 public:
  TouchMoveSlopSuppressor(double slop_suppression_length_dips)
      : slop_suppression_length_dips_squared_(0),
        suppressing_touchmoves_(false) {
    if (slop_suppression_length_dips) {
      slop_suppression_length_dips += kSlopEpsilon;
      slop_suppression_length_dips_squared_ =
          slop_suppression_length_dips * slop_suppression_length_dips;
    }
  }

  bool FilterEvent(const WebTouchEvent& event) {
    if (WebTouchEventTraits::IsTouchSequenceStart(event)) {
      touch_sequence_start_position_ =
          gfx::PointF(event.touches[0].position);
      suppressing_touchmoves_ = slop_suppression_length_dips_squared_ != 0;
    }

    if (event.type == WebInputEvent::TouchEnd ||
        event.type == WebInputEvent::TouchCancel)
      suppressing_touchmoves_ = false;

    if (event.type != WebInputEvent::TouchMove)
      return false;

    if (suppressing_touchmoves_) {
      // Movement with a secondary pointer should terminate suppression.
      if (event.touchesLength > 1) {
        suppressing_touchmoves_ = false;
      } else if (event.touchesLength == 1) {
        // Movement outside of the slop region should terminate suppression.
        gfx::PointF position(event.touches[0].position);
        if ((position - touch_sequence_start_position_).LengthSquared() >
            slop_suppression_length_dips_squared_)
          suppressing_touchmoves_ = false;
      }
    }
    return suppressing_touchmoves_;
  }

  void ConfirmTouchEvent(InputEventAckState ack_result) {
    if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
      suppressing_touchmoves_ = false;
  }

  bool suppressing_touchmoves() const { return suppressing_touchmoves_; }

 private:
  double slop_suppression_length_dips_squared_;
  gfx::PointF touch_sequence_start_position_;
  bool suppressing_touchmoves_;

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
                         bool suppress_client_ack)
      : coalesced_event_(event), suppress_client_ack_(suppress_client_ack) {
    TRACE_EVENT_ASYNC_BEGIN0("input", "TouchEventQueue::QueueEvent", this);
  }

  ~CoalescedWebTouchEvent() {
    TRACE_EVENT_ASYNC_END0("input", "TouchEventQueue::QueueEvent", this);
  }

  // Coalesces the event with the existing event if possible. Returns whether
  // the event was coalesced.
  bool CoalesceEventIfPossible(
      const TouchEventWithLatencyInfo& event_with_latency) {
    if (suppress_client_ack_)
      return false;

    if (!coalesced_event_.CanCoalesceWith(event_with_latency))
      return false;

    // Addition of the first event to |uncoaleseced_events_to_ack_| is deferred
    // until the first coalesced event, optimizing the (common) case where the
    // event is not coalesced at all.
    if (uncoaleseced_events_to_ack_.empty())
      uncoaleseced_events_to_ack_.push_back(coalesced_event_);

    TRACE_EVENT_INSTANT0(
        "input", "TouchEventQueue::MoveCoalesced", TRACE_EVENT_SCOPE_THREAD);
    coalesced_event_.CoalesceWith(event_with_latency);
    uncoaleseced_events_to_ack_.push_back(event_with_latency);
    DCHECK_GE(uncoaleseced_events_to_ack_.size(), 2U);
    return true;
  }

  void DispatchAckToClient(InputEventAckState ack_result,
                           const ui::LatencyInfo* optional_latency_info,
                           TouchEventQueueClient* client) {
    DCHECK(client);
    if (suppress_client_ack_)
      return;

    if (uncoaleseced_events_to_ack_.empty()) {
      if (optional_latency_info)
        coalesced_event_.latency.AddNewLatencyFrom(*optional_latency_info);
      client->OnTouchEventAck(coalesced_event_, ack_result);
      return;
    }

    DCHECK_GE(uncoaleseced_events_to_ack_.size(), 2U);
    for (WebTouchEventWithLatencyList::iterator
             iter = uncoaleseced_events_to_ack_.begin(),
             end = uncoaleseced_events_to_ack_.end();
         iter != end;
         ++iter) {
      if (optional_latency_info)
        iter->latency.AddNewLatencyFrom(*optional_latency_info);
      client->OnTouchEventAck(*iter, ack_result);
    }
  }

  const TouchEventWithLatencyInfo& coalesced_event() const {
    return coalesced_event_;
  }

 private:
  // This is the event that is forwarded to the renderer.
  TouchEventWithLatencyInfo coalesced_event_;

  // This is the list of the original events that were coalesced, each requiring
  // future ack dispatch to the client.
  // Note that this will be empty if no coalescing has occurred.
  typedef std::vector<TouchEventWithLatencyInfo> WebTouchEventWithLatencyList;
  WebTouchEventWithLatencyList uncoaleseced_events_to_ack_;

  bool suppress_client_ack_;

  DISALLOW_COPY_AND_ASSIGN(CoalescedWebTouchEvent);
};

TouchEventQueue::Config::Config()
    : touchmove_slop_suppression_length_dips(0),
      touch_scrolling_mode(TOUCH_SCROLLING_MODE_DEFAULT),
      touch_ack_timeout_delay(base::TimeDelta::FromMilliseconds(200)),
      touch_ack_timeout_supported(false) {
}

TouchEventQueue::TouchEventQueue(TouchEventQueueClient* client,
                                 const Config& config)
    : client_(client),
      dispatching_touch_ack_(NULL),
      dispatching_touch_(false),
      has_handlers_(true),
      drop_remaining_touches_in_sequence_(false),
      touchmove_slop_suppressor_(new TouchMoveSlopSuppressor(
          config.touchmove_slop_suppression_length_dips)),
      send_touch_events_async_(false),
      needs_async_touchmove_for_outer_slop_region_(false),
      last_sent_touch_timestamp_sec_(0),
      touch_scrolling_mode_(config.touch_scrolling_mode) {
  DCHECK(client);
  if (config.touch_ack_timeout_supported) {
    timeout_handler_.reset(
        new TouchTimeoutHandler(this, config.touch_ack_timeout_delay));
  }
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
    PreFilterResult filter_result = FilterBeforeForwarding(event.event);
    if (filter_result != FORWARD_TO_RENDERER) {
      client_->OnTouchEventAck(event,
                               filter_result == ACK_WITH_NO_CONSUMER_EXISTS
                                   ? INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS
                                   : INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      return;
    }

    // There is no touch event in the queue. Forward it to the renderer
    // immediately.
    touch_queue_.push_back(new CoalescedWebTouchEvent(event, false));
    ForwardNextEventToRenderer();
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

  PopTouchEventToClient(ack_result, latency_info);
  TryForwardNextEventToRenderer();
}

void TouchEventQueue::TryForwardNextEventToRenderer() {
  DCHECK(!dispatching_touch_ack_);
  // If there are queued touch events, then try to forward them to the renderer
  // immediately, or ACK the events back to the client if appropriate.
  while (!touch_queue_.empty()) {
    PreFilterResult filter_result =
        FilterBeforeForwarding(touch_queue_.front()->coalesced_event().event);
    switch (filter_result) {
      case ACK_WITH_NO_CONSUMER_EXISTS:
        PopTouchEventToClient(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        break;
      case ACK_WITH_NOT_CONSUMED:
        PopTouchEventToClient(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        break;
      case FORWARD_TO_RENDERER:
        ForwardNextEventToRenderer();
        return;
    }
  }
}

void TouchEventQueue::ForwardNextEventToRenderer() {
  TRACE_EVENT0("input", "TouchEventQueue::ForwardNextEventToRenderer");

  DCHECK(!empty());
  DCHECK(!dispatching_touch_);
  TouchEventWithLatencyInfo touch = touch_queue_.front()->coalesced_event();

  if (send_touch_events_async_ &&
      touch.event.type == WebInputEvent::TouchMove) {
    // Throttling touchmove's in a continuous touchmove stream while scrolling
    // reduces the risk of jank. However, it's still important that the web
    // application be sent touches at key points in the gesture stream,
    // e.g., when the application slop region is exceeded or touchmove
    // coalescing fails because of different modifiers.
    const bool send_touchmove_now =
        size() > 1 ||
        (touch.event.timeStampSeconds >=
         last_sent_touch_timestamp_sec_ + kAsyncTouchMoveIntervalSec) ||
        (needs_async_touchmove_for_outer_slop_region_ &&
         OutsideApplicationSlopRegion(touch.event,
                                      touch_sequence_start_position_)) ||
        (pending_async_touchmove_ &&
         !pending_async_touchmove_->CanCoalesceWith(touch));

    if (!send_touchmove_now) {
      if (!pending_async_touchmove_) {
        pending_async_touchmove_.reset(new TouchEventWithLatencyInfo(touch));
      } else {
        DCHECK(pending_async_touchmove_->CanCoalesceWith(touch));
        pending_async_touchmove_->CoalesceWith(touch);
      }
      DCHECK_EQ(1U, size());
      PopTouchEventToClient(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      // It's possible (though unlikely) that ack'ing the current touch will
      // trigger the queueing of another touch event (e.g., a touchcancel). As
      // forwarding of the queued event will be deferred while the ack is being
      // dispatched (see |OnTouchEvent()|), try forwarding it now.
      TryForwardNextEventToRenderer();
      return;
    }
  }

  last_sent_touch_timestamp_sec_ = touch.event.timeStampSeconds;

  // Flush any pending async touch move. If it can be combined with the current
  // (touchmove) event, great, otherwise send it immediately but separately. Its
  // ack will trigger forwarding of the original |touch| event.
  if (pending_async_touchmove_) {
    if (pending_async_touchmove_->CanCoalesceWith(touch)) {
      pending_async_touchmove_->CoalesceWith(touch);
      pending_async_touchmove_->event.cancelable = !send_touch_events_async_;
      touch = *pending_async_touchmove_.Pass();
    } else {
      scoped_ptr<TouchEventWithLatencyInfo> async_move =
          pending_async_touchmove_.Pass();
      async_move->event.cancelable = false;
      touch_queue_.push_front(new CoalescedWebTouchEvent(*async_move, true));
      SendTouchEventImmediately(*async_move);
      return;
    }
  }

  // Note: Marking touchstart events as not-cancelable prevents them from
  // blocking subsequent gestures, but it may not be the best long term solution
  // for tracking touch point dispatch.
  if (send_touch_events_async_)
    touch.event.cancelable = false;

  // A synchronous ack will reset |dispatching_touch_|, in which case
  // the touch timeout should not be started.
  base::AutoReset<bool> dispatching_touch(&dispatching_touch_, true);
  SendTouchEventImmediately(touch);
  if (dispatching_touch_ && timeout_handler_)
    timeout_handler_->StartIfNecessary(touch);
}

void TouchEventQueue::OnGestureScrollEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (gesture_event.event.type == blink::WebInputEvent::GestureScrollBegin) {
    if (!touch_consumer_states_.is_empty() &&
        !drop_remaining_touches_in_sequence_) {
      DCHECK(!touchmove_slop_suppressor_->suppressing_touchmoves())
          <<  "A touch handler should be offered a touchmove before scrolling.";
    }
    if (touch_scrolling_mode_ == TOUCH_SCROLLING_MODE_ASYNC_TOUCHMOVE &&
        !drop_remaining_touches_in_sequence_ &&
        touch_consumer_states_.is_empty()) {
      // If no touch points have a consumer, prevent all subsequent touch events
      // received during the scroll from reaching the renderer. This ensures
      // that the first touchstart the renderer sees in any given sequence can
      // always be preventDefault'ed (cancelable == true).
      // TODO(jdduke): Revisit if touchstarts during scroll are made cancelable.
      drop_remaining_touches_in_sequence_ = true;
    }

    if (touch_scrolling_mode_ == TOUCH_SCROLLING_MODE_ASYNC_TOUCHMOVE) {
      needs_async_touchmove_for_outer_slop_region_ = true;
      pending_async_touchmove_.reset();
    }

    return;
  }

  if (gesture_event.event.type != blink::WebInputEvent::GestureScrollUpdate)
    return;

  if (touch_scrolling_mode_ == TOUCH_SCROLLING_MODE_ASYNC_TOUCHMOVE)
    send_touch_events_async_ = true;

  if (touch_scrolling_mode_ != TOUCH_SCROLLING_MODE_TOUCHCANCEL)
    return;

  // We assume that scroll events are generated synchronously from
  // dispatching a touch event ack. This allows us to generate a synthetic
  // cancel event that has the same touch ids as the touch event that
  // is being acked. Otherwise, we don't perform the touch-cancel optimization.
  if (!dispatching_touch_ack_)
    return;

  if (drop_remaining_touches_in_sequence_)
    return;

  drop_remaining_touches_in_sequence_ = true;

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
  if (touch_scrolling_mode_ != TOUCH_SCROLLING_MODE_ASYNC_TOUCHMOVE)
    return;

  if (event.event.type != blink::WebInputEvent::GestureScrollUpdate)
    return;

  // Throttle sending touchmove events as long as the scroll events are handled.
  // Note that there's no guarantee that this ACK is for the most recent
  // gesture event (or even part of the current sequence).  Worst case, the
  // delay in updating the absorption state will result in minor UI glitches.
  // A valid |pending_async_touchmove_| will be flushed when the next event is
  // forwarded.
  send_touch_events_async_ = (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED);
  if (!send_touch_events_async_)
    needs_async_touchmove_for_outer_slop_region_ = false;
}

void TouchEventQueue::OnHasTouchEventHandlers(bool has_handlers) {
  DCHECK(!dispatching_touch_ack_);
  DCHECK(!dispatching_touch_);
  has_handlers_ = has_handlers;
}

bool TouchEventQueue::IsPendingAckTouchStart() const {
  DCHECK(!dispatching_touch_ack_);
  if (touch_queue_.empty())
    return false;

  const blink::WebTouchEvent& event =
      touch_queue_.front()->coalesced_event().event;
  return (event.type == WebInputEvent::TouchStart);
}

void TouchEventQueue::SetAckTimeoutEnabled(bool enabled) {
  if (timeout_handler_)
    timeout_handler_->SetEnabled(enabled);
}

bool TouchEventQueue::IsAckTimeoutEnabled() const {
  return timeout_handler_ && timeout_handler_->enabled();
}

bool TouchEventQueue::HasPendingAsyncTouchMoveForTesting() const {
  return pending_async_touchmove_;
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
  pending_async_touchmove_.reset();
  drop_remaining_touches_in_sequence_ = true;
  while (!touch_queue_.empty())
    PopTouchEventToClient(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
}

void TouchEventQueue::PopTouchEventToClient(InputEventAckState ack_result) {
  AckTouchEventToClient(ack_result, PopTouchEvent(), NULL);
}

void TouchEventQueue::PopTouchEventToClient(
    InputEventAckState ack_result,
    const LatencyInfo& renderer_latency_info) {
  AckTouchEventToClient(ack_result, PopTouchEvent(), &renderer_latency_info);
}

void TouchEventQueue::AckTouchEventToClient(
    InputEventAckState ack_result,
    scoped_ptr<CoalescedWebTouchEvent> acked_event,
    const ui::LatencyInfo* optional_latency_info) {
  DCHECK(acked_event);
  DCHECK(!dispatching_touch_ack_);
  UpdateTouchConsumerStates(acked_event->coalesced_event().event, ack_result);

  // Note that acking the touch-event may result in multiple gestures being sent
  // to the renderer, or touch-events being queued.
  base::AutoReset<const CoalescedWebTouchEvent*> dispatching_touch_ack(
      &dispatching_touch_ack_, acked_event.get());
  acked_event->DispatchAckToClient(ack_result, optional_latency_info, client_);
}

scoped_ptr<CoalescedWebTouchEvent> TouchEventQueue::PopTouchEvent() {
  DCHECK(!touch_queue_.empty());
  scoped_ptr<CoalescedWebTouchEvent> event(touch_queue_.front());
  touch_queue_.pop_front();
  return event.Pass();
}

void TouchEventQueue::SendTouchEventImmediately(
    const TouchEventWithLatencyInfo& touch) {
  if (needs_async_touchmove_for_outer_slop_region_) {
    // Any event other than a touchmove (e.g., touchcancel or secondary
    // touchstart) after a scroll has started will interrupt the need to send a
    // an outer slop-region exceeding touchmove.
    if (touch.event.type != WebInputEvent::TouchMove ||
        OutsideApplicationSlopRegion(touch.event,
                                     touch_sequence_start_position_))
      needs_async_touchmove_for_outer_slop_region_ = false;
  }

  client_->SendTouchEventImmediately(touch);
}

TouchEventQueue::PreFilterResult
TouchEventQueue::FilterBeforeForwarding(const WebTouchEvent& event) {
  if (timeout_handler_ && timeout_handler_->FilterEvent(event))
    return ACK_WITH_NO_CONSUMER_EXISTS;

  if (touchmove_slop_suppressor_->FilterEvent(event))
    return ACK_WITH_NOT_CONSUMED;

  if (WebTouchEventTraits::IsTouchSequenceStart(event)) {
    touch_consumer_states_.clear();
    send_touch_events_async_ = false;
    pending_async_touchmove_.reset();
    touch_sequence_start_position_ = gfx::PointF(event.touches[0].position);
    drop_remaining_touches_in_sequence_ = false;
    if (!has_handlers_) {
      drop_remaining_touches_in_sequence_ = true;
      return ACK_WITH_NO_CONSUMER_EXISTS;
    }
  }

  if (drop_remaining_touches_in_sequence_ &&
      event.type != WebInputEvent::TouchCancel) {
    return ACK_WITH_NO_CONSUMER_EXISTS;
  }

  if (event.type == WebInputEvent::TouchStart)
    return has_handlers_ ? FORWARD_TO_RENDERER : ACK_WITH_NO_CONSUMER_EXISTS;

  for (unsigned int i = 0; i < event.touchesLength; ++i) {
    const WebTouchPoint& point = event.touches[i];
    // If a point has been stationary, then don't take it into account.
    if (point.state == WebTouchPoint::StateStationary)
      continue;

    if (touch_consumer_states_.has_bit(point.id))
      return FORWARD_TO_RENDERER;
  }

  return ACK_WITH_NO_CONSUMER_EXISTS;
}

void TouchEventQueue::UpdateTouchConsumerStates(const WebTouchEvent& event,
                                                InputEventAckState ack_result) {
  // Update the ACK status for each touch point in the ACKed event.
  if (event.type == WebInputEvent::TouchEnd ||
      event.type == WebInputEvent::TouchCancel) {
    // The points have been released. Erase the ACK states.
    for (unsigned i = 0; i < event.touchesLength; ++i) {
      const WebTouchPoint& point = event.touches[i];
      if (point.state == WebTouchPoint::StateReleased ||
          point.state == WebTouchPoint::StateCancelled)
        touch_consumer_states_.clear_bit(point.id);
    }
  } else if (event.type == WebInputEvent::TouchStart) {
    for (unsigned i = 0; i < event.touchesLength; ++i) {
      const WebTouchPoint& point = event.touches[i];
      if (point.state == WebTouchPoint::StatePressed) {
        if (ack_result != INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS)
          touch_consumer_states_.mark_bit(point.id);
        else
          touch_consumer_states_.clear_bit(point.id);
      }
    }
  }
}

}  // namespace content
