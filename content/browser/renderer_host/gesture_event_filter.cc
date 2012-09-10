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
static const int kTapDownDeferralTimeMs = 150;

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


// TODO(rjkroege): Coalesce pinch updates.
// Returns |true| if two gesture events should be coalesced.
bool ShouldCoalesceGestureEvents(const WebKit::WebGestureEvent& last_event,
                                 const WebKit::WebGestureEvent& new_event) {
  return new_event.type == WebInputEvent::GestureScrollUpdate &&
      last_event.type == new_event.type &&
      last_event.modifiers == new_event.modifiers;
}
} // namespace

GestureEventFilter::GestureEventFilter(RenderWidgetHostImpl* rwhv)
     : render_widget_host_(rwhv),
       fling_in_progress_(false),
       tap_suppression_controller_(new TapSuppressionController(rwhv)),
       maximum_tap_gap_time_ms_(GetTapDownDeferralTimeMs()) {
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

// TODO(rjkroege): separate touchpad and touchscreen events.
bool GestureEventFilter::ShouldForward(const WebGestureEvent& gesture_event) {
  switch (gesture_event.type) {
    case WebInputEvent::GestureFlingCancel:
      if (!ShouldDiscardFlingCancelEvent(gesture_event)) {
        coalesced_gesture_events_.push_back(gesture_event);
        fling_in_progress_ = false;
        return ShouldHandleEventNow();
      }
      return false;
    case WebInputEvent::GestureFlingStart:
      fling_in_progress_ = true;
      coalesced_gesture_events_.push_back(gesture_event);
      return ShouldHandleEventNow();
    case WebInputEvent::GestureTapDown:
      deferred_tap_down_event_ = gesture_event;
      send_gtd_timer_.Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(maximum_tap_gap_time_ms_),
          this,
          &GestureEventFilter::SendGestureTapDownNow);
      return false;
    case WebInputEvent::GestureTap:
      send_gtd_timer_.Stop();
      coalesced_gesture_events_.push_back(deferred_tap_down_event_);
      if (ShouldHandleEventNow()) {
          render_widget_host_->ForwardGestureEventImmediately(
              deferred_tap_down_event_);
      }
      coalesced_gesture_events_.push_back(gesture_event);
      return false;
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GesturePinchBegin:
      send_gtd_timer_.Stop();
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
  coalesced_gesture_events_.clear();
  // TODO(rjkroege): Reset the tap suppression controller.
}

void GestureEventFilter::ProcessGestureAck(bool processed, int type) {
  coalesced_gesture_events_.pop_front();
  if (!coalesced_gesture_events_.empty()) {
    WebGestureEvent next_gesture_event = coalesced_gesture_events_.front();
    render_widget_host_->ForwardGestureEventImmediately(next_gesture_event);
  }
}

TapSuppressionController*  GestureEventFilter::GetTapSuppressionController() {
  return tap_suppression_controller_.get();
}

void GestureEventFilter::FlingHasBeenHalted() {
  fling_in_progress_ = false;
}

bool GestureEventFilter::ShouldHandleEventNow() {
  return coalesced_gesture_events_.size() == 1;
}

void GestureEventFilter::SendGestureTapDownNow() {
  coalesced_gesture_events_.push_back(deferred_tap_down_event_);
  if (ShouldHandleEventNow()) {
      render_widget_host_->ForwardGestureEventImmediately(
          deferred_tap_down_event_);
  }
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
    // TODO(rbyers): deltaX/deltaY fields going away. crbug.com/143237
    last_gesture_event->deltaX += gesture_event.deltaX;
    last_gesture_event->deltaY += gesture_event.deltaY;
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
