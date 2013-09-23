// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/gesture_event_filter.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/browser/renderer_host/input/touchscreen_tap_suppression_controller.h"
#include "content/public/common/content_switches.h"

using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;

namespace content {
namespace {

// Default maximum time between the GestureRecognizer generating a
// GestureTapDown and when it is forwarded to the renderer.
#if !defined(OS_ANDROID)
static const int kTapDownDeferralTimeMs = 150;
#else
// Android OS sends this gesture with a delay already.
static const int kTapDownDeferralTimeMs = 0;
#endif

// Default debouncing interval duration: if a scroll is in progress, non-scroll
// events during this interval are deferred to either its end or discarded on
// receipt of another GestureScrollUpdate.
static const int kDebouncingIntervalTimeMs = 30;

// Sets |*value| to |switchKey| if it exists or sets it to |defaultValue|.
static void GetParamHelper(int* value,
                           int defaultValue,
                           const char switchKey[]) {
  if (*value < 0) {
    *value = defaultValue;
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    std::string command_line_param =
        command_line->GetSwitchValueASCII(switchKey);
    if (!command_line_param.empty()) {
      int v;
      if (base::StringToInt(command_line_param, &v))
        *value = v;
    }
    DCHECK_GE(*value, 0);
  }
}

static int GetTapDownDeferralTimeMs() {
  static int tap_down_deferral_time_window = -1;
  GetParamHelper(&tap_down_deferral_time_window,
                 kTapDownDeferralTimeMs,
                 switches::kTapDownDeferralTimeMs);
  return tap_down_deferral_time_window;
}
}  // namespace

GestureEventFilter::GestureEventFilter(InputRouter* input_router)
     : input_router_(input_router),
       fling_in_progress_(false),
       scrolling_in_progress_(false),
       ignore_next_ack_(false),
       combined_scroll_pinch_(gfx::Transform()),
       touchpad_tap_suppression_controller_(
           new TouchpadTapSuppressionController(input_router)),
       touchscreen_tap_suppression_controller_(
           new TouchscreenTapSuppressionController(this)),
       maximum_tap_gap_time_ms_(GetTapDownDeferralTimeMs()),
       debounce_interval_time_ms_(kDebouncingIntervalTimeMs) {
}

GestureEventFilter::~GestureEventFilter() { }

bool GestureEventFilter::ShouldDiscardFlingCancelEvent(
    const GestureEventWithLatencyInfo& gesture_event) const {
  if (coalesced_gesture_events_.empty() && fling_in_progress_)
    return false;
  GestureEventQueue::const_reverse_iterator it =
      coalesced_gesture_events_.rbegin();
  while (it != coalesced_gesture_events_.rend()) {
    if (it->event.type == WebInputEvent::GestureFlingStart)
      return false;
    if (it->event.type == WebInputEvent::GestureFlingCancel)
      return true;
    it++;
  }
  return true;
}

bool GestureEventFilter::ShouldForwardForBounceReduction(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (debounce_interval_time_ms_ ==  0)
    return true;
  switch (gesture_event.event.type) {
    case WebInputEvent::GestureScrollUpdate:
      if (!scrolling_in_progress_) {
        debounce_deferring_timer_.Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(debounce_interval_time_ms_),
            this,
            &GestureEventFilter::SendScrollEndingEventsNow);
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

  NOTREACHED();
  return false;
}

// NOTE: The filters are applied successively. This simplifies the change.
bool GestureEventFilter::ShouldForward(
    const GestureEventWithLatencyInfo& gesture_event) {
  return ShouldForwardForZeroVelocityFlingStart(gesture_event) &&
      ShouldForwardForBounceReduction(gesture_event) &&
      ShouldForwardForGFCFiltering(gesture_event) &&
      ShouldForwardForTapSuppression(gesture_event) &&
      ShouldForwardForTapDeferral(gesture_event) &&
      ShouldForwardForCoalescing(gesture_event);
}

bool GestureEventFilter::ShouldForwardForZeroVelocityFlingStart(
    const GestureEventWithLatencyInfo& gesture_event) const {
  return gesture_event.event.type != WebInputEvent::GestureFlingStart ||
      gesture_event.event.sourceDevice != WebGestureEvent::Touchpad ||
      gesture_event.event.data.flingStart.velocityX != 0 ||
      gesture_event.event.data.flingStart.velocityY != 0;
}

bool GestureEventFilter::ShouldForwardForGFCFiltering(
    const GestureEventWithLatencyInfo& gesture_event) const {
  return gesture_event.event.type != WebInputEvent::GestureFlingCancel ||
      !ShouldDiscardFlingCancelEvent(gesture_event);
}

bool GestureEventFilter::ShouldForwardForTapSuppression(
    const GestureEventWithLatencyInfo& gesture_event) {
  switch (gesture_event.event.type) {
    case WebInputEvent::GestureFlingCancel:
      if (gesture_event.event.sourceDevice == WebGestureEvent::Touchscreen)
        touchscreen_tap_suppression_controller_->GestureFlingCancel();
      else
        touchpad_tap_suppression_controller_->GestureFlingCancel();
      return true;
    case WebInputEvent::GestureTapDown:
      return !touchscreen_tap_suppression_controller_->
          ShouldDeferGestureTapDown(gesture_event);
    case WebInputEvent::GestureTapCancel:
      return !touchscreen_tap_suppression_controller_->
          ShouldSuppressGestureTapCancel();
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureTapUnconfirmed:
      return !touchscreen_tap_suppression_controller_->
          ShouldSuppressGestureTap();
    default:
      return true;
  }
  NOTREACHED();
  return false;
}

bool GestureEventFilter::ShouldForwardForTapDeferral(
    const GestureEventWithLatencyInfo& gesture_event) {
  switch (gesture_event.event.type) {
    case WebInputEvent::GestureTapDown:
      // GestureTapDown is always paired with either a Tap, or TapCancel, so it
      // should be impossible to have more than one outstanding at a time.
      DCHECK_EQ(deferred_tap_down_event_.event.type, WebInputEvent::Undefined);
      deferred_tap_down_event_ = gesture_event;
      send_gtd_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(maximum_tap_gap_time_ms_),
          this,
          &GestureEventFilter::SendGestureTapDownNow);
      return false;
    case WebInputEvent::GestureTapCancel:
      if (deferred_tap_down_event_.event.type == WebInputEvent::Undefined) {
        // The TapDown has already been put in the queue, must send the
        // corresponding TapCancel as well.
        return true;
      }
      // Cancelling a deferred TapDown, just drop them on the floor.
      send_gtd_timer_.Stop();
      deferred_tap_down_event_.event.type = WebInputEvent::Undefined;
      return false;
    case WebInputEvent::GestureTap:
      send_gtd_timer_.Stop();
      if (deferred_tap_down_event_.event.type != WebInputEvent::Undefined) {
        ForwardGestureEventSkipDeferral(deferred_tap_down_event_);
        deferred_tap_down_event_.event.type = WebInputEvent::Undefined;
      }
      return true;
    case WebInputEvent::GestureFlingStart:
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GesturePinchBegin:
      send_gtd_timer_.Stop();
      deferred_tap_down_event_.event.type = WebInputEvent::Undefined;
      return true;
    default:
      return true;
  }

  NOTREACHED();
  return true;
}

bool GestureEventFilter::ShouldForwardForCoalescing(
    const GestureEventWithLatencyInfo& gesture_event) {
  switch (gesture_event.event.type) {
    case WebInputEvent::GestureFlingCancel:
      fling_in_progress_ = false;
      break;
    case WebInputEvent::GestureFlingStart:
      fling_in_progress_ = true;
      break;
    case WebInputEvent::GesturePinchUpdate:
    case WebInputEvent::GestureScrollUpdate:
      MergeOrInsertScrollAndPinchEvent(gesture_event);
      return ShouldHandleEventNow();
    default:
      break;
  }
  coalesced_gesture_events_.push_back(gesture_event);
  return ShouldHandleEventNow();
}

void GestureEventFilter::ProcessGestureAck(bool processed, int type) {
  if (coalesced_gesture_events_.empty()) {
    DLOG(ERROR) << "Received unexpected ACK for event type " << type;
    return;
  }
  DCHECK_EQ(coalesced_gesture_events_.front().event.type, type);
  if (type == WebInputEvent::GestureFlingCancel) {
    if (coalesced_gesture_events_.front().event.sourceDevice ==
        WebGestureEvent::Touchscreen)
      touchscreen_tap_suppression_controller_->GestureFlingCancelAck(processed);
    else
      touchpad_tap_suppression_controller_->GestureFlingCancelAck(processed);
  }
  coalesced_gesture_events_.pop_front();
  if (ignore_next_ack_) {
    ignore_next_ack_ = false;
  } else if (!coalesced_gesture_events_.empty()) {
    const GestureEventWithLatencyInfo& next_gesture_event =
        coalesced_gesture_events_.front();
    input_router_->SendGestureEventImmediately(next_gesture_event);
    // TODO(yusufo): Introduce GesturePanScroll so that these can be combined
    // into one gesture and kept inside the queue that way.
    if (coalesced_gesture_events_.size() > 1) {
      const GestureEventWithLatencyInfo& second_gesture_event =
          coalesced_gesture_events_[1];
      if (next_gesture_event.event.type ==
              WebInputEvent::GestureScrollUpdate &&
          second_gesture_event.event.type ==
              WebInputEvent::GesturePinchUpdate) {
        input_router_->SendGestureEventImmediately(second_gesture_event);
        ignore_next_ack_ = true;
        combined_scroll_pinch_ = gfx::Transform();
      }
    }
  }
}

TouchpadTapSuppressionController*
    GestureEventFilter::GetTouchpadTapSuppressionController() {
  return touchpad_tap_suppression_controller_.get();
}

bool GestureEventFilter::HasQueuedGestureEvents() const {
  return !coalesced_gesture_events_.empty();
}

const WebKit::WebGestureEvent&
GestureEventFilter::GetGestureEventAwaitingAck() const {
  DCHECK(!coalesced_gesture_events_.empty());
  if (!ignore_next_ack_)
    return coalesced_gesture_events_.front().event;
  else
    return coalesced_gesture_events_.at(1).event;
}

void GestureEventFilter::FlingHasBeenHalted() {
  fling_in_progress_ = false;
}

bool GestureEventFilter::ShouldHandleEventNow() const {
  return coalesced_gesture_events_.size() == 1;
}

void GestureEventFilter::ForwardGestureEventForDeferral(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (ShouldForwardForTapDeferral(gesture_event))
    ForwardGestureEventSkipDeferral(gesture_event);
}

void GestureEventFilter::ForwardGestureEventSkipDeferral(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (ShouldForwardForCoalescing(gesture_event))
      input_router_->SendGestureEventImmediately(gesture_event);
}

void GestureEventFilter::SendGestureTapDownNow() {
  // We must not have already sent the deferred TapDown (if we did, we would
  // have stopped the timer, which prevents this task from running - even if
  // it's time had already elapsed).
  DCHECK_EQ(deferred_tap_down_event_.event.type, WebInputEvent::GestureTapDown);
  ForwardGestureEventSkipDeferral(deferred_tap_down_event_);
  deferred_tap_down_event_.event.type = WebInputEvent::Undefined;
}

void GestureEventFilter::SendScrollEndingEventsNow() {
  scrolling_in_progress_ = false;
  for (GestureEventQueue::const_iterator it =
      debouncing_deferral_queue_.begin();
      it != debouncing_deferral_queue_.end(); it++) {
    if (ShouldForwardForGFCFiltering(*it) &&
        ShouldForwardForTapSuppression(*it) &&
        ShouldForwardForTapDeferral(*it) &&
        ShouldForwardForCoalescing(*it)) {
      input_router_->SendGestureEventImmediately(*it);
    }
  }
  debouncing_deferral_queue_.clear();
}

void GestureEventFilter::MergeOrInsertScrollAndPinchEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (coalesced_gesture_events_.size() <= 1) {
    coalesced_gesture_events_.push_back(gesture_event);
    return;
  }
  GestureEventWithLatencyInfo* last_event = &coalesced_gesture_events_.back();
  if (gesture_event.event.type == WebInputEvent::GestureScrollUpdate &&
      last_event->event.type == WebInputEvent::GestureScrollUpdate &&
      last_event->event.modifiers == gesture_event.event.modifiers) {
    last_event->event.data.scrollUpdate.deltaX +=
        gesture_event.event.data.scrollUpdate.deltaX;
    last_event->event.data.scrollUpdate.deltaY +=
        gesture_event.event.data.scrollUpdate.deltaY;
    last_event->latency.MergeWith(gesture_event.latency);
    return;
  }
  if (coalesced_gesture_events_.size() == 2 ||
      (coalesced_gesture_events_.size() == 3 && ignore_next_ack_) ||
      !ShouldTryMerging(gesture_event, *last_event)) {
    coalesced_gesture_events_.push_back(gesture_event);
    return;
  }
  GestureEventWithLatencyInfo scroll_event;
  GestureEventWithLatencyInfo pinch_event;
  scroll_event.event.modifiers |= gesture_event.event.modifiers;
  scroll_event.event.timeStampSeconds = gesture_event.event.timeStampSeconds;
  scroll_event.latency = gesture_event.latency;
  scroll_event.latency.MergeWith(last_event->latency);
  pinch_event = scroll_event;
  scroll_event.event.type = WebInputEvent::GestureScrollUpdate;
  pinch_event.event.type = WebInputEvent::GesturePinchUpdate;
  pinch_event.event.x = gesture_event.event.type ==
      WebInputEvent::GesturePinchUpdate ?
          gesture_event.event.x : last_event->event.x;
  pinch_event.event.y = gesture_event.event.type ==
      WebInputEvent::GesturePinchUpdate ?
          gesture_event.event.y : last_event->event.y;

  combined_scroll_pinch_.ConcatTransform(GetTransformForEvent(gesture_event));
  GestureEventWithLatencyInfo* second_last_event = &coalesced_gesture_events_
      [coalesced_gesture_events_.size() - 2];
  if (ShouldTryMerging(gesture_event, *second_last_event)) {
    scroll_event.latency.MergeWith(second_last_event->latency);
    pinch_event.latency.MergeWith(second_last_event->latency);
    coalesced_gesture_events_.pop_back();
  } else {
    DCHECK(combined_scroll_pinch_ == GetTransformForEvent(gesture_event));
    combined_scroll_pinch_.
        PreconcatTransform(GetTransformForEvent(*last_event));
  }
  coalesced_gesture_events_.pop_back();
  float combined_scale = combined_scroll_pinch_.matrix().getDouble(0, 0);
  scroll_event.event.data.scrollUpdate.deltaX =
      (combined_scroll_pinch_.matrix().getDouble(0, 3) + pinch_event.event.x)
          / combined_scale - pinch_event.event.x;
  scroll_event.event.data.scrollUpdate.deltaY =
      (combined_scroll_pinch_.matrix().getDouble(1, 3) + pinch_event.event.y)
          / combined_scale - pinch_event.event.y;
  coalesced_gesture_events_.push_back(scroll_event);
  pinch_event.event.data.pinchUpdate.scale = combined_scale;
  coalesced_gesture_events_.push_back(pinch_event);
}

bool GestureEventFilter::ShouldTryMerging(
    const GestureEventWithLatencyInfo& new_event,
    const GestureEventWithLatencyInfo& event_in_queue) const {
  DLOG_IF(WARNING,
          new_event.event.timeStampSeconds <
          event_in_queue.event.timeStampSeconds)
          << "Event time not monotonic?\n";
  return (event_in_queue.event.type == WebInputEvent::GestureScrollUpdate ||
      event_in_queue.event.type == WebInputEvent::GesturePinchUpdate) &&
      event_in_queue.event.modifiers == new_event.event.modifiers;
}

gfx::Transform GestureEventFilter::GetTransformForEvent(
    const GestureEventWithLatencyInfo& gesture_event) const {
  gfx::Transform gesture_transform = gfx::Transform();
  if (gesture_event.event.type == WebInputEvent::GestureScrollUpdate) {
    gesture_transform.Translate(gesture_event.event.data.scrollUpdate.deltaX,
                                gesture_event.event.data.scrollUpdate.deltaY);
  } else if (gesture_event.event.type == WebInputEvent::GesturePinchUpdate) {
    float scale = gesture_event.event.data.pinchUpdate.scale;
    gesture_transform.Translate(-gesture_event.event.x, -gesture_event.event.y);
    gesture_transform.Scale(scale,scale);
    gesture_transform.Translate(gesture_event.event.x, gesture_event.event.y);
  }
  return gesture_transform;
}
}  // namespace content
