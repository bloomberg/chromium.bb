// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/input/browser_input_event.h"
#include "content/browser/renderer_host/input/input_queue.h"
#include "content/browser/renderer_host/input/input_queue_client.h"
#include "content/common/input/event_packet.h"
#include "content/common/input/input_event.h"
#include "content/common/input/ipc_input_event_payload.h"
#include "content/common/input/web_input_event_payload.h"
#include "content/common/input_messages.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/latency_info.h"

namespace content {
namespace {

using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebTouchEvent;

class InputQueueTest : public testing::Test,
                       public InputQueueClient,
                       public BrowserInputEventClient {
 public:
  InputQueueTest()
      : queue_(new InputQueue(this)),
        routing_id_(0),
        num_flush_completions_(0),
        num_flush_requests_(0),
        num_packet_deliveries_(0),
        next_input_id_(1) {}

  // InputQueueClient
  virtual void Deliver(const EventPacket& packet) OVERRIDE {
    EXPECT_LT(0u, packet.size());
    ++num_packet_deliveries_;
    current_packet_id_ = packet.id();
    current_packet_dispositions_.resize(packet.size(), INPUT_EVENT_UNHANDLED);
  }

  virtual void DidFinishFlush() OVERRIDE { ++num_flush_completions_; }
  virtual void SetNeedsFlush() OVERRIDE { ++num_flush_requests_; }

  // BrowserInputEventClient
  virtual void OnDispatched(const BrowserInputEvent& event,
                            InputEventDisposition disposition) OVERRIDE {
    acked_dispositions_.push_back(disposition);
  }

  virtual void OnDispatched(
      const BrowserInputEvent& event,
      InputEventDisposition disposition,
      ScopedVector<BrowserInputEvent>* followup) OVERRIDE {
    acked_followup_dispositions_.push_back(disposition);
    if (event_to_inject_)
      followup->push_back(event_to_inject_.release());
  }

  int num_flush_requests() const { return num_flush_requests_; }
  int num_flush_completions() const { return num_flush_completions_; }
  int num_packet_deliveries() const { return num_packet_deliveries_; }

 protected:
  scoped_ptr<BrowserInputEvent> CreateIPCInputEvent(IPC::Message* message) {
    return BrowserInputEvent::Create(
        NextInputID(),
        IPCInputEventPayload::Create(make_scoped_ptr(message)),
        this);
  }

  scoped_ptr<BrowserInputEvent> CreateWebInputEvent(
      WebInputEvent::Type web_type) {
    WebKit::WebMouseEvent mouse;
    WebKit::WebMouseWheelEvent wheel;
    WebKit::WebTouchEvent touch;
    WebKit::WebGestureEvent gesture;
    WebKit::WebKeyboardEvent keyboard;

    WebKit::WebInputEvent* web_event = NULL;
    if (WebInputEvent::isMouseEventType(web_type))
      web_event = &mouse;
    else if (WebInputEvent::isKeyboardEventType(web_type))
      web_event = &keyboard;
    else if (WebInputEvent::isTouchEventType(web_type))
      web_event = &touch;
    else if (WebInputEvent::isGestureEventType(web_type))
      web_event = &gesture;
    else
      web_event = &wheel;
    web_event->type = web_type;

    return BrowserInputEvent::Create(
        NextInputID(),
        WebInputEventPayload::Create(*web_event, ui::LatencyInfo(), false),
        this);
  }

  void QueueEvent(IPC::Message* message) {
    queue_->QueueEvent(CreateIPCInputEvent(message));
  }

  void QueueEvent(WebInputEvent::Type web_type) {
    queue_->QueueEvent(CreateWebInputEvent(web_type));
  }

  bool Flush(InputEventDisposition disposition) {
    StartFlush();
    return FinishFlush(disposition);
  }

  void StartFlush() {
    acked_dispositions_.clear();
    acked_followup_dispositions_.clear();
    current_packet_id_ = 0;
    current_packet_dispositions_.clear();
    queue_->BeginFlush();
  }

  bool FinishFlush(InputEventDisposition disposition) {
    if (!current_packet_id_)
      return false;
    current_packet_dispositions_ = InputEventDispositions(
        current_packet_dispositions_.size(), disposition);
    return InputQueue::ACK_OK ==
           queue_->OnEventPacketAck(current_packet_id_,
                                    current_packet_dispositions_);
  }

  int64 NextInputID() { return next_input_id_++; }

  scoped_ptr<InputQueue> queue_;

  int routing_id_;
  int64 current_packet_id_;
  InputEventDispositions current_packet_dispositions_;

  InputEventDispositions acked_dispositions_;
  InputEventDispositions acked_followup_dispositions_;
  scoped_ptr<BrowserInputEvent> event_to_inject_;

  int num_flush_completions_;
  int num_flush_requests_;
  int num_packet_deliveries_;
  int next_input_id_;
};

TEST_F(InputQueueTest, SetNeedsFlushOnQueueEvent) {
  EXPECT_EQ(0, num_flush_requests());

  QueueEvent(WebInputEvent::MouseDown);
  EXPECT_EQ(1, num_flush_requests());

  // Additional queued events should not trigger additional flush requests.
  QueueEvent(WebInputEvent::MouseUp);
  EXPECT_EQ(1, num_flush_requests());
  QueueEvent(WebInputEvent::TouchStart);
  EXPECT_EQ(1, num_flush_requests());
}

TEST_F(InputQueueTest, NoSetNeedsFlushOnQueueIfFlushing) {
  QueueEvent(WebInputEvent::GestureScrollBegin);
  EXPECT_EQ(1, num_flush_requests());

  StartFlush();
  EXPECT_EQ(1, num_flush_requests());
  EXPECT_EQ(1, num_packet_deliveries());

  // Events queued after a flush will not trigger an additional flush request.
  QueueEvent(WebInputEvent::GestureScrollBegin);
  EXPECT_EQ(1, num_flush_requests());
  QueueEvent(WebInputEvent::GestureScrollEnd);
  EXPECT_EQ(1, num_flush_requests());
}

TEST_F(InputQueueTest, SetNeedsFlushAfterDidFinishFlushIfEventsQueued) {
  QueueEvent(WebInputEvent::GestureScrollBegin);
  EXPECT_EQ(1, num_flush_requests());

  StartFlush();
  EXPECT_EQ(1, num_packet_deliveries());

  QueueEvent(WebInputEvent::GestureScrollBegin);
  EXPECT_EQ(1, num_flush_requests());

  // An additional flush request is sent for the event queued after the flush.
  ASSERT_TRUE(current_packet_id_);
  ASSERT_TRUE(FinishFlush(INPUT_EVENT_IMPL_THREAD_CONSUMED));
  EXPECT_EQ(1, num_flush_completions());
  EXPECT_EQ(2, num_flush_requests());
}

TEST_F(InputQueueTest, EventPacketSentAfterFlush) {
  EXPECT_EQ(0, num_packet_deliveries());
  QueueEvent(WebInputEvent::GestureScrollBegin);
  EXPECT_EQ(0, num_packet_deliveries());
  StartFlush();
  EXPECT_EQ(1, num_packet_deliveries());
}

TEST_F(InputQueueTest, AcksHandledInProperOrder) {
  QueueEvent(WebInputEvent::GestureScrollBegin);
  QueueEvent(WebInputEvent::GestureScrollEnd);
  QueueEvent(WebInputEvent::GestureFlingStart);

  queue_->BeginFlush();
  ASSERT_EQ(3u, current_packet_dispositions_.size());
  current_packet_dispositions_[0] = INPUT_EVENT_IMPL_THREAD_CONSUMED;
  current_packet_dispositions_[1] = INPUT_EVENT_MAIN_THREAD_CONSUMED;
  current_packet_dispositions_[2] = INPUT_EVENT_MAIN_THREAD_NO_CONSUMER_EXISTS;
  queue_->OnEventPacketAck(current_packet_id_, current_packet_dispositions_);
  EXPECT_EQ(1, num_flush_completions());

  ASSERT_EQ(3u, acked_dispositions_.size());
  EXPECT_EQ(acked_dispositions_[0], INPUT_EVENT_IMPL_THREAD_CONSUMED);
  EXPECT_EQ(acked_dispositions_[1], INPUT_EVENT_MAIN_THREAD_CONSUMED);
  EXPECT_EQ(acked_dispositions_[2], INPUT_EVENT_MAIN_THREAD_NO_CONSUMER_EXISTS);
}

TEST_F(InputQueueTest, FollowupWhenFollowupEventNotConsumed) {
  InputEventDisposition unconsumed_dispositions[] = {
    INPUT_EVENT_IMPL_THREAD_NO_CONSUMER_EXISTS,
    INPUT_EVENT_MAIN_THREAD_NOT_PREVENT_DEFAULTED,
    INPUT_EVENT_MAIN_THREAD_NO_CONSUMER_EXISTS
  };
  for (size_t i = 0; i < arraysize(unconsumed_dispositions); ++i) {
    QueueEvent(WebInputEvent::GestureScrollBegin);
    QueueEvent(WebInputEvent::TouchStart);
    QueueEvent(WebInputEvent::TouchMove);

    Flush(unconsumed_dispositions[i]);
    EXPECT_EQ(1u, acked_dispositions_.size()) << i;
    EXPECT_EQ(2u, acked_followup_dispositions_.size()) << i;
  }
}

TEST_F(InputQueueTest, NoFollowupWhenFollowupEventConsumed) {
  InputEventDisposition consumed_dispositions[] = {
    INPUT_EVENT_IMPL_THREAD_CONSUMED,
    INPUT_EVENT_MAIN_THREAD_PREVENT_DEFAULTED,
    INPUT_EVENT_MAIN_THREAD_CONSUMED
  };
  for (size_t i = 0; i < arraysize(consumed_dispositions); ++i) {
    QueueEvent(WebInputEvent::GestureScrollBegin);
    QueueEvent(WebInputEvent::TouchStart);
    QueueEvent(WebInputEvent::TouchMove);

    Flush(consumed_dispositions[i]);
    EXPECT_EQ(3u, acked_dispositions_.size()) << i;
    EXPECT_EQ(0u, acked_followup_dispositions_.size()) << i;
  }
}

TEST_F(InputQueueTest, FlushOnEmptyQueueIgnored) {
  Flush(INPUT_EVENT_MAIN_THREAD_CONSUMED);
  EXPECT_EQ(0, num_flush_requests());
  EXPECT_EQ(0, num_flush_completions());

  QueueEvent(WebInputEvent::GestureScrollBegin);
  Flush(INPUT_EVENT_MAIN_THREAD_CONSUMED);
  EXPECT_EQ(1, num_flush_requests());
  EXPECT_EQ(1, num_flush_completions());

  Flush(INPUT_EVENT_MAIN_THREAD_CONSUMED);
  EXPECT_EQ(1, num_flush_requests());
  EXPECT_EQ(1, num_flush_completions());
}

TEST_F(InputQueueTest, FlushContinuesUntilAllEventsProcessed) {
  QueueEvent(WebInputEvent::GestureScrollBegin);
  QueueEvent(WebInputEvent::GestureScrollEnd);
  QueueEvent(WebInputEvent::GestureFlingStart);

  EXPECT_EQ(1, num_flush_requests());
  Flush(INPUT_EVENT_COULD_NOT_DELIVER);
  EXPECT_EQ(0, num_flush_completions());

  FinishFlush(INPUT_EVENT_COULD_NOT_DELIVER);
  EXPECT_EQ(0, num_flush_completions());

  ASSERT_TRUE(FinishFlush(INPUT_EVENT_MAIN_THREAD_CONSUMED));
  EXPECT_EQ(1, num_flush_completions());
}

TEST_F(InputQueueTest, InvalidPacketAckIgnored) {
  // Packet never flushed, any ack should be ignored.
  InputQueue::AckResult result =
      queue_->OnEventPacketAck(0, InputEventDispositions());
  EXPECT_EQ(InputQueue::ACK_UNEXPECTED, result);

  QueueEvent(WebInputEvent::GestureScrollBegin);
  StartFlush();
  // Tamper with the sent packet by adding an extra event.
  current_packet_dispositions_.push_back(INPUT_EVENT_MAIN_THREAD_CONSUMED);
  bool valid_packet_ack = FinishFlush(INPUT_EVENT_MAIN_THREAD_CONSUMED);
  EXPECT_EQ(0, num_flush_completions());
  EXPECT_FALSE(valid_packet_ack);

  // Fix the packet.
  current_packet_dispositions_.pop_back();
  valid_packet_ack = FinishFlush(INPUT_EVENT_MAIN_THREAD_CONSUMED);
  EXPECT_EQ(1, num_flush_completions());
  EXPECT_TRUE(valid_packet_ack);

  // Tamper with the packet by changing the id.
  QueueEvent(WebInputEvent::GestureScrollBegin);
  StartFlush();
  int64 packet_ack_id = -1;
  std::swap(current_packet_id_, packet_ack_id);
  valid_packet_ack = FinishFlush(INPUT_EVENT_MAIN_THREAD_CONSUMED);
  EXPECT_EQ(1, num_flush_completions());
  EXPECT_FALSE(valid_packet_ack);

  // Fix the packet.
  std::swap(current_packet_id_, packet_ack_id);
  valid_packet_ack = FinishFlush(INPUT_EVENT_MAIN_THREAD_CONSUMED);
  EXPECT_EQ(2, num_flush_completions());
  EXPECT_TRUE(valid_packet_ack);
}

TEST_F(InputQueueTest, InjectedEventsAckedBeforeDidFinishFlush) {
  QueueEvent(WebInputEvent::GestureScrollBegin);
  QueueEvent(WebInputEvent::TouchMove);

  event_to_inject_ = CreateIPCInputEvent(new InputMsg_Copy(routing_id_));
  Flush(INPUT_EVENT_IMPL_THREAD_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0, num_flush_completions());

  // The injected event should now be in the event packet.
  EXPECT_EQ(1u, current_packet_dispositions_.size());
  EXPECT_EQ(1u, acked_followup_dispositions_.size());
  ASSERT_TRUE(FinishFlush(INPUT_EVENT_MAIN_THREAD_CONSUMED));
  EXPECT_EQ(1, num_flush_completions());

  QueueEvent(WebInputEvent::GestureScrollBegin);
  QueueEvent(WebInputEvent::TouchStart);
  event_to_inject_ = CreateWebInputEvent(WebInputEvent::TouchMove);
  Flush(INPUT_EVENT_MAIN_THREAD_NO_CONSUMER_EXISTS);
  // |event_to_inject_| is now in the event packet.
  EXPECT_EQ(1u, acked_followup_dispositions_.size());
  EXPECT_EQ(1u, current_packet_dispositions_.size());

  event_to_inject_ = CreateWebInputEvent(WebInputEvent::TouchMove);
  // the next |event_to_inject_| is now in the event packet.
  ASSERT_TRUE(FinishFlush(INPUT_EVENT_MAIN_THREAD_NO_CONSUMER_EXISTS));

  EXPECT_EQ(2u, acked_followup_dispositions_.size());
  EXPECT_EQ(1u, current_packet_dispositions_.size());
  EXPECT_EQ(1, num_flush_completions());

  ASSERT_TRUE(FinishFlush(INPUT_EVENT_MAIN_THREAD_CONSUMED));
  EXPECT_EQ(2, num_flush_completions());
}

}  // namespace
}  // namespace content
