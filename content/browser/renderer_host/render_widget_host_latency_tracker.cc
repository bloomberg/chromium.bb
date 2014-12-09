// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_latency_tracker.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using ui::LatencyInfo;

namespace content {
namespace {

const uint32 kMaxInputCoordinates = LatencyInfo::kMaxInputCoordinates;

void UpdateLatencyCoordinatesImpl(const blink::WebTouchEvent& touch,
                                  LatencyInfo* latency) {
  latency->input_coordinates_size =
      std::min(kMaxInputCoordinates, touch.touchesLength);
  for (uint32 i = 0; i < latency->input_coordinates_size; ++i) {
    latency->input_coordinates[i] = LatencyInfo::InputCoordinate(
        touch.touches[i].position.x, touch.touches[i].position.y);
  }
}

void UpdateLatencyCoordinatesImpl(const WebGestureEvent& gesture,
                                  LatencyInfo* latency) {
  latency->input_coordinates_size = 1;
  latency->input_coordinates[0] =
      LatencyInfo::InputCoordinate(gesture.x, gesture.y);
}

void UpdateLatencyCoordinatesImpl(const WebMouseEvent& mouse,
                                  LatencyInfo* latency) {
  latency->input_coordinates_size = 1;
  latency->input_coordinates[0] =
      LatencyInfo::InputCoordinate(mouse.x, mouse.y);
}

void UpdateLatencyCoordinatesImpl(const WebMouseWheelEvent& wheel,
                                  LatencyInfo* latency) {
  latency->input_coordinates_size = 1;
  latency->input_coordinates[0] =
      LatencyInfo::InputCoordinate(wheel.x, wheel.y);
}

void UpdateLatencyCoordinates(const WebInputEvent& event,
                              float device_scale_factor,
                              LatencyInfo* latency) {
  if (WebInputEvent::isMouseEventType(event.type)) {
    UpdateLatencyCoordinatesImpl(static_cast<const WebMouseEvent&>(event),
                                 latency);
  } else if (WebInputEvent::isGestureEventType(event.type)) {
    UpdateLatencyCoordinatesImpl(static_cast<const WebGestureEvent&>(event),
                                 latency);
  } else if (WebInputEvent::isTouchEventType(event.type)) {
    UpdateLatencyCoordinatesImpl(static_cast<const WebTouchEvent&>(event),
                                 latency);
  } else if (event.type == WebInputEvent::MouseWheel) {
    UpdateLatencyCoordinatesImpl(static_cast<const WebMouseWheelEvent&>(event),
                                 latency);
  }
  if (device_scale_factor == 1)
    return;
  for (uint32 i = 0; i < latency->input_coordinates_size; ++i) {
    latency->input_coordinates[i].x *= device_scale_factor;
    latency->input_coordinates[i].y *= device_scale_factor;
  }
}

void ComputeInputLatencyHistograms(WebInputEvent::Type type,
                                   int64 latency_component_id,
                                   const LatencyInfo& latency) {
  LatencyInfo::LatencyComponent rwh_component;
  if (!latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                           latency_component_id, &rwh_component)) {
    return;
  }
  DCHECK_EQ(rwh_component.event_count, 1u);

  LatencyInfo::LatencyComponent ui_component;
  if (latency.FindLatency(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0,
                          &ui_component)) {
    DCHECK_EQ(ui_component.event_count, 1u);
    base::TimeDelta ui_delta =
        rwh_component.event_time - ui_component.event_time;
    switch (type) {
      case blink::WebInputEvent::MouseWheel:
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Event.Latency.Browser.WheelUI",
            ui_delta.InMicroseconds(), 1, 20000, 100);
        break;
      case blink::WebInputEvent::TouchTypeFirst:
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Event.Latency.Browser.TouchUI",
            ui_delta.InMicroseconds(), 1, 20000, 100);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  LatencyInfo::LatencyComponent acked_component;
  if (latency.FindLatency(ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0,
                          &acked_component)) {
    DCHECK_EQ(acked_component.event_count, 1u);
    base::TimeDelta acked_delta =
        acked_component.event_time - rwh_component.event_time;
    switch (type) {
      case blink::WebInputEvent::MouseWheel:
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Event.Latency.Browser.WheelAcked",
            acked_delta.InMicroseconds(), 1, 1000000, 100);
        break;
      case blink::WebInputEvent::TouchTypeFirst:
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Event.Latency.Browser.TouchAcked",
            acked_delta.InMicroseconds(), 1, 1000000, 100);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

// LatencyComponents generated in the renderer must have component IDs
// provided to them by the browser process. This function adds the correct
// component ID where necessary.
void AddLatencyInfoComponentIds(LatencyInfo* latency,
                                int64 latency_component_id) {
  LatencyInfo::LatencyMap new_components;
  auto lc = latency->latency_components.begin();
  while (lc != latency->latency_components.end()) {
    ui::LatencyComponentType component_type = lc->first.first;
    if (component_type == ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT ||
        component_type == ui::WINDOW_OLD_SNAPSHOT_FRAME_NUMBER_COMPONENT) {
      // Generate a new component entry with the correct component ID
      auto key = std::make_pair(component_type, latency_component_id);
      new_components[key] = lc->second;

      // Remove the old entry
      latency->latency_components.erase(lc++);
    } else {
      ++lc;
    }
  }

  // Add newly generated components into the latency info
  for (lc = new_components.begin(); lc != new_components.end(); ++lc) {
    latency->latency_components[lc->first] = lc->second;
  }
}

}  // namespace

RenderWidgetHostLatencyTracker::RenderWidgetHostLatencyTracker()
    : last_event_id_(0), latency_component_id_(0), device_scale_factor_(1) {
}

RenderWidgetHostLatencyTracker::~RenderWidgetHostLatencyTracker() {
}

void RenderWidgetHostLatencyTracker::Initialize(int routing_id,
                                                int process_id) {
  DCHECK_EQ(0, last_event_id_);
  DCHECK_EQ(0, latency_component_id_);
  last_event_id_ = static_cast<int64>(process_id) << 32;
  latency_component_id_ = routing_id | last_event_id_;
}

void RenderWidgetHostLatencyTracker::OnInputEvent(
    const blink::WebInputEvent& event,
    LatencyInfo* latency) {
  DCHECK(latency);
  if (latency->FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                           latency_component_id_, NULL)) {
    return;
  }

  latency->AddLatencyNumber(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                            latency_component_id_, ++last_event_id_);
  latency->TraceEventType(WebInputEventTraits::GetName(event.type));
  UpdateLatencyCoordinates(event, device_scale_factor_, latency);

  if (event.type == blink::WebInputEvent::GestureScrollUpdate) {
    latency->AddLatencyNumber(
        ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_RWH_COMPONENT,
        latency_component_id_, ++last_event_id_);

    // Make a copy of the INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT with a
    // different name INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT.
    // So we can track the latency specifically for scroll update events.
    LatencyInfo::LatencyComponent original_component;
    if (latency->FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0,
                             &original_component)) {
      latency->AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          latency_component_id_, original_component.sequence_number,
          original_component.event_time, original_component.event_count);
    }
  }
}

void RenderWidgetHostLatencyTracker::OnInputEventAck(
    const blink::WebInputEvent& event,
    LatencyInfo* latency) {
  DCHECK(latency);

  // Latency ends when it is acked but does not cause render scheduling.
  bool rendering_scheduled = latency->FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_COMPONENT, 0, NULL);

  if (WebInputEvent::isGestureEventType(event.type)) {
    if (!rendering_scheduled) {
      latency->AddLatencyNumber(
          ui::INPUT_EVENT_LATENCY_TERMINATED_GESTURE_COMPONENT, 0, 0);
      // TODO(jdduke): Consider exposing histograms for gesture event types.
    }
    return;
  }

  if (WebInputEvent::isTouchEventType(event.type)) {
    latency->AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0);
    if (!rendering_scheduled) {
      latency->AddLatencyNumber(
          ui::INPUT_EVENT_LATENCY_TERMINATED_TOUCH_COMPONENT, 0, 0);
    }
    ComputeInputLatencyHistograms(WebInputEvent::TouchTypeFirst,
                                  latency_component_id_, *latency);
    return;
  }

  if (event.type == WebInputEvent::MouseWheel) {
    latency->AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0);
    if (!rendering_scheduled) {
      latency->AddLatencyNumber(
          ui::INPUT_EVENT_LATENCY_TERMINATED_MOUSE_COMPONENT, 0, 0);
    }
    ComputeInputLatencyHistograms(WebInputEvent::MouseWheel,
                                  latency_component_id_, *latency);
    return;
  }

  // TODO(jdduke): Determine if mouse and keyboard events are worth hooking
  // into LatencyInfo.
}

void RenderWidgetHostLatencyTracker::OnSwapCompositorFrame(
    std::vector<LatencyInfo>* latencies) {
  DCHECK(latencies);
  for (LatencyInfo& latency : *latencies)
    AddLatencyInfoComponentIds(&latency, latency_component_id_);
}

void RenderWidgetHostLatencyTracker::OnFrameSwapped(
    const ui::LatencyInfo& latency) {
  LatencyInfo::LatencyComponent swap_component;
  if (!latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0,
          &swap_component)) {
    return;
  }

  LatencyInfo::LatencyComponent tab_switch_component;
  if (latency.FindLatency(ui::TAB_SHOW_COMPONENT, latency_component_id_,
                          &tab_switch_component)) {
    base::TimeDelta delta =
        swap_component.event_time - tab_switch_component.event_time;
    for (size_t i = 0; i < tab_switch_component.event_count; i++) {
      UMA_HISTOGRAM_TIMES("MPArch.RWH_TabSwitchPaintDuration", delta);
    }
  }

  LatencyInfo::LatencyComponent rwh_component;
  if (!latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                           latency_component_id_, &rwh_component)) {
    return;
  }

  LatencyInfo::LatencyComponent original_component;
  if (latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          latency_component_id_, &original_component)) {
    // This UMA metric tracks the time from when the original touch event is
    // created (averaged if there are multiple) to when the scroll gesture
    // results in final frame swap.
    for (size_t i = 0; i < original_component.event_count; i++) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Latency.TouchToScrollUpdateSwap",
          (swap_component.event_time - original_component.event_time)
              .InMicroseconds(),
          1,
          1000000,
          100);
    }
  }
}

}  // namespace content
