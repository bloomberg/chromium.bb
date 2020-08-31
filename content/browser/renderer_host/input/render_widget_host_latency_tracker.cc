// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/render_widget_host_latency_tracker.h"

#include <stddef.h>
#include <string>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/latency/latency_histogram_macros.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using ui::LatencyInfo;

namespace content {
namespace {
const char* GetTraceNameFromType(blink::WebInputEvent::Type type) {
#define CASE_TYPE(t)              \
  case WebInputEvent::Type::k##t: \
    return "InputLatency::" #t
  switch (type) {
    CASE_TYPE(Undefined);
    CASE_TYPE(MouseDown);
    CASE_TYPE(MouseUp);
    CASE_TYPE(MouseMove);
    CASE_TYPE(MouseEnter);
    CASE_TYPE(MouseLeave);
    CASE_TYPE(ContextMenu);
    CASE_TYPE(MouseWheel);
    CASE_TYPE(RawKeyDown);
    CASE_TYPE(KeyDown);
    CASE_TYPE(KeyUp);
    CASE_TYPE(Char);
    CASE_TYPE(GestureScrollBegin);
    CASE_TYPE(GestureScrollEnd);
    CASE_TYPE(GestureScrollUpdate);
    CASE_TYPE(GestureFlingStart);
    CASE_TYPE(GestureFlingCancel);
    CASE_TYPE(GestureShowPress);
    CASE_TYPE(GestureTap);
    CASE_TYPE(GestureTapUnconfirmed);
    CASE_TYPE(GestureTapDown);
    CASE_TYPE(GestureTapCancel);
    CASE_TYPE(GestureDoubleTap);
    CASE_TYPE(GestureTwoFingerTap);
    CASE_TYPE(GestureLongPress);
    CASE_TYPE(GestureLongTap);
    CASE_TYPE(GesturePinchBegin);
    CASE_TYPE(GesturePinchEnd);
    CASE_TYPE(GesturePinchUpdate);
    CASE_TYPE(TouchStart);
    CASE_TYPE(TouchMove);
    CASE_TYPE(TouchEnd);
    CASE_TYPE(TouchCancel);
    CASE_TYPE(TouchScrollStarted);
    CASE_TYPE(PointerDown);
    CASE_TYPE(PointerUp);
    CASE_TYPE(PointerMove);
    CASE_TYPE(PointerRawUpdate);
    CASE_TYPE(PointerCancel);
    CASE_TYPE(PointerCausedUaAction);
  }
#undef CASE_TYPE
  NOTREACHED();
  return "";
}
}  // namespace

RenderWidgetHostLatencyTracker::RenderWidgetHostLatencyTracker(
    RenderWidgetHostDelegate* delegate)
    : has_seen_first_gesture_scroll_update_(false),
      gesture_scroll_id_(-1),
      active_multi_finger_gesture_(false),
      touch_start_default_prevented_(false),
      render_widget_host_delegate_(delegate) {}

RenderWidgetHostLatencyTracker::~RenderWidgetHostLatencyTracker() {}

void RenderWidgetHostLatencyTracker::ComputeInputLatencyHistograms(
    WebInputEvent::Type type,
    const LatencyInfo& latency,
    blink::mojom::InputEventResultState ack_result,
    base::TimeTicks ack_timestamp) {
  DCHECK(!ack_timestamp.is_null());
  // If this event was coalesced into another event, ignore it, as the event it
  // was coalesced into will reflect the full latency.
  if (latency.coalesced())
    return;

  if (latency.source_event_type() == ui::SourceEventType::UNKNOWN ||
      latency.source_event_type() == ui::SourceEventType::OTHER) {
    return;
  }

  // The event will have gone through OnInputEvent(). So the BEGIN_RWH component
  // should always be available here.
  base::TimeTicks rwh_timestamp;
  bool found_component = latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, &rwh_timestamp);
  DCHECK(found_component);

  bool multi_finger_touch_gesture =
      WebInputEvent::IsTouchEventType(type) && active_multi_finger_gesture_;

  bool action_prevented =
      ack_result == blink::mojom::InputEventResultState::kConsumed;
  // Touchscreen tap and scroll gestures depend on the disposition of the touch
  // start and the current touch. For touch start,
  // touch_start_default_prevented_ == (ack_result ==
  // blink::mojom::InputEventResultState::kConsumed).
  if (WebInputEvent::IsTouchEventType(type))
    action_prevented |= touch_start_default_prevented_;

  std::string event_name = WebInputEvent::GetName(type);

  if (latency.source_event_type() == ui::SourceEventType::KEY_PRESS) {
    event_name = "KeyPress";
  } else if (event_name != "TouchEnd" && event_name != "TouchMove" &&
             event_name != "TouchStart") {
    // Only log events we care about (that are documented in histograms.xml),
    // to avoid using memory and bandwidth for metrics that are not important.
    return;
  }

  std::string default_action_status =
      action_prevented ? "DefaultPrevented" : "DefaultAllowed";

  base::TimeTicks main_thread_timestamp;
  if (latency.FindLatency(ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT,
                          &main_thread_timestamp)) {
    if (!multi_finger_touch_gesture) {
      UMA_HISTOGRAM_INPUT_LATENCY_MILLISECONDS(
          "Event.Latency.QueueingTime." + event_name + default_action_status,
          rwh_timestamp, main_thread_timestamp);
    }
  }

  if (!multi_finger_touch_gesture && !main_thread_timestamp.is_null()) {
    UMA_HISTOGRAM_INPUT_LATENCY_MILLISECONDS(
        "Event.Latency.BlockingTime." + event_name + default_action_status,
        main_thread_timestamp, ack_timestamp);
  }
}

void RenderWidgetHostLatencyTracker::OnInputEvent(
    const blink::WebInputEvent& event,
    LatencyInfo* latency) {
  DCHECK(latency);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  OnEventStart(latency);

  if (event.GetType() == WebInputEvent::Type::kTouchStart) {
    const WebTouchEvent& touch_event =
        *static_cast<const WebTouchEvent*>(&event);
    DCHECK_GE(touch_event.touches_length, static_cast<unsigned>(1));
    active_multi_finger_gesture_ = touch_event.touches_length != 1;
  }

  if (latency->source_event_type() == ui::SourceEventType::KEY_PRESS) {
    DCHECK(event.GetType() == WebInputEvent::Type::kChar ||
           event.GetType() == WebInputEvent::Type::kRawKeyDown);
  }

  // This is the only place to add the BEGIN_RWH component. So this component
  // should not already be present in the latency info.
  bool found_component = latency->FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr);
  DCHECK(!found_component);

  if (!event.TimeStamp().is_null() &&
      !latency->FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                            nullptr)) {
    base::TimeTicks timestamp_now = base::TimeTicks::Now();
    base::TimeTicks timestamp_original = event.TimeStamp();

    // Timestamp from platform input can wrap, e.g. 32 bits timestamp
    // for Xserver and Window MSG time will wrap about 49.6 days. Do a
    // sanity check here and if wrap does happen, use TimeTicks::Now()
    // as the timestamp instead.
    if ((timestamp_now - timestamp_original).InDays() > 0)
      timestamp_original = timestamp_now;

    latency->AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, timestamp_original);
  }

  latency->AddLatencyNumberWithTraceName(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      GetTraceNameFromType(event.GetType()));

  if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
    has_seen_first_gesture_scroll_update_ = false;
    gesture_scroll_id_ = latency->trace_id();
    latency->set_gesture_scroll_id(gesture_scroll_id_);
  } else if (event.GetType() ==
             blink::WebInputEvent::Type::kGestureScrollUpdate) {
    // Make a copy of the INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT with a
    // different name INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT.
    // So we can track the latency specifically for scroll update events.
    base::TimeTicks original_event_timestamp;
    if (latency->FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                             &original_event_timestamp)) {
      latency->AddLatencyNumberWithTimestamp(
          has_seen_first_gesture_scroll_update_
              ? ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT
              : ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          original_event_timestamp);
      latency->AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT,
          original_event_timestamp);
    }

    has_seen_first_gesture_scroll_update_ = true;
    latency->set_gesture_scroll_id(gesture_scroll_id_);
    latency->set_scroll_update_delta(
        static_cast<const WebGestureEvent&>(event).data.scroll_update.delta_y);
    latency->set_predicted_scroll_update_delta(
        static_cast<const WebGestureEvent&>(event).data.scroll_update.delta_y);
  } else if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollEnd) {
    latency->set_gesture_scroll_id(gesture_scroll_id_);
    gesture_scroll_id_ = -1;
  }
}

void RenderWidgetHostLatencyTracker::OnInputEventAck(
    const blink::WebInputEvent& event,
    LatencyInfo* latency,
    blink::mojom::InputEventResultState ack_result) {
  DCHECK(latency);

  // Latency ends if an event is acked but does not cause render scheduling.
  bool rendering_scheduled = latency->FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT, nullptr);
  rendering_scheduled |= latency->FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT, nullptr);

  if (WebInputEvent::IsTouchEventType(event.GetType())) {
    const WebTouchEvent& touch_event =
        *static_cast<const WebTouchEvent*>(&event);
    if (event.GetType() == WebInputEvent::Type::kTouchStart) {
      touch_start_default_prevented_ =
          ack_result == blink::mojom::InputEventResultState::kConsumed;
    } else if (event.GetType() == WebInputEvent::Type::kTouchEnd ||
               event.GetType() == WebInputEvent::Type::kTouchCancel) {
      active_multi_finger_gesture_ = touch_event.touches_length > 2;
    }
  }

  // If this event couldn't have caused a gesture event, and it didn't trigger
  // rendering, we're done processing it. If the event got coalesced then
  // terminate it as well. We also exclude cases where we're against the scroll
  // extent from scrolling metrics.
  if (!rendering_scheduled || latency->coalesced() ||
      (event.GetType() == WebInputEvent::Type::kGestureScrollUpdate &&
       ack_result == blink::mojom::InputEventResultState::kNoConsumerExists)) {
    latency->Terminate();
  }

  ComputeInputLatencyHistograms(event.GetType(), *latency, ack_result,
                                base::TimeTicks::Now());
}

void RenderWidgetHostLatencyTracker::OnEventStart(ui::LatencyInfo* latency) {
  static uint64_t global_trace_id = 0;
  latency->set_trace_id(++global_trace_id);
  latency->set_ukm_source_id(
      render_widget_host_delegate_->GetCurrentPageUkmSourceId());
}

}  // namespace content
