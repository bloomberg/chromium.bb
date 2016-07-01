// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/web_input_event_traits.h"

#include <limits>

#include "content/common/input/event_with_latency_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using std::numeric_limits;

namespace content {
namespace {

// TODO(tapted): Move the Coalesce tests to event_with_latency_info_unittest.cc.
// Merge this test harness into EventWithLatencyInfoTest.
// http://crbug.com/625000.
class WebInputEventTraitsTest : public testing::Test {
 protected:
  static WebTouchPoint CreateTouchPoint(WebTouchPoint::State state, int id) {
    WebTouchPoint touch;
    touch.state = state;
    touch.id = id;
    return touch;
  }

  static TouchEventWithLatencyInfo CreateTouch(WebInputEvent::Type type) {
    return CreateTouch(type, 1);
  }

  static TouchEventWithLatencyInfo CreateTouch(WebInputEvent::Type type,
                                               unsigned touch_count) {
    WebTouchEvent event;
    event.touchesLength = touch_count;
    event.type = type;
    return TouchEventWithLatencyInfo(event);
  }

  static GestureEventWithLatencyInfo CreateGesture(WebInputEvent::Type type,
                                                   float x,
                                                   float y) {
    WebGestureEvent event;
    event.type = type;
    event.x = x;
    event.y = y;
    return GestureEventWithLatencyInfo(event);
  }

  static MouseWheelEventWithLatencyInfo CreateMouseWheel(float deltaX,
                                                         float deltaY) {
    WebMouseWheelEvent event;
    event.type = WebInputEvent::MouseWheel;
    event.deltaX = deltaX;
    event.deltaY = deltaY;
    return MouseWheelEventWithLatencyInfo(event);
  }
};

template <class T>
bool CanCoalesce(const T& event_to_coalesce, const T& event) {
  return event.CanCoalesceWith(event_to_coalesce);
}

template <class T>
void Coalesce(const T& event_to_coalesce, T* event) {
  return event->CoalesceWith(event_to_coalesce);
}

TEST_F(WebInputEventTraitsTest, TouchEventCoalescing) {
  TouchEventWithLatencyInfo touch0 = CreateTouch(WebInputEvent::TouchStart);
  TouchEventWithLatencyInfo touch1 = CreateTouch(WebInputEvent::TouchMove);

  // Non touch-moves won't coalesce.
  EXPECT_FALSE(CanCoalesce(touch0, touch0));

  // Touches of different types won't coalesce.
  EXPECT_FALSE(CanCoalesce(touch0, touch1));

  // Touch moves with idential touch lengths and touch ids should coalesce.
  EXPECT_TRUE(CanCoalesce(touch1, touch1));

  // Touch moves with different touch ids should not coalesce.
  touch0 = CreateTouch(WebInputEvent::TouchMove);
  touch1 = CreateTouch(WebInputEvent::TouchMove);
  touch0.event.touches[0].id = 7;
  EXPECT_FALSE(CanCoalesce(touch0, touch1));
  touch0 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch1 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch0.event.touches[0].id = 1;
  touch1.event.touches[0].id = 0;
  EXPECT_FALSE(CanCoalesce(touch0, touch1));

  // Touch moves with different touch lengths should not coalesce.
  touch0 = CreateTouch(WebInputEvent::TouchMove, 1);
  touch1 = CreateTouch(WebInputEvent::TouchMove, 2);
  EXPECT_FALSE(CanCoalesce(touch0, touch1));

  // Touch moves with identical touch ids in different orders should coalesce.
  touch0 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch1 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch0.event.touches[0] = touch1.event.touches[1] =
      CreateTouchPoint(WebTouchPoint::StateMoved, 1);
  touch0.event.touches[1] = touch1.event.touches[0] =
      CreateTouchPoint(WebTouchPoint::StateMoved, 0);
  EXPECT_TRUE(CanCoalesce(touch0, touch1));

  // Pointers with the same ID's should coalesce.
  touch0 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch1 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch0.event.touches[0] = touch1.event.touches[1] =
      CreateTouchPoint(WebTouchPoint::StateMoved, 1);
  Coalesce(touch0, &touch1);
  ASSERT_EQ(1, touch1.event.touches[0].id);
  ASSERT_EQ(0, touch1.event.touches[1].id);
  EXPECT_EQ(WebTouchPoint::StateUndefined, touch1.event.touches[1].state);
  EXPECT_EQ(WebTouchPoint::StateMoved, touch1.event.touches[0].state);

  // Movement from now-stationary pointers should be preserved.
  touch0 = touch1 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch0.event.touches[0] = CreateTouchPoint(WebTouchPoint::StateMoved, 1);
  touch1.event.touches[1] = CreateTouchPoint(WebTouchPoint::StateStationary, 1);
  touch0.event.touches[1] = CreateTouchPoint(WebTouchPoint::StateStationary, 0);
  touch1.event.touches[0] = CreateTouchPoint(WebTouchPoint::StateMoved, 0);
  Coalesce(touch0, &touch1);
  ASSERT_EQ(1, touch1.event.touches[0].id);
  ASSERT_EQ(0, touch1.event.touches[1].id);
  EXPECT_EQ(WebTouchPoint::StateMoved, touch1.event.touches[0].state);
  EXPECT_EQ(WebTouchPoint::StateMoved, touch1.event.touches[1].state);

  // Touch moves with different dispatchTypes coalesce.
  touch0 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch0.event.dispatchType = WebInputEvent::DispatchType::Blocking;
  touch1 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch1.event.dispatchType = WebInputEvent::DispatchType::EventNonBlocking;
  touch0.event.touches[0] = touch1.event.touches[1] =
      CreateTouchPoint(WebTouchPoint::StateMoved, 1);
  touch0.event.touches[1] = touch1.event.touches[0] =
      CreateTouchPoint(WebTouchPoint::StateMoved, 0);
  EXPECT_TRUE(CanCoalesce(touch0, touch1));
  Coalesce(touch0, &touch1);
  ASSERT_EQ(WebInputEvent::DispatchType::Blocking, touch1.event.dispatchType);

  touch0 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch0.event.dispatchType =
      WebInputEvent::DispatchType::ListenersForcedNonBlockingPassive;
  touch1 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch1.event.dispatchType =
      WebInputEvent::DispatchType::ListenersNonBlockingPassive;
  touch0.event.touches[0] = touch1.event.touches[1] =
      CreateTouchPoint(WebTouchPoint::StateMoved, 1);
  touch0.event.touches[1] = touch1.event.touches[0] =
      CreateTouchPoint(WebTouchPoint::StateMoved, 0);
  EXPECT_TRUE(CanCoalesce(touch0, touch1));
  Coalesce(touch0, &touch1);
  ASSERT_EQ(WebInputEvent::DispatchType::ListenersNonBlockingPassive,
            touch1.event.dispatchType);
}

TEST_F(WebInputEventTraitsTest, PinchEventCoalescing) {
  GestureEventWithLatencyInfo pinch0 =
      CreateGesture(WebInputEvent::GesturePinchBegin, 1, 1);
  GestureEventWithLatencyInfo pinch1 =
      CreateGesture(WebInputEvent::GesturePinchUpdate, 2, 2);

  // Only GesturePinchUpdate's coalesce.
  EXPECT_FALSE(CanCoalesce(pinch0, pinch0));

  // Pinch gestures of different types should not coalesce.
  EXPECT_FALSE(CanCoalesce(pinch0, pinch1));

  // Pinches with different focal points should not coalesce.
  pinch0 = CreateGesture(WebInputEvent::GesturePinchUpdate, 1, 1);
  pinch1 = CreateGesture(WebInputEvent::GesturePinchUpdate, 2, 2);
  EXPECT_FALSE(CanCoalesce(pinch0, pinch1));
  EXPECT_TRUE(CanCoalesce(pinch0, pinch0));

  // Coalesced scales are multiplicative.
  pinch0 = CreateGesture(WebInputEvent::GesturePinchUpdate, 1, 1);
  pinch0.event.data.pinchUpdate.scale = 2.f;
  pinch1 = CreateGesture(WebInputEvent::GesturePinchUpdate, 1, 1);
  pinch1.event.data.pinchUpdate.scale = 3.f;
  EXPECT_TRUE(CanCoalesce(pinch0, pinch0));
  Coalesce(pinch0, &pinch1);
  EXPECT_EQ(2.f * 3.f, pinch1.event.data.pinchUpdate.scale);

  // Scales have a minimum value and can never reach 0.
  ASSERT_GT(numeric_limits<float>::min(), 0);
  pinch0 = CreateGesture(WebInputEvent::GesturePinchUpdate, 1, 1);
  pinch0.event.data.pinchUpdate.scale = numeric_limits<float>::min() * 2.0f;
  pinch1 = CreateGesture(WebInputEvent::GesturePinchUpdate, 1, 1);
  pinch1.event.data.pinchUpdate.scale = numeric_limits<float>::min() * 5.0f;
  EXPECT_TRUE(CanCoalesce(pinch0, pinch1));
  Coalesce(pinch0, &pinch1);
  EXPECT_EQ(numeric_limits<float>::min(), pinch1.event.data.pinchUpdate.scale);

  // Scales have a maximum value and can never reach Infinity.
  pinch0 = CreateGesture(WebInputEvent::GesturePinchUpdate, 1, 1);
  pinch0.event.data.pinchUpdate.scale = numeric_limits<float>::max() / 2.0f;
  pinch1 = CreateGesture(WebInputEvent::GesturePinchUpdate, 1, 1);
  pinch1.event.data.pinchUpdate.scale = 10.0f;
  EXPECT_TRUE(CanCoalesce(pinch0, pinch1));
  Coalesce(pinch0, &pinch1);
  EXPECT_EQ(numeric_limits<float>::max(), pinch1.event.data.pinchUpdate.scale);
}

TEST_F(WebInputEventTraitsTest, WebMouseWheelEventCoalescing) {
  MouseWheelEventWithLatencyInfo mouse_wheel_0 = CreateMouseWheel(1, 1);
  MouseWheelEventWithLatencyInfo mouse_wheel_1 = CreateMouseWheel(2, 2);

  // WebMouseWheelEvent objects with same values except different deltaX and
  // deltaY should coalesce.
  EXPECT_TRUE(CanCoalesce(mouse_wheel_0, mouse_wheel_1));

  // WebMouseWheelEvent objects with different modifiers should not coalesce.
  mouse_wheel_0 = CreateMouseWheel(1, 1);
  mouse_wheel_1 = CreateMouseWheel(1, 1);
  mouse_wheel_0.event.modifiers = blink::WebInputEvent::ControlKey;
  mouse_wheel_1.event.modifiers = blink::WebInputEvent::ShiftKey;
  EXPECT_FALSE(CanCoalesce(mouse_wheel_0, mouse_wheel_1));
}

// Coalescing preserves the newer timestamp.
TEST_F(WebInputEventTraitsTest, TimestampCoalescing) {
  MouseWheelEventWithLatencyInfo mouse_wheel_0 = CreateMouseWheel(1, 1);
  mouse_wheel_0.event.timeStampSeconds = 5.0;
  MouseWheelEventWithLatencyInfo mouse_wheel_1 = CreateMouseWheel(2, 2);
  mouse_wheel_1.event.timeStampSeconds = 10.0;

  EXPECT_TRUE(CanCoalesce(mouse_wheel_0, mouse_wheel_1));
  Coalesce(mouse_wheel_1, &mouse_wheel_0);
  EXPECT_EQ(10.0, mouse_wheel_0.event.timeStampSeconds);
}

// Very basic smoke test to ensure stringification doesn't explode.
TEST_F(WebInputEventTraitsTest, ToString) {
  WebKeyboardEvent key;
  key.type = WebInputEvent::RawKeyDown;
  EXPECT_FALSE(WebInputEventTraits::ToString(key).empty());

  WebMouseEvent mouse;
  mouse.type = WebInputEvent::MouseMove;
  EXPECT_FALSE(WebInputEventTraits::ToString(mouse).empty());

  WebMouseWheelEvent mouse_wheel;
  mouse_wheel.type = WebInputEvent::MouseWheel;
  EXPECT_FALSE(WebInputEventTraits::ToString(mouse_wheel).empty());

  WebGestureEvent gesture;
  gesture.type = WebInputEvent::GesturePinchBegin;
  gesture.x = 1;
  gesture.y = 1;
  EXPECT_FALSE(WebInputEventTraits::ToString(gesture).empty());

  WebTouchEvent touch;
  touch.type = WebInputEvent::TouchStart;
  touch.touchesLength = 1;
  EXPECT_FALSE(WebInputEventTraits::ToString(touch).empty());
}

}  // namespace
}  // namespace content
