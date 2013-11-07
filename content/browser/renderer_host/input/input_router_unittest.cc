// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_router_unittest.h"

#include "content/browser/renderer_host/input/input_router.h"
#include "content/common/input_messages.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

InputRouterTest::InputRouterTest() {}
InputRouterTest::~InputRouterTest() {}

void InputRouterTest::SetUp() {
  browser_context_.reset(new TestBrowserContext());
  process_.reset(new MockRenderProcessHost(browser_context_.get()));
  client_.reset(new MockInputRouterClient());
  ack_handler_.reset(new MockInputAckHandler());
  input_router_ = CreateInputRouter(process_.get(),
                                    client_.get(),
                                    ack_handler_.get(),
                                    MSG_ROUTING_NONE);
  client_->set_input_router(input_router_.get());
  ack_handler_->set_input_router(input_router_.get());
}

void InputRouterTest::TearDown() {
  // Process all pending tasks to avoid InputRouterTest::leaks.
  base::MessageLoop::current()->RunUntilIdle();

  input_router_.reset();
  client_.reset();
  process_.reset();
  browser_context_.reset();
}

void InputRouterTest::SimulateKeyboardEvent(WebInputEvent::Type type,
                                            bool is_shortcut) {
  input_router_->SendKeyboardEvent(
      SyntheticWebKeyboardEventBuilder::Build(type),
      ui::LatencyInfo(),
      is_shortcut);
}

void InputRouterTest::SimulateWheelEvent(float dX,
                                         float dY,
                                         int modifiers,
                                         bool precise) {
  input_router_->SendWheelEvent(
      MouseWheelEventWithLatencyInfo(
          SyntheticWebMouseWheelEventBuilder::Build(dX, dY, modifiers, precise),
          ui::LatencyInfo()));
}

void InputRouterTest::SimulateMouseMove(int x, int y, int modifiers) {
  input_router_->SendMouseEvent(
      MouseEventWithLatencyInfo(SyntheticWebMouseEventBuilder::Build(
                                    WebInputEvent::MouseMove, x, y, modifiers),
                                ui::LatencyInfo()));
}

void InputRouterTest::SimulateWheelEventWithPhase(
    WebMouseWheelEvent::Phase phase) {
  input_router_->SendWheelEvent(
      MouseWheelEventWithLatencyInfo(
          SyntheticWebMouseWheelEventBuilder::Build(phase), ui::LatencyInfo()));
}

// Inject provided synthetic WebGestureEvent instance.
void InputRouterTest::SimulateGestureEvent(
    const WebGestureEvent& gesture) {
  GestureEventWithLatencyInfo gesture_with_latency(gesture, ui::LatencyInfo());
  input_router_->SendGestureEvent(gesture_with_latency);
}

// Inject simple synthetic WebGestureEvent instances.
void InputRouterTest::SimulateGestureEvent(
    WebInputEvent::Type type,
    WebGestureEvent::SourceDevice sourceDevice) {
  SimulateGestureEvent(
      SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
}

void InputRouterTest::SimulateGestureScrollUpdateEvent(float dX,
                                                       float dY,
                                                       int modifiers) {
  SimulateGestureEvent(
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(dX, dY, modifiers));
}

void InputRouterTest::SimulateGesturePinchUpdateEvent(float scale,
                                                      float anchorX,
                                                      float anchorY,
                                                      int modifiers) {
  SimulateGestureEvent(
      SyntheticWebGestureEventBuilder::BuildPinchUpdate(scale,
                                                        anchorX,
                                                        anchorY,
                                                        modifiers));
}

// Inject synthetic GestureFlingStart events.
void InputRouterTest::SimulateGestureFlingStartEvent(
    float velocityX,
    float velocityY,
    WebGestureEvent::SourceDevice sourceDevice) {
  SimulateGestureEvent(
      SyntheticWebGestureEventBuilder::BuildFling(velocityX,
                                                  velocityY,
                                                  sourceDevice));
}

void InputRouterTest::SimulateTouchEvent(int x, int y) {
  PressTouchPoint(x, y);
  SendTouchEvent();
}

// Set the timestamp for the touch-event.
void InputRouterTest::SetTouchTimestamp(base::TimeDelta timestamp) {
  touch_event_.SetTimestamp(timestamp);
}

// Sends a touch event (irrespective of whether the page has a touch-event
// handler or not).
void InputRouterTest::SendTouchEvent() {
  input_router_->SendTouchEvent(
      TouchEventWithLatencyInfo(touch_event_, ui::LatencyInfo()));
  touch_event_.ResetPoints();
}

int InputRouterTest::PressTouchPoint(int x, int y) {
  return touch_event_.PressPoint(x, y);
}

void InputRouterTest::MoveTouchPoint(int index, int x, int y) {
  touch_event_.MovePoint(index, x, y);
}

void InputRouterTest::ReleaseTouchPoint(int index) {
  touch_event_.ReleasePoint(index);
}

}  // namespace content
