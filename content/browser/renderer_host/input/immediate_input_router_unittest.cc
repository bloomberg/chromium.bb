// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/input/gesture_event_filter.h"
#include "content/browser/renderer_host/input/immediate_input_router.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/input_router_unittest.h"
#include "content/browser/renderer_host/input/mock_input_router_client.h"
#include "content/common/content_constants_internal.h"
#include "content/common/edit_command.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(OS_WIN) || defined(USE_AURA)
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/events/event.h"
#endif

using base::TimeDelta;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

namespace {

const WebInputEvent* GetInputEventFromMessage(const IPC::Message& message) {
  PickleIterator iter(message);
  const char* data;
  int data_length;
  if (!message.ReadData(&iter, &data, &data_length))
    return NULL;
  return reinterpret_cast<const WebInputEvent*>(data);
}

bool GetIsShortcutFromHandleInputEventMessage(const IPC::Message* msg) {
  InputMsg_HandleInputEvent::Schema::Param param;
  InputMsg_HandleInputEvent::Read(msg, &param);
  return param.c;
}

template<typename MSG_T, typename ARG_T1>
void ExpectIPCMessageWithArg1(const IPC::Message* msg, const ARG_T1& arg1) {
  ASSERT_EQ(MSG_T::ID, msg->type());
  typename MSG_T::Schema::Param param;
  ASSERT_TRUE(MSG_T::Read(msg, &param));
  EXPECT_EQ(arg1, param.a);
}

template<typename MSG_T, typename ARG_T1, typename ARG_T2>
void ExpectIPCMessageWithArg2(const IPC::Message* msg,
                              const ARG_T1& arg1,
                              const ARG_T2& arg2) {
  ASSERT_EQ(MSG_T::ID, msg->type());
  typename MSG_T::Schema::Param param;
  ASSERT_TRUE(MSG_T::Read(msg, &param));
  EXPECT_EQ(arg1, param.a);
  EXPECT_EQ(arg2, param.b);
}

#if defined(OS_WIN) || defined(USE_AURA)
bool TouchEventsAreEquivalent(const ui::TouchEvent& first,
                              const ui::TouchEvent& second) {
  if (first.type() != second.type())
    return false;
  if (first.location() != second.location())
    return false;
  if (first.touch_id() != second.touch_id())
    return false;
  if (second.time_stamp().InSeconds() != first.time_stamp().InSeconds())
    return false;
  return true;
}

bool EventListIsSubset(const ScopedVector<ui::TouchEvent>& subset,
                       const ScopedVector<ui::TouchEvent>& set) {
  if (subset.size() > set.size())
    return false;
  for (size_t i = 0; i < subset.size(); ++i) {
    const ui::TouchEvent* first = subset[i];
    const ui::TouchEvent* second = set[i];
    bool equivalent = TouchEventsAreEquivalent(*first, *second);
    if (!equivalent)
      return false;
  }

  return true;
}
#endif  // defined(OS_WIN) || defined(USE_AURA)

}  // namespace

class ImmediateInputRouterTest : public InputRouterTest {
 public:
  ImmediateInputRouterTest() {}
  virtual ~ImmediateInputRouterTest() {}

 protected:
  // InputRouterTest
  virtual scoped_ptr<InputRouter> CreateInputRouter(RenderProcessHost* process,
                                                    InputRouterClient* client,
                                                    InputAckHandler* handler,
                                                    int routing_id) OVERRIDE {
    return scoped_ptr<InputRouter>(
        new ImmediateInputRouter(process, client, handler, routing_id));
  }

  void SendInputEventACK(blink::WebInputEvent::Type type,
                         InputEventAckState ack_result) {
    scoped_ptr<IPC::Message> response(
        new InputHostMsg_HandleInputEvent_ACK(0, type, ack_result,
                                              ui::LatencyInfo()));
    input_router_->OnMessageReceived(*response);
  }

  ImmediateInputRouter* input_router() const {
    return static_cast<ImmediateInputRouter*>(input_router_.get());
  }

  bool no_touch_to_renderer() {
    return input_router()->touch_event_queue_->no_touch_to_renderer();
  }

  bool TouchEventQueueEmpty() const {
    return input_router()->touch_event_queue_->empty();
  }

  size_t GetSentMessageCountAndResetSink() {
    size_t count = process_->sink().message_count();
    process_->sink().ClearMessages();
    return count;
  }
};

TEST_F(ImmediateInputRouterTest, CoalescesRangeSelection) {
  input_router_->SendInput(scoped_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(1, 2), gfx::Point(3, 4))));
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0),
      gfx::Point(1, 2),
      gfx::Point(3, 4));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Send two more messages without acking.
  input_router_->SendInput(scoped_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(5, 6), gfx::Point(7, 8))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(scoped_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(9, 10), gfx::Point(11, 12))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Now ack the first message.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two messages are coalesced into one message.
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0),
      gfx::Point(9, 10),
      gfx::Point(11, 12));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Acking the coalesced msg should not send any more msg.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
}

TEST_F(ImmediateInputRouterTest, CoalescesCaretMove) {
  input_router_->SendInput(
      scoped_ptr<IPC::Message>(new InputMsg_MoveCaret(0, gfx::Point(1, 2))));
  ExpectIPCMessageWithArg1<InputMsg_MoveCaret>(
      process_->sink().GetMessageAt(0), gfx::Point(1, 2));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Send two more messages without acking.
  input_router_->SendInput(
      scoped_ptr<IPC::Message>(new InputMsg_MoveCaret(0, gfx::Point(5, 6))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(
      scoped_ptr<IPC::Message>(new InputMsg_MoveCaret(0, gfx::Point(9, 10))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Now ack the first message.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_MoveCaret_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two messages are coalesced into one message.
  ExpectIPCMessageWithArg1<InputMsg_MoveCaret>(
      process_->sink().GetMessageAt(0), gfx::Point(9, 10));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Acking the coalesced msg should not send any more msg.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_MoveCaret_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
}

TEST_F(ImmediateInputRouterTest, HandledInputEvent) {
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown, false);

  // Make sure no input event is sent to the renderer.
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // OnKeyboardEventAck should be triggered without actual ack.
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());

  // As the event was acked already, keyboard event queue should be
  // empty.
  ASSERT_EQ(NULL, input_router_->GetLastKeyboardEvent());
}

TEST_F(ImmediateInputRouterTest, ClientCanceledKeyboardEvent) {
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  // Simulate a keyboard event that has no consumer.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown, false);

  // Make sure no input event is sent to the renderer.
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());


  // Simulate a keyboard event that should be dropped.
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_UNKNOWN);
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown, false);

  // Make sure no input event is sent to the renderer, and no ack is sent.
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
}

TEST_F(ImmediateInputRouterTest, ShortcutKeyboardEvent) {
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown, true);
  EXPECT_TRUE(GetIsShortcutFromHandleInputEventMessage(
      process_->sink().GetMessageAt(0)));

  process_->sink().ClearMessages();

  SimulateKeyboardEvent(WebInputEvent::RawKeyDown, false);
  EXPECT_FALSE(GetIsShortcutFromHandleInputEventMessage(
      process_->sink().GetMessageAt(0)));
}

TEST_F(ImmediateInputRouterTest, NoncorrespondingKeyEvents) {
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown, false);

  SendInputEventACK(WebInputEvent::KeyUp,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(ack_handler_->unexpected_event_ack_called());
}

// Tests ported from RenderWidgetHostTest --------------------------------------

TEST_F(ImmediateInputRouterTest, HandleKeyEventsWeSent) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown, false);
  ASSERT_TRUE(input_router_->GetLastKeyboardEvent());
  EXPECT_EQ(WebInputEvent::RawKeyDown,
            input_router_->GetLastKeyboardEvent()->type);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::RawKeyDown,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::RawKeyDown,
            ack_handler_->acked_keyboard_event().type);
}

TEST_F(ImmediateInputRouterTest, IgnoreKeyEventsWeDidntSend) {
  // Send a simulated, unrequested key response. We should ignore this.
  SendInputEventACK(WebInputEvent::RawKeyDown,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
}

TEST_F(ImmediateInputRouterTest, CoalescesWheelEvents) {
  // Simulate wheel events.
  SimulateWheelEvent(0, -5, 0, false);  // sent directly
  SimulateWheelEvent(0, -10, 0, false);  // enqueued
  SimulateWheelEvent(8, -6, 0, false);  // coalesced into previous event
  SimulateWheelEvent(9, -7, 1, false);  // enqueued, different modifiers

  // Check that only the first event was sent.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Check that the ACK sends the second message via ImmediateInputForwarder
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  // The coalesced events can queue up a delayed ack
  // so that additional input events can be processed before
  // we turn off coalescing.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
          InputMsg_HandleInputEvent::ID));
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // One more time.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // After the final ack, the queue should be empty.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
}

TEST_F(ImmediateInputRouterTest,
       CoalescesWheelEventsQueuedPhaseEndIsNotDropped) {
  // Send an initial gesture begin and ACK it.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchpad);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();

  // Send a wheel event, should get sent directly.
  SimulateWheelEvent(0, -5, 0, false);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Send a wheel phase end event before an ACK is received for the previous
  // wheel event, which should get queued.
  SimulateWheelEventWithPhase(WebMouseWheelEvent::PhaseEnded);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // A gesture event should now result in the queued phase ended event being
  // transmitted before it.
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchpad);

  // Verify the events that were sent.
  const WebInputEvent* input_event =
      GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(input_event);
  ASSERT_EQ(WebMouseWheelEvent::PhaseEnded, wheel_event->phase);

  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(1));
  EXPECT_EQ(WebInputEvent::GestureScrollEnd, input_event->type);

  ASSERT_EQ(2U, GetSentMessageCountAndResetSink());
}

// Tests that touch-events are queued properly.
TEST_F(ImmediateInputRouterTest, TouchEventQueue) {
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(client_->GetAndResetFilterEventCalled());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // The second touch should not be sent since one is already in queue.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  EXPECT_FALSE(client_->GetAndResetFilterEventCalled());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // Receive an ACK for the first touch-event.
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::TouchStart,
            ack_handler_->acked_touch_event().event.type);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::TouchMove,
            ack_handler_->acked_touch_event().event.type);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
}

// Tests that the touch-queue is emptied if a page stops listening for touch
// events.
TEST_F(ImmediateInputRouterTest, TouchEventQueueFlush) {
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_TRUE(client_->has_touch_handler());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(TouchEventQueueEmpty());

  EXPECT_TRUE(input_router_->ShouldForwardTouchEvent());

  // Send a touch-press event.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_FALSE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // The page stops listening for touch-events. The touch-event queue should now
  // be emptied, but none of the queued touch-events should be sent to the
  // renderer.
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, false));
  EXPECT_FALSE(client_->has_touch_handler());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_FALSE(input_router_->ShouldForwardTouchEvent());
}

#if defined(OS_WIN) || defined(USE_AURA)
// Tests that the acked events have correct state. (ui::Events are used only on
// windows and aura)
TEST_F(ImmediateInputRouterTest, AckedTouchEventState) {
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_TRUE(input_router_->ShouldForwardTouchEvent());

  // Send a bunch of events, and make sure the ACKed events are correct.
  ScopedVector<ui::TouchEvent> expected_events;

  // Use a custom timestamp for all the events to test that the acked events
  // have the same timestamp;
  base::TimeDelta timestamp = base::Time::NowFromSystemTime() - base::Time();
  timestamp -= base::TimeDelta::FromSeconds(600);

  // Press the first finger.
  PressTouchPoint(1, 1);
  SetTouchTimestamp(timestamp);
  SendTouchEvent();
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
      gfx::Point(1, 1), 0, timestamp));

  // Move the finger.
  timestamp += base::TimeDelta::FromSeconds(10);
  MoveTouchPoint(0, 5, 5);
  SetTouchTimestamp(timestamp);
  SendTouchEvent();
  EXPECT_FALSE(TouchEventQueueEmpty());
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_MOVED,
      gfx::Point(5, 5), 0, timestamp));

  // Now press a second finger.
  timestamp += base::TimeDelta::FromSeconds(10);
  PressTouchPoint(2, 2);
  SetTouchTimestamp(timestamp);
  SendTouchEvent();
  EXPECT_FALSE(TouchEventQueueEmpty());
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
      gfx::Point(2, 2), 1, timestamp));

  // Move both fingers.
  timestamp += base::TimeDelta::FromSeconds(10);
  MoveTouchPoint(0, 10, 10);
  MoveTouchPoint(1, 20, 20);
  SetTouchTimestamp(timestamp);
  SendTouchEvent();
  EXPECT_FALSE(TouchEventQueueEmpty());
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_MOVED,
      gfx::Point(10, 10), 0, timestamp));
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_MOVED,
      gfx::Point(20, 20), 1, timestamp));

  // Receive the ACKs and make sure the generated events from the acked events
  // are correct.
  WebInputEvent::Type acks[] = { WebInputEvent::TouchStart,
                                 WebInputEvent::TouchMove,
                                 WebInputEvent::TouchStart,
                                 WebInputEvent::TouchMove };

  TouchEventCoordinateSystem coordinate_system = LOCAL_COORDINATES;
#if !defined(OS_WIN)
  coordinate_system = SCREEN_COORDINATES;
#endif
  for (size_t i = 0; i < arraysize(acks); ++i) {
    SendInputEventACK(acks[i],
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(acks[i], ack_handler_->acked_touch_event().event.type);
    ScopedVector<ui::TouchEvent> acked;

    MakeUITouchEventsFromWebTouchEvents(
        ack_handler_->acked_touch_event(), &acked, coordinate_system);
    bool success = EventListIsSubset(acked, expected_events);
    EXPECT_TRUE(success) << "Failed on step: " << i;
    if (!success)
      break;
    expected_events.erase(expected_events.begin(),
                          expected_events.begin() + acked.size());
  }

  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_EQ(0U, expected_events.size());
}
#endif  // defined(OS_WIN) || defined(USE_AURA)

TEST_F(ImmediateInputRouterTest, UnhandledWheelEvent) {
  // Simulate wheel events.
  SimulateWheelEvent(0, -5, 0, false);  // sent directly
  SimulateWheelEvent(0, -10, 0, false);  // enqueued

  // Check that only the first event was sent.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Indicate that the wheel event was unhandled.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Check that the correct unhandled wheel event was received.
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, ack_handler_->ack_state());
  EXPECT_EQ(ack_handler_->acked_wheel_event().deltaY, -5);

  // Check that the second event was sent.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Check that the correct unhandled wheel event was received.
  EXPECT_EQ(ack_handler_->acked_wheel_event().deltaY, -5);
}

// Test that GestureShowPress, GestureTapDown and GestureTapCancel events don't
// wait for ACKs.
TEST_F(ImmediateInputRouterTest, GestureTypesIgnoringAck) {
  // The show press, tap down and tap cancel events will be acked immediately,
  // since they ignore ack disposition.
  SimulateGestureEvent(WebInputEvent::GestureShowPress,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::GestureTapCancel,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());

  // Interleave a few events that do and do not ignore acks, ensuring that
  // ack-ignoring events aren't dispatched until all prior events which observe
  // their ack disposition have been dispatched.
  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       WebGestureEvent::Touchpad);
  ASSERT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::GesturePinchUpdate,
                       WebGestureEvent::Touchpad);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::GestureShowPress,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::GesturePinchEnd,
                       WebGestureEvent::Touchpad);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::GestureTapCancel,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  // Now ack each event. Ack-ignoring events should not be dispatched until all
  // prior events which observe ack disposition have been fired, at which
  // point they should be sent immediately.
  SendInputEventACK(WebInputEvent::GesturePinchBegin,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(2U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(2U, ack_handler_->GetAndResetAckCount());

  // For events which ignore ack disposition, non-synthetic acks are ignored.
  SendInputEventACK(WebInputEvent::GestureTapDown,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(2U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(2U, ack_handler_->GetAndResetAckCount());

  // For events which ignore ack disposition, non-synthetic acks are ignored.
  SendInputEventACK(WebInputEvent::GestureShowPress,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SendInputEventACK(WebInputEvent::GesturePinchEnd,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(2U, ack_handler_->GetAndResetAckCount());

  // For events which ignore ack disposition, non-synthetic acks are ignored.
  SendInputEventACK(WebInputEvent::GestureTapCancel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
}

// Test that GestureShowPress events don't get out of order due to
// ignoring their acks.
TEST_F(ImmediateInputRouterTest, GestureShowPressIsInOrder) {
  SimulateGestureEvent(WebInputEvent::GestureTap,
                       WebGestureEvent::Touchscreen);

  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::GestureShowPress,
                       WebGestureEvent::Touchscreen);

  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  // The ShowPress, though it ignores ack, is still stuck in the queue
  // behind the Tap which requires an ack.
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::GestureShowPress,
                       WebGestureEvent::Touchscreen);

  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  // ShowPress has entered the queue.
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SendInputEventACK(WebInputEvent::GestureTap,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Now that the Tap has been ACKed, the ShowPress events should receive
  // synthetics acks, and fire immediately.
  EXPECT_EQ(2U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(3U, ack_handler_->GetAndResetAckCount());
}

}  // namespace content
