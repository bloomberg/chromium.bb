// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/input/synthetic_web_input_event_builders.h"
#include "content/browser/renderer_host/input/touch_event_queue.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

class TouchEventQueueTest : public testing::Test,
                            public TouchEventQueueClient {
 public:
  TouchEventQueueTest()
      : sent_event_count_(0),
        acked_event_count_(0),
        last_acked_event_state_(INPUT_EVENT_ACK_STATE_UNKNOWN) {}

  virtual ~TouchEventQueueTest() {}

  // testing::Test
  virtual void SetUp() OVERRIDE {
    queue_.reset(new TouchEventQueue(this));
  }

  virtual void TearDown() OVERRIDE {
    queue_.reset();
  }

  // TouchEventQueueClient
  virtual void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& event) OVERRIDE {
    ++sent_event_count_;
    last_sent_event_ = event.event;
  }

  virtual void OnTouchEventAck(
      const TouchEventWithLatencyInfo& event,
      InputEventAckState ack_result) OVERRIDE {
    ++acked_event_count_;
    last_acked_event_ = event.event;
    last_acked_event_state_ = ack_result;
    if (followup_touch_event_) {
      scoped_ptr<WebTouchEvent> followup_touch_event =
          followup_touch_event_.Pass();
      SendTouchEvent(*followup_touch_event);
    }
    if (followup_gesture_event_) {
      scoped_ptr<WebGestureEvent> followup_gesture_event =
          followup_gesture_event_.Pass();
      queue_->OnGestureScrollEvent(
          GestureEventWithLatencyInfo(*followup_gesture_event,
                                      ui::LatencyInfo()));
    }
  }

 protected:

  void SendTouchEvent(const WebTouchEvent& event) {
    queue_->QueueEvent(TouchEventWithLatencyInfo(event, ui::LatencyInfo()));
  }

  void SendTouchEvent() {
    SendTouchEvent(touch_event_);
    touch_event_.ResetPoints();
  }

  void SendGestureEvent(WebInputEvent::Type type) {
    WebGestureEvent event;
    event.type = type;
    queue_->OnGestureScrollEvent(
        GestureEventWithLatencyInfo(event, ui::LatencyInfo()));
  }

  void SendTouchEventACK(InputEventAckState ack_result) {
    queue_->ProcessTouchAck(ack_result, ui::LatencyInfo());
  }

  void SetFollowupEvent(const WebTouchEvent& event) {
    followup_touch_event_.reset(new WebTouchEvent(event));
  }

  void SetFollowupEvent(const WebGestureEvent& event) {
    followup_gesture_event_.reset(new WebGestureEvent(event));
  }

  int PressTouchPoint(int x, int y) {
    return touch_event_.PressPoint(x, y);
  }

  void MoveTouchPoint(int index, int x, int y) {
    touch_event_.MovePoint(index, x, y);
  }

  void ReleaseTouchPoint(int index) {
    touch_event_.ReleasePoint(index);
  }

  size_t GetAndResetAckedEventCount() {
    size_t count = acked_event_count_;
    acked_event_count_ = 0;
    return count;
  }

  size_t GetAndResetSentEventCount() {
    size_t count = sent_event_count_;
    sent_event_count_ = 0;
    return count;
  }

  void Flush() {
    queue_->FlushQueue();
  }

  size_t queued_event_count() const {
    return queue_->GetQueueSize();
  }

  const WebTouchEvent& latest_event() const {
    return queue_->GetLatestEvent().event;
  }

  const WebTouchEvent& acked_event() const {
    return last_acked_event_;
  }

  const WebTouchEvent& sent_event() const {
    return last_sent_event_;
  }

  InputEventAckState acked_event_state() const {
    return last_acked_event_state_;
  }

  void set_no_touch_to_renderer(bool no_touch) {
    queue_->no_touch_to_renderer_ = no_touch;
  }

  bool no_touch_to_renderer() const {
    return queue_->no_touch_to_renderer_;
  }

 private:
  scoped_ptr<TouchEventQueue> queue_;
  size_t sent_event_count_;
  size_t acked_event_count_;
  WebTouchEvent last_sent_event_;
  WebTouchEvent last_acked_event_;
  InputEventAckState last_acked_event_state_;
  SyntheticWebTouchEvent touch_event_;
  scoped_ptr<WebTouchEvent> followup_touch_event_;
  scoped_ptr<WebGestureEvent> followup_gesture_event_;
};


// Tests that touch-events are queued properly.
TEST_F(TouchEventQueueTest, Basic) {
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The second touch should not be sent since one is already in queue.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Receive an ACK for the first touch-event.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);

  // Receive an ACK for the second touch-event.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);
}

// Tests that the touch-queue is emptied if a page stops listening for touch
// events.
TEST_F(TouchEventQueueTest, Flush) {
  Flush();
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Send a touch-press event.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  ReleaseTouchPoint(0);
  SendTouchEvent();

  // Events will be queued until the first sent event is ack'ed.
  for (int i = 5; i < 15; ++i) {
    PressTouchPoint(1, 1);
    SendTouchEvent();
    MoveTouchPoint(0, i, i);
    SendTouchEvent();
    ReleaseTouchPoint(0);
    SendTouchEvent();
  }
  EXPECT_EQ(32U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Receive an ACK for the first touch-event. One of the queued touch-event
  // should be forwarded.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(31U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);

  // Flush the queue. The touch-event queue should now be emptied, but none of
  // the queued touch-events should be sent to the renderer.
  Flush();
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(31U, GetAndResetAckedEventCount());
}

// Tests that touch-events are coalesced properly in the queue.
TEST_F(TouchEventQueueTest, Coalesce) {
  // Send a touch-press event.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Send a few touch-move events, followed by a touch-release event. All the
  // touch-move events should be coalesced into a single event.
  for (int i = 5; i < 15; ++i) {
    MoveTouchPoint(0, i, i);
    SendTouchEvent();
  }
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(3U, queued_event_count());

  // ACK the press.  Coalesced touch-move events should be sent.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, acked_event_state());

  // ACK the moves.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(10U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);

  // ACK the release.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchEnd, acked_event().type);
}

// Tests that an event that has already been sent but hasn't been ack'ed yet
// doesn't get coalesced with newer events.
TEST_F(TouchEventQueueTest, SentTouchEventDoesNotCoalesce) {
  // Send a touch-press event.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Send a few touch-move events, followed by a touch-release event. All the
  // touch-move events should be coalesced into a single event.
  for (int i = 5; i < 15; ++i) {
    MoveTouchPoint(0, i, i);
    SendTouchEvent();
  }
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());

  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // The coalesced touch-move event has been sent to the renderer. Any new
  // touch-move event should not be coalesced with the sent event.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  EXPECT_EQ(2U, queued_event_count());

  MoveTouchPoint(0, 7, 7);
  SendTouchEvent();
  EXPECT_EQ(2U, queued_event_count());
}

// Tests that coalescing works correctly for multi-touch events.
TEST_F(TouchEventQueueTest, MultiTouch) {
  // Press the first finger.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Move the finger.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  EXPECT_EQ(2U, queued_event_count());

  // Now press a second finger.
  PressTouchPoint(2, 2);
  SendTouchEvent();
  EXPECT_EQ(3U, queued_event_count());

  // Move both fingers.
  MoveTouchPoint(0, 10, 10);
  MoveTouchPoint(1, 20, 20);
  SendTouchEvent();
  EXPECT_EQ(4U, queued_event_count());

  // Move only one finger now.
  MoveTouchPoint(0, 15, 15);
  SendTouchEvent();
  EXPECT_EQ(4U, queued_event_count());

  // Move the other finger.
  MoveTouchPoint(1, 25, 25);
  SendTouchEvent();
  EXPECT_EQ(4U, queued_event_count());

  // Make sure both fingers are marked as having been moved in the coalesced
  // event.
  const WebTouchEvent& event = latest_event();
  EXPECT_EQ(WebTouchPoint::StateMoved, event.touches[0].state);
  EXPECT_EQ(WebTouchPoint::StateMoved, event.touches[1].state);
}

// Tests that if a touch-event queue is destroyed in response to a touch-event
// in the renderer, then there is no crash when the ACK for that touch-event
// comes back.
TEST_F(TouchEventQueueTest, AckAfterQueueFlushed) {
  // Send some touch-events to the renderer.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  MoveTouchPoint(0, 10, 10);
  SendTouchEvent();
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // Receive an ACK for the press. This should cause the queued touch-move to
  // be sent to the renderer.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  Flush();
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Now receive an ACK for the move.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());
}

// Tests that touch-move events are not sent to the renderer if the preceding
// touch-press event did not have a consumer (and consequently, did not hit the
// main thread in the renderer). Also tests that all queued/coalesced touch
// events are flushed immediately when the ACK for the touch-press comes back
// with NO_CONSUMER status.
TEST_F(TouchEventQueueTest, NoConsumer) {
  // The first touch-press should reach the renderer.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The second touch should not be sent since one is already in queue.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // Receive an ACK for the first touch-event. This should release the queued
  // touch-event, but it should not be sent to the renderer.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);
  EXPECT_EQ(2U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Send a release event. This should not reach the renderer.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(WebInputEvent::TouchEnd, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Send a press-event, followed by move and release events, and another press
  // event, before the ACK for the first press event comes back. All of the
  // events should be queued first. After the NO_CONSUMER ack for the first
  // touch-press, all events upto the second touch-press should be flushed.
  PressTouchPoint(10, 10);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  MoveTouchPoint(0, 6, 5);
  SendTouchEvent();
  ReleaseTouchPoint(0);
  SendTouchEvent();

  PressTouchPoint(6, 5);
  SendTouchEvent();
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  // The queue should hold the first sent touch-press event, the coalesced
  // touch-move event, the touch-end event and the second touch-press event.
  EXPECT_EQ(4U, queued_event_count());

  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(WebInputEvent::TouchEnd, acked_event().type);
  EXPECT_EQ(4U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // ACK the second press event as NO_CONSUMER too.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Send a second press event. Even though the first touch had NO_CONSUMER,
  // this press event should reach the renderer.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());
}

TEST_F(TouchEventQueueTest, ConsumerIgnoreMultiFinger) {
  // Press two touch points and move them around a bit. The renderer consumes
  // the events for the first touch point, but returns NO_CONSUMER_EXISTS for
  // the second touch point.

  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();

  PressTouchPoint(10, 10);
  SendTouchEvent();

  MoveTouchPoint(0, 2, 2);
  SendTouchEvent();

  MoveTouchPoint(1, 4, 10);
  SendTouchEvent();

  MoveTouchPoint(0, 10, 10);
  MoveTouchPoint(1, 20, 20);
  SendTouchEvent();

  // Since the first touch-press is still pending ACK, no other event should
  // have been sent to the renderer.
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  // The queue includes the two presses, the first touch-move of the first
  // point, and a coalesced touch-move of both points.
  EXPECT_EQ(4U, queued_event_count());

  // ACK the first press as CONSUMED. This should cause the first touch-move of
  // the first touch-point to be dispatched.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(3U, queued_event_count());

  // ACK the first move as CONSUMED.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // ACK the second press as NO_CONSUMER_EXISTS. This will dequeue the coalesced
  // touch-move event (which contains both touch points). Although the second
  // touch-point does not need to be sent to the renderer, the first touch-point
  // did move, and so the coalesced touch-event will be sent to the renderer.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // ACK the coalesced move as NOT_CONSUMED.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Move just the second touch point. Because the first touch point did not
  // move, this event should not reach the renderer.
  MoveTouchPoint(1, 30, 30);
  SendTouchEvent();
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Move just the first touch point. This should reach the renderer.
  MoveTouchPoint(0, 10, 10);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Move both fingers. This event should reach the renderer (after the ACK of
  // the previous move event is received), because the first touch point did
  // move.
  MoveTouchPoint(0, 15, 15);
  MoveTouchPoint(1, 25, 25);
  SendTouchEvent();
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Release the first finger. Then move the second finger around some, then
  // press another finger. Once the release event is ACKed, the move events of
  // the second finger should be immediately released to the view, and the
  // touch-press event should be dispatched to the renderer.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  MoveTouchPoint(1, 40, 40);
  SendTouchEvent();

  MoveTouchPoint(1, 50, 50);
  SendTouchEvent();

  PressTouchPoint(1, 1);
  SendTouchEvent();

  MoveTouchPoint(1, 30, 30);
  SendTouchEvent();
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(4U, queued_event_count());

  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);

  // ACK the press with NO_CONSUMED_EXISTS. This should release the queued
  // touch-move events to the view.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);

  ReleaseTouchPoint(2);
  ReleaseTouchPoint(1);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());
}

// Tests that touch-event's enqueued via a touch ack are properly handled.
TEST_F(TouchEventQueueTest, AckWithFollowupEvents) {
  // Queue a touch down.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Create a touch event that will be queued synchronously by a touch ack.
  // Note, this will be triggered by all subsequent touch acks.
  WebTouchEvent followup_event;
  followup_event.type = WebInputEvent::TouchStart;
  followup_event.touchesLength = 1;
  followup_event.touches[0].id = 1;
  followup_event.touches[0].state = WebTouchPoint::StatePressed;
  SetFollowupEvent(followup_event);

  // Receive an ACK for the press. This should cause the followup touch-move to
  // be sent to the renderer.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, acked_event_state());
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);

  // Queue another event.
  PressTouchPoint(1, 1);
  MoveTouchPoint(0, 2, 2);
  SendTouchEvent();
  EXPECT_EQ(2U, queued_event_count());

  // Receive an ACK for the touch-move followup event. This should cause the
  // subsequent touch move event be sent to the renderer.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that followup events triggered by an immediate ack from
// TouchEventQueue::QueueEvent() are properly handled.
TEST_F(TouchEventQueueTest, ImmediateAckWithFollowupEvents) {
  // Create a touch event that will be queued synchronously by a touch ack.
  WebTouchEvent followup_event;
  followup_event.type = WebInputEvent::TouchStart;
  followup_event.touchesLength = 1;
  followup_event.touches[0].id = 1;
  followup_event.touches[0].state = WebTouchPoint::StatePressed;
  SetFollowupEvent(followup_event);

  // Now, enqueue a stationary touch that will not be forwarded.  This should be
  // immediately ack'ed with "NO_CONSUMER_EXISTS".  The followup event should
  // then be enqueued and immediately sent to the renderer.
  WebTouchEvent stationary_event;
  stationary_event.touchesLength = 1;
  stationary_event.type = WebInputEvent::TouchMove;
  stationary_event.touches[0].id = 1;
  stationary_event.touches[0].state = WebTouchPoint::StateStationary;
  SendTouchEvent(stationary_event);

  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);
}

// Tests basic TouchEvent forwarding suppression.
TEST_F(TouchEventQueueTest, NoTouchBasic) {
  // Disable TouchEvent forwarding.
  set_no_touch_to_renderer(true);
  MoveTouchPoint(0, 30, 5);
  SendTouchEvent();
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove should not be sent to renderer.
  MoveTouchPoint(0, 65, 10);
  SendTouchEvent();
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchEnd should not be sent to renderer.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchStart should not be sent to renderer.
  PressTouchPoint(5, 5);
  SendTouchEvent();
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Enable TouchEvent forwarding.
  set_no_touch_to_renderer(false);

  PressTouchPoint(80, 10);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 80, 20);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that no TouchEvents are sent to renderer during scrolling.
TEST_F(TouchEventQueueTest, NoTouchOnScroll) {
  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 20, 5);
  SendTouchEvent();
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Queue another TouchStart.
  PressTouchPoint(20, 20);
  SendTouchEvent();
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, latest_event().type);

  // GestureScrollBegin inserts a synthetic TouchCancel before the TouchStart.
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  ASSERT_FALSE(no_touch_to_renderer());
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(no_touch_to_renderer());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(WebInputEvent::TouchCancel, sent_event().type);
  EXPECT_EQ(WebInputEvent::TouchStart, latest_event().type);

  // Acking the TouchCancel will result in dispatch of the next TouchStart.
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  // The synthetic TouchCancel should not reach client, only the TouchStart.
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);

  // TouchMove should not be sent to renderer.
  MoveTouchPoint(0, 30, 5);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());

  // GestureScrollUpdates should not change affect touch forwarding.
  SendGestureEvent(WebInputEvent::GestureScrollUpdate);
  EXPECT_TRUE(no_touch_to_renderer());

  // TouchEnd should not be sent to renderer.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());

  // GestureScrollEnd will resume the sending of TouchEvents to renderer.
  SendGestureEvent(blink::WebInputEvent::GestureScrollEnd);
  EXPECT_FALSE(no_touch_to_renderer());

  // Now TouchEvents should be forwarded normally.
  PressTouchPoint(80, 10);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 80, 20);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventACK(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

}  // namespace content
