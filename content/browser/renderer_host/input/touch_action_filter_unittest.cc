// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_action_filter.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/port/browser/event_with_latency_info.h"
#include "content/port/common/input_event_ack_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace content {

TEST(TouchActionFilterTest, SimpleFilter) {
  TouchActionFilter filter;

  const WebGestureEvent scroll_begin = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureScrollBegin, WebGestureEvent::Touchscreen);
  const WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(0, 10, 0);
  const WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureScrollEnd, WebGestureEvent::Touchscreen);
  const WebGestureEvent tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTap, WebGestureEvent::Touchscreen);

  // No events filtered by default.
  EXPECT_FALSE(filter.FilterGestureEvent(scroll_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(scroll_update));
  EXPECT_FALSE(filter.FilterGestureEvent(scroll_end));
  EXPECT_FALSE(filter.FilterGestureEvent(tap));

  // TOUCH_ACTION_AUTO doesn't cause any filtering.
  filter.OnSetTouchAction(TOUCH_ACTION_AUTO);
  EXPECT_FALSE(filter.FilterGestureEvent(scroll_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(scroll_update));
  EXPECT_FALSE(filter.FilterGestureEvent(scroll_end));

  // TOUCH_ACTION_NONE filters out all scroll events, but no other events.
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_FALSE(filter.FilterGestureEvent(tap));
  EXPECT_TRUE(filter.FilterGestureEvent(scroll_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(scroll_update));
  EXPECT_TRUE(filter.FilterGestureEvent(scroll_update));
  EXPECT_TRUE(filter.FilterGestureEvent(scroll_end));

  // After the end of a gesture the state is reset.
  EXPECT_FALSE(filter.FilterGestureEvent(scroll_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(scroll_update));
  EXPECT_FALSE(filter.FilterGestureEvent(scroll_end));

  // Setting touch action doesn't impact any in-progress gestures.
  EXPECT_FALSE(filter.FilterGestureEvent(scroll_begin));
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_FALSE(filter.FilterGestureEvent(scroll_update));
  EXPECT_FALSE(filter.FilterGestureEvent(scroll_end));

  // And the state is still cleared for the next gesture.
  EXPECT_FALSE(filter.FilterGestureEvent(scroll_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(scroll_end));

  // Changing the touch action during a gesture has no effect.
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_TRUE(filter.FilterGestureEvent(scroll_begin));
  filter.OnSetTouchAction(TOUCH_ACTION_AUTO);
  EXPECT_TRUE(filter.FilterGestureEvent(scroll_update));
  EXPECT_TRUE(filter.FilterGestureEvent(scroll_update));
  EXPECT_TRUE(filter.FilterGestureEvent(scroll_end));
}

TEST(TouchActionFilterTest, MultiTouch) {
  TouchActionFilter filter;

  const WebGestureEvent scroll_begin = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureScrollBegin, WebGestureEvent::Touchscreen);
  const WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(0, 10, 0);
  const WebGestureEvent scrollEnd = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureScrollEnd, WebGestureEvent::Touchscreen);

  // For multiple points, the intersection is what matters.
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  filter.OnSetTouchAction(TOUCH_ACTION_AUTO);
  EXPECT_TRUE(filter.FilterGestureEvent(scroll_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(scroll_update));
  EXPECT_TRUE(filter.FilterGestureEvent(scroll_update));
  EXPECT_TRUE(filter.FilterGestureEvent(scrollEnd));

  filter.OnSetTouchAction(TOUCH_ACTION_AUTO);
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  filter.OnSetTouchAction(TOUCH_ACTION_AUTO);
  EXPECT_TRUE(filter.FilterGestureEvent(scroll_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(scroll_update));
  EXPECT_TRUE(filter.FilterGestureEvent(scroll_update));
  EXPECT_TRUE(filter.FilterGestureEvent(scrollEnd));
}

}  // namespace content
