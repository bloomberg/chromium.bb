// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/input.pb.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/input_message_converter.h"
#include "blimp/net/input_message_generator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebGestureDevice.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace blimp {
namespace {

void ValidateWebInputEventRoundTripping(const blink::WebInputEvent& event) {
  InputMessageGenerator generator;
  InputMessageConverter processor;

  scoped_ptr<BlimpMessage> proto = generator.GenerateMessage(event);
  EXPECT_NE(nullptr, proto.get());
  EXPECT_TRUE(proto->has_input());
  EXPECT_EQ(BlimpMessage::INPUT, proto->type());

  scoped_ptr<blink::WebInputEvent> new_event = processor.ProcessMessage(
      proto->input());
  EXPECT_NE(nullptr, new_event.get());

  EXPECT_EQ(event.size, new_event->size);
  EXPECT_EQ(0, memcmp(&event, new_event.get(), event.size));
}

blink::WebGestureEvent BuildBaseTestEvent() {
  blink::WebGestureEvent event;
  event.timeStampSeconds = 1.23;
  event.x = 2;
  event.y = 3;
  event.globalX = 4;
  event.globalY = 5;
  event.sourceDevice = blink::WebGestureDevice::WebGestureDeviceTouchscreen;
  return event;
}

}  // namespace

TEST(InputMessageTest, TestGestureScrollBeginRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GestureScrollBegin;
  event.data.scrollBegin.deltaXHint = 2.34f;
  event.data.scrollBegin.deltaYHint = 3.45f;
  event.data.scrollBegin.targetViewport = true;
  ValidateWebInputEventRoundTripping(event);
}

TEST(InputMessageTest, TestGestureScrollEndRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GestureScrollEnd;
  ValidateWebInputEventRoundTripping(event);
}

TEST(InputMessageTest, TestGestureScrollUpdateRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GestureScrollUpdate;
  event.data.scrollUpdate.deltaX = 2.34f;
  event.data.scrollUpdate.deltaY = 3.45f;
  event.data.scrollUpdate.velocityX = 4.56f;
  event.data.scrollUpdate.velocityY = 5.67f;
  event.data.scrollUpdate.previousUpdateInSequencePrevented = true;
  event.data.scrollUpdate.preventPropagation = true;
  event.data.scrollUpdate.inertial = true;
  ValidateWebInputEventRoundTripping(event);
}

TEST(InputMessageTest, TestGestureFlingStartRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GestureFlingStart;
  event.data.flingStart.velocityX = 2.34f;
  event.data.flingStart.velocityY = 3.45f;
  event.data.flingStart.targetViewport = true;
  ValidateWebInputEventRoundTripping(event);
}

TEST(InputMessageTest, TestGestureFlingCancelRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GestureFlingCancel;
  event.data.flingCancel.preventBoosting = true;
  ValidateWebInputEventRoundTripping(event);
}

TEST(InputMessageTest, TestGestureTapRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebGestureEvent::Type::GestureTap;
  event.data.tap.tapCount = 3;
  event.data.tap.width = 2.34f;
  event.data.tap.height = 3.45f;
  ValidateWebInputEventRoundTripping(event);
}

TEST(InputMessageTest, TestGesturePinchBeginRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GesturePinchBegin;
  ValidateWebInputEventRoundTripping(event);
}

TEST(InputMessageTest, TestGesturePinchEndRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GesturePinchEnd;
  ValidateWebInputEventRoundTripping(event);
}

TEST(InputMessageTest, TestGesturePinchUpdateRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GesturePinchUpdate;
  event.data.pinchUpdate.zoomDisabled = true;
  event.data.pinchUpdate.scale = 2.34f;
  ValidateWebInputEventRoundTripping(event);
}

TEST(InputMessageTest, TestUnsupportedInputEventSerializationFails) {
  // We currently do not support WebMouseWheelEvents.  If that changes update
  // this test.
  blink::WebMouseWheelEvent event;
  event.type = blink::WebInputEvent::Type::MouseWheel;
  InputMessageGenerator generator;
  EXPECT_EQ(nullptr, generator.GenerateMessage(event).get());
}

}  // namespace blimp
