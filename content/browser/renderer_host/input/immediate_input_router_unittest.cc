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
using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebTouchEvent;
using WebKit::WebTouchPoint;

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

  void SendInputEventACK(WebKit::WebInputEvent::Type type,
                         InputEventAckState ack_result) {
    scoped_ptr<IPC::Message> response(
        new InputHostMsg_HandleInputEvent_ACK(0, type, ack_result,
                                              ui::LatencyInfo()));
    input_router_->OnMessageReceived(*response);
  }

  ImmediateInputRouter* input_router() const {
    return static_cast<ImmediateInputRouter*>(input_router_.get());
  }

  void set_debounce_interval_time_ms(int ms) {
    input_router()->gesture_event_filter()->debounce_interval_time_ms_ = ms;
  }

  void set_maximum_tap_gap_time_ms(int delay_ms) {
    input_router()->gesture_event_filter()->maximum_tap_gap_time_ms_ = delay_ms;
  }

  bool no_touch_to_renderer() {
    return input_router()->touch_event_queue()->no_touch_to_renderer();
  }

  bool TouchEventQueueEmpty() const {
    return input_router()->touch_event_queue()->empty();
  }

  unsigned GestureEventLastQueueEventSize() {
    return gesture_event_filter()->coalesced_gesture_events_.size();
  }

  WebGestureEvent GestureEventSecondFromLastQueueEvent() {
    return gesture_event_filter()->coalesced_gesture_events_.at(
      GestureEventLastQueueEventSize() - 2).event;
  }

  WebGestureEvent GestureEventLastQueueEvent() {
    return gesture_event_filter()->coalesced_gesture_events_.back().event;
  }

  unsigned GestureEventDebouncingQueueSize() {
    return gesture_event_filter()->debouncing_deferral_queue_.size();
  }

  WebGestureEvent GestureEventQueueEventAt(int i) {
    return gesture_event_filter()->coalesced_gesture_events_.at(i).event;
  }

  bool ShouldDeferTapDownEvents() {
    return gesture_event_filter()->maximum_tap_gap_time_ms_ != 0;
  }

  bool ScrollingInProgress() {
    return gesture_event_filter()->scrolling_in_progress_;
  }

  bool FlingInProgress() {
    return gesture_event_filter()->fling_in_progress_;
  }

  bool WillIgnoreNextACK() {
    return gesture_event_filter()->ignore_next_ack_;
  }

  GestureEventFilter* gesture_event_filter() const {
    return input_router()->gesture_event_filter();
  }
};

#if GTEST_HAS_PARAM_TEST
// This is for tests that are to be run for all source devices.
class ImmediateInputRouterWithSourceTest
    : public ImmediateInputRouterTest,
      public testing::WithParamInterface<WebGestureEvent::SourceDevice> {
};
#endif  // GTEST_HAS_PARAM_TEST

TEST_F(ImmediateInputRouterTest, CoalescesRangeSelection) {
  input_router_->SendInput(scoped_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(1, 2), gfx::Point(3, 4))));
  EXPECT_EQ(1u, process_->sink().message_count());
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0),
      gfx::Point(1, 2),
      gfx::Point(3, 4));
  process_->sink().ClearMessages();

  // Send two more messages without acking.
  input_router_->SendInput(scoped_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(5, 6), gfx::Point(7, 8))));
  EXPECT_EQ(0u, process_->sink().message_count());

  input_router_->SendInput(scoped_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(9, 10), gfx::Point(11, 12))));
  EXPECT_EQ(0u, process_->sink().message_count());

  // Now ack the first message.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two messages are coalesced into one message.
  EXPECT_EQ(1u, process_->sink().message_count());
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0),
      gfx::Point(9, 10),
      gfx::Point(11, 12));
  process_->sink().ClearMessages();

  // Acking the coalesced msg should not send any more msg.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, process_->sink().message_count());
}

TEST_F(ImmediateInputRouterTest, CoalescesCaretMove) {
  input_router_->SendInput(
      scoped_ptr<IPC::Message>(new InputMsg_MoveCaret(0, gfx::Point(1, 2))));
  EXPECT_EQ(1u, process_->sink().message_count());
  ExpectIPCMessageWithArg1<InputMsg_MoveCaret>(
      process_->sink().GetMessageAt(0), gfx::Point(1, 2));
  process_->sink().ClearMessages();

  // Send two more messages without acking.
  input_router_->SendInput(
      scoped_ptr<IPC::Message>(new InputMsg_MoveCaret(0, gfx::Point(5, 6))));
  EXPECT_EQ(0u, process_->sink().message_count());

  input_router_->SendInput(
      scoped_ptr<IPC::Message>(new InputMsg_MoveCaret(0, gfx::Point(9, 10))));
  EXPECT_EQ(0u, process_->sink().message_count());

  // Now ack the first message.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_MoveCaret_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two messages are coalesced into one message.
  EXPECT_EQ(1u, process_->sink().message_count());
  ExpectIPCMessageWithArg1<InputMsg_MoveCaret>(
      process_->sink().GetMessageAt(0), gfx::Point(9, 10));
  process_->sink().ClearMessages();

  // Acking the coalesced msg should not send any more msg.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_MoveCaret_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, process_->sink().message_count());
}

TEST_F(ImmediateInputRouterTest, HandledInputEvent) {
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  // Make sure no input event is sent to the renderer.
  EXPECT_EQ(0u, process_->sink().message_count());

  // OnKeyboardEventAck should be triggered without actual ack.
  ack_handler_->ExpectAckCalled(1);

  // As the event was acked already, keyboard event queue should be
  // empty.
  ASSERT_EQ(NULL, input_router_->GetLastKeyboardEvent());
}

TEST_F(ImmediateInputRouterTest, ClientCanceledKeyboardEvent) {
  client_->set_allow_send_event(false);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  // Make sure no input event is sent to the renderer.
  EXPECT_EQ(0u, process_->sink().message_count());
  ack_handler_->ExpectAckCalled(0);
}

TEST_F(ImmediateInputRouterTest, ShortcutKeyboardEvent) {
  client_->set_is_shortcut(true);
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_TRUE(GetIsShortcutFromHandleInputEventMessage(
      process_->sink().GetMessageAt(0)));

  process_->sink().ClearMessages();

  client_->set_is_shortcut(false);
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_FALSE(GetIsShortcutFromHandleInputEventMessage(
      process_->sink().GetMessageAt(0)));
}

TEST_F(ImmediateInputRouterTest, NoncorrespondingKeyEvents) {
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  SendInputEventACK(WebInputEvent::KeyUp,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(ack_handler_->unexpected_event_ack_called());
}

// Tests ported from RenderWidgetHostTest --------------------------------------

TEST_F(ImmediateInputRouterTest, HandleKeyEventsWeSent) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
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
  ack_handler_->ExpectAckCalled(1);
  EXPECT_EQ(WebInputEvent::RawKeyDown,
            ack_handler_->acked_keyboard_event().type);
}

TEST_F(ImmediateInputRouterTest, IgnoreKeyEventsWeDidntSend) {
  // Send a simulated, unrequested key response. We should ignore this.
  SendInputEventACK(WebInputEvent::RawKeyDown,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  ack_handler_->ExpectAckCalled(0);
}

// GestureEventFilter tests should be factored out of
// ImmediateInputRouterTest. See crbug.com/301807.
TEST_F(ImmediateInputRouterTest, CoalescesWheelEvents) {
  // Simulate wheel events.
  SimulateWheelEvent(0, -5, 0, false);  // sent directly
  SimulateWheelEvent(0, -10, 0, false);  // enqueued
  SimulateWheelEvent(8, -6, 0, false);  // coalesced into previous event
  SimulateWheelEvent(9, -7, 1, false);  // enqueued, different modifiers

  // Check that only the first event was sent.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Check that the ACK sends the second message via ImmediateInputForwarder
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  // The coalesced events can queue up a delayed ack
  // so that additional input events can be processed before
  // we turn off coalescing.
  base::MessageLoop::current()->RunUntilIdle();
  ack_handler_->ExpectAckCalled(1);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
          InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // One more time.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  ack_handler_->ExpectAckCalled(1);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // After the final ack, the queue should be empty.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  ack_handler_->ExpectAckCalled(1);
  EXPECT_EQ(0U, process_->sink().message_count());

  // FIXME(kouhei): Below is testing gesture event filter. Maybe separate test?
  {
    WebGestureEvent gesture_event;
    gesture_event.type = WebInputEvent::GestureFlingStart;
    gesture_event.sourceDevice = WebGestureEvent::Touchpad;
    gesture_event.data.flingStart.velocityX = 0.f;
    gesture_event.data.flingStart.velocityY = 0.f;
    EXPECT_FALSE(input_router_->ShouldForwardGestureEvent(
        GestureEventWithLatencyInfo(gesture_event, ui::LatencyInfo())));
  }
}

TEST_F(ImmediateInputRouterTest,
       CoalescesWheelEventsQueuedPhaseEndIsNotDropped) {
  // Send an initial gesture begin and ACK it.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchpad);
  EXPECT_EQ(1U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();

  // Send a wheel event, should get sent directly.
  SimulateWheelEvent(0, -5, 0, false);
  EXPECT_EQ(2U, process_->sink().message_count());

  // Send a wheel phase end event before an ACK is received for the previous
  // wheel event, which should get queued.
  SimulateWheelEventWithPhase(WebMouseWheelEvent::PhaseEnded);
  EXPECT_EQ(2U, process_->sink().message_count());

  // A gesture event should now result in the queued phase ended event being
  // transmitted before it.
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchpad);
  ASSERT_EQ(4U, process_->sink().message_count());

  // Verify the events that were sent.
  const WebInputEvent* input_event =
      GetInputEventFromMessage(*process_->sink().GetMessageAt(2));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(input_event);
  ASSERT_EQ(WebMouseWheelEvent::PhaseEnded, wheel_event->phase);

  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(3));
  EXPECT_EQ(WebInputEvent::GestureScrollEnd, input_event->type);
}

TEST_F(ImmediateInputRouterTest, CoalescesScrollGestureEvents) {
  // Turn off debounce handling for test isolation.
  set_debounce_interval_time_ms(0);

  // Test coalescing of only GestureScrollUpdate events.
  // Simulate gesture events.

  // Sent.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -5, 0);

  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);

  // Coalesced.
  SimulateGestureScrollUpdateEvent(8, -6, 0);

  // Check that coalescing updated the correct values.
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(0, merged_event.modifiers);
  EXPECT_EQ(16, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-11, merged_event.data.scrollUpdate.deltaY);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -7, 1);

  // Check that we didn't wrongly coalesce.
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(1, merged_event.modifiers);

  // Different.
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);

  // Check that only the first event was sent.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  ack_handler_->ExpectAckCalled(1);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Ack for queued coalesced event.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  ack_handler_->ExpectAckCalled(1);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Ack for queued uncoalesced event.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  ack_handler_->ExpectAckCalled(1);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // After the final ack, the queue should be empty.
  SendInputEventACK(WebInputEvent::GestureScrollEnd,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  ack_handler_->ExpectAckCalled(1);
  EXPECT_EQ(0U, process_->sink().message_count());
}

TEST_F(ImmediateInputRouterTest, CoalescesScrollAndPinchEvents) {
  // Turn off debounce handling for test isolation.
  set_debounce_interval_time_ms(0);

  // Test coalescing of only GestureScrollUpdate events.
  // Simulate gesture events.

  // Sent.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);

  // Sent.
  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       WebGestureEvent::Touchscreen);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -4, 1);

  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(3U, GestureEventLastQueueEventSize());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);

  // Coalesced without changing event order. Note anchor at (60, 60). Anchoring
  // from a poinht that is not the origin should still give us the wight scroll.
  SimulateGesturePinchUpdateEvent(1.5, 60, 60, 1);
  EXPECT_EQ(4U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(8, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-4, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -3, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGesturePinchUpdateEvent(2, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(3, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGesturePinchUpdateEvent(2, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Check that only the first event was sent.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -6, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(3U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(13, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-7, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // At this point ACKs shouldn't be getting ignored.
  EXPECT_FALSE(WillIgnoreNextACK());

  // Check that the ACK sends both scroll and pinch updates.
  SendInputEventACK(WebInputEvent::GesturePinchBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetFirstMessageMatching(
              InputMsg_HandleInputEvent::ID));
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // The next ACK should be getting ignored.
  EXPECT_TRUE(WillIgnoreNextACK());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(1, -1, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(3U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(1, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-1, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(2, -2, 1);

  // Coalescing scrolls should still work.
  EXPECT_EQ(3U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(3, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-3, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGesturePinchUpdateEvent(0.5, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(0.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(3, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-3, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Check that the ACK gets ignored.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0U, process_->sink().message_count());
  // The flag should have been flipped back to false.
  EXPECT_FALSE(WillIgnoreNextACK());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(2, -2, 2);

  // Shouldn't coalesce with different modifiers.
  EXPECT_EQ(4U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(2, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-2, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(2, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(0.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);

  // Check that the ACK sends the next scroll pinch pair.
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetFirstMessageMatching(
              InputMsg_HandleInputEvent::ID));
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0U, process_->sink().message_count());

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Check that the queue is empty after ACK and no messages get sent.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());
}

#if GTEST_HAS_PARAM_TEST
TEST_P(ImmediateInputRouterWithSourceTest, GestureFlingCancelsFiltered) {
  WebGestureEvent::SourceDevice source_device = GetParam();

  // Turn off debounce handling for test isolation.
  set_debounce_interval_time_ms(0);
  // GFC without previous GFS is dropped.
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  // GFC after previous GFS is dispatched and acked.
  process_->sink().ClearMessages();
  SimulateGestureFlingStartEvent(0, -10, source_device);
  EXPECT_TRUE(FlingInProgress());
  SendInputEventACK(WebInputEvent::GestureFlingStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  ack_handler_->ExpectAckCalled(1);
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(2U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::GestureFlingCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  ack_handler_->ExpectAckCalled(1);
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  // GFC before previous GFS is acked.
  process_->sink().ClearMessages();
  SimulateGestureFlingStartEvent(0, -10, source_device);
  EXPECT_TRUE(FlingInProgress());
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());

  // Advance state realistically.
  SendInputEventACK(WebInputEvent::GestureFlingStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  SendInputEventACK(WebInputEvent::GestureFlingCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  ack_handler_->ExpectAckCalled(2);
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  // GFS is added to the queue if another event is pending
  process_->sink().ClearMessages();
  SimulateGestureScrollUpdateEvent(8, -7, 0);
  SimulateGestureFlingStartEvent(0, -10, source_device);
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(1U, process_->sink().message_count());
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingStart, merged_event.type);
  EXPECT_TRUE(FlingInProgress());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());

  // GFS in queue means that a GFC is added to the queue
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  merged_event =GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingCancel, merged_event.type);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(3U, GestureEventLastQueueEventSize());

  // Adding a second GFC is dropped.
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(3U, GestureEventLastQueueEventSize());

  // Adding another GFS will add it to the queue.
  SimulateGestureFlingStartEvent(0, -10, source_device);
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingStart, merged_event.type);
  EXPECT_TRUE(FlingInProgress());
  EXPECT_EQ(4U, GestureEventLastQueueEventSize());

  // GFS in queue means that a GFC is added to the queue
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingCancel, merged_event.type);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(5U, GestureEventLastQueueEventSize());

  // Adding another GFC with a GFC already there is dropped.
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingCancel, merged_event.type);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(5U, GestureEventLastQueueEventSize());
}

INSTANTIATE_TEST_CASE_P(AllSources,
                        ImmediateInputRouterWithSourceTest,
                        testing::Values(WebGestureEvent::Touchscreen,
                                        WebGestureEvent::Touchpad));
#endif  // GTEST_HAS_PARAM_TEST

// Test that GestureTapDown events are deferred.
TEST_F(ImmediateInputRouterTest, DeferredGestureTapDown) {
  // Set some sort of short deferral timeout
  set_maximum_tap_gap_time_ms(5);

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  // Wait long enough for first timeout and see if it fired.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  EXPECT_EQ(1U, process_->sink().message_count());
  // The tap down event will have escaped the queue, since they're async.
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());
}

// Test that GestureTapDown events are sent immediately on GestureTap.
TEST_F(ImmediateInputRouterTest, DeferredGestureTapDownSentOnTap) {
  // Set some sort of short deferral timeout
  set_maximum_tap_gap_time_ms(5);

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  SimulateGestureEvent(WebInputEvent::GestureTap,
                       WebGestureEvent::Touchscreen);

  EXPECT_EQ(2U, process_->sink().message_count());
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());
  EXPECT_EQ(WebInputEvent::GestureTap,
            GestureEventLastQueueEvent().type);

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  EXPECT_EQ(WebInputEvent::GestureTapDown,
            client_->immediately_sent_gesture_event().event.type);

  // If the deferral timer incorrectly fired, it sent an extra message.
  EXPECT_EQ(2U, process_->sink().message_count());
}

// Test that only a single GestureTapDown event is sent when tap occurs after
// the timeout.
TEST_F(ImmediateInputRouterTest, DeferredGestureTapDownOnlyOnce) {
  // Set some sort of short deferral timeout
  set_maximum_tap_gap_time_ms(5);

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  // Wait long enough for the timeout and verify it fired.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  EXPECT_EQ(1U, process_->sink().message_count());
  // The tap down events will have escaped the queue, since they're async.
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  // Now send the tap gesture and verify we didn't get an extra TapDown.
  SimulateGestureEvent(WebInputEvent::GestureTap,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(2U, process_->sink().message_count());
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());
  EXPECT_EQ(WebInputEvent::GestureTap,
            GestureEventLastQueueEvent().type);
}

// Test that GestureTapDown events don't wait for ACKs.
TEST_F(ImmediateInputRouterTest, GestureTapDownIsAsync) {
  // Set some sort of short deferral timeout
  set_maximum_tap_gap_time_ms(5);

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  // Wait for tap deferral to end.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);

  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  // Wait for tap deferral to end.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);

  EXPECT_EQ(2U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  // Wait for tap deferral to end.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  EXPECT_EQ(3U, process_->sink().message_count());
  // The tap down events will have escaped the queue, since they're async.
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());
}

// Test that GestureTapDown events don't get out of order due to asynchronicity.
TEST_F(ImmediateInputRouterTest, GestureTapDownIsInOrder) {
  // Set some sort of short deferral timeout
  set_maximum_tap_gap_time_ms(5);

  SimulateGestureEvent(WebInputEvent::GestureTap,
                       WebGestureEvent::Touchscreen);

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  // TapDown is deferred, hasn't entered the queue yet.
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());

  // Wait long enough for the first timeout and verify it fired.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  EXPECT_EQ(1U, process_->sink().message_count());
  // The TapDown, though asynchronous, is still stuck in the queue
  // behind the synchronous Tap.
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  // TapDown is deferred, hasn't entered the queue yet.
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());

  // Wait long enough for the second timeout and verify it fired.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  EXPECT_EQ(1U, process_->sink().message_count());
  // TapDown has entered the queue.
  EXPECT_EQ(3U, GestureEventLastQueueEventSize());

  SendInputEventACK(WebInputEvent::GestureTap,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Now that the Tap has been ACKed, the TapDowns should fire immediately.
  EXPECT_EQ(3U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());
}

// Test that scroll events during the deferral interval drop the GestureTapDown.
TEST_F(ImmediateInputRouterTest, DeferredGestureTapDownAnulledOnScroll) {
  // Set some sort of short deferral timeout
  set_maximum_tap_gap_time_ms(5);

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());
  EXPECT_EQ(WebInputEvent::GestureScrollBegin,
            GestureEventLastQueueEvent().type);

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  // If the deferral timer incorrectly fired, it will send an extra message.
  EXPECT_EQ(1U, process_->sink().message_count());
}

// Test that a tap cancel event during the deferral interval drops the
// GestureTapDown.
TEST_F(ImmediateInputRouterTest, DeferredGestureTapDownAnulledOnTapCancel) {
  // Set some sort of short deferral timeout
  set_maximum_tap_gap_time_ms(5);

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  SimulateGestureEvent(WebInputEvent::GestureTapCancel,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  // If the deferral timer incorrectly fired, it will send an extra message.
  EXPECT_EQ(0U, process_->sink().message_count());
}

// Test that if a GestureTapDown gets sent, any corresponding GestureTapCancel
// is also sent.
TEST_F(ImmediateInputRouterTest, DeferredGestureTapDownTapCancel) {
  // Set some sort of short deferral timeout
  set_maximum_tap_gap_time_ms(5);

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  SimulateGestureEvent(WebInputEvent::GestureTapCancel,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(2U, process_->sink().message_count());
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());
}

// Test that a GestureScrollEnd | GestureFlingStart are deferred during the
// debounce interval, that Scrolls are not and that the deferred events are
// sent after that timer fires.
TEST_F(ImmediateInputRouterTest, DebounceDefersFollowingGestureEvents) {
  set_debounce_interval_time_ms(3);

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureFlingStartEvent(0, 10, WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(2U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(3U, GestureEventDebouncingQueueSize());

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(5));
  base::MessageLoop::current()->Run();

  // The deferred events are correctly queued in coalescing queue.
  EXPECT_EQ(1U, process_->sink().message_count());
  if (ShouldDeferTapDownEvents())
    // NOTE: The  TapDown is still deferred hence not queued.
    EXPECT_EQ(4U, GestureEventLastQueueEventSize());
  else
    EXPECT_EQ(5U, GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_FALSE(ScrollingInProgress());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {
      WebInputEvent::GestureScrollUpdate,
      WebInputEvent::GestureScrollUpdate,
      WebInputEvent::GestureScrollEnd,
      WebInputEvent::GestureFlingStart};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
      i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.type);
  }
}

// Test that non-scroll events are deferred while scrolling during the debounce
// interval and are discarded if a GestureScrollUpdate event arrives before the
// interval end.
TEST_F(ImmediateInputRouterTest, DebounceDropsDeferredEvents) {
  set_debounce_interval_time_ms(3);
  EXPECT_FALSE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // This event should get discarded.
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {
      WebInputEvent::GestureScrollUpdate,
      WebInputEvent::GestureScrollUpdate};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
      i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.type);
  }
}

// Tests that touch-events are queued properly.
TEST_F(ImmediateInputRouterTest, TouchEventQueue) {
  PressTouchPoint(1, 1);
  SendTouchEvent();
  client_->ExpectSendImmediatelyCalled(true);
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  EXPECT_FALSE(TouchEventQueueEmpty());

  // The second touch should not be sent since one is already in queue.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  client_->ExpectSendImmediatelyCalled(false);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // Receive an ACK for the first touch-event.
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(TouchEventQueueEmpty());
  ack_handler_->ExpectAckCalled(1);
  EXPECT_EQ(WebInputEvent::TouchStart,
            ack_handler_->acked_touch_event().event.type);
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(TouchEventQueueEmpty());
  ack_handler_->ExpectAckCalled(1);
  EXPECT_EQ(WebInputEvent::TouchMove,
            ack_handler_->acked_touch_event().event.type);
  EXPECT_EQ(0U, process_->sink().message_count());
}

// Tests that the touch-queue is emptied if a page stops listening for touch
// events.
TEST_F(ImmediateInputRouterTest, TouchEventQueueFlush) {
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_TRUE(client_->has_touch_handler());
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_TRUE(TouchEventQueueEmpty());

  EXPECT_TRUE(input_router_->ShouldForwardTouchEvent());

  // Send a touch-press event.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_FALSE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // The page stops listening for touch-events. The touch-event queue should now
  // be emptied, but none of the queued touch-events should be sent to the
  // renderer.
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, false));
  EXPECT_FALSE(client_->has_touch_handler());
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_FALSE(input_router_->ShouldForwardTouchEvent());
}

#if defined(OS_WIN) || defined(USE_AURA)
// Tests that the acked events have correct state. (ui::Events are used only on
// windows and aura)
TEST_F(ImmediateInputRouterTest, AckedTouchEventState) {
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_EQ(0U, process_->sink().message_count());
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
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
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
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Indicate that the wheel event was unhandled.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Check that the correct unhandled wheel event was received.
  ack_handler_->ExpectAckCalled(1);
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, ack_handler_->ack_state());
  EXPECT_EQ(ack_handler_->acked_wheel_event().deltaY, -5);

  // Check that the second event was sent.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Check that the correct unhandled wheel event was received.
  EXPECT_EQ(ack_handler_->acked_wheel_event().deltaY, -5);
}

}  // namespace content
