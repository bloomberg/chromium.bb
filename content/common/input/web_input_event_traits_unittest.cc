// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/web_input_event_traits.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {
namespace {

class WebInputEventTraitsTest : public testing::Test {
 protected:
  static WebTouchPoint Create(int id, WebTouchPoint::State state) {
    WebTouchPoint touch;
    touch.id = id;
    touch.state = state;
    return touch;
  }

  static WebTouchEvent Create(WebInputEvent::Type type) {
    return Create(type, 1);
  }

  static WebTouchEvent Create(WebInputEvent::Type type, unsigned touch_count) {
    WebTouchEvent event;
    event.touchesLength = touch_count;
    event.type = type;
    return event;
  }
};

TEST_F(WebInputEventTraitsTest, TouchEventCoalescing) {
  WebTouchEvent touch0 = Create(WebInputEvent::TouchStart);
  WebTouchEvent touch1 = Create(WebInputEvent::TouchMove);

  // Non touch-moves won't coalesce.
  EXPECT_FALSE(WebInputEventTraits::CanCoalesce(touch0, touch0));

  // Touches of different types won't coalesce.
  EXPECT_FALSE(WebInputEventTraits::CanCoalesce(touch0, touch1));

  // Touch moves with idential touch lengths and touch ids should coalesce.
  EXPECT_TRUE(WebInputEventTraits::CanCoalesce(touch1, touch1));

  // Touch moves with different touch ids should not coalesce.
  touch0 = Create(WebInputEvent::TouchMove);
  touch1 = Create(WebInputEvent::TouchMove);
  touch0.touches[0].id = 7;
  EXPECT_FALSE(WebInputEventTraits::CanCoalesce(touch0, touch1));
  touch0 = Create(WebInputEvent::TouchMove, 2);
  touch1 = Create(WebInputEvent::TouchMove, 2);
  touch0.touches[0].id = 1;
  touch1.touches[0].id = 0;
  EXPECT_FALSE(WebInputEventTraits::CanCoalesce(touch0, touch1));

  // Touch moves with different touch lengths should not coalesce.
  touch0 = Create(WebInputEvent::TouchMove, 1);
  touch1 = Create(WebInputEvent::TouchMove, 2);
  EXPECT_FALSE(WebInputEventTraits::CanCoalesce(touch0, touch1));

  // Touch moves with identical touch ids in different orders should coalesce.
  touch0 = Create(WebInputEvent::TouchMove, 2);
  touch1 = Create(WebInputEvent::TouchMove, 2);
  touch0.touches[0] = touch1.touches[1] = Create(1, WebTouchPoint::StateMoved);
  touch0.touches[1] = touch1.touches[0] = Create(0, WebTouchPoint::StateMoved);
  EXPECT_TRUE(WebInputEventTraits::CanCoalesce(touch0, touch1));

  // Pointers with the same ID's should coalesce.
  touch0 = Create(WebInputEvent::TouchMove, 2);
  touch1 = Create(WebInputEvent::TouchMove, 2);
  touch0.touches[0] = touch1.touches[1] = Create(1, WebTouchPoint::StateMoved);
  WebInputEventTraits::Coalesce(touch0, &touch1);
  ASSERT_EQ(1, touch1.touches[0].id);
  ASSERT_EQ(0, touch1.touches[1].id);
  EXPECT_EQ(WebTouchPoint::StateUndefined, touch1.touches[1].state);
  EXPECT_EQ(WebTouchPoint::StateMoved, touch1.touches[0].state);

  // Movement from now-stationary pointers should be preserved.
  touch0 = touch1 = Create(WebInputEvent::TouchMove, 2);
  touch0.touches[0] = Create(1, WebTouchPoint::StateMoved);
  touch1.touches[1] = Create(1, WebTouchPoint::StateStationary);
  touch0.touches[1] = Create(0, WebTouchPoint::StateStationary);
  touch1.touches[0] = Create(0, WebTouchPoint::StateMoved);
  WebInputEventTraits::Coalesce(touch0, &touch1);
  ASSERT_EQ(1, touch1.touches[0].id);
  ASSERT_EQ(0, touch1.touches[1].id);
  EXPECT_EQ(WebTouchPoint::StateMoved, touch1.touches[0].state);
  EXPECT_EQ(WebTouchPoint::StateMoved, touch1.touches[1].state);
}

}  // namespace
}  // namespace content
