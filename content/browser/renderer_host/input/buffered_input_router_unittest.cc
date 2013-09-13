// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "content/browser/renderer_host/input/buffered_input_router.h"
#include "content/browser/renderer_host/input/input_router_unittest.h"
#include "content/common/input/event_packet.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "testing/gtest/include/gtest/gtest.h"

using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebTouchEvent;
using WebKit::WebTouchPoint;

namespace content {

class TestBufferedInputRouter : public BufferedInputRouter {
 public:
  TestBufferedInputRouter(IPC::Sender* sender,
                          InputRouterClient* client,
                          InputAckHandler* ack_handler,
                          int routing_id)
      : BufferedInputRouter(sender, client, ack_handler, routing_id) {}


  size_t QueuedEventCount() const { return input_queue()->QueuedEventCount(); }
};

class BufferedInputRouterTest : public InputRouterTest {
 public:
  BufferedInputRouterTest() {}
  virtual ~BufferedInputRouterTest() {}

 protected:
  // InputRouterTest
  virtual scoped_ptr<InputRouter> CreateInputRouter(RenderProcessHost* process,
                                                    InputRouterClient* client,
                                                    InputAckHandler* handler,
                                                    int routing_id) OVERRIDE {
    return scoped_ptr<InputRouter>(
        new TestBufferedInputRouter(process, client, handler, routing_id));
  }

  bool FinishFlush(const InputEventDispositions& dispositions) {
    if (!process_->sink().message_count())
      return false;
    IPC::Message message(*process_->sink().GetMessageAt(0));
    process_->sink().ClearMessages();

    InputMsg_HandleEventPacket::Param param;
    InputMsg_HandleEventPacket::Read(&message, &param);
    EventPacket& packet = param.a;

    return SendEventPacketACK(packet.id(), dispositions);
  }

  bool FinishFlush(InputEventDisposition disposition) {
    if (!process_->sink().message_count())
      return false;
    IPC::Message message(*process_->sink().GetMessageAt(0));
    process_->sink().ClearMessages();

    InputMsg_HandleEventPacket::Param param;
    InputMsg_HandleEventPacket::Read(&message, &param);
    EventPacket& packet = param.a;

    return SendEventPacketACK(
        packet.id(), InputEventDispositions(packet.size(), disposition));
  }

  bool SendEventPacketACK(int id, const InputEventDispositions& dispositions) {
    return input_router_->OnMessageReceived(
        InputHostMsg_HandleEventPacket_ACK(0, id, dispositions));
  }

  size_t QueuedEventCount() const {
    return buffered_input_router()->QueuedEventCount();
  }

  TestBufferedInputRouter* buffered_input_router() const {
    return static_cast<TestBufferedInputRouter*>(input_router_.get());
  }
};

TEST_F(BufferedInputRouterTest, InputEventsProperlyQueued) {
  EXPECT_TRUE(input_router_->SendInput(
      scoped_ptr<IPC::Message>(new InputMsg_Redo(MSG_ROUTING_NONE))));
  EXPECT_EQ(1U, QueuedEventCount());

  EXPECT_TRUE(input_router_->SendInput(
      scoped_ptr<IPC::Message>(new InputMsg_Cut(MSG_ROUTING_NONE))));
  EXPECT_EQ(2U, QueuedEventCount());

  EXPECT_TRUE(input_router_->SendInput(
      scoped_ptr<IPC::Message>(new InputMsg_Copy(MSG_ROUTING_NONE))));
  EXPECT_EQ(3U, QueuedEventCount());

  EXPECT_TRUE(input_router_->SendInput(
      scoped_ptr<IPC::Message>(new InputMsg_Paste(MSG_ROUTING_NONE))));
  EXPECT_EQ(4U, QueuedEventCount());
}

#define SCOPED_EXPECT(CALL, MESSAGE) { SCOPED_TRACE(MESSAGE); CALL; }

TEST_F(BufferedInputRouterTest, ClientOnSendEventCalled) {
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_EQ(1U, QueuedEventCount());

  SimulateWheelEvent(5, 0, 0, false);
  EXPECT_EQ(2U, QueuedEventCount());

  SimulateMouseMove(5, 0, 0);
  EXPECT_EQ(3U, QueuedEventCount());

  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchpad);
  EXPECT_EQ(4U, QueuedEventCount());

  SimulateTouchEvent(1, 1);
  EXPECT_EQ(5U, QueuedEventCount());
}

TEST_F(BufferedInputRouterTest, ClientOnSendEventHonored) {
  client_->set_allow_send_event(false);

  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_EQ(0U, QueuedEventCount());

  SimulateWheelEvent(5, 0, 0, false);
  EXPECT_EQ(0U, QueuedEventCount());

  SimulateMouseMove(5, 0, 0);
  EXPECT_EQ(0U, QueuedEventCount());

  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchpad);
  EXPECT_EQ(0U, QueuedEventCount());

  SimulateTouchEvent(1, 1);
  EXPECT_EQ(0U, QueuedEventCount());
}

TEST_F(BufferedInputRouterTest, FlightCountIncrementedOnDeliver) {
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_EQ(0, client_->in_flight_event_count());

  input_router_->Flush();
  EXPECT_EQ(1, client_->in_flight_event_count());
}

TEST_F(BufferedInputRouterTest, FlightCountDecrementedOnAck) {
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_EQ(0, client_->in_flight_event_count());

  input_router_->Flush();
  EXPECT_EQ(1, client_->in_flight_event_count());

  // The in-flight count should continue until the flush has finished.
  ASSERT_TRUE(FinishFlush(INPUT_EVENT_COULD_NOT_DELIVER));
  EXPECT_EQ(1, client_->in_flight_event_count());

  ASSERT_TRUE(FinishFlush(INPUT_EVENT_IMPL_THREAD_CONSUMED));
  EXPECT_EQ(0, client_->in_flight_event_count());
}

TEST_F(BufferedInputRouterTest, FilteredEventsNeverQueued) {
  // Event should not be queued, but should be ack'ed.
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  SCOPED_EXPECT(ack_handler_->ExpectAckCalled(1), "AckCalled");
  EXPECT_EQ(0U, QueuedEventCount());
  ASSERT_EQ(NULL, input_router_->GetLastKeyboardEvent());

  // Event should not be queued, but should be ack'ed.
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  SCOPED_EXPECT(ack_handler_->ExpectAckCalled(1), "AckCalled");
  EXPECT_EQ(0U, QueuedEventCount());
  ASSERT_EQ(NULL, input_router_->GetLastKeyboardEvent());

  // |INPUT_EVENT_DISPOSITION_UNKNOWN| should drop the event without ack'ing.
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_UNKNOWN);
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  SCOPED_EXPECT(ack_handler_->ExpectAckCalled(0), "AckNotCalled");
  EXPECT_EQ(0U, QueuedEventCount());
  ASSERT_EQ(NULL, input_router_->GetLastKeyboardEvent());

  // Event should be queued.
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  SCOPED_EXPECT(ack_handler_->ExpectAckCalled(0), "AckNotCalled");
  EXPECT_EQ(1U, QueuedEventCount());
}

TEST_F(BufferedInputRouterTest, FollowupEventsInjected) {
  // Enable a followup gesture event.
  WebGestureEvent followup_event;
  followup_event.type = WebInputEvent::GestureScrollBegin;
  followup_event.data.scrollUpdate.deltaX = 10;
  ack_handler_->set_followup_touch_event(make_scoped_ptr(
      new GestureEventWithLatencyInfo(followup_event, ui::LatencyInfo())));

  // Create an initial packet of { Touch, Key } and start flushing.
  SimulateTouchEvent(1, 1);
  EXPECT_EQ(1U, QueuedEventCount());
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_EQ(2U, QueuedEventCount());
  input_router_->Flush();

  // Followup only triggered when event handled.
  ASSERT_TRUE(FinishFlush(INPUT_EVENT_COULD_NOT_DELIVER));
  SCOPED_EXPECT(client_->ExpectDidFlushCalled(false), "DidFlushNotCalled");
  EXPECT_EQ(2U, QueuedEventCount());

  // Ack the touch event.
  InputEventDispositions dispositions;
  dispositions.push_back(INPUT_EVENT_MAIN_THREAD_NOT_PREVENT_DEFAULTED);
  dispositions.push_back(INPUT_EVENT_COULD_NOT_DELIVER);
  ASSERT_TRUE(FinishFlush(dispositions));

  // Ack'ing the touch event should have inserted the followup gesture event;
  // the flush is not complete until the inserted event is ack'ed.
  SCOPED_EXPECT(client_->ExpectDidFlushCalled(false), "DidFlushNotCalled");
  SCOPED_EXPECT(client_->ExpectSendCalled(true), "SendGestureCalled");
  EXPECT_EQ(followup_event.type, client_->sent_gesture_event().event.type);
  EXPECT_EQ(2U, QueuedEventCount());

  // Our packet is now { Gesture, Key }.
  InputMsg_HandleEventPacket::Param param;
  ASSERT_EQ(1U, process_->sink().message_count());
  ASSERT_TRUE(InputMsg_HandleEventPacket::Read(process_->sink().GetMessageAt(0),
                                               &param));
  EventPacket& followup_packet = param.a;
  ASSERT_EQ(2U, followup_packet.size());
  ASSERT_EQ(InputEvent::Payload::WEB_INPUT_EVENT,
            followup_packet.events()[0]->payload()->GetType());
  ASSERT_EQ(InputEvent::Payload::WEB_INPUT_EVENT,
            followup_packet.events()[1]->payload()->GetType());
  const WebInputEventPayload* payload0 =
      WebInputEventPayload::Cast(followup_packet.events()[0]->payload());
  const WebInputEventPayload* payload1 =
      WebInputEventPayload::Cast(followup_packet.events()[1]->payload());
  EXPECT_EQ(followup_event.type, payload0->web_event()->type);
  EXPECT_EQ(WebInputEvent::RawKeyDown, payload1->web_event()->type);

  // Complete the flush; the gesture should have been ack'ed.
  ASSERT_TRUE(FinishFlush(INPUT_EVENT_IMPL_THREAD_CONSUMED));
  SCOPED_EXPECT(client_->ExpectDidFlushCalled(true), "DidFlushCalled");
  EXPECT_EQ(followup_event.type, ack_handler_->acked_gesture_event().type);
  EXPECT_EQ(0U, QueuedEventCount());
}

TEST_F(BufferedInputRouterTest, FlushRequestedOnQueue) {
  // The first queued event should trigger a flush request.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_EQ(1U, QueuedEventCount());
  SCOPED_EXPECT(client_->ExpectNeedsFlushCalled(true), "SetNeedsFlushCalled");

  // Subsequently queued events will not trigger another flush request.
  SimulateWheelEvent(5, 0, 0, false);
  EXPECT_EQ(2U, QueuedEventCount());
  SCOPED_EXPECT(client_->ExpectNeedsFlushCalled(false), "SetNeedsFlushCalled");
}

TEST_F(BufferedInputRouterTest, GetLastKeyboardEvent) {
  EXPECT_EQ(NULL, input_router_->GetLastKeyboardEvent());

  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_EQ(WebInputEvent::RawKeyDown,
            input_router_->GetLastKeyboardEvent()->type);

  // Queueing another key event does not effect the "last" event.
  SimulateKeyboardEvent(WebInputEvent::KeyUp);
  EXPECT_EQ(WebInputEvent::RawKeyDown,
            input_router_->GetLastKeyboardEvent()->type);

  input_router_->Flush();

  // Ack'ing the first event should make the second event the "last" event.
  InputEventDispositions dispositions;
  dispositions.push_back(INPUT_EVENT_IMPL_THREAD_CONSUMED);
  dispositions.push_back(INPUT_EVENT_COULD_NOT_DELIVER);
  ASSERT_TRUE(FinishFlush(dispositions));
  EXPECT_EQ(WebInputEvent::KeyUp, input_router_->GetLastKeyboardEvent()->type);

  // A key event queued during a flush becomes "last" upon flush completion.
  SimulateKeyboardEvent(WebInputEvent::Char);
  ASSERT_TRUE(FinishFlush(INPUT_EVENT_IMPL_THREAD_CONSUMED));
  EXPECT_EQ(WebInputEvent::Char, input_router_->GetLastKeyboardEvent()->type);

  // An empty queue should produce a null "last" event.
  input_router_->Flush();
  ASSERT_TRUE(FinishFlush(INPUT_EVENT_IMPL_THREAD_CONSUMED));
  EXPECT_EQ(NULL, input_router_->GetLastKeyboardEvent());
}

TEST_F(BufferedInputRouterTest, UnexpectedAck) {
  ASSERT_FALSE(ack_handler_->unexpected_event_ack_called());
  input_router_->OnMessageReceived(
      InputHostMsg_HandleEventPacket_ACK(0, 0, InputEventDispositions()));
  EXPECT_TRUE(ack_handler_->unexpected_event_ack_called());
}

TEST_F(BufferedInputRouterTest, BadAck) {
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  input_router_->Flush();

  ASSERT_FALSE(ack_handler_->unexpected_event_ack_called());
  EventPacket packet;
  input_router_->OnMessageReceived(
      InputHostMsg_HandleEventPacket_ACK(0, 0, InputEventDispositions()));
  EXPECT_TRUE(ack_handler_->unexpected_event_ack_called());
}

}  // namespace content
