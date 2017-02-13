// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/gesture_event_queue.h"

#include "base/trace_event/trace_event.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/browser/renderer_host/input/touchscreen_tap_suppression_controller.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/web_input_event_traits.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace content {

GestureEventQueue::Config::Config() {
}

GestureEventQueue::GestureEventQueue(
    GestureEventQueueClient* client,
    TouchpadTapSuppressionControllerClient* touchpad_client,
    const Config& config)
    : client_(client),
      fling_in_progress_(false),
      scrolling_in_progress_(false),
      ignore_next_ack_(false),
      allow_multiple_inflight_events_(
          base::FeatureList::IsEnabled(features::kVsyncAlignedInputEvents)),
      touchpad_tap_suppression_controller_(
          touchpad_client,
          config.touchpad_tap_suppression_config),
      touchscreen_tap_suppression_controller_(
          this,
          config.touchscreen_tap_suppression_config),
      debounce_interval_(config.debounce_interval) {
  DCHECK(client);
  DCHECK(touchpad_client);
}

GestureEventQueue::~GestureEventQueue() { }

void GestureEventQueue::QueueEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  TRACE_EVENT0("input", "GestureEventQueue::QueueEvent");
  if (!ShouldForwardForBounceReduction(gesture_event) ||
      !ShouldForwardForGFCFiltering(gesture_event) ||
      !ShouldForwardForTapSuppression(gesture_event)) {
    return;
  }

  QueueAndForwardIfNecessary(gesture_event);
}

bool GestureEventQueue::ShouldDiscardFlingCancelEvent(
    const GestureEventWithLatencyInfo& gesture_event) const {
  if (coalesced_gesture_events_.empty() && fling_in_progress_)
    return false;
  GestureQueue::const_reverse_iterator it =
      coalesced_gesture_events_.rbegin();
  while (it != coalesced_gesture_events_.rend()) {
    if (it->event.type() == WebInputEvent::GestureFlingStart)
      return false;
    if (it->event.type() == WebInputEvent::GestureFlingCancel)
      return true;
    it++;
  }
  return true;
}

bool GestureEventQueue::ShouldForwardForBounceReduction(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (debounce_interval_ <= base::TimeDelta())
    return true;
  switch (gesture_event.event.type()) {
    case WebInputEvent::GestureScrollUpdate:
      if (!scrolling_in_progress_) {
        debounce_deferring_timer_.Start(
            FROM_HERE,
            debounce_interval_,
            this,
            &GestureEventQueue::SendScrollEndingEventsNow);
      } else {
        // Extend the bounce interval.
        debounce_deferring_timer_.Reset();
      }
      scrolling_in_progress_ = true;
      debouncing_deferral_queue_.clear();
      return true;

    case WebInputEvent::GesturePinchBegin:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate:
      // TODO(rjkroege): Debounce pinch (http://crbug.com/147647)
      return true;
    default:
      if (scrolling_in_progress_) {
        debouncing_deferral_queue_.push_back(gesture_event);
        return false;
      }
      return true;
  }
}

bool GestureEventQueue::ShouldForwardForGFCFiltering(
    const GestureEventWithLatencyInfo& gesture_event) const {
  return gesture_event.event.type() != WebInputEvent::GestureFlingCancel ||
         !ShouldDiscardFlingCancelEvent(gesture_event);
}

bool GestureEventQueue::ShouldForwardForTapSuppression(
    const GestureEventWithLatencyInfo& gesture_event) {
  switch (gesture_event.event.type()) {
    case WebInputEvent::GestureFlingCancel:
      if (gesture_event.event.sourceDevice ==
          blink::WebGestureDeviceTouchscreen)
        touchscreen_tap_suppression_controller_.GestureFlingCancel();
      else
        touchpad_tap_suppression_controller_.GestureFlingCancel();
      return true;
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureShowPress:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureDoubleTap:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap:
    case WebInputEvent::GestureTwoFingerTap:
      if (gesture_event.event.sourceDevice ==
          blink::WebGestureDeviceTouchscreen) {
        return !touchscreen_tap_suppression_controller_.FilterTapEvent(
            gesture_event);
      }
      return true;
    default:
      return true;
  }
}

void GestureEventQueue::QueueAndForwardIfNecessary(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (allow_multiple_inflight_events_) {
    // Event coalescing should be handled in compositor thread event queue.
    if (gesture_event.event.type() == WebInputEvent::GestureFlingCancel)
      fling_in_progress_ = false;
    else if (gesture_event.event.type() == WebInputEvent::GestureFlingStart)
      fling_in_progress_ = true;
    coalesced_gesture_events_.push_back(gesture_event);
    client_->SendGestureEventImmediately(gesture_event);
    return;
  }

  switch (gesture_event.event.type()) {
    case WebInputEvent::GestureFlingCancel:
      fling_in_progress_ = false;
      break;
    case WebInputEvent::GestureFlingStart:
      fling_in_progress_ = true;
      break;
    case WebInputEvent::GesturePinchUpdate:
    case WebInputEvent::GestureScrollUpdate:
      QueueScrollOrPinchAndForwardIfNecessary(gesture_event);
      return;
    case WebInputEvent::GestureScrollBegin:
      if (OnScrollBegin(gesture_event))
        return;
    default:
      break;
  }

  coalesced_gesture_events_.push_back(gesture_event);
  if (coalesced_gesture_events_.size() == 1)
    client_->SendGestureEventImmediately(gesture_event);
}

bool GestureEventQueue::OnScrollBegin(
    const GestureEventWithLatencyInfo& gesture_event) {
  // If a synthetic scroll begin is encountered, it can cancel out a previous
  // synthetic scroll end. This allows a later gesture scroll update to coalesce
  // with the previous one. crbug.com/607340.
  bool synthetic = gesture_event.event.data.scrollBegin.synthetic;
  bool have_unsent_events =
      EventsInFlightCount() < coalesced_gesture_events_.size();
  if (synthetic && have_unsent_events) {
    GestureEventWithLatencyInfo* last_event = &coalesced_gesture_events_.back();
    if (last_event->event.type() == WebInputEvent::GestureScrollEnd &&
        last_event->event.data.scrollEnd.synthetic) {
      coalesced_gesture_events_.pop_back();
      return true;
    }
  }
  return false;
}

void GestureEventQueue::ProcessGestureAck(InputEventAckState ack_result,
                                          WebInputEvent::Type type,
                                          const ui::LatencyInfo& latency) {
  TRACE_EVENT0("input", "GestureEventQueue::ProcessGestureAck");

  if (coalesced_gesture_events_.empty()) {
    DLOG(ERROR) << "Received unexpected ACK for event type " << type;
    return;
  }

  size_t event_index = 0;
  if (allow_multiple_inflight_events_) {
    // Events are forwarded immediately.
    // Assuming events of the same type are acked in a FIFO order, but it's
    // still possible that GestureFling events are acked before
    // GestureScroll/Pinch as they don't need to go through the queue in
    // |InputHandlerProxy::HandleInputEventWithLatencyInfo|.
    for (size_t i = 0; i < coalesced_gesture_events_.size(); ++i) {
      if (coalesced_gesture_events_[i].event.type() == type) {
        event_index = i;
        break;
      }
    }
  } else {
    // Events are forwarded one-by-one.
    // It's possible that the ack for the second event in an in-flight,
    // coalesced Gesture{Scroll,Pinch}Update pair is received prior to the first
    // event ack.
    if (ignore_next_ack_ && coalesced_gesture_events_.size() > 1 &&
        coalesced_gesture_events_[0].event.type() != type &&
        coalesced_gesture_events_[1].event.type() == type) {
      event_index = 1;
    }
  }
  GestureEventWithLatencyInfo event_with_latency =
      coalesced_gesture_events_[event_index];
  DCHECK_EQ(event_with_latency.event.type(), type);
  event_with_latency.latency.AddNewLatencyFrom(latency);

  // Ack'ing an event may enqueue additional gesture events.  By ack'ing the
  // event before the forwarding of queued events below, such additional events
  // can be coalesced with existing queued events prior to dispatch.
  client_->OnGestureEventAck(event_with_latency, ack_result);

  const bool processed = (INPUT_EVENT_ACK_STATE_CONSUMED == ack_result);
  if (type == WebInputEvent::GestureFlingCancel) {
    if (event_with_latency.event.sourceDevice ==
        blink::WebGestureDeviceTouchscreen)
      touchscreen_tap_suppression_controller_.GestureFlingCancelAck(processed);
    else
      touchpad_tap_suppression_controller_.GestureFlingCancelAck(processed);
  }
  DCHECK_LT(event_index, coalesced_gesture_events_.size());
  coalesced_gesture_events_.erase(coalesced_gesture_events_.begin() +
                                  event_index);

  // Events have been forwarded already.
  if (allow_multiple_inflight_events_)
    return;

  if (ignore_next_ack_) {
    ignore_next_ack_ = false;
    return;
  }

  if (coalesced_gesture_events_.empty())
    return;

  const GestureEventWithLatencyInfo& first_gesture_event =
      coalesced_gesture_events_.front();

  // Check for the coupled GesturePinchUpdate before sending either event,
  // handling the case where the first GestureScrollUpdate ack is synchronous.
  GestureEventWithLatencyInfo second_gesture_event;
  if (first_gesture_event.event.type() == WebInputEvent::GestureScrollUpdate &&
      coalesced_gesture_events_.size() > 1 &&
      coalesced_gesture_events_[1].event.type() ==
          WebInputEvent::GesturePinchUpdate) {
    second_gesture_event = coalesced_gesture_events_[1];
    ignore_next_ack_ = true;
  }

  client_->SendGestureEventImmediately(first_gesture_event);
  if (second_gesture_event.event.type() != WebInputEvent::Undefined)
    client_->SendGestureEventImmediately(second_gesture_event);
}

TouchpadTapSuppressionController*
    GestureEventQueue::GetTouchpadTapSuppressionController() {
  return &touchpad_tap_suppression_controller_;
}

void GestureEventQueue::FlingHasBeenHalted() {
  fling_in_progress_ = false;
}

void GestureEventQueue::ForwardGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  QueueAndForwardIfNecessary(gesture_event);
}

void GestureEventQueue::SendScrollEndingEventsNow() {
  scrolling_in_progress_ = false;
  if (debouncing_deferral_queue_.empty())
    return;
  GestureQueue debouncing_deferral_queue;
  debouncing_deferral_queue.swap(debouncing_deferral_queue_);
  for (GestureQueue::const_iterator it = debouncing_deferral_queue.begin();
       it != debouncing_deferral_queue.end(); it++) {
    if (ShouldForwardForGFCFiltering(*it) &&
        ShouldForwardForTapSuppression(*it)) {
      QueueAndForwardIfNecessary(*it);
    }
  }
}

void GestureEventQueue::QueueScrollOrPinchAndForwardIfNecessary(
    const GestureEventWithLatencyInfo& gesture_event) {
  DCHECK_GE(coalesced_gesture_events_.size(), EventsInFlightCount());
  const size_t unsent_events_count =
      coalesced_gesture_events_.size() - EventsInFlightCount();
  if (!unsent_events_count) {
    coalesced_gesture_events_.push_back(gesture_event);
    if (coalesced_gesture_events_.size() == 1) {
      client_->SendGestureEventImmediately(gesture_event);
    } else if (coalesced_gesture_events_.size() == 2) {
      DCHECK(!ignore_next_ack_);
      // If there is an in-flight scroll, the new pinch can be forwarded
      // immediately, avoiding a potential frame delay between the two
      // (similarly for an in-flight pinch with a new scroll).
      const GestureEventWithLatencyInfo& first_event =
          coalesced_gesture_events_.front();
      if (gesture_event.event.type() != first_event.event.type() &&
          ui::IsCompatibleScrollorPinch(gesture_event.event,
                                        first_event.event)) {
        ignore_next_ack_ = true;
        client_->SendGestureEventImmediately(gesture_event);
      }
    }
    return;
  }

  GestureEventWithLatencyInfo* last_event = &coalesced_gesture_events_.back();
  if (last_event->CanCoalesceWith(gesture_event)) {
    last_event->CoalesceWith(gesture_event);
    return;
  }

  if (!ui::IsCompatibleScrollorPinch(gesture_event.event, last_event->event)) {
    coalesced_gesture_events_.push_back(gesture_event);
    return;
  }

  // Extract the last event in queue.
  blink::WebGestureEvent last_gesture_event =
      coalesced_gesture_events_.back().event;
  DCHECK_LE(coalesced_gesture_events_.back().latency.trace_id(),
            gesture_event.latency.trace_id());
  ui::LatencyInfo oldest_latency = coalesced_gesture_events_.back().latency;
  oldest_latency.set_coalesced();
  coalesced_gesture_events_.pop_back();

  // Extract the second last event in queue.
  ui::WebScopedInputEvent second_last_gesture_event = nullptr;
  if (unsent_events_count > 1 &&
      ui::IsCompatibleScrollorPinch(gesture_event.event,
                                    coalesced_gesture_events_.back().event)) {
    second_last_gesture_event =
        ui::WebInputEventTraits::Clone(coalesced_gesture_events_.back().event);
    DCHECK_LE(coalesced_gesture_events_.back().latency.trace_id(),
              oldest_latency.trace_id());
    oldest_latency = coalesced_gesture_events_.back().latency;
    oldest_latency.set_coalesced();
    coalesced_gesture_events_.pop_back();
  }

  std::pair<blink::WebGestureEvent, blink::WebGestureEvent> coalesced_events =
      ui::CoalesceScrollAndPinch(
          second_last_gesture_event
              ? &ui::ToWebGestureEvent(*second_last_gesture_event)
              : nullptr,
          last_gesture_event, gesture_event.event);

  GestureEventWithLatencyInfo scroll_event;
  scroll_event.event = coalesced_events.first;
  scroll_event.latency = oldest_latency;

  GestureEventWithLatencyInfo pinch_event;
  pinch_event.event = coalesced_events.second;
  pinch_event.latency = oldest_latency;

  coalesced_gesture_events_.push_back(scroll_event);
  coalesced_gesture_events_.push_back(pinch_event);
}

size_t GestureEventQueue::EventsInFlightCount() const {
  if (allow_multiple_inflight_events_) {
    // Currently unused, can be removed if compositor event queue was enabled by
    // default.
    NOTREACHED();
    return coalesced_gesture_events_.size();
  }

  if (coalesced_gesture_events_.empty())
    return 0;

  if (!ignore_next_ack_)
    return 1;

  DCHECK_GT(coalesced_gesture_events_.size(), 1U);
  return 2;
}

}  // namespace content
