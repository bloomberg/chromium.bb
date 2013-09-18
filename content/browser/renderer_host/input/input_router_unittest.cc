// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_router_unittest.h"

#include "content/browser/renderer_host/input/input_router.h"
#include "content/common/input_messages.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(OS_WIN) || defined(USE_AURA)
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/events/event.h"
#endif

using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebTouchEvent;
using WebKit::WebTouchPoint;

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

void InputRouterTest::SimulateKeyboardEvent(WebInputEvent::Type type) {
  NativeWebKeyboardEvent key_event;
  key_event.type = type;
  key_event.windowsKeyCode = ui::VKEY_L;  // non-null made up value.
  input_router_->SendKeyboardEvent(key_event, ui::LatencyInfo());
  client_->ExpectSendCalled(true);
  EXPECT_EQ(type, client_->sent_key_event().type);
  EXPECT_EQ(key_event.windowsKeyCode,
            client_->sent_key_event().windowsKeyCode);
}

void InputRouterTest::SimulateWheelEvent(float dX,
                                         float dY,
                                         int modifiers,
                                         bool precise) {
  WebMouseWheelEvent wheel_event;
  wheel_event.type = WebInputEvent::MouseWheel;
  wheel_event.deltaX = dX;
  wheel_event.deltaY = dY;
  wheel_event.modifiers = modifiers;
  wheel_event.hasPreciseScrollingDeltas = precise;
  input_router_->SendWheelEvent(
      MouseWheelEventWithLatencyInfo(wheel_event, ui::LatencyInfo()));
  client_->ExpectSendCalled(true);
  EXPECT_EQ(wheel_event.type, client_->sent_wheel_event().event.type);
  EXPECT_EQ(dX, client_->sent_wheel_event().event.deltaX);
}

void InputRouterTest::SimulateMouseMove(int x, int y, int modifiers) {
  WebMouseEvent mouse_event;
  mouse_event.type = WebInputEvent::MouseMove;
  mouse_event.x = mouse_event.windowX = x;
  mouse_event.y = mouse_event.windowY = y;
  mouse_event.modifiers = modifiers;
  input_router_->SendMouseEvent(
      MouseEventWithLatencyInfo(mouse_event, ui::LatencyInfo()));
  client_->ExpectSendCalled(true);
  EXPECT_EQ(mouse_event.type, client_->sent_mouse_event().event.type);
  EXPECT_EQ(x, client_->sent_mouse_event().event.x);
}

void InputRouterTest::SimulateWheelEventWithPhase(
    WebMouseWheelEvent::Phase phase) {
  WebMouseWheelEvent wheel_event;
  wheel_event.type = WebInputEvent::MouseWheel;
  wheel_event.phase = phase;
  input_router_->SendWheelEvent(
      MouseWheelEventWithLatencyInfo(wheel_event, ui::LatencyInfo()));
  client_->ExpectSendCalled(true);
  EXPECT_EQ(wheel_event.type, client_->sent_wheel_event().event.type);
  EXPECT_EQ(phase, client_->sent_wheel_event().event.phase);
}

// Inject provided synthetic WebGestureEvent instance.
void InputRouterTest::SimulateGestureEventCore(WebInputEvent::Type type,
                          WebGestureEvent::SourceDevice sourceDevice,
                          WebGestureEvent* gesture_event) {
  gesture_event->type = type;
  gesture_event->sourceDevice = sourceDevice;
  GestureEventWithLatencyInfo gesture_with_latency(
      *gesture_event, ui::LatencyInfo());
  input_router_->SendGestureEvent(gesture_with_latency);
  client_->ExpectSendCalled(true);
  EXPECT_EQ(type, client_->sent_gesture_event().event.type);
  EXPECT_EQ(sourceDevice, client_->sent_gesture_event().event.sourceDevice);
}

// Inject simple synthetic WebGestureEvent instances.
void InputRouterTest::SimulateGestureEvent(WebInputEvent::Type type,
                          WebGestureEvent::SourceDevice sourceDevice) {
  WebGestureEvent gesture_event;
  SimulateGestureEventCore(type, sourceDevice, &gesture_event);
}

void InputRouterTest::SimulateGestureScrollUpdateEvent(float dX,
                                                       float dY,
                                                       int modifiers) {
  WebGestureEvent gesture_event;
  gesture_event.data.scrollUpdate.deltaX = dX;
  gesture_event.data.scrollUpdate.deltaY = dY;
  gesture_event.modifiers = modifiers;
  SimulateGestureEventCore(WebInputEvent::GestureScrollUpdate,
                           WebGestureEvent::Touchscreen, &gesture_event);
}

void InputRouterTest::SimulateGesturePinchUpdateEvent(float scale,
                                                      float anchorX,
                                                      float anchorY,
                                                      int modifiers) {
  WebGestureEvent gesture_event;
  gesture_event.data.pinchUpdate.scale = scale;
  gesture_event.x = anchorX;
  gesture_event.y = anchorY;
  gesture_event.modifiers = modifiers;
  SimulateGestureEventCore(WebInputEvent::GesturePinchUpdate,
                           WebGestureEvent::Touchscreen, &gesture_event);
}

// Inject synthetic GestureFlingStart events.
void InputRouterTest::SimulateGestureFlingStartEvent(
    float velocityX,
    float velocityY,
    WebGestureEvent::SourceDevice sourceDevice) {
  WebGestureEvent gesture_event;
  gesture_event.data.flingStart.velocityX = velocityX;
  gesture_event.data.flingStart.velocityY = velocityY;
  SimulateGestureEventCore(WebInputEvent::GestureFlingStart, sourceDevice,
                           &gesture_event);
}

void InputRouterTest::SimulateTouchEvent(
    int x,
    int y) {
  PressTouchPoint(x, y);
  SendTouchEvent();
}

// Set the timestamp for the touch-event.
void InputRouterTest::SetTouchTimestamp(base::TimeDelta timestamp) {
  touch_event_.timeStampSeconds = timestamp.InSecondsF();
}

// Sends a touch event (irrespective of whether the page has a touch-event
// handler or not).
void InputRouterTest::SendTouchEvent() {
  input_router_->SendTouchEvent(
      TouchEventWithLatencyInfo(touch_event_, ui::LatencyInfo()));

  // Mark all the points as stationary. And remove the points that have been
  // released.
  int point = 0;
  for (unsigned int i = 0; i < touch_event_.touchesLength; ++i) {
    if (touch_event_.touches[i].state == WebTouchPoint::StateReleased)
      continue;

    touch_event_.touches[point] = touch_event_.touches[i];
    touch_event_.touches[point].state =
        WebTouchPoint::StateStationary;
    ++point;
  }
  touch_event_.touchesLength = point;
  touch_event_.type = WebInputEvent::Undefined;
}

int InputRouterTest::PressTouchPoint(int x, int y) {
  if (touch_event_.touchesLength == touch_event_.touchesLengthCap)
    return -1;
  WebTouchPoint& point =
      touch_event_.touches[touch_event_.touchesLength];
  point.id = touch_event_.touchesLength;
  point.position.x = point.screenPosition.x = x;
  point.position.y = point.screenPosition.y = y;
  point.state = WebTouchPoint::StatePressed;
  point.radiusX = point.radiusY = 1.f;
  ++touch_event_.touchesLength;
  touch_event_.type = WebInputEvent::TouchStart;
  return point.id;
}

void InputRouterTest::MoveTouchPoint(int index, int x, int y) {
  CHECK(index >= 0 && index < touch_event_.touchesLengthCap);
  WebTouchPoint& point = touch_event_.touches[index];
  point.position.x = point.screenPosition.x = x;
  point.position.y = point.screenPosition.y = y;
  touch_event_.touches[index].state = WebTouchPoint::StateMoved;
  touch_event_.type = WebInputEvent::TouchMove;
}

void InputRouterTest::ReleaseTouchPoint(int index) {
  CHECK(index >= 0 && index < touch_event_.touchesLengthCap);
  touch_event_.touches[index].state = WebTouchPoint::StateReleased;
  touch_event_.type = WebInputEvent::TouchEnd;
}

}  // namespace content
