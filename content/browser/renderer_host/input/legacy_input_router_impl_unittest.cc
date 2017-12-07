// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/legacy_input_router_impl.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/input/touch_action.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/mock_input_disposition_handler.h"
#include "content/browser/renderer_host/input/mock_input_router_client.h"
#include "content/common/content_constants_internal.h"
#include "content/common/edit_command.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/events/event.h"
#endif

using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebKeyboardEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::DidOverscrollParams;
using ui::WebInputEventTraits;

namespace content {

namespace {

bool ShouldBlockEventStream(const blink::WebInputEvent& event) {
  return ui::WebInputEventTraits::ShouldBlockEventStream(
      event,
      base::FeatureList::IsEnabled(features::kTouchpadAndWheelScrollLatching));
}

const WebInputEvent* GetInputEventFromMessage(const IPC::Message& message) {
  base::PickleIterator iter(message);
  const char* data;
  int data_length;
  if (!iter.ReadData(&data, &data_length))
    return nullptr;
  return reinterpret_cast<const WebInputEvent*>(data);
}

WebInputEvent& GetEventWithType(WebInputEvent::Type type) {
  WebInputEvent* event = nullptr;
  if (WebInputEvent::IsMouseEventType(type)) {
    static WebMouseEvent mouse;
    event = &mouse;
  } else if (WebInputEvent::IsTouchEventType(type)) {
    static WebTouchEvent touch;
    event = &touch;
  } else if (WebInputEvent::IsKeyboardEventType(type)) {
    static WebKeyboardEvent key;
    event = &key;
  } else if (WebInputEvent::IsGestureEventType(type)) {
    static WebGestureEvent gesture;
    event = &gesture;
  } else if (type == WebInputEvent::kMouseWheel) {
    static WebMouseWheelEvent wheel;
    event = &wheel;
  }
  CHECK(event);
  event->SetType(type);
  return *event;
}

template <typename MSG_T, typename ARG_T1>
void ExpectIPCMessageWithArg1(const IPC::Message* msg, const ARG_T1& arg1) {
  ASSERT_EQ(static_cast<uint32_t>(MSG_T::ID), msg->type());
  typename MSG_T::Schema::Param param;
  ASSERT_TRUE(MSG_T::Read(msg, &param));
  EXPECT_EQ(arg1, std::get<0>(param));
}

template <typename MSG_T, typename ARG_T1, typename ARG_T2>
void ExpectIPCMessageWithArg2(const IPC::Message* msg,
                              const ARG_T1& arg1,
                              const ARG_T2& arg2) {
  ASSERT_EQ(static_cast<uint32_t>(MSG_T::ID), msg->type());
  typename MSG_T::Schema::Param param;
  ASSERT_TRUE(MSG_T::Read(msg, &param));
  EXPECT_EQ(arg1, std::get<0>(param));
  EXPECT_EQ(arg2, std::get<1>(param));
}

enum WheelScrollingMode {
  kWheelScrollingModeNone,
  kWheelScrollLatching,
  kAsyncWheelEvents,
};

}  // namespace

class LegacyInputRouterImplTest : public testing::Test {
 public:
  LegacyInputRouterImplTest(
      WheelScrollingMode wheel_scrolling_mode = kWheelScrollLatching)
      : wheel_scroll_latching_enabled_(wheel_scrolling_mode !=
                                       kWheelScrollingModeNone),
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {
    if (wheel_scrolling_mode == kAsyncWheelEvents) {
      feature_list_.InitWithFeatures({features::kTouchpadAndWheelScrollLatching,
                                      features::kAsyncWheelEvents},
                                     {});
    } else if (wheel_scrolling_mode == kWheelScrollLatching) {
      feature_list_.InitWithFeatures(
          {features::kTouchpadAndWheelScrollLatching},
          {features::kAsyncWheelEvents});
    } else if (wheel_scrolling_mode == kWheelScrollingModeNone) {
      feature_list_.InitWithFeatures({},
                                     {features::kTouchpadAndWheelScrollLatching,
                                      features::kAsyncWheelEvents});
    }

    vsync_feature_list_.InitAndEnableFeature(
        features::kVsyncAlignedInputEvents);
  }

  ~LegacyInputRouterImplTest() override {}

 protected:
  // testing::Test
  void SetUp() override {
    browser_context_.reset(new TestBrowserContext());
    process_.reset(new MockRenderProcessHost(browser_context_.get()));
    client_.reset(new MockInputRouterClient());
    disposition_handler_.reset(new MockInputDispositionHandler());
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kValidateInputEventStream);
    input_router_.reset(new LegacyInputRouterImpl(process_.get(), client_.get(),
                                                  disposition_handler_.get(),
                                                  MSG_ROUTING_NONE, config_));
    client_->set_input_router(input_router());
    disposition_handler_->set_input_router(input_router());
  }

  void TearDown() override {
    // Process all pending tasks to avoid leaks.
    base::RunLoop().RunUntilIdle();

    input_router_.reset();
    client_.reset();
    process_.reset();
    browser_context_.reset();
  }

  void SetUpForTouchAckTimeoutTest(int desktop_timeout_ms,
                                   int mobile_timeout_ms) {
    config_.touch_config.desktop_touch_ack_timeout_delay =
        base::TimeDelta::FromMilliseconds(desktop_timeout_ms);
    config_.touch_config.mobile_touch_ack_timeout_delay =
        base::TimeDelta::FromMilliseconds(mobile_timeout_ms);
    config_.touch_config.touch_ack_timeout_supported = true;
    TearDown();
    SetUp();
    input_router()->NotifySiteIsMobileOptimized(false);
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type) {
    NativeWebKeyboardEventWithLatencyInfo key_event(
        type, WebInputEvent::kNoModifiers,
        ui::EventTimeStampToSeconds(ui::EventTimeForNow()), ui::LatencyInfo());
    input_router_->SendKeyboardEvent(key_event);
  }

  void SimulateWheelEvent(float x,
                          float y,
                          float dX,
                          float dY,
                          int modifiers,
                          bool precise) {
    input_router_->SendWheelEvent(MouseWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(x, y, dX, dY, modifiers,
                                                  precise)));
  }

  void SimulateWheelEventWithPhase(float x,
                                   float y,
                                   float dX,
                                   float dY,
                                   int modifiers,
                                   bool precise,
                                   WebMouseWheelEvent::Phase phase) {
    WebMouseWheelEvent wheel_event = SyntheticWebMouseWheelEventBuilder::Build(
        x, y, dX, dY, modifiers, precise);
    wheel_event.phase = phase;
    input_router_->SendWheelEvent(MouseWheelEventWithLatencyInfo(wheel_event));
  }

  void SimulateWheelEventPossiblyIncludingPhase(
      bool ignore_phase,
      float x,
      float y,
      float dX,
      float dY,
      int modifiers,
      bool precise,
      WebMouseWheelEvent::Phase phase) {
    if (ignore_phase)
      SimulateWheelEvent(x, y, dX, dY, modifiers, precise);
    else
      SimulateWheelEventWithPhase(x, y, dX, dY, modifiers, precise, phase);
  }

  void SimulateMouseEvent(WebInputEvent::Type type, int x, int y) {
    input_router_->SendMouseEvent(MouseEventWithLatencyInfo(
        SyntheticWebMouseEventBuilder::Build(type, x, y, 0)));
  }

  void SimulateWheelEventWithPhase(WebMouseWheelEvent::Phase phase) {
    input_router_->SendWheelEvent(MouseWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(phase)));
  }

  void SimulateGestureEvent(WebGestureEvent gesture) {
    if (gesture.GetType() == WebInputEvent::kGestureScrollBegin &&
        gesture.source_device == blink::kWebGestureDeviceTouchscreen &&
        !gesture.data.scroll_begin.delta_x_hint &&
        !gesture.data.scroll_begin.delta_y_hint) {
      // Ensure non-zero scroll-begin offset-hint to make the event sane,
      // prevents unexpected filtering at TouchActionFilter.
      gesture.data.scroll_begin.delta_y_hint = 2.f;
    } else if (gesture.GetType() == WebInputEvent::kGestureFlingStart &&
               gesture.source_device == blink::kWebGestureDeviceTouchscreen &&
               !gesture.data.fling_start.velocity_x &&
               !gesture.data.fling_start.velocity_y) {
      // Ensure non-zero touchscreen fling velocities, as the router will
      // validate against such.
      gesture.data.fling_start.velocity_x = 5.f;
    }

    input_router_->SendGestureEvent(GestureEventWithLatencyInfo(gesture));
  }

  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureDevice source_device) {
    SimulateGestureEvent(
        SyntheticWebGestureEventBuilder::Build(type, source_device));
  }

  void SimulateGestureScrollUpdateEvent(float dX,
                                        float dY,
                                        int modifiers,
                                        WebGestureDevice source_device) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollUpdate(
        dX, dY, modifiers, source_device));
  }

  void SimulateGesturePinchUpdateEvent(float scale,
                                       float anchor_x,
                                       float anchor_y,
                                       int modifiers,
                                       WebGestureDevice source_device) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildPinchUpdate(
        scale, anchor_x, anchor_y, modifiers, source_device));
  }

  void SimulateGestureFlingStartEvent(float velocity_x,
                                      float velocity_y,
                                      WebGestureDevice source_device) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildFling(
        velocity_x, velocity_y, source_device));
  }

  void SetTouchTimestamp(base::TimeTicks timestamp) {
    touch_event_.SetTimestamp(timestamp);
  }

  uint32_t SendTouchEvent() {
    uint32_t touch_event_id = touch_event_.unique_touch_event_id;
    input_router_->SendTouchEvent(TouchEventWithLatencyInfo(touch_event_));
    touch_event_.ResetPoints();
    return touch_event_id;
  }

  int PressTouchPoint(int x, int y) { return touch_event_.PressPoint(x, y); }

  void MoveTouchPoint(int index, int x, int y) {
    touch_event_.MovePoint(index, x, y);
  }

  void ReleaseTouchPoint(int index) { touch_event_.ReleasePoint(index); }

  void CancelTouchPoint(int index) { touch_event_.CancelPoint(index); }

  void SendInputEventACK(blink::WebInputEvent::Type type,
                         InputEventAckState ack_result) {
    DCHECK(!WebInputEvent::IsTouchEventType(type));
    InputEventAck ack(InputEventAckSource::COMPOSITOR_THREAD, type, ack_result);
    input_router_->OnMessageReceived(InputHostMsg_HandleInputEvent_ACK(0, ack));
  }

  void SendScrollBeginAckIfNeeded(InputEventAckState ack_result) {
    if (wheel_scroll_latching_enabled_) {
      // GSB events are blocking, send the ack.
      SendInputEventACK(WebInputEvent::kGestureScrollBegin, ack_result);
    }
  }

  void SendTouchEventACK(blink::WebInputEvent::Type type,
                         InputEventAckState ack_result,
                         uint32_t touch_event_id) {
    DCHECK(WebInputEvent::IsTouchEventType(type));
    InputEventAck ack(InputEventAckSource::COMPOSITOR_THREAD, type, ack_result,
                      touch_event_id);
    input_router_->OnMessageReceived(InputHostMsg_HandleInputEvent_ACK(0, ack));
  }

  LegacyInputRouterImpl* input_router() const { return input_router_.get(); }

  bool TouchEventQueueEmpty() const {
    return input_router()->touch_event_queue_->Empty();
  }

  bool TouchEventTimeoutEnabled() const {
    return input_router()->touch_event_queue_->IsAckTimeoutEnabled();
  }

  bool HasPendingEvents() const { return input_router_->HasPendingEvents(); }

  void OnHasTouchEventHandlers(bool has_handlers) {
    input_router_->OnMessageReceived(
        ViewHostMsg_HasTouchEventHandlers(0, has_handlers));
  }

  void OnSetTouchAction(cc::TouchAction touch_action) {
    input_router_->OnMessageReceived(
        InputHostMsg_SetTouchAction(0, touch_action));
  }

  void OnSetWhiteListedTouchAction(cc::TouchAction white_listed_touch_action,
                                   uint32_t unique_touch_event_id,
                                   InputEventAckState ack_result) {
    input_router_->OnMessageReceived(InputHostMsg_SetWhiteListedTouchAction(
        0, white_listed_touch_action, unique_touch_event_id, ack_result));
  }

  size_t GetSentMessageCountAndResetSink() {
    size_t count = process_->sink().message_count();
    process_->sink().ClearMessages();
    return count;
  }

  static void RunTasksAndWait(base::TimeDelta delay) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(), delay);
    base::RunLoop().Run();
  }

  void UnhandledWheelEvent();

  void OverscrollDispatch();

  InputRouter::Config config_;
  std::unique_ptr<MockRenderProcessHost> process_;
  std::unique_ptr<MockInputRouterClient> client_;
  std::unique_ptr<MockInputDispositionHandler> disposition_handler_;
  std::unique_ptr<LegacyInputRouterImpl> input_router_;
  bool wheel_scroll_latching_enabled_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  SyntheticWebTouchEvent touch_event_;

  base::test::ScopedFeatureList vsync_feature_list_;
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<TestBrowserContext> browser_context_;
};

class LegacyInputRouterImplWheelScrollLatchingDisabledTest
    : public LegacyInputRouterImplTest {
 public:
  LegacyInputRouterImplWheelScrollLatchingDisabledTest()
      : LegacyInputRouterImplTest(kWheelScrollingModeNone) {}
};

class LegacyInputRouterImplAsyncWheelEventEnabledTest
    : public LegacyInputRouterImplTest {
 public:
  LegacyInputRouterImplAsyncWheelEventEnabledTest()
      : LegacyInputRouterImplTest(kAsyncWheelEvents) {}
};

TEST_F(LegacyInputRouterImplTest, CoalescesRangeSelection) {
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(1, 2), gfx::Point(3, 4))));
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0), gfx::Point(1, 2), gfx::Point(3, 4));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Send two more messages without acking.
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(5, 6), gfx::Point(7, 8))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(9, 10), gfx::Point(11, 12))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Now ack the first message.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two messages are coalesced into one message.
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0), gfx::Point(9, 10), gfx::Point(11, 12));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Acking the coalesced msg should not send any more msg.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
}

TEST_F(LegacyInputRouterImplTest, CoalescesMoveRangeSelectionExtent) {
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(1, 2))));
  ExpectIPCMessageWithArg1<InputMsg_MoveRangeSelectionExtent>(
      process_->sink().GetMessageAt(0), gfx::Point(1, 2));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Send two more messages without acking.
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(3, 4))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(5, 6))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Now ack the first message.
  {
    std::unique_ptr<IPC::Message> response(
        new InputHostMsg_MoveRangeSelectionExtent_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two messages are coalesced into one message.
  ExpectIPCMessageWithArg1<InputMsg_MoveRangeSelectionExtent>(
      process_->sink().GetMessageAt(0), gfx::Point(5, 6));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Acking the coalesced msg should not send any more msg.
  {
    std::unique_ptr<IPC::Message> response(
        new InputHostMsg_MoveRangeSelectionExtent_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
}

TEST_F(LegacyInputRouterImplTest,
       InterleaveSelectRangeAndMoveRangeSelectionExtent) {
  // Send first message: SelectRange.
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(1, 2), gfx::Point(3, 4))));
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0), gfx::Point(1, 2), gfx::Point(3, 4));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Send second message: MoveRangeSelectionExtent.
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(5, 6))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Send third message: SelectRange.
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(7, 8), gfx::Point(9, 10))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Ack the messages and verify that they're not coalesced and that they're
  // in correct order.

  // Ack the first message.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  ExpectIPCMessageWithArg1<InputMsg_MoveRangeSelectionExtent>(
      process_->sink().GetMessageAt(0), gfx::Point(5, 6));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Ack the second message.
  {
    std::unique_ptr<IPC::Message> response(
        new InputHostMsg_MoveRangeSelectionExtent_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0), gfx::Point(7, 8), gfx::Point(9, 10));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Ack the third message.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
}

TEST_F(LegacyInputRouterImplTest,
       CoalescesInterleavedSelectRangeAndMoveRangeSelectionExtent) {
  // Send interleaved SelectRange and MoveRangeSelectionExtent messages. They
  // should be coalesced as shown by the arrows.
  //  > SelectRange
  //    MoveRangeSelectionExtent
  //    MoveRangeSelectionExtent
  //  > MoveRangeSelectionExtent
  //    SelectRange
  //  > SelectRange
  //  > MoveRangeSelectionExtent

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(1, 2), gfx::Point(3, 4))));
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0), gfx::Point(1, 2), gfx::Point(3, 4));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(5, 6))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(7, 8))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(9, 10))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(11, 12), gfx::Point(13, 14))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(15, 16), gfx::Point(17, 18))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(19, 20))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Ack the first message.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the three MoveRangeSelectionExtent messages are coalesced
  // into one message.
  ExpectIPCMessageWithArg1<InputMsg_MoveRangeSelectionExtent>(
      process_->sink().GetMessageAt(0), gfx::Point(9, 10));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Ack the second message.
  {
    std::unique_ptr<IPC::Message> response(
        new InputHostMsg_MoveRangeSelectionExtent_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two SelectRange messages are coalesced into one message.
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0), gfx::Point(15, 16), gfx::Point(17, 18));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Ack the third message.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify the fourth message.
  ExpectIPCMessageWithArg1<InputMsg_MoveRangeSelectionExtent>(
      process_->sink().GetMessageAt(0), gfx::Point(19, 20));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Ack the fourth message.
  {
    std::unique_ptr<IPC::Message> response(
        new InputHostMsg_MoveRangeSelectionExtent_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
}

TEST_F(LegacyInputRouterImplTest, CoalescesCaretMove) {
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveCaret(0, gfx::Point(1, 2))));
  ExpectIPCMessageWithArg1<InputMsg_MoveCaret>(process_->sink().GetMessageAt(0),
                                               gfx::Point(1, 2));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Send two more messages without acking.
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveCaret(0, gfx::Point(5, 6))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveCaret(0, gfx::Point(9, 10))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Now ack the first message.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_MoveCaret_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two messages are coalesced into one message.
  ExpectIPCMessageWithArg1<InputMsg_MoveCaret>(process_->sink().GetMessageAt(0),
                                               gfx::Point(9, 10));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Acking the coalesced msg should not send any more msg.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_MoveCaret_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
}

TEST_F(LegacyInputRouterImplTest, HandledInputEvent) {
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure no input event is sent to the renderer.
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // OnKeyboardEventAck should be triggered without actual ack.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
}

TEST_F(LegacyInputRouterImplTest, ClientCanceledKeyboardEvent) {
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  // Simulate a keyboard event that has no consumer.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure no input event is sent to the renderer.
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // Simulate a keyboard event that should be dropped.
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_UNKNOWN);
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure no input event is sent to the renderer, and no ack is sent.
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
}

TEST_F(LegacyInputRouterImplTest, NoncorrespondingKeyEvents) {
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  SendInputEventACK(WebInputEvent::kKeyUp, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(disposition_handler_->unexpected_event_ack_called());
}

// Tests ported from RenderWidgetHostTest -------------------------------------

TEST_F(LegacyInputRouterImplTest, HandleKeyEventsWeSent) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::kRawKeyDown,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            disposition_handler_->acked_keyboard_event().GetType());
}

TEST_F(LegacyInputRouterImplTest, IgnoreKeyEventsWeDidntSend) {
  // Send a simulated, unrequested key response. We should ignore this.
  SendInputEventACK(WebInputEvent::kRawKeyDown,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
}

TEST_F(LegacyInputRouterImplTest, CoalescesWheelEvents) {
  // Simulate wheel events.
  SimulateWheelEventPossiblyIncludingPhase(
      !wheel_scroll_latching_enabled_, 0, 0, 0, -5, 0, false,
      WebMouseWheelEvent::kPhaseBegan);  // sent directly
  SimulateWheelEventPossiblyIncludingPhase(
      !wheel_scroll_latching_enabled_, 0, 0, 0, -10, 0, false,
      WebMouseWheelEvent::kPhaseChanged);  // enqueued
  SimulateWheelEventPossiblyIncludingPhase(
      !wheel_scroll_latching_enabled_, 0, 0, 8, -6, 0, false,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEventPossiblyIncludingPhase(
      !wheel_scroll_latching_enabled_, 0, 0, 9, -7, 1, false,
      WebMouseWheelEvent::kPhaseChanged);  // enqueued, different modifiers
  SimulateWheelEventPossiblyIncludingPhase(
      !wheel_scroll_latching_enabled_, 0, 0, 0, -10, 0, false,
      WebMouseWheelEvent::kPhaseChanged);  // enqueued, different modifiers
  // Explicitly verify that PhaseEnd isn't coalesced to avoid bugs like
  // https://crbug.com/154740.
  SimulateWheelEventWithPhase(WebMouseWheelEvent::kPhaseEnded);  // enqueued

  // Check that only the first event was sent.
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(InputMsg_HandleInputEvent::ID));
  const WebInputEvent* input_event =
      GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::kMouseWheel, input_event->GetType());
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(0, wheel_event->delta_x);
  EXPECT_EQ(-5, wheel_event->delta_y);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Check that the ACK sends the second message immediately.
  SendInputEventACK(WebInputEvent::kMouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);
  // The coalesced events can queue up a delayed ack
  // so that additional input events can be processed before
  // we turn off coalescing.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(InputMsg_HandleInputEvent::ID));
  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::kMouseWheel, input_event->GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(8, wheel_event->delta_x);
  EXPECT_EQ(-10 + -6, wheel_event->delta_y);  // coalesced
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Ack the second event (which had the third coalesced into it).
  SendInputEventACK(WebInputEvent::kMouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(InputMsg_HandleInputEvent::ID));
  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::kMouseWheel, input_event->GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(9, wheel_event->delta_x);
  EXPECT_EQ(-7, wheel_event->delta_y);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Ack the fourth event.
  SendInputEventACK(WebInputEvent::kMouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(InputMsg_HandleInputEvent::ID));
  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::kMouseWheel, input_event->GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(0, wheel_event->delta_x);
  EXPECT_EQ(-10, wheel_event->delta_y);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Ack the fifth event.
  SendInputEventACK(WebInputEvent::kMouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(InputMsg_HandleInputEvent::ID));
  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::kMouseWheel, input_event->GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(0, wheel_event->delta_x);
  EXPECT_EQ(0, wheel_event->delta_y);
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_event->phase);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // After the final ack, the queue should be empty.
  SendInputEventACK(WebInputEvent::kMouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
}

// Tests that touch-events are sent properly.
TEST_F(LegacyInputRouterImplTest, TouchEventQueue) {
  OnHasTouchEventHandlers(true);

  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id = SendTouchEvent();
  EXPECT_TRUE(client_->GetAndResetFilterEventCalled());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // The second touch should be sent right away.
  MoveTouchPoint(0, 5, 5);
  uint32_t touch_move_event_id = SendTouchEvent();
  EXPECT_TRUE(client_->GetAndResetFilterEventCalled());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // Receive an ACK for the first touch-event.
  SendTouchEventACK(WebInputEvent::kTouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id);
  EXPECT_FALSE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kTouchStart,
            disposition_handler_->acked_touch_event().event.GetType());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  SendTouchEventACK(WebInputEvent::kTouchMove, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_move_event_id);
  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kTouchMove,
            disposition_handler_->acked_touch_event().event.GetType());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
}

// Tests that the touch-queue is emptied after a page stops listening for
// touch events and the outstanding ack is received.
TEST_F(LegacyInputRouterImplTest, TouchEventQueueFlush) {
  OnHasTouchEventHandlers(true);
  EXPECT_TRUE(client_->has_touch_handler());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(TouchEventQueueEmpty());

  // Send a touch-press event.
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id = SendTouchEvent();
  MoveTouchPoint(0, 2, 2);
  MoveTouchPoint(0, 3, 3);
  EXPECT_FALSE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // The page stops listening for touch-events. Note that flushing is deferred
  // until the outstanding ack is received.
  OnHasTouchEventHandlers(false);
  EXPECT_FALSE(client_->has_touch_handler());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // After the ack, the touch-event queue should be empty, and none of the
  // flushed touch-events should have been sent to the renderer.
  SendTouchEventACK(WebInputEvent::kTouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(TouchEventQueueEmpty());
}

void LegacyInputRouterImplTest::UnhandledWheelEvent() {
  // Simulate wheel events.
  SimulateWheelEventPossiblyIncludingPhase(!wheel_scroll_latching_enabled_, 0,
                                           0, 0, -5, 0, false,
                                           WebMouseWheelEvent::kPhaseBegan);
  SimulateWheelEventPossiblyIncludingPhase(!wheel_scroll_latching_enabled_, 0,
                                           0, 0, -10, 0, false,
                                           WebMouseWheelEvent::kPhaseChanged);

  // Check that only the first event was sent.
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(InputMsg_HandleInputEvent::ID));
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Indicate that the wheel event was unhandled.
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the ack for the MouseWheel, ScrollBegin were processed.
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());

  if (wheel_scroll_latching_enabled_) {
    // There should be a ScrollBegin, ScrollUpdate, and MouseWheel sent.
    EXPECT_EQ("GestureScrollBegin GestureScrollUpdate MouseWheel",
              GetInputMessageTypes(process_.get()));
  } else {
    // There should be a ScrollBegin, ScrollUpdate, ScrollEnd, and MouseWheel
    // sent.
    EXPECT_EQ(
        "GestureScrollBegin GestureScrollUpdate GestureScrollEnd MouseWheel",
        GetInputMessageTypes(process_.get()));
  }

  EXPECT_EQ(disposition_handler_->acked_wheel_event().delta_y, -5);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  if (wheel_scroll_latching_enabled_) {
    // Check that the ack for ScrollUpdate were processed.
    EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  } else {
    // GestureScrollEnd should have already been sent.
    EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

    // Check that the ack for the ScrollUpdate and ScrollEnd were processed.
    EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  }

  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  if (wheel_scroll_latching_enabled_) {
    // There should be a ScrollUpdate sent.
    EXPECT_EQ("GestureScrollUpdate", GetInputMessageTypes(process_.get()));
    EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  } else {
    // There should be a ScrollBegin and ScrollUpdate sent.
    EXPECT_EQ("GestureScrollBegin GestureScrollUpdate GestureScrollEnd",
              GetInputMessageTypes(process_.get()));
    EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  }

  // Check that the correct unhandled wheel event was received.
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
            disposition_handler_->acked_wheel_event_state());
  EXPECT_EQ(disposition_handler_->acked_wheel_event().delta_y, -10);

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  if (wheel_scroll_latching_enabled_) {
    // Check that the ack for ScrollUpdate were processed.
    EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  } else {
    // GestureScrollEnd should have already been sent.
    EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

    // Check that the ack for the ScrollUpdate and ScrollEnd were processed.
    EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  }
}

TEST_F(LegacyInputRouterImplTest, UnhandledWheelEvent) {
  UnhandledWheelEvent();
}
TEST_F(LegacyInputRouterImplWheelScrollLatchingDisabledTest,
       UnhandledWheelEvent) {
  UnhandledWheelEvent();
}
TEST_F(LegacyInputRouterImplAsyncWheelEventEnabledTest, UnhandledWheelEvent) {
  // Simulate wheel events.
  SimulateWheelEventWithPhase(0, 0, 0, -5, 0, false,
                              WebMouseWheelEvent::kPhaseBegan);
  SimulateWheelEventWithPhase(0, 0, 0, -10, 0, false,
                              WebMouseWheelEvent::kPhaseChanged);

  // Check that only the first event was sent.
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(InputMsg_HandleInputEvent::ID));
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Indicate that the wheel event was unhandled.
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the ack for the first MouseWheel, ScrollBegin, and the second
  // MouseWheel were processed.
  EXPECT_EQ(3U, disposition_handler_->GetAndResetAckCount());

  // There should be a ScrollBegin, MouseWheel, and two ScrollUpdate sent.
  EXPECT_EQ(4U, GetSentMessageCountAndResetSink());

  // The last acked wheel event should be the second one since the input router
  // has already sent the immediate ack for the second wheel event.
  EXPECT_EQ(disposition_handler_->acked_wheel_event().delta_y, -10);
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_IGNORED,
            disposition_handler_->acked_wheel_event_state());

  // Ack the first ScrollUpdate.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(
      -5,
      disposition_handler_->acked_gesture_event().data.scroll_update.delta_y);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // Ack the second ScrollUpdate.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(
      -10,
      disposition_handler_->acked_gesture_event().data.scroll_update.delta_y);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
}

TEST_F(LegacyInputRouterImplTest, TouchTypesIgnoringAck) {
  OnHasTouchEventHandlers(true);
  // Only acks for TouchCancel should always be ignored.
  ASSERT_TRUE(
      ShouldBlockEventStream(GetEventWithType(WebInputEvent::kTouchStart)));
  ASSERT_TRUE(
      ShouldBlockEventStream(GetEventWithType(WebInputEvent::kTouchMove)));
  ASSERT_TRUE(
      ShouldBlockEventStream(GetEventWithType(WebInputEvent::kTouchEnd)));

  // Precede the TouchCancel with an appropriate TouchStart;
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::kTouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id);
  ASSERT_EQ(1U, GetSentMessageCountAndResetSink());
  ASSERT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  ASSERT_EQ(0, client_->in_flight_event_count());

  // The TouchCancel ack is always ignored.
  CancelTouchPoint(0);
  uint32_t touch_cancel_event_id = SendTouchEvent();
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());
  EXPECT_FALSE(HasPendingEvents());
  SendTouchEventACK(WebInputEvent::kTouchCancel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED, touch_cancel_event_id);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_FALSE(HasPendingEvents());
}

TEST_F(LegacyInputRouterImplTest, GestureTypesIgnoringAck) {
  // We test every gesture type, ensuring that the stream of gestures is valid.
  const WebInputEvent::Type eventTypes[] = {
      WebInputEvent::kGestureTapDown,     WebInputEvent::kGestureShowPress,
      WebInputEvent::kGestureTapCancel,   WebInputEvent::kGestureScrollBegin,
      WebInputEvent::kGestureFlingStart,  WebInputEvent::kGestureFlingCancel,
      WebInputEvent::kGestureTapDown,     WebInputEvent::kGestureTap,
      WebInputEvent::kGestureTapDown,     WebInputEvent::kGestureLongPress,
      WebInputEvent::kGestureTapCancel,   WebInputEvent::kGestureLongTap,
      WebInputEvent::kGestureTapDown,     WebInputEvent::kGestureTapUnconfirmed,
      WebInputEvent::kGestureTapCancel,   WebInputEvent::kGestureTapDown,
      WebInputEvent::kGestureDoubleTap,   WebInputEvent::kGestureTapDown,
      WebInputEvent::kGestureTapCancel,   WebInputEvent::kGestureTwoFingerTap,
      WebInputEvent::kGestureTapDown,     WebInputEvent::kGestureTapCancel,
      WebInputEvent::kGestureScrollBegin, WebInputEvent::kGestureScrollUpdate,
      WebInputEvent::kGesturePinchBegin,  WebInputEvent::kGesturePinchUpdate,
      WebInputEvent::kGesturePinchEnd,    WebInputEvent::kGestureScrollEnd};
  for (size_t i = 0; i < arraysize(eventTypes); ++i) {
    WebInputEvent::Type type = eventTypes[i];
    if (ShouldBlockEventStream(GetEventWithType(type))) {
      SimulateGestureEvent(type, blink::kWebGestureDeviceTouchscreen);
      if (type == WebInputEvent::kGestureScrollUpdate)
        EXPECT_EQ(2U, GetSentMessageCountAndResetSink());
      else
        EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
      EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
      EXPECT_EQ(1, client_->in_flight_event_count());
      EXPECT_TRUE(HasPendingEvents());

      SendInputEventACK(type, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
      EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
      EXPECT_EQ(0, client_->in_flight_event_count());
      EXPECT_FALSE(HasPendingEvents());
      continue;
    }

    SimulateGestureEvent(type, blink::kWebGestureDeviceTouchscreen);
    EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
    EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
    EXPECT_EQ(0, client_->in_flight_event_count());
    EXPECT_FALSE(HasPendingEvents());
  }
}

TEST_F(LegacyInputRouterImplTest, MouseTypesIgnoringAck) {
  int start_type = static_cast<int>(WebInputEvent::kMouseDown);
  int end_type = static_cast<int>(WebInputEvent::kContextMenu);
  ASSERT_LT(start_type, end_type);
  for (int i = start_type; i <= end_type; ++i) {
    WebInputEvent::Type type = static_cast<WebInputEvent::Type>(i);

    SimulateMouseEvent(type, 0, 0);
    EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

    if (ShouldBlockEventStream(GetEventWithType(type))) {
      EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
      EXPECT_EQ(1, client_->in_flight_event_count());

      SendInputEventACK(type, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
      EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
      EXPECT_EQ(0, client_->in_flight_event_count());
    } else {
      // Note: events which don't block the event stream immediately receive
      // synthetic ACKs.
      EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
      EXPECT_EQ(0, client_->in_flight_event_count());
    }
  }
}

// Guard against breaking changes to the list of ignored event ack types in
// |WebInputEventTraits::ShouldBlockEventStream|.
TEST_F(LegacyInputRouterImplTest, RequiredEventAckTypes) {
  const WebInputEvent::Type kRequiredEventAckTypes[] = {
      WebInputEvent::kMouseMove,
      WebInputEvent::kMouseWheel,
      WebInputEvent::kRawKeyDown,
      WebInputEvent::kKeyDown,
      WebInputEvent::kKeyUp,
      WebInputEvent::kChar,
      WebInputEvent::kGestureScrollUpdate,
      WebInputEvent::kGestureFlingStart,
      WebInputEvent::kGestureFlingCancel,
      WebInputEvent::kGesturePinchUpdate,
      WebInputEvent::kTouchStart,
      WebInputEvent::kTouchMove};
  for (size_t i = 0; i < arraysize(kRequiredEventAckTypes); ++i) {
    const WebInputEvent::Type required_ack_type = kRequiredEventAckTypes[i];
    ASSERT_TRUE(ShouldBlockEventStream(GetEventWithType(required_ack_type)));
  }
}

TEST_F(LegacyInputRouterImplTest, GestureTypesIgnoringAckInterleaved) {
  // Interleave a few events that do and do not ignore acks. All gesture events
  // should be dispatched immediately, but the acks will be blocked on blocking
  // events.

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  ASSERT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  ASSERT_EQ(2U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(2, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::kGestureShowPress,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(2, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(3, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::kGestureTapCancel,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(3, client_->in_flight_event_count());

  // Now ack each ack-respecting event. Should see in-flight event count
  // decreasing and additional acks coming back.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(2, client_->in_flight_event_count());

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());
}

// Test that GestureShowPress events don't get out of order due to
// ignoring their acks.
TEST_F(LegacyInputRouterImplTest, GestureShowPressIsInOrder) {
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // GesturePinchBegin ignores its ack.
  SimulateGestureEvent(WebInputEvent::kGesturePinchBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // GesturePinchUpdate waits for an ack.
  // This also verifies that GesturePinchUpdates for touchscreen are sent
  // to the renderer (in contrast to the TrackpadPinchUpdate test).
  SimulateGestureEvent(WebInputEvent::kGesturePinchUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  // GestureShowPress will be sent immediately since GestureEventQueue allows
  // multiple in-flight events. However the acks will be blocked on outstanding
  // in-flight events.
  SimulateGestureEvent(WebInputEvent::kGestureShowPress,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::kGestureShowPress,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  // Ack the GesturePinchUpdate to release two GestureShowPress ack.
  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(3U, disposition_handler_->GetAndResetAckCount());
}

// Test that touch ack timeout behavior is properly configured for
// mobile-optimized sites and allowed touch actions.
TEST_F(LegacyInputRouterImplTest, TouchAckTimeoutConfigured) {
  const int kDesktopTimeoutMs = 1;
  const int kMobileTimeoutMs = 0;
  SetUpForTouchAckTimeoutTest(kDesktopTimeoutMs, kMobileTimeoutMs);
  ASSERT_TRUE(TouchEventTimeoutEnabled());

  // Verify that the touch ack timeout fires upon the delayed ack.
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id1 = SendTouchEvent();
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(kDesktopTimeoutMs + 1));

  // The timed-out event should have been ack'ed.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // Ack'ing the timed-out event should fire a TouchCancel.
  SendTouchEventACK(WebInputEvent::kTouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id1);
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // The remainder of the touch sequence should be dropped.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  ASSERT_TRUE(TouchEventTimeoutEnabled());

  // A mobile-optimized site should use the mobile timeout. For this test that
  // timeout value is 0, which disables the timeout.
  input_router()->NotifySiteIsMobileOptimized(true);
  EXPECT_FALSE(TouchEventTimeoutEnabled());

  input_router()->NotifySiteIsMobileOptimized(false);
  EXPECT_TRUE(TouchEventTimeoutEnabled());

  // kTouchActionNone (and no other touch-action) should disable the timeout.
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id2 = SendTouchEvent();
  OnSetTouchAction(cc::kTouchActionPanY);
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id2 = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::kTouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id2);
  SendTouchEventACK(WebInputEvent::kTouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_release_event_id2);

  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id3 = SendTouchEvent();
  OnSetTouchAction(cc::kTouchActionNone);
  EXPECT_FALSE(TouchEventTimeoutEnabled());
  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id3 = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::kTouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id3);
  SendTouchEventACK(WebInputEvent::kTouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_release_event_id3);

  // As the touch-action is reset by a new touch sequence, the timeout behavior
  // should be restored.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(TouchEventTimeoutEnabled());
}

// Test that a touch sequenced preceded by kTouchActionNone is not affected by
// the touch timeout.
TEST_F(LegacyInputRouterImplTest,
       TouchAckTimeoutDisabledForTouchSequenceAfterTouchActionNone) {
  const int kDesktopTimeoutMs = 1;
  const int kMobileTimeoutMs = 2;
  SetUpForTouchAckTimeoutTest(kDesktopTimeoutMs, kMobileTimeoutMs);
  ASSERT_TRUE(TouchEventTimeoutEnabled());
  OnHasTouchEventHandlers(true);

  // Start a touch sequence.
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id = SendTouchEvent();
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // kTouchActionNone should disable the timeout.
  OnSetTouchAction(cc::kTouchActionNone);
  SendTouchEventACK(WebInputEvent::kTouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_FALSE(TouchEventTimeoutEnabled());

  MoveTouchPoint(0, 1, 2);
  uint32_t touch_move_event_id = SendTouchEvent();
  EXPECT_FALSE(TouchEventTimeoutEnabled());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Delay the move ack. The timeout should not fire.
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(kDesktopTimeoutMs + 1));
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  SendTouchEventACK(WebInputEvent::kTouchMove, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_move_event_id);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // End the touch sequence.
  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::kTouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_release_event_id);
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  disposition_handler_->GetAndResetAckCount();
  GetSentMessageCountAndResetSink();

  // Start another touch sequence.  This should restore the touch timeout.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  // Wait for the touch ack timeout to fire.
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(kDesktopTimeoutMs + 1));
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
}

// Test that TouchActionFilter::ResetTouchAction is called before the
// first touch event for a touch sequence reaches the renderer.
TEST_F(LegacyInputRouterImplTest, TouchActionResetBeforeEventReachesRenderer) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id1 = SendTouchEvent();
  OnSetTouchAction(cc::kTouchActionNone);
  MoveTouchPoint(0, 50, 50);
  uint32_t touch_move_event_id1 = SendTouchEvent();
  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id1 = SendTouchEvent();

  // Sequence 2.
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id2 = SendTouchEvent();
  MoveTouchPoint(0, 50, 50);
  uint32_t touch_move_event_id2 = SendTouchEvent();
  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id2 = SendTouchEvent();

  SendTouchEventACK(WebInputEvent::kTouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id1);
  SendTouchEventACK(WebInputEvent::kTouchMove, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_move_event_id1);

  // Ensure touch action is still none, as the next touch start hasn't been
  // acked yet. ScrollBegin and ScrollEnd don't require acks.
  EXPECT_EQ(6U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // This allows the next touch sequence to start.
  SendTouchEventACK(WebInputEvent::kTouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_release_event_id1);

  // Ensure touch action has been set to auto, as a new touch sequence has
  // started.
  SendTouchEventACK(WebInputEvent::kTouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id2);
  SendTouchEventACK(WebInputEvent::kTouchMove, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_move_event_id2);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SendTouchEventACK(WebInputEvent::kTouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_release_event_id2);
}

// Test that TouchActionFilter::ResetTouchAction is called when a new touch
// sequence has no consumer.
TEST_F(LegacyInputRouterImplTest, TouchActionResetWhenTouchHasNoConsumer) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id1 = SendTouchEvent();
  MoveTouchPoint(0, 50, 50);
  uint32_t touch_move_event_id1 = SendTouchEvent();
  OnSetTouchAction(cc::kTouchActionNone);
  SendTouchEventACK(WebInputEvent::kTouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id1);
  SendTouchEventACK(WebInputEvent::kTouchMove, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_move_event_id1);

  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id1 = SendTouchEvent();

  // Sequence 2
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id2 = SendTouchEvent();
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  ReleaseTouchPoint(0);
  SendTouchEvent();

  // Ensure we have touch-action:none. ScrollBegin and ScrollEnd don't require
  // acks.
  EXPECT_EQ(6U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  SendTouchEventACK(WebInputEvent::kTouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_release_event_id1);
  SendTouchEventACK(WebInputEvent::kTouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
                    touch_press_event_id2);

  // Ensure touch action has been set to auto, as the touch had no consumer.
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
}

// Test that TouchActionFilter::ResetTouchAction is called when the touch
// handler is removed.
TEST_F(LegacyInputRouterImplTest, TouchActionResetWhenTouchHandlerRemoved) {
  // Touch sequence with touch handler.
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id = SendTouchEvent();
  MoveTouchPoint(0, 50, 50);
  uint32_t touch_move_event_id = SendTouchEvent();
  OnSetTouchAction(cc::kTouchActionNone);
  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id = SendTouchEvent();
  EXPECT_EQ(3U, GetSentMessageCountAndResetSink());

  // Ensure we have touch-action:none, suppressing scroll events.
  SendTouchEventACK(WebInputEvent::kTouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  SendTouchEventACK(WebInputEvent::kTouchMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED, touch_move_event_id);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  SendTouchEventACK(WebInputEvent::kTouchEnd,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED, touch_release_event_id);
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // Sequence without a touch handler. Note that in this case, the view may not
  // necessarily forward touches to the router (as no touch handler exists).
  OnHasTouchEventHandlers(false);

  // Ensure touch action has been set to auto, as the touch handler has been
  // removed.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
}

// Tests that async touch-moves are ack'd from the browser side.
TEST_F(LegacyInputRouterImplTest, AsyncTouchMoveAckedImmediately) {
  OnHasTouchEventHandlers(true);

  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id = SendTouchEvent();
  EXPECT_TRUE(client_->GetAndResetFilterEventCalled());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // Receive an ACK for the first touch-event.
  SendTouchEventACK(WebInputEvent::kTouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(2U, GetSentMessageCountAndResetSink());

  // Now send an async move.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
}

// Test that the double tap gesture depends on the touch action of the first
// tap.
TEST_F(LegacyInputRouterImplTest, DoubleTapGestureDependsOnFirstTap) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id1 = SendTouchEvent();
  OnSetTouchAction(cc::kTouchActionNone);
  SendTouchEventACK(WebInputEvent::kTouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id1);

  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id = SendTouchEvent();

  // Sequence 2
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id2 = SendTouchEvent();

  // First tap.
  EXPECT_EQ(3U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // The GestureTapUnconfirmed is converted into a tap, as the touch action is
  // none.
  SimulateGestureEvent(WebInputEvent::kGestureTapUnconfirmed,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  // This test will become invalid if GestureTap stops requiring an ack.
  ASSERT_TRUE(
      ShouldBlockEventStream(GetEventWithType(WebInputEvent::kGestureTap)));
  EXPECT_EQ(3, client_->in_flight_event_count());
  SendInputEventACK(WebInputEvent::kGestureTap, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2, client_->in_flight_event_count());

  // This tap gesture is dropped, since the GestureTapUnconfirmed was turned
  // into a tap.
  SimulateGestureEvent(WebInputEvent::kGestureTap,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  SendTouchEventACK(WebInputEvent::kTouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_release_event_id);
  SendTouchEventACK(WebInputEvent::kTouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
                    touch_press_event_id2);

  // Second Tap.
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Although the touch-action is now auto, the double tap still won't be
  // dispatched, because the first tap occured when the touch-action was none.
  SimulateGestureEvent(WebInputEvent::kGestureDoubleTap,
                       blink::kWebGestureDeviceTouchscreen);
  // This test will become invalid if GestureDoubleTap stops requiring an ack.
  ASSERT_TRUE(ShouldBlockEventStream(
      GetEventWithType(WebInputEvent::kGestureDoubleTap)));
  EXPECT_EQ(1, client_->in_flight_event_count());
  SendInputEventACK(WebInputEvent::kGestureTap, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0, client_->in_flight_event_count());
}

// Test that GesturePinchUpdate is handled specially for trackpad
TEST_F(LegacyInputRouterImplTest, TouchpadPinchUpdate) {
  // GesturePinchUpdate for trackpad sends synthetic wheel events.
  // Note that the Touchscreen case is verified as NOT doing this as
  // part of the ShowPressIsInOrder test.

  SimulateGesturePinchUpdateEvent(1.5f, 20, 25, 0,
                                  blink::kWebGestureDeviceTouchpad);

  // Verify we actually sent a special wheel event to the renderer.
  const WebInputEvent* input_event =
      GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::kGesturePinchUpdate, input_event->GetType());
  const WebGestureEvent* gesture_event =
      static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(20, gesture_event->x);
  EXPECT_EQ(25, gesture_event->y);
  EXPECT_EQ(20, gesture_event->global_x);
  EXPECT_EQ(25, gesture_event->global_y);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Indicate that the wheel event was unhandled.
  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Check that the correct unhandled pinch event was received.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  ASSERT_EQ(WebInputEvent::kGesturePinchUpdate,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
            disposition_handler_->ack_state());
  EXPECT_EQ(
      1.5f,
      disposition_handler_->acked_gesture_event().data.pinch_update.scale);
  EXPECT_EQ(0, client_->in_flight_event_count());

  // Second a second pinch event.
  SimulateGesturePinchUpdateEvent(0.3f, 20, 25, 0,
                                  blink::kWebGestureDeviceTouchpad);
  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::kGesturePinchUpdate, input_event->GetType());
  gesture_event = static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Indicate that the wheel event was handled this time.
  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the correct HANDLED pinch event was received.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            disposition_handler_->ack_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, disposition_handler_->ack_state());
  EXPECT_FLOAT_EQ(
      0.3f,
      disposition_handler_->acked_gesture_event().data.pinch_update.scale);
}

// Test proper handling of touchpad Gesture{Pinch,Scroll}Update sequences.
TEST_F(LegacyInputRouterImplTest, TouchpadPinchAndScrollUpdate) {
  // All gesture events should be sent immediately.
  SimulateGestureScrollUpdateEvent(1.5f, 0.f, 0,
                                   blink::kWebGestureDeviceTouchpad);
  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchpad);
  ASSERT_EQ(2U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(2, client_->in_flight_event_count());

  // Subsequent scroll and pinch events will also be sent immediately.
  SimulateGesturePinchUpdateEvent(1.5f, 20, 25, 0,
                                  blink::kWebGestureDeviceTouchpad);
  ASSERT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(3, client_->in_flight_event_count());

  SimulateGestureScrollUpdateEvent(1.5f, 1.5f, 0,
                                   blink::kWebGestureDeviceTouchpad);
  ASSERT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(4, client_->in_flight_event_count());

  SimulateGesturePinchUpdateEvent(1.5f, 20, 25, 0,
                                  blink::kWebGestureDeviceTouchpad);
  ASSERT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(5, client_->in_flight_event_count());

  SimulateGestureScrollUpdateEvent(0.f, 1.5f, 0,
                                   blink::kWebGestureDeviceTouchpad);
  ASSERT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(6, client_->in_flight_event_count());

  // Ack'ing events should decrease in-flight event count.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(5, client_->in_flight_event_count());

  // Ack the second scroll.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(4, client_->in_flight_event_count());

  // Ack the pinch event.
  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(3, client_->in_flight_event_count());

  // Ack the scroll event.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(2, client_->in_flight_event_count());

  // Ack the pinch event.
  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Ack the scroll event.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());
}

// Test proper routing of overscroll notifications received either from
// event acks or from |DidOverscroll| IPC messages.
void LegacyInputRouterImplTest::OverscrollDispatch() {
  DidOverscrollParams overscroll;
  overscroll.accumulated_overscroll = gfx::Vector2dF(-14, 14);
  overscroll.latest_overscroll_delta = gfx::Vector2dF(-7, 0);
  overscroll.current_fling_velocity = gfx::Vector2dF(-1, 0);

  input_router_->OnMessageReceived(InputHostMsg_DidOverscroll(0, overscroll));
  DidOverscrollParams client_overscroll = client_->GetAndResetOverscroll();
  EXPECT_EQ(overscroll.accumulated_overscroll,
            client_overscroll.accumulated_overscroll);
  EXPECT_EQ(overscroll.latest_overscroll_delta,
            client_overscroll.latest_overscroll_delta);
  EXPECT_EQ(overscroll.current_fling_velocity,
            client_overscroll.current_fling_velocity);

  DidOverscrollParams wheel_overscroll;
  wheel_overscroll.accumulated_overscroll = gfx::Vector2dF(7, -7);
  wheel_overscroll.latest_overscroll_delta = gfx::Vector2dF(3, 0);
  wheel_overscroll.current_fling_velocity = gfx::Vector2dF(1, 0);

  SimulateWheelEventPossiblyIncludingPhase(!wheel_scroll_latching_enabled_, 0,
                                           0, 3, 0, 0, false,
                                           WebMouseWheelEvent::kPhaseBegan);
  InputEventAck ack(InputEventAckSource::COMPOSITOR_THREAD,
                    WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ack.overscroll.reset(new DidOverscrollParams(wheel_overscroll));
  input_router_->OnMessageReceived(InputHostMsg_HandleInputEvent_ACK(0, ack));

  client_overscroll = client_->GetAndResetOverscroll();
  EXPECT_EQ(wheel_overscroll.accumulated_overscroll,
            client_overscroll.accumulated_overscroll);
  EXPECT_EQ(wheel_overscroll.latest_overscroll_delta,
            client_overscroll.latest_overscroll_delta);
  EXPECT_EQ(wheel_overscroll.current_fling_velocity,
            client_overscroll.current_fling_velocity);
}
TEST_F(LegacyInputRouterImplTest, OverscrollDispatch) {
  OverscrollDispatch();
}
TEST_F(LegacyInputRouterImplWheelScrollLatchingDisabledTest,
       OverscrollDispatch) {
  OverscrollDispatch();
}
TEST_F(LegacyInputRouterImplAsyncWheelEventEnabledTest, OverscrollDispatch) {
  OverscrollDispatch();
}

// Test proper routing of whitelisted touch action notifications received from
// |SetWhiteListedTouchAction| IPC messages.
TEST_F(LegacyInputRouterImplTest, OnSetWhiteListedTouchAction) {
  cc::TouchAction touch_action = cc::kTouchActionPanY;
  input_router_->OnMessageReceived(InputHostMsg_SetWhiteListedTouchAction(
      0, touch_action, 0, INPUT_EVENT_ACK_STATE_NOT_CONSUMED));
  cc::TouchAction white_listed_touch_action =
      client_->GetAndResetWhiteListedTouchAction();
  EXPECT_EQ(touch_action, white_listed_touch_action);
}

// Tests that touch event stream validation passes when events are filtered
// out. See crbug.com/581231 for details.
TEST_F(LegacyInputRouterImplTest,
       TouchValidationPassesWithFilteredInputEvents) {
  // Touch sequence with touch handler.
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::kTouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
                    touch_press_event_id);

  PressTouchPoint(1, 1);
  touch_press_event_id = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::kTouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
                    touch_press_event_id);

  // This event will be filtered out, since no consumer exists.
  ReleaseTouchPoint(1);
  uint32_t touch_release_event_id = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::kTouchEnd,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED, touch_release_event_id);

  // If the validator didn't see the filtered out release event, it will crash
  // now, upon seeing a press for a touch which it believes to be still pressed.
  PressTouchPoint(1, 1);
  touch_press_event_id = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::kTouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
                    touch_press_event_id);
}

namespace {

class LegacyInputRouterImplScaleEventTest : public LegacyInputRouterImplTest {
 public:
  LegacyInputRouterImplScaleEventTest() {}

  void SetUp() override {
    LegacyInputRouterImplTest::SetUp();
    input_router_->SetDeviceScaleFactor(2.f);
  }

  template <typename T>
  const T* GetSentWebInputEvent() const {
    EXPECT_EQ(1u, process_->sink().message_count());

    InputMsg_HandleInputEvent::Schema::Param param;
    InputMsg_HandleInputEvent::Read(process_->sink().GetMessageAt(0), &param);
    return static_cast<const T*>(std::get<0>(param));
  }

  template <typename T>
  const T* GetFilterWebInputEvent() const {
    return static_cast<const T*>(client_->last_filter_event());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LegacyInputRouterImplScaleEventTest);
};

class LegacyInputRouterImplScaleMouseEventTest
    : public LegacyInputRouterImplScaleEventTest {
 public:
  LegacyInputRouterImplScaleMouseEventTest() {}

  void RunMouseEventTest(const std::string& name, WebInputEvent::Type type) {
    SCOPED_TRACE(name);
    SimulateMouseEvent(type, 10, 10);
    const WebMouseEvent* sent_event = GetSentWebInputEvent<WebMouseEvent>();
    EXPECT_EQ(20, sent_event->PositionInWidget().x);
    EXPECT_EQ(20, sent_event->PositionInWidget().y);

    const WebMouseEvent* filter_event = GetFilterWebInputEvent<WebMouseEvent>();
    EXPECT_EQ(10, filter_event->PositionInWidget().x);
    EXPECT_EQ(10, filter_event->PositionInWidget().y);

    process_->sink().ClearMessages();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LegacyInputRouterImplScaleMouseEventTest);
};

}  // namespace

TEST_F(LegacyInputRouterImplScaleMouseEventTest, ScaleMouseEventTest) {
  RunMouseEventTest("Enter", WebInputEvent::kMouseEnter);
  RunMouseEventTest("Down", WebInputEvent::kMouseDown);
  RunMouseEventTest("Move", WebInputEvent::kMouseMove);
  RunMouseEventTest("Up", WebInputEvent::kMouseUp);
}

TEST_F(LegacyInputRouterImplScaleEventTest, ScaleMouseWheelEventTest) {
  ASSERT_EQ(0u, process_->sink().message_count());
  SimulateWheelEventWithPhase(5, 5, 10, 10, 0, false,
                              WebMouseWheelEvent::kPhaseBegan);
  ASSERT_EQ(1u, process_->sink().message_count());

  const WebMouseWheelEvent* sent_event =
      GetSentWebInputEvent<WebMouseWheelEvent>();
  EXPECT_EQ(10, sent_event->PositionInWidget().x);
  EXPECT_EQ(10, sent_event->PositionInWidget().y);
  EXPECT_EQ(20, sent_event->delta_x);
  EXPECT_EQ(20, sent_event->delta_y);
  EXPECT_EQ(2, sent_event->wheel_ticks_x);
  EXPECT_EQ(2, sent_event->wheel_ticks_y);

  const WebMouseWheelEvent* filter_event =
      GetFilterWebInputEvent<WebMouseWheelEvent>();
  EXPECT_EQ(5, filter_event->PositionInWidget().x);
  EXPECT_EQ(5, filter_event->PositionInWidget().y);
  EXPECT_EQ(10, filter_event->delta_x);
  EXPECT_EQ(10, filter_event->delta_y);
  EXPECT_EQ(1, filter_event->wheel_ticks_x);
  EXPECT_EQ(1, filter_event->wheel_ticks_y);

  EXPECT_EQ(sent_event->acceleration_ratio_x,
            filter_event->acceleration_ratio_x);
  EXPECT_EQ(sent_event->acceleration_ratio_y,
            filter_event->acceleration_ratio_y);
}

namespace {

class LegacyInputRouterImplScaleTouchEventTest
    : public LegacyInputRouterImplScaleEventTest {
 public:
  LegacyInputRouterImplScaleTouchEventTest() {}

  // Test tests if two finger touch event at (10, 20) and (100, 200) are
  // properly scaled. The touch event must be generated ans flushed into
  // the message sink prior to this method.
  void RunTouchEventTest(const std::string& name, WebTouchPoint::State state) {
    SCOPED_TRACE(name);
    ASSERT_EQ(1u, process_->sink().message_count());
    const WebTouchEvent* sent_event = GetSentWebInputEvent<WebTouchEvent>();
    ASSERT_EQ(2u, sent_event->touches_length);
    EXPECT_EQ(state, sent_event->touches[0].state);
    EXPECT_EQ(20, sent_event->touches[0].PositionInWidget().x);
    EXPECT_EQ(40, sent_event->touches[0].PositionInWidget().y);
    EXPECT_EQ(10, sent_event->touches[0].PositionInScreen().x);
    EXPECT_EQ(20, sent_event->touches[0].PositionInScreen().y);
    EXPECT_EQ(2, sent_event->touches[0].radius_x);
    EXPECT_EQ(2, sent_event->touches[0].radius_y);

    EXPECT_EQ(200, sent_event->touches[1].PositionInWidget().x);
    EXPECT_EQ(400, sent_event->touches[1].PositionInWidget().y);
    EXPECT_EQ(100, sent_event->touches[1].PositionInScreen().x);
    EXPECT_EQ(200, sent_event->touches[1].PositionInScreen().y);
    EXPECT_EQ(2, sent_event->touches[1].radius_x);
    EXPECT_EQ(2, sent_event->touches[1].radius_y);

    const WebTouchEvent* filter_event = GetFilterWebInputEvent<WebTouchEvent>();
    ASSERT_EQ(2u, filter_event->touches_length);
    EXPECT_EQ(10, filter_event->touches[0].PositionInWidget().x);
    EXPECT_EQ(20, filter_event->touches[0].PositionInWidget().y);
    EXPECT_EQ(10, filter_event->touches[0].PositionInScreen().x);
    EXPECT_EQ(20, filter_event->touches[0].PositionInScreen().y);
    EXPECT_EQ(1, filter_event->touches[0].radius_x);
    EXPECT_EQ(1, filter_event->touches[0].radius_y);

    EXPECT_EQ(100, filter_event->touches[1].PositionInWidget().x);
    EXPECT_EQ(200, filter_event->touches[1].PositionInWidget().y);
    EXPECT_EQ(100, filter_event->touches[1].PositionInScreen().x);
    EXPECT_EQ(200, filter_event->touches[1].PositionInScreen().y);
    EXPECT_EQ(1, filter_event->touches[1].radius_x);
    EXPECT_EQ(1, filter_event->touches[1].radius_y);
  }

  void FlushTouchEvent(WebInputEvent::Type type) {
    uint32_t touch_event_id = SendTouchEvent();
    SendTouchEventACK(type, INPUT_EVENT_ACK_STATE_CONSUMED, touch_event_id);
    ASSERT_TRUE(TouchEventQueueEmpty());
    ASSERT_NE(0u, process_->sink().message_count());
  }

  void ReleaseTouchPointAndAck(int index) {
    ReleaseTouchPoint(index);
    int release_event_id = SendTouchEvent();
    SendTouchEventACK(WebInputEvent::kTouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                      release_event_id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LegacyInputRouterImplScaleTouchEventTest);
};

}  // namespace

TEST_F(LegacyInputRouterImplScaleTouchEventTest, ScaleTouchEventTest) {
  // Press
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::kTouchStart);

  RunTouchEventTest("Press", WebTouchPoint::kStatePressed);
  ReleaseTouchPointAndAck(1);
  ReleaseTouchPointAndAck(0);
  EXPECT_EQ(3u, GetSentMessageCountAndResetSink());

  // Move
  PressTouchPoint(0, 0);
  PressTouchPoint(0, 0);
  FlushTouchEvent(WebInputEvent::kTouchStart);
  process_->sink().ClearMessages();

  MoveTouchPoint(0, 10, 20);
  MoveTouchPoint(1, 100, 200);
  FlushTouchEvent(WebInputEvent::kTouchMove);
  RunTouchEventTest("Move", WebTouchPoint::kStateMoved);
  ReleaseTouchPointAndAck(1);
  ReleaseTouchPointAndAck(0);
  EXPECT_EQ(3u, GetSentMessageCountAndResetSink());

  // Release
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::kTouchMove);
  process_->sink().ClearMessages();

  ReleaseTouchPoint(0);
  ReleaseTouchPoint(1);
  FlushTouchEvent(WebInputEvent::kTouchEnd);
  RunTouchEventTest("Release", WebTouchPoint::kStateReleased);

  // Cancel
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::kTouchStart);
  process_->sink().ClearMessages();

  CancelTouchPoint(0);
  CancelTouchPoint(1);
  FlushTouchEvent(WebInputEvent::kTouchCancel);
  RunTouchEventTest("Cancel", WebTouchPoint::kStateCancelled);
}

namespace {

class LegacyInputRouterImplScaleGestureEventTest
    : public LegacyInputRouterImplScaleEventTest {
 public:
  LegacyInputRouterImplScaleGestureEventTest() {}

  WebGestureEvent BuildGestureEvent(WebInputEvent::Type type,
                                    const gfx::Point& point) {
    WebGestureEvent event = SyntheticWebGestureEventBuilder::Build(
        type, blink::kWebGestureDeviceTouchpad);
    event.global_x = event.x = point.x();
    event.global_y = event.y = point.y();
    return event;
  }

  void TestTap(const std::string& name, WebInputEvent::Type type) {
    SCOPED_TRACE(name);
    const gfx::Point orig(10, 20), scaled(20, 40);
    WebGestureEvent event = BuildGestureEvent(type, orig);
    event.data.tap.width = 30;
    event.data.tap.height = 40;
    SimulateGestureEvent(event);
    FlushGestureEvent(type);

    const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
    TestLocationInSentEvent(sent_event, orig, scaled);
    EXPECT_EQ(60, sent_event->data.tap.width);
    EXPECT_EQ(80, sent_event->data.tap.height);

    const WebGestureEvent* filter_event =
        GetFilterWebInputEvent<WebGestureEvent>();
    TestLocationInFilterEvent(filter_event, orig);
    EXPECT_EQ(30, filter_event->data.tap.width);
    EXPECT_EQ(40, filter_event->data.tap.height);
    process_->sink().ClearMessages();
  }

  void TestLongPress(const std::string& name, WebInputEvent::Type type) {
    const gfx::Point orig(10, 20), scaled(20, 40);
    WebGestureEvent event = BuildGestureEvent(type, orig);
    event.data.long_press.width = 30;
    event.data.long_press.height = 40;
    SimulateGestureEvent(event);
    FlushGestureEvent(type);
    const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
    TestLocationInSentEvent(sent_event, orig, scaled);
    EXPECT_EQ(60, sent_event->data.long_press.width);
    EXPECT_EQ(80, sent_event->data.long_press.height);

    const WebGestureEvent* filter_event =
        GetFilterWebInputEvent<WebGestureEvent>();
    TestLocationInFilterEvent(filter_event, orig);
    EXPECT_EQ(30, filter_event->data.long_press.width);
    EXPECT_EQ(40, filter_event->data.long_press.height);
    process_->sink().ClearMessages();
  }

  void FlushGestureEvent(WebInputEvent::Type type) {
    SendInputEventACK(type, INPUT_EVENT_ACK_STATE_CONSUMED);
    ASSERT_NE(0u, process_->sink().message_count());
  }

  void TestLocationInSentEvent(const WebGestureEvent* sent_event,
                               const gfx::Point& orig,
                               const gfx::Point& scaled) {
    EXPECT_EQ(20, sent_event->x);
    EXPECT_EQ(40, sent_event->y);
    EXPECT_EQ(10, sent_event->global_x);
    EXPECT_EQ(20, sent_event->global_y);
  }

  void TestLocationInFilterEvent(const WebGestureEvent* filter_event,
                                 const gfx::Point& point) {
    EXPECT_EQ(10, filter_event->x);
    EXPECT_EQ(20, filter_event->y);
    EXPECT_EQ(10, filter_event->global_x);
    EXPECT_EQ(20, filter_event->global_y);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LegacyInputRouterImplScaleGestureEventTest);
};

}  // namespace

TEST_F(LegacyInputRouterImplScaleGestureEventTest, GestureScrollUpdate) {
  SimulateGestureScrollUpdateEvent(10.f, 20, 0,
                                   blink::kWebGestureDeviceTouchpad);
  FlushGestureEvent(WebInputEvent::kGestureScrollUpdate);
  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();

  EXPECT_EQ(20.f, sent_event->data.scroll_update.delta_x);
  EXPECT_EQ(40.f, sent_event->data.scroll_update.delta_y);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  EXPECT_EQ(10.f, filter_event->data.scroll_update.delta_x);
  EXPECT_EQ(20.f, filter_event->data.scroll_update.delta_y);
}

TEST_F(LegacyInputRouterImplScaleGestureEventTest, GestureScrollBegin) {
  SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollBegin(
      10.f, 20.f, blink::kWebGestureDeviceTouchscreen));
  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  EXPECT_EQ(20.f, sent_event->data.scroll_begin.delta_x_hint);
  EXPECT_EQ(40.f, sent_event->data.scroll_begin.delta_y_hint);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  EXPECT_EQ(10.f, filter_event->data.scroll_begin.delta_x_hint);
  EXPECT_EQ(20.f, filter_event->data.scroll_begin.delta_y_hint);
}

TEST_F(LegacyInputRouterImplScaleGestureEventTest, GesturePinchUpdate) {
  const gfx::Point orig(10, 20), scaled(20, 40);
  SimulateGesturePinchUpdateEvent(1.5f, orig.x(), orig.y(), 0,
                                  blink::kWebGestureDeviceTouchpad);
  FlushGestureEvent(WebInputEvent::kGesturePinchUpdate);
  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  TestLocationInSentEvent(sent_event, orig, scaled);
  EXPECT_EQ(1.5f, sent_event->data.pinch_update.scale);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  TestLocationInFilterEvent(filter_event, orig);
  EXPECT_EQ(1.5f, filter_event->data.pinch_update.scale);
}

TEST_F(LegacyInputRouterImplScaleGestureEventTest, GestureTapDown) {
  const gfx::Point orig(10, 20), scaled(20, 40);
  WebGestureEvent event =
      BuildGestureEvent(WebInputEvent::kGestureTapDown, orig);
  event.data.tap_down.width = 30;
  event.data.tap_down.height = 40;
  SimulateGestureEvent(event);
  // FlushGestureEvent(WebInputEvent::GestureTapDown);
  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  TestLocationInSentEvent(sent_event, orig, scaled);
  EXPECT_EQ(60, sent_event->data.tap_down.width);
  EXPECT_EQ(80, sent_event->data.tap_down.height);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  TestLocationInFilterEvent(filter_event, orig);
  EXPECT_EQ(30, filter_event->data.tap_down.width);
  EXPECT_EQ(40, filter_event->data.tap_down.height);
}

TEST_F(LegacyInputRouterImplScaleGestureEventTest, GestureTapOthers) {
  TestTap("GestureDoubleTap", WebInputEvent::kGestureDoubleTap);
  TestTap("GestureTap", WebInputEvent::kGestureTap);
  TestTap("GestureTapUnconfirmed", WebInputEvent::kGestureTapUnconfirmed);
}

TEST_F(LegacyInputRouterImplScaleGestureEventTest, GestureShowPress) {
  const gfx::Point orig(10, 20), scaled(20, 40);
  WebGestureEvent event =
      BuildGestureEvent(WebInputEvent::kGestureShowPress, orig);
  event.data.show_press.width = 30;
  event.data.show_press.height = 40;
  SimulateGestureEvent(event);

  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  TestLocationInSentEvent(sent_event, orig, scaled);
  EXPECT_EQ(60, sent_event->data.show_press.width);
  EXPECT_EQ(80, sent_event->data.show_press.height);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  TestLocationInFilterEvent(filter_event, orig);
  EXPECT_EQ(30, filter_event->data.show_press.width);
  EXPECT_EQ(40, filter_event->data.show_press.height);
}

TEST_F(LegacyInputRouterImplScaleGestureEventTest, GestureLongPress) {
  TestLongPress("LongPress", WebInputEvent::kGestureLongPress);
  TestLongPress("LongPap", WebInputEvent::kGestureLongTap);
}

TEST_F(LegacyInputRouterImplScaleGestureEventTest, GestureTwoFingerTap) {
  WebGestureEvent event = BuildGestureEvent(WebInputEvent::kGestureTwoFingerTap,
                                            gfx::Point(10, 20));
  event.data.two_finger_tap.first_finger_width = 30;
  event.data.two_finger_tap.first_finger_height = 40;
  SimulateGestureEvent(event);

  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  EXPECT_EQ(20, sent_event->x);
  EXPECT_EQ(40, sent_event->y);
  EXPECT_EQ(60, sent_event->data.two_finger_tap.first_finger_width);
  EXPECT_EQ(80, sent_event->data.two_finger_tap.first_finger_height);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  EXPECT_EQ(10, filter_event->x);
  EXPECT_EQ(20, filter_event->y);
  EXPECT_EQ(30, filter_event->data.two_finger_tap.first_finger_width);
  EXPECT_EQ(40, filter_event->data.two_finger_tap.first_finger_height);
}

TEST_F(LegacyInputRouterImplScaleGestureEventTest, GestureFlingStart) {
  const gfx::Point orig(10, 20), scaled(20, 40);
  WebGestureEvent event =
      BuildGestureEvent(WebInputEvent::kGestureFlingStart, orig);
  event.data.fling_start.velocity_x = 30;
  event.data.fling_start.velocity_y = 40;
  SimulateGestureEvent(event);

  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  TestLocationInSentEvent(sent_event, orig, scaled);
  EXPECT_EQ(60, sent_event->data.fling_start.velocity_x);
  EXPECT_EQ(80, sent_event->data.fling_start.velocity_y);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  TestLocationInFilterEvent(filter_event, orig);
  EXPECT_EQ(30, filter_event->data.fling_start.velocity_x);
  EXPECT_EQ(40, filter_event->data.fling_start.velocity_y);
}

}  // namespace content
