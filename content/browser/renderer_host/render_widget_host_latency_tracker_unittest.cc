// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_latency_tracker.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const int kTestRoutingId = 3;
const int kTestProcessId = 1;

}  // namespace

TEST(RenderWidgetHostLatencyTrackerTest, Basic) {
  RenderWidgetHostLatencyTracker tracker;
  tracker.Initialize(kTestRoutingId, kTestProcessId);

  {
    auto scroll =
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(5.f, -5.f, 0);
    ui::LatencyInfo scroll_latency;
    tracker.OnInputEvent(scroll, &scroll_latency);
    EXPECT_TRUE(
        scroll_latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                   tracker.latency_component_id(), nullptr));
    EXPECT_TRUE(scroll_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_RWH_COMPONENT,
        tracker.latency_component_id(), nullptr));
    EXPECT_EQ(1U, scroll_latency.input_coordinates_size);
  }

  {
    auto wheel = SyntheticWebMouseWheelEventBuilder::Build(
        blink::WebMouseWheelEvent::PhaseChanged);
    ui::LatencyInfo wheel_latency;
    tracker.OnInputEvent(wheel, &wheel_latency);
    EXPECT_TRUE(
        wheel_latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                  tracker.latency_component_id(), nullptr));
    EXPECT_EQ(1U, wheel_latency.input_coordinates_size);
  }

  {
    SyntheticWebTouchEvent touch;
    touch.PressPoint(0, 0);
    touch.PressPoint(1, 1);
    ui::LatencyInfo touch_latency;
    tracker.OnInputEvent(touch, &touch_latency);
    EXPECT_TRUE(
        touch_latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                  tracker.latency_component_id(), nullptr));
    EXPECT_EQ(2U, touch_latency.input_coordinates_size);
  }
}

TEST(RenderWidgetHostLatencyTrackerTest,
     LatencyTerminatedOnAckIfRenderingNotScheduled) {
  RenderWidgetHostLatencyTracker tracker;
  tracker.Initialize(kTestRoutingId, kTestProcessId);

  {
    auto scroll = SyntheticWebGestureEventBuilder::BuildScrollBegin(5.f, -5.f);
    ui::LatencyInfo scroll_latency;
    tracker.OnInputEvent(scroll, &scroll_latency);
    tracker.OnInputEventAck(scroll, &scroll_latency);
    EXPECT_TRUE(scroll_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_GESTURE_COMPONENT, 0, nullptr));
    EXPECT_TRUE(scroll_latency.terminated);
  }

  {
    auto wheel = SyntheticWebMouseWheelEventBuilder::Build(
        blink::WebMouseWheelEvent::PhaseChanged);
    ui::LatencyInfo wheel_latency;
    tracker.OnInputEvent(wheel, &wheel_latency);
    tracker.OnInputEventAck(wheel, &wheel_latency);
    EXPECT_TRUE(wheel_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_MOUSE_COMPONENT, 0, nullptr));
    EXPECT_TRUE(wheel_latency.terminated);
  }

  {
    SyntheticWebTouchEvent touch;
    touch.PressPoint(0, 0);
    ui::LatencyInfo touch_latency;
    tracker.OnInputEvent(touch, &touch_latency);
    tracker.OnInputEventAck(touch, &touch_latency);
    EXPECT_TRUE(touch_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_TOUCH_COMPONENT, 0, nullptr));
    EXPECT_TRUE(touch_latency.terminated);
  }
}

}  // namespace content
