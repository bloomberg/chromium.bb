// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/input/touch_disposition_gesture_filter.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input/web_input_event_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

class TouchDispositionGestureFilterTest
    : public testing::Test,
      public TouchDispositionGestureFilterClient {
 public:
  TouchDispositionGestureFilterTest() : sent_gesture_count_(0) {}

  virtual ~TouchDispositionGestureFilterTest() {}

  // testing::Test
  virtual void SetUp() OVERRIDE {
    queue_.reset(new TouchDispositionGestureFilter(this));
  }

  virtual void TearDown() OVERRIDE {
    queue_.reset();
  }

  // TouchDispositionGestureFilterClient
  virtual void ForwardGestureEvent(const WebGestureEvent& event) OVERRIDE {
    ++sent_gesture_count_;
    sent_gestures_.push_back(event.type);
  }

 protected:
  typedef std::vector<WebInputEvent::Type> GestureList;

  ::testing::AssertionResult GesturesMatch(const GestureList& expected,
                                           const GestureList& actual) {
    if (expected.size() != actual.size()) {
      return ::testing::AssertionFailure()
          << "actual.size(" << actual.size()
          << ") != expected.size(" << expected.size() << ")";
    }

    for (size_t i = 0; i < expected.size(); ++i) {
      if (expected[i] != actual[i]) {
        return ::testing::AssertionFailure()
            << "actual[" << i << "] ("
            << WebInputEventTraits::GetName(actual[i])
            << ") != expected[" << i << "] ("
            << WebInputEventTraits::GetName(expected[i]) << ")";
      }
    }

    return ::testing::AssertionSuccess();
  }

  GestureList Gestures(WebInputEvent::Type type) {
    return GestureList(1, type);
  }

  GestureList Gestures(WebInputEvent::Type type0, WebInputEvent::Type type1) {
    GestureList gestures(2);
    gestures[0] = type0;
    gestures[1] = type1;
    return gestures;
  }

  GestureList Gestures(WebInputEvent::Type type0,
                       WebInputEvent::Type type1,
                       WebInputEvent::Type type2) {
    GestureList gestures(3);
    gestures[0] = type0;
    gestures[1] = type1;
    gestures[2] = type2;
    return gestures;
  }

  GestureList Gestures(WebInputEvent::Type type0,
                       WebInputEvent::Type type1,
                       WebInputEvent::Type type2,
                       WebInputEvent::Type type3) {
    GestureList gestures(4);
    gestures[0] = type0;
    gestures[1] = type1;
    gestures[2] = type2;
    gestures[3] = type3;
    return gestures;
  }

  void SendTouchGestures() {
    GestureEventPacket gesture_packet;
    std::swap(gesture_packet, pending_gesture_packet_);
    EXPECT_EQ(TouchDispositionGestureFilter::SUCCESS,
              SendTouchGestures(touch_event_, gesture_packet));
    touch_event_.ResetPoints();
  }

  TouchDispositionGestureFilter::PacketResult
  SendTouchGestures(const WebTouchEvent& touch,
                    const GestureEventPacket& packet) {
    GestureEventPacket touch_packet = GestureEventPacket::FromTouch(touch);
    for (size_t i = 0; i < packet.gesture_count(); ++i)
      touch_packet.Push(packet.gesture(i));
    return queue_->OnGestureEventPacket(touch_packet);
  }

  TouchDispositionGestureFilter::PacketResult
  SendTimeoutGesture(WebInputEvent::Type type) {
    return queue_->OnGestureEventPacket(
        GestureEventPacket::FromTouchTimeout(CreateGesture(type)));
  }

  TouchDispositionGestureFilter::PacketResult
  SendGesturePacket(const GestureEventPacket& packet) {
    return queue_->OnGestureEventPacket(packet);
  }

  void SendTouchEventACK(InputEventAckState ack_result) {
    queue_->OnTouchEventAck(ack_result);
  }

  void PushGesture(WebInputEvent::Type type) {
    pending_gesture_packet_.Push(CreateGesture(type));
  }

  void PressTouchPoint(int x, int y) {
    touch_event_.PressPoint(x, y);
    SendTouchGestures();
  }

  void MoveTouchPoint(int index, int x, int y) {
    touch_event_.MovePoint(index, x, y);
    SendTouchGestures();
  }

  void ReleaseTouchPoint(int index) {
    touch_event_.ReleasePoint(index);
    SendTouchGestures();
  }

  void CancelTouchPoint(int index) {
    touch_event_.CancelPoint(index);
    SendTouchGestures();
  }

  bool GesturesSent() const {
    return !sent_gestures_.empty();
  }

  bool IsEmpty() const {
    return queue_->IsEmpty();
  }

  GestureList GetAndResetSentGestures() {
    GestureList sent_gestures;
    sent_gestures.swap(sent_gestures_);
    return sent_gestures;
  }

  static WebGestureEvent CreateGesture(WebInputEvent::Type type) {
    return SyntheticWebGestureEventBuilder::Build(
        type, WebGestureEvent::Touchscreen);
  }

 private:
  scoped_ptr<TouchDispositionGestureFilter> queue_;
  SyntheticWebTouchEvent touch_event_;
  GestureEventPacket pending_gesture_packet_;
  size_t sent_gesture_count_;
  GestureList sent_gestures_;
};

TEST_F(TouchDispositionGestureFilterTest, BasicNoGestures) {
  PressTouchPoint(1, 1);
  EXPECT_FALSE(GesturesSent());

  MoveTouchPoint(0, 2, 2);
  EXPECT_FALSE(GesturesSent());

  // No gestures should be dispatched by the ack, as the queued packets
  // contained no gestures.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  // Release the touch gesture.
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, BasicGestures) {
  // An unconsumed touch's gesture should be sent.
  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  EXPECT_FALSE(GesturesSent());
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollBegin),
                            GetAndResetSentGestures()));

  // Multiple gestures can be queued for a single event.
  PushGesture(WebInputEvent::GestureFlingStart);
  PushGesture(WebInputEvent::GestureFlingCancel);
  ReleaseTouchPoint(0);
  EXPECT_FALSE(GesturesSent());
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureFlingStart,
                                     WebInputEvent::GestureFlingCancel),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, BasicGesturesConsumed) {
  // A consumed touch's gesture should not be sent.
  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(WebInputEvent::GestureFlingStart);
  PushGesture(WebInputEvent::GestureFlingCancel);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, ConsumedThenNotConsumed) {
  // A consumed touch's gesture should not be sent.
  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  // Even if the subsequent touch is not consumed, continue dropping gestures.
  PushGesture(WebInputEvent::GestureScrollUpdate);
  MoveTouchPoint(0, 2, 2);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  // Even if the subsequent touch had no consumer, continue dropping gestures.
  PushGesture(WebInputEvent::GestureFlingStart);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, NotConsumedThenConsumed) {
  // A not consumed touch's gesture should be sent.
  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollBegin),
                            GetAndResetSentGestures()));

  // A newly consumed gesture should not be sent.
  PushGesture(WebInputEvent::GesturePinchBegin);
  PressTouchPoint(10, 10);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  // And subsequent non-consumed pinch updates should not be sent.
  PushGesture(WebInputEvent::GestureScrollUpdate);
  PushGesture(WebInputEvent::GesturePinchUpdate);
  MoveTouchPoint(0, 2, 2);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollUpdate),
                            GetAndResetSentGestures()));

  // End events dispatched only when their start events were.
  PushGesture(WebInputEvent::GesturePinchEnd);
  ReleaseTouchPoint(1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(WebInputEvent::GestureScrollEnd);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollEnd),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, ScrollAlternatelyConsumed) {
  // A consumed touch's gesture should not be sent.
  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollBegin),
                            GetAndResetSentGestures()));

  for (size_t i = 0; i < 3; ++i) {
    PushGesture(WebInputEvent::GestureScrollUpdate);
    MoveTouchPoint(0, 2, 2);
    SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
    EXPECT_FALSE(GesturesSent());

    PushGesture(WebInputEvent::GestureScrollUpdate);
    MoveTouchPoint(0, 3, 3);
    SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollUpdate),
                              GetAndResetSentGestures()));
  }

  PushGesture(WebInputEvent::GestureScrollEnd);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollEnd),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, NotConsumedThenNoConsumer) {
  // An unconsumed touch's gesture should be sent.
  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollBegin),
                            GetAndResetSentGestures()));

  // If the subsequent touch has no consumer (e.g., a secondary pointer is
  // pressed but not on a touch handling rect), send the gesture.
  PushGesture(WebInputEvent::GesturePinchBegin);
  PressTouchPoint(2, 2);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GesturePinchBegin),
                            GetAndResetSentGestures()));

  // End events should be dispatched when their start events were, independent
  // of the ack state.
  PushGesture(WebInputEvent::GesturePinchEnd);
  ReleaseTouchPoint(1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GesturePinchEnd),
                            GetAndResetSentGestures()));

  PushGesture(WebInputEvent::GestureScrollEnd);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollEnd),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, EndingEventsSent) {
  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollBegin),
                            GetAndResetSentGestures()));

  PushGesture(WebInputEvent::GesturePinchBegin);
  PressTouchPoint(2, 2);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GesturePinchBegin),
                            GetAndResetSentGestures()));

  // Consuming the touchend event can't suppress the match end gesture.
  PushGesture(WebInputEvent::GesturePinchEnd);
  ReleaseTouchPoint(1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GesturePinchEnd),
                            GetAndResetSentGestures()));

  // But other events in the same packet are still suppressed.
  PushGesture(WebInputEvent::GestureScrollUpdate);
  PushGesture(WebInputEvent::GestureScrollEnd);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollEnd),
                            GetAndResetSentGestures()));

  // GestureScrollEnd and GestureFlingStart behave the same in this regard.
  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollBegin),
                            GetAndResetSentGestures()));

  PushGesture(WebInputEvent::GestureFlingStart);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureFlingStart),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, EndingEventsNotSent) {
  // Consuming a begin event ensures no end events are sent.
  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(WebInputEvent::GesturePinchBegin);
  PressTouchPoint(2, 2);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(WebInputEvent::GesturePinchEnd);
  ReleaseTouchPoint(1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(WebInputEvent::GestureScrollEnd);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, UpdateEventsSuppressedPerEvent) {
  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollBegin),
                            GetAndResetSentGestures()));

  // Consuming a single scroll or pinch update should suppress only that event.
  PushGesture(WebInputEvent::GestureScrollUpdate);
  MoveTouchPoint(0, 2, 2);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(WebInputEvent::GesturePinchBegin);
  PressTouchPoint(2, 2);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GesturePinchBegin),
                            GetAndResetSentGestures()));

  PushGesture(WebInputEvent::GesturePinchUpdate);
  MoveTouchPoint(1, 2, 3);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  // Subsequent updates should not be affected.
  PushGesture(WebInputEvent::GestureScrollUpdate);
  MoveTouchPoint(0, 4, 4);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollUpdate),
                            GetAndResetSentGestures()));

  PushGesture(WebInputEvent::GesturePinchUpdate);
  MoveTouchPoint(0, 4, 5);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GesturePinchUpdate),
                            GetAndResetSentGestures()));

  PushGesture(WebInputEvent::GesturePinchEnd);
  ReleaseTouchPoint(1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GesturePinchEnd),
                            GetAndResetSentGestures()));

  PushGesture(WebInputEvent::GestureScrollEnd);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollEnd),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, UpdateEventsDependOnBeginEvents) {
  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  // Scroll and pinch gestures depend on the scroll begin gesture being
  // dispatched.
  PushGesture(WebInputEvent::GestureScrollUpdate);
  MoveTouchPoint(0, 2, 2);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(WebInputEvent::GesturePinchBegin);
  PressTouchPoint(2, 2);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(WebInputEvent::GesturePinchUpdate);
  MoveTouchPoint(1, 2, 3);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(WebInputEvent::GesturePinchEnd);
  ReleaseTouchPoint(1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(WebInputEvent::GestureScrollEnd);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, MultipleTouchSequences) {
  // Queue two touch-to-gestures sequences.
  PushGesture(WebInputEvent::GestureTapDown);
  PressTouchPoint(1, 1);
  PushGesture(WebInputEvent::GestureTap);
  ReleaseTouchPoint(0);
  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  PushGesture(WebInputEvent::GestureScrollEnd);
  ReleaseTouchPoint(0);

  // The first gesture sequence should not be allowed.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  // The subsequent sequence should "reset" allowance.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollBegin,
                                     WebInputEvent::GestureScrollEnd),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, FlingCancelledOnNewTouchSequence) {
  // Simulate a fling.
  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollBegin),
                            GetAndResetSentGestures()));
  PushGesture(WebInputEvent::GestureFlingStart);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureFlingStart),
                            GetAndResetSentGestures()));

  // A new touch seqeuence should cancel the outstanding fling.
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureFlingCancel),
                            GetAndResetSentGestures()));
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, FlingCancelledOnScrollBegin) {
  // Simulate a fling sequence.
  PushGesture(WebInputEvent::GestureScrollBegin);
  PushGesture(WebInputEvent::GestureFlingStart);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollBegin,
                                     WebInputEvent::GestureFlingStart),
                            GetAndResetSentGestures()));

  // The new fling should cancel the preceding one.
  PushGesture(WebInputEvent::GestureScrollBegin);
  PushGesture(WebInputEvent::GestureFlingStart);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureFlingCancel,
                                     WebInputEvent::GestureScrollBegin,
                                     WebInputEvent::GestureFlingStart),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, FlingNotCancelledIfGFCEventReceived) {
  // Simulate a fling that is started then cancelled.
  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  PushGesture(WebInputEvent::GestureFlingStart);
  MoveTouchPoint(0, 1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  PushGesture(WebInputEvent::GestureFlingCancel);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollBegin,
                                     WebInputEvent::GestureFlingStart,
                                     WebInputEvent::GestureFlingCancel),
                            GetAndResetSentGestures()));

  // A new touch sequence will not inject a GestureFlingCancel, as the fling
  // has already been cancelled.
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelledWhenScrollBegins) {
  PushGesture(WebInputEvent::GestureTapDown);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureTapDown),
                            GetAndResetSentGestures()));

  // If the subsequent touch turns into a scroll, the tap should be cancelled.
  PushGesture(WebInputEvent::GestureScrollBegin);
  MoveTouchPoint(0, 2, 2);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureTapCancel,
                                     WebInputEvent::GestureScrollBegin),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelledWhenTouchConsumed) {
  PushGesture(WebInputEvent::GestureTapDown);
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureTapDown),
                            GetAndResetSentGestures()));

  // If the subsequent touch is consumed, the tap should be cancelled.
  PushGesture(WebInputEvent::GestureScrollBegin);
  MoveTouchPoint(0, 2, 2);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureTapCancel),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest,
       TapNotCancelledIfTapEndingEventReceived) {
  PushGesture(WebInputEvent::GestureTapDown);
  PressTouchPoint(1, 1);
  PressTouchPoint(2, 2);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureTapDown),
                            GetAndResetSentGestures()));

  PushGesture(WebInputEvent::GestureTap);
  ReleaseTouchPoint(1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureTap),
                            GetAndResetSentGestures()));

  // The tap should not be cancelled as it was terminated by a |GestureTap|.
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, TimeoutGestures) {
  // If the sequence is allowed, and there are no preceding gestures, the
  // timeout gestures should be forwarded immediately.
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  SendTimeoutGesture(WebInputEvent::GestureShowPress);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureShowPress),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(WebInputEvent::GestureLongPress);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureLongPress),
                            GetAndResetSentGestures()));

  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // If the sequence is disallowed, and there are no preceding gestures, the
  // timeout gestures should be dropped immediately.
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  SendTimeoutGesture(WebInputEvent::GestureShowPress);
  EXPECT_FALSE(GesturesSent());
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // If the sequence has a pending ack, the timeout gestures should
  // remain queued until the ack is received.
  PressTouchPoint(1, 1);
  EXPECT_FALSE(GesturesSent());

  SendTimeoutGesture(WebInputEvent::GestureLongPress);
  EXPECT_FALSE(GesturesSent());

  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureLongPress),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, SpuriousAcksIgnored) {
  // Acks received when the queue is empty will be safely ignored.
  ASSERT_TRUE(IsEmpty());
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());

  PushGesture(WebInputEvent::GestureScrollBegin);
  PressTouchPoint(1, 1);
  PushGesture(WebInputEvent::GestureScrollUpdate);
  MoveTouchPoint(0, 3,3);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureScrollBegin,
                                     WebInputEvent::GestureScrollUpdate),
                            GetAndResetSentGestures()));

  // Even if all packets have been dispatched, the filter may not be empty as
  // there could be follow-up timeout events.  Spurious acks in such cases
  // should also be safely ignored.
  ASSERT_FALSE(IsEmpty());
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, PacketWithInvalidTypeIgnored) {
  GestureEventPacket packet;
  EXPECT_EQ(TouchDispositionGestureFilter::INVALID_PACKET_TYPE,
            SendGesturePacket(packet));
  EXPECT_TRUE(IsEmpty());
}

TEST_F(TouchDispositionGestureFilterTest, PacketsWithInvalidOrderIgnored) {
  EXPECT_EQ(TouchDispositionGestureFilter::INVALID_PACKET_ORDER,
            SendTimeoutGesture(WebInputEvent::GestureShowPress));
  EXPECT_TRUE(IsEmpty());

  WebTouchEvent touch;
  touch.type = WebInputEvent::TouchCancel;
  EXPECT_EQ(TouchDispositionGestureFilter::INVALID_PACKET_TYPE,
            SendTouchGestures(WebInputEvent::GestureShowPress,
                              GestureEventPacket()));
  EXPECT_TRUE(IsEmpty());
}

TEST_F(TouchDispositionGestureFilterTest, ConsumedTouchCancel) {
  // An unconsumed touch's gesture should be sent.
  PushGesture(WebInputEvent::GestureTapDown);
  PressTouchPoint(1, 1);
  EXPECT_FALSE(GesturesSent());
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureTapDown),
                            GetAndResetSentGestures()));

  PushGesture(WebInputEvent::GestureTapCancel);
  PushGesture(WebInputEvent::GestureScrollEnd);
  CancelTouchPoint(0);
  EXPECT_FALSE(GesturesSent());
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureTapCancel,
                                     WebInputEvent::GestureScrollEnd),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, TimeoutEventAfterRelease) {
  PressTouchPoint(1, 1);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(GesturesSent());
  PushGesture(WebInputEvent::GestureTapUnconfirmed);
  ReleaseTouchPoint(0);
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureTapUnconfirmed),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(WebInputEvent::GestureTap);
  EXPECT_TRUE(GesturesMatch(Gestures(WebInputEvent::GestureTap),
                            GetAndResetSentGestures()));
}

}  // namespace content
