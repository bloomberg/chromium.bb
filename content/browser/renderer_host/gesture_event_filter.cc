// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/gesture_event_filter.h"

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/tap_suppression_controller.h"
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
} // namespace

GestureEventFilter::GestureEventFilter(RenderWidgetHostImpl* rwhv)
     : render_widget_host_(rwhv),
       fling_in_progress_(false),
       scrolling_in_progress_(false),
       tap_suppression_controller_(new TapSuppressionController(rwhv)),
       maximum_tap_gap_time_ms_(GetTapDownDeferralTimeMs()),
       debounce_interval_time_ms_(kDebouncingIntervalTimeMs) {
}

GestureEventFilter::~GestureEventFilter() { }

bool GestureEventFilter::ShouldDiscardFlingCancelEvent(
    const WebKit::WebGestureEvent& gesture_event) {
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

bool GestureEventFilter::ShouldForwardForBounceReduction(
    const WebGestureEvent& gesture_event) {
  switch (gesture_event.type) {
    case WebInputEvent::GestureScrollUpdate:
      if (!scrolling_in_progress_) {
        debounce_deferring_timer_.Start(FROM_HERE,
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
bool GestureEventFilter::ShouldForward(const WebGestureEvent& gesture_event) {
  // Discard a zero-velocity fling start from the trackpad.
  if (gesture_event.type == WebInputEvent::GestureFlingStart &&
      gesture_event.sourceDevice == WebGestureEvent::Touchpad &&
      gesture_event.data.flingStart.velocityX == 0 &&
      gesture_event.data.flingStart.velocityY == 0) {
    return false;
  }

  if (debounce_interval_time_ms_ ==  0 ||
      ShouldForwardForBounceReduction(gesture_event))
    return ShouldForwardForTapDeferral(gesture_event);
  return false;
}

// TODO(rjkroege): separate touchpad and touchscreen events.
bool GestureEventFilter::ShouldForwardForTapDeferral(
    const WebGestureEvent& gesture_event) {
  switch (gesture_event.type) {
    case WebInputEvent::GestureFlingCancel:
      if (!ShouldDiscardFlingCancelEvent(gesture_event)) {
        coalesced_gesture_events_.push_back(gesture_event);
        fling_in_progress_ = false;
        tap_suppression_controller_->GestureFlingCancel(
            gesture_event.timeStampSeconds);
        return ShouldHandleEventNow();
      }
      return false;
    case WebInputEvent::GestureTapDown:
      // GestureTapDown is always paired with either a Tap, or TapCancel, so it
      // should be impossible to have more than one outstanding at a time.
      DCHECK_EQ(deferred_tap_down_event_.type, WebInputEvent::Undefined);
      deferred_tap_down_event_ = gesture_event;
      send_gtd_timer_.Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(maximum_tap_gap_time_ms_),
          this,
          &GestureEventFilter::SendGestureTapDownNow);
      return false;
    case WebInputEvent::GestureTapCancel:
      if (deferred_tap_down_event_.type == WebInputEvent::Undefined) {
        // The TapDown has already been put in the queue, must send the
        // corresponding TapCancel as well.
        coalesced_gesture_events_.push_back(gesture_event);
        return ShouldHandleEventNow();
      }
      // Cancelling a deferred TapDown, just drop them on the floor.
      send_gtd_timer_.Stop();
      deferred_tap_down_event_.type = WebInputEvent::Undefined;
      return false;
    case WebInputEvent::GestureTap:
      send_gtd_timer_.Stop();
      if (deferred_tap_down_event_.type != WebInputEvent::Undefined) {
        coalesced_gesture_events_.push_back(deferred_tap_down_event_);
        if (ShouldHandleEventNow())
          render_widget_host_->ForwardGestureEventImmediately(
              deferred_tap_down_event_);
        deferred_tap_down_event_.type = WebInputEvent::Undefined;
        coalesced_gesture_events_.push_back(gesture_event);
        return false;
      }
      coalesced_gesture_events_.push_back(gesture_event);
      return ShouldHandleEventNow();
    case WebInputEvent::GestureFlingStart:
      fling_in_progress_ = true;
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GesturePinchBegin:
      send_gtd_timer_.Stop();
      deferred_tap_down_event_.type = WebInputEvent::Undefined;
      coalesced_gesture_events_.push_back(gesture_event);
      return ShouldHandleEventNow();
    case WebInputEvent::GestureScrollUpdate:
      MergeOrInsertScrollEvent(gesture_event);
      return ShouldHandleEventNow();
    default:
      coalesced_gesture_events_.push_back(gesture_event);
      return ShouldHandleEventNow();
  }

  NOTREACHED();
  return false;
}

void GestureEventFilter::Reset() {
  fling_in_progress_ = false;
  scrolling_in_progress_ = false;
  coalesced_gesture_events_.clear();
  deferred_tap_down_event_.type = WebInputEvent::Undefined;
  debouncing_deferral_queue_.clear();
  send_gtd_timer_.Stop();
  debounce_deferring_timer_.Stop();
  // TODO(rjkroege): Reset the tap suppression controller.
}

void GestureEventFilter::ProcessGestureAck(bool processed, int type) {
  if (coalesced_gesture_events_.empty()) {
    DLOG(ERROR) << "Received unexpected ACK for event type " << type;
    return;
  }
  DCHECK_EQ(coalesced_gesture_events_.front().type, type);
  coalesced_gesture_events_.pop_front();
  if (type == WebInputEvent::GestureFlingCancel)
    tap_suppression_controller_->GestureFlingCancelAck(processed);
  if (!coalesced_gesture_events_.empty()) {
    WebGestureEvent next_gesture_event = coalesced_gesture_events_.front();
    render_widget_host_->ForwardGestureEventImmediately(next_gesture_event);
  }
}

TapSuppressionController*  GestureEventFilter::GetTapSuppressionController() {
  return tap_suppression_controller_.get();
}

bool GestureEventFilter::HasQueuedGestureEvents() const {
  return !coalesced_gesture_events_.empty();
}

const WebKit::WebInputEvent&
GestureEventFilter::GetGestureEventAwaitingAck() const {
  DCHECK(!coalesced_gesture_events_.empty());
  return coalesced_gesture_events_.front();
}

void GestureEventFilter::FlingHasBeenHalted() {
  fling_in_progress_ = false;
}

bool GestureEventFilter::ShouldHandleEventNow() {
  return coalesced_gesture_events_.size() == 1;
}

void GestureEventFilter::SendGestureTapDownNow() {
  // We must not have already sent the deferred TapDown (if we did, we would
  // have stopped the timer, which prevents this task from running - even if
  // it's time had already elapsed).
  DCHECK_EQ(deferred_tap_down_event_.type, WebInputEvent::GestureTapDown);
  coalesced_gesture_events_.push_back(deferred_tap_down_event_);
  if (ShouldHandleEventNow()) {
      render_widget_host_->ForwardGestureEventImmediately(
          deferred_tap_down_event_);
  }
  deferred_tap_down_event_.type = WebInputEvent::Undefined;
}

void GestureEventFilter::SendScrollEndingEventsNow() {
  scrolling_in_progress_ = false;
  for (GestureEventQueue::iterator it =
      debouncing_deferral_queue_.begin();
      it != debouncing_deferral_queue_.end(); it++) {
    if (ShouldForwardForTapDeferral(*it)) {
      render_widget_host_->ForwardGestureEventImmediately(*it);
    }
  }
  debouncing_deferral_queue_.clear();
}

void GestureEventFilter::MergeOrInsertScrollEvent(
    const WebGestureEvent& gesture_event) {
  WebGestureEvent* last_gesture_event = coalesced_gesture_events_.empty() ? 0 :
      &coalesced_gesture_events_.back();
  if (coalesced_gesture_events_.size() > 1 &&
      last_gesture_event->type == gesture_event.type &&
      last_gesture_event->modifiers == gesture_event.modifiers) {
    last_gesture_event->data.scrollUpdate.deltaX +=
        gesture_event.data.scrollUpdate.deltaX;
    last_gesture_event->data.scrollUpdate.deltaY +=
        gesture_event.data.scrollUpdate.deltaY;
    DLOG_IF(WARNING,
            gesture_event.timeStampSeconds <=
            last_gesture_event->timeStampSeconds)
            << "Event time not monotonic?\n";
    DCHECK(last_gesture_event->type == WebInputEvent::GestureScrollUpdate);
    last_gesture_event->timeStampSeconds = gesture_event.timeStampSeconds;
  } else {
    coalesced_gesture_events_.push_back(gesture_event);
  }
}

} // namespace content
