// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mouse_wheel_event_queue.h"

#include <stddef.h>
#include <utility>

#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseWheelEvent;

namespace content {
namespace {

const int64_t kScrollEndTimeoutMs = 100;

base::TimeDelta DefaultScrollEndTimeoutDelay() {
  return base::TimeDelta::FromMilliseconds(kScrollEndTimeoutMs);
}

}  // namespace

class MouseWheelEventQueueTest : public testing::Test,
                                 public MouseWheelEventQueueClient {
 public:
  MouseWheelEventQueueTest()
      : acked_event_count_(0),
        last_acked_event_state_(INPUT_EVENT_ACK_STATE_UNKNOWN) {
    SetUpForGestureTesting(false);
  }

  ~MouseWheelEventQueueTest() override {}

  // MouseWheelEventQueueClient
  void SendMouseWheelEventImmediately(
      const MouseWheelEventWithLatencyInfo& event) override {
    sent_events_.push_back(event.event);
  }

  void SendGestureEvent(const GestureEventWithLatencyInfo& event) override {
    sent_events_.push_back(event.event);
  }

  void OnMouseWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                            InputEventAckState ack_result) override {
    ++acked_event_count_;
    last_acked_event_ = event.event;
    last_acked_event_state_ = ack_result;
  }

 protected:
  void SetUpForGestureTesting(bool send_gestures) {
    queue_.reset(
        new MouseWheelEventQueue(this, send_gestures, kScrollEndTimeoutMs));
  }

  size_t queued_event_count() const { return queue_->queued_size(); }

  bool event_in_flight() const { return queue_->event_in_flight(); }

  std::vector<WebInputEvent>& all_sent_events() { return sent_events_; }

  const WebMouseWheelEvent& acked_event() const { return last_acked_event_; }

  size_t GetAndResetSentEventCount() {
    size_t count = sent_events_.size();
    sent_events_.clear();
    return count;
  }

  size_t GetAndResetAckedEventCount() {
    size_t count = acked_event_count_;
    acked_event_count_ = 0;
    return count;
  }

  void SendMouseWheelEventAck(InputEventAckState ack_result) {
    queue_->ProcessMouseWheelAck(ack_result, ui::LatencyInfo());
  }

  void SendMouseWheel(float x, float y, float dX, float dY, int modifiers) {
    queue_->QueueEvent(MouseWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(x, y, dX, dY, modifiers,
                                                  false)));
  }

  void SendGestureEvent(WebInputEvent::Type type) {
    WebGestureEvent event;
    event.type = type;
    event.sourceDevice = blink::WebGestureDeviceTouchscreen;
    queue_->OnGestureScrollEvent(
        GestureEventWithLatencyInfo(event, ui::LatencyInfo()));
  }

  static void RunTasksAndWait(base::TimeDelta delay) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(), delay);
    base::MessageLoop::current()->Run();
  }

  scoped_ptr<MouseWheelEventQueue> queue_;
  std::vector<WebInputEvent> sent_events_;
  size_t acked_event_count_;
  InputEventAckState last_acked_event_state_;
  base::MessageLoopForUI message_loop_;
  WebMouseWheelEvent last_acked_event_;
};

// Tests that mouse wheel events are queued properly.
TEST_F(MouseWheelEventQueueTest, Basic) {
  SendMouseWheel(10, 10, 1, 1, 0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The second mouse wheel should not be sent since one is already in queue.
  SendMouseWheel(10, 10, 5, 5, 0);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Receive an ACK for the first mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::MouseWheel, acked_event().type);

  // Receive an ACK for the second mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::MouseWheel, acked_event().type);
}

TEST_F(MouseWheelEventQueueTest, GestureSending) {
  SetUpForGestureTesting(true);
  SendMouseWheel(10, 10, 1, 1, 0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The second mouse wheel should not be sent since one is already in queue.
  SendMouseWheel(10, 10, 5, 5, 0);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Receive an ACK for the mouse wheel event and release the next
  // mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(WebInputEvent::MouseWheel, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(3U, all_sent_events().size());
  EXPECT_EQ(WebInputEvent::GestureScrollBegin, all_sent_events()[0].type);
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, all_sent_events()[1].type);
  EXPECT_EQ(WebInputEvent::MouseWheel, all_sent_events()[2].type);
  EXPECT_EQ(3U, GetAndResetSentEventCount());

  RunTasksAndWait(DefaultScrollEndTimeoutDelay() * 2);
  EXPECT_EQ(1U, all_sent_events().size());
  EXPECT_EQ(WebInputEvent::GestureScrollEnd, all_sent_events()[0].type);
}

TEST_F(MouseWheelEventQueueTest, GestureSendingInterrupted) {
  SetUpForGestureTesting(true);
  SendMouseWheel(10, 10, 1, 1, 0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive an ACK for the mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::MouseWheel, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_EQ(WebInputEvent::GestureScrollBegin, all_sent_events()[0].type);
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, all_sent_events()[1].type);
  EXPECT_EQ(2U, GetAndResetSentEventCount());

  // Ensure that a gesture scroll begin terminates the current scroll event.
  SendGestureEvent(WebInputEvent::GestureScrollBegin);
  EXPECT_EQ(1U, all_sent_events().size());
  EXPECT_EQ(WebInputEvent::GestureScrollEnd, all_sent_events()[0].type);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  SendMouseWheel(10, 10, 1, 1, 0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // New mouse wheel events won't cause gestures because a scroll
  // is already in progress by another device.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::MouseWheel, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, all_sent_events().size());

  SendGestureEvent(WebInputEvent::GestureScrollEnd);
  EXPECT_EQ(0U, all_sent_events().size());

  SendMouseWheel(10, 10, 1, 1, 0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive an ACK for the mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::MouseWheel, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_EQ(WebInputEvent::GestureScrollBegin, all_sent_events()[0].type);
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, all_sent_events()[1].type);
  EXPECT_EQ(2U, GetAndResetSentEventCount());
}

}  // namespace content
