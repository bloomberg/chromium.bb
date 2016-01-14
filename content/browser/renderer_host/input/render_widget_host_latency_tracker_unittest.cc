// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/render_widget_host_latency_tracker.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::WebInputEvent;

namespace content {
namespace {

const int kTestRoutingId = 3;
const int kTestProcessId = 1;

}  // namespace

TEST(RenderWidgetHostLatencyTrackerTest, Basic) {
  RenderWidgetHostLatencyTracker tracker;
  tracker.Initialize(kTestRoutingId, kTestProcessId);

  {
    auto scroll = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
        5.f, -5.f, 0, blink::WebGestureDeviceTouchscreen);
    scroll.timeStampSeconds =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
    ui::LatencyInfo scroll_latency;
    tracker.OnInputEvent(scroll, &scroll_latency);
    EXPECT_TRUE(
        scroll_latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                   tracker.latency_component_id(), nullptr));
    EXPECT_TRUE(
        scroll_latency.FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                                   0, nullptr));
    EXPECT_EQ(1U, scroll_latency.input_coordinates_size());
  }

  {
    auto wheel = SyntheticWebMouseWheelEventBuilder::Build(
        blink::WebMouseWheelEvent::PhaseChanged);
    wheel.timeStampSeconds =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
    ui::LatencyInfo wheel_latency;
    tracker.OnInputEvent(wheel, &wheel_latency);
    EXPECT_TRUE(
        wheel_latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                  tracker.latency_component_id(), nullptr));
    EXPECT_TRUE(
        wheel_latency.FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                                   0, nullptr));
    EXPECT_EQ(1U, wheel_latency.input_coordinates_size());
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
    EXPECT_TRUE(
        touch_latency.FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                                   0, nullptr));
    EXPECT_EQ(2U, touch_latency.input_coordinates_size());
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
    EXPECT_TRUE(scroll_latency.terminated());
  }

  {
    auto wheel = SyntheticWebMouseWheelEventBuilder::Build(
        blink::WebMouseWheelEvent::PhaseChanged);
    ui::LatencyInfo wheel_latency;
    tracker.OnInputEvent(wheel, &wheel_latency);
    tracker.OnInputEventAck(wheel, &wheel_latency);
    EXPECT_TRUE(wheel_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_MOUSE_WHEEL_COMPONENT, 0, nullptr));
    EXPECT_TRUE(wheel_latency.terminated());
  }

  {
    SyntheticWebTouchEvent touch;
    touch.PressPoint(0, 0);
    ui::LatencyInfo touch_latency;
    tracker.OnInputEvent(touch, &touch_latency);
    tracker.OnInputEventAck(touch, &touch_latency);
    EXPECT_TRUE(touch_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_TOUCH_COMPONENT, 0, nullptr));
    EXPECT_TRUE(touch_latency.terminated());
  }

  {
    auto mouse_move = SyntheticWebMouseEventBuilder::Build(
        blink::WebMouseEvent::MouseMove);
    ui::LatencyInfo mouse_latency;
    tracker.OnInputEvent(mouse_move, &mouse_latency);
    tracker.OnInputEventAck(mouse_move, &mouse_latency);
    EXPECT_TRUE(mouse_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_MOUSE_COMPONENT, 0, nullptr));
    EXPECT_TRUE(mouse_latency.terminated());
  }

  {
    auto key_event = SyntheticWebKeyboardEventBuilder::Build(
        blink::WebKeyboardEvent::Char);
    ui::LatencyInfo key_latency;
    tracker.OnInputEvent(key_event, &key_latency);
    tracker.OnInputEventAck(key_event, &key_latency);
    EXPECT_TRUE(key_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_KEYBOARD_COMPONENT, 0, nullptr));
    EXPECT_TRUE(key_latency.terminated());
  }
}

TEST(RenderWidgetHostLatencyTrackerTest, InputCoordinatesPopulated) {
  RenderWidgetHostLatencyTracker tracker;
  tracker.Initialize(kTestRoutingId, kTestProcessId);

  {
    auto event =
        SyntheticWebMouseWheelEventBuilder::Build(0, 0, -5, 0, 0, true);
    event.x = 100;
    event.y = 200;
    ui::LatencyInfo latency_info;
    tracker.OnInputEvent(event, &latency_info);
    EXPECT_EQ(1u, latency_info.input_coordinates_size());
    EXPECT_EQ(100, latency_info.input_coordinates()[0].x);
    EXPECT_EQ(200, latency_info.input_coordinates()[0].y);
  }

  {
    auto event = SyntheticWebMouseEventBuilder::Build(WebInputEvent::MouseMove);
    event.x = 300;
    event.y = 400;
    ui::LatencyInfo latency_info;
    tracker.OnInputEvent(event, &latency_info);
    EXPECT_EQ(1u, latency_info.input_coordinates_size());
    EXPECT_EQ(300, latency_info.input_coordinates()[0].x);
    EXPECT_EQ(400, latency_info.input_coordinates()[0].y);
  }

  {
    auto event = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::GestureScrollBegin, blink::WebGestureDeviceTouchscreen);
    event.x = 500;
    event.y = 600;
    ui::LatencyInfo latency_info;
    tracker.OnInputEvent(event, &latency_info);
    EXPECT_EQ(1u, latency_info.input_coordinates_size());
    EXPECT_EQ(500, latency_info.input_coordinates()[0].x);
    EXPECT_EQ(600, latency_info.input_coordinates()[0].y);
  }

  {
    SyntheticWebTouchEvent event;
    event.PressPoint(700, 800);
    event.PressPoint(900, 1000);
    event.PressPoint(1100, 1200);  // LatencyInfo only holds two coordinates.
    ui::LatencyInfo latency_info;
    tracker.OnInputEvent(event, &latency_info);
    EXPECT_EQ(2u, latency_info.input_coordinates_size());
    EXPECT_EQ(700, latency_info.input_coordinates()[0].x);
    EXPECT_EQ(800, latency_info.input_coordinates()[0].y);
    EXPECT_EQ(900, latency_info.input_coordinates()[1].x);
    EXPECT_EQ(1000, latency_info.input_coordinates()[1].y);
  }

  {
    NativeWebKeyboardEvent event;
    event.type = blink::WebKeyboardEvent::KeyDown;
    ui::LatencyInfo latency_info;
    tracker.OnInputEvent(event, &latency_info);
    EXPECT_EQ(0u, latency_info.input_coordinates_size());
  }
}

TEST(RenderWidgetHostLatencyTrackerTest, ScrollLatency) {
  RenderWidgetHostLatencyTracker tracker;
  tracker.Initialize(kTestRoutingId, kTestProcessId);

  auto scroll_begin = SyntheticWebGestureEventBuilder::BuildScrollBegin(5, -5);
  ui::LatencyInfo scroll_latency;
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0,
                                  0);
  tracker.OnInputEvent(scroll_begin, &scroll_latency);
  EXPECT_TRUE(
      scroll_latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                 tracker.latency_component_id(), nullptr));
  EXPECT_EQ(2U, scroll_latency.latency_components().size());

  // The first GestureScrollUpdate should be provided with
  // INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT.
  auto first_scroll_update = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
      5.f, -5.f, 0, blink::WebGestureDeviceTouchscreen);
  scroll_latency = ui::LatencyInfo();
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0,
                                  0);
  tracker.OnInputEvent(first_scroll_update, &scroll_latency);
  EXPECT_TRUE(
      scroll_latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                 tracker.latency_component_id(), nullptr));
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      tracker.latency_component_id(), nullptr));
  EXPECT_FALSE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      tracker.latency_component_id(), nullptr));
  EXPECT_EQ(3U, scroll_latency.latency_components().size());

  // Subseqeunt GestureScrollUpdates should be provided with
  // INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT.
  auto scroll_update = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
      -5.f, 5.f, 0, blink::WebGestureDeviceTouchscreen);
  scroll_latency = ui::LatencyInfo();
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0,
                                  0);
  tracker.OnInputEvent(scroll_update, &scroll_latency);
  EXPECT_TRUE(
      scroll_latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                 tracker.latency_component_id(), nullptr));
  EXPECT_FALSE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      tracker.latency_component_id(), nullptr));
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      tracker.latency_component_id(), nullptr));
  EXPECT_EQ(3U, scroll_latency.latency_components().size());
}

}  // namespace content
