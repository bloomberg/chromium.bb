// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_router_impl.h"

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
#include "content/browser/renderer_host/input/mock_widget_input_handler.h"
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
      base::FeatureList::IsEnabled(features::kRafAlignedTouchInputEvents),
      base::FeatureList::IsEnabled(features::kTouchpadAndWheelScrollLatching));
}

WebInputEvent& GetEventWithType(WebInputEvent::Type type) {
  WebInputEvent* event = NULL;
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

void CallCallback(mojom::WidgetInputHandler::DispatchEventCallback callback,
                  InputEventAckState state) {
  std::move(callback).Run(InputEventAckSource::COMPOSITOR_THREAD,
                          ui::LatencyInfo(), state, base::nullopt,
                          base::nullopt);
}

void CallCallbackWithTouchAction(
    mojom::WidgetInputHandler::DispatchEventCallback callback,
    InputEventAckState state,
    cc::TouchAction touch_action) {
  std::move(callback).Run(InputEventAckSource::COMPOSITOR_THREAD,
                          ui::LatencyInfo(), state, base::nullopt,
                          touch_action);
}

enum WheelScrollingMode {
  kWheelScrollingModeNone,
  kWheelScrollLatching,
  kAsyncWheelEvents,
};

}  // namespace

// TODO(dtapuska): Remove this class when we don't have multiple implementations
// of InputRouters.
class MockInputRouterImplClient : public InputRouterImplClient {
 public:
  mojom::WidgetInputHandler* GetWidgetInputHandler() override {
    return &widget_input_handler_;
  }

  std::vector<MockWidgetInputHandler::DispatchedEvent>
  GetAndResetDispatchedEvents() {
    return widget_input_handler_.GetAndResetDispatchedEvents();
  }

  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info) override {
    return input_router_client_.FilterInputEvent(input_event, latency_info);
  }

  void IncrementInFlightEventCount(
      blink::WebInputEvent::Type event_type) override {
    input_router_client_.IncrementInFlightEventCount(event_type);
  }

  void DecrementInFlightEventCount(InputEventAckSource ack_source) override {
    input_router_client_.DecrementInFlightEventCount(ack_source);
  }

  void OnHasTouchEventHandlers(bool has_handlers) override {
    input_router_client_.OnHasTouchEventHandlers(has_handlers);
  }

  void DidOverscroll(const ui::DidOverscrollParams& params) override {
    input_router_client_.DidOverscroll(params);
  }

  void DidStopFlinging() override { input_router_client_.DidStopFlinging(); }

  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency_info) override {
    input_router_client_.ForwardGestureEventWithLatencyInfo(gesture_event,
                                                            latency_info);
  }

  void OnSetWhiteListedTouchAction(cc::TouchAction touch_action) override {
    input_router_client_.OnSetWhiteListedTouchAction(touch_action);
  }

  bool GetAndResetFilterEventCalled() {
    return input_router_client_.GetAndResetFilterEventCalled();
  }

  ui::DidOverscrollParams GetAndResetOverscroll() {
    return input_router_client_.GetAndResetOverscroll();
  }

  cc::TouchAction GetAndResetWhiteListedTouchAction() {
    return input_router_client_.GetAndResetWhiteListedTouchAction();
  }

  void set_input_router(InputRouter* input_router) {
    input_router_client_.set_input_router(input_router);
  }

  bool has_touch_handler() const {
    return input_router_client_.has_touch_handler();
  }

  void set_filter_state(InputEventAckState filter_state) {
    input_router_client_.set_filter_state(filter_state);
  }
  int in_flight_event_count() const {
    return input_router_client_.in_flight_event_count();
  }
  void set_allow_send_event(bool allow) {
    input_router_client_.set_allow_send_event(allow);
  }
  const blink::WebInputEvent* last_filter_event() const {
    return input_router_client_.last_filter_event();
  }

  MockInputRouterClient input_router_client_;
  MockWidgetInputHandler widget_input_handler_;
};

class InputRouterImplTest : public testing::Test {
 public:
  InputRouterImplTest(
      bool raf_aligned_touch = true,
      WheelScrollingMode wheel_scrolling_mode = kWheelScrollLatching)
      : wheel_scroll_latching_enabled_(wheel_scrolling_mode !=
                                       kWheelScrollingModeNone),
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {
    if (raf_aligned_touch && wheel_scrolling_mode == kAsyncWheelEvents) {
      feature_list_.InitWithFeatures({features::kRafAlignedTouchInputEvents,
                                      features::kTouchpadAndWheelScrollLatching,
                                      features::kAsyncWheelEvents},
                                     {});
    } else if (raf_aligned_touch &&
               wheel_scrolling_mode == kWheelScrollLatching) {
      feature_list_.InitWithFeatures(
          {features::kRafAlignedTouchInputEvents,
           features::kTouchpadAndWheelScrollLatching},
          {features::kAsyncWheelEvents});
    } else if (raf_aligned_touch &&
               wheel_scrolling_mode == kWheelScrollingModeNone) {
      feature_list_.InitWithFeatures({features::kRafAlignedTouchInputEvents},
                                     {features::kTouchpadAndWheelScrollLatching,
                                      features::kAsyncWheelEvents});
    } else if (!raf_aligned_touch &&
               wheel_scrolling_mode == kAsyncWheelEvents) {
      feature_list_.InitWithFeatures({features::kTouchpadAndWheelScrollLatching,
                                      features::kAsyncWheelEvents},
                                     {features::kRafAlignedTouchInputEvents});
    } else if (!raf_aligned_touch &&
               wheel_scrolling_mode == kWheelScrollLatching) {
      feature_list_.InitWithFeatures(
          {features::kTouchpadAndWheelScrollLatching},
          {features::kRafAlignedTouchInputEvents, features::kAsyncWheelEvents});
    } else {  // !raf_aligned_touch && wheel_scroll_latching ==
              // kWheelScrollingModeNone.
      feature_list_.InitWithFeatures({},
                                     {features::kRafAlignedTouchInputEvents,
                                      features::kTouchpadAndWheelScrollLatching,
                                      features::kAsyncWheelEvents});
    }
  }

  ~InputRouterImplTest() override {}

 protected:
  using DispatchedEvents = std::vector<MockWidgetInputHandler::DispatchedEvent>;
  // testing::Test
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kValidateInputEventStream);
    client_.reset(new MockInputRouterImplClient());
    disposition_handler_.reset(new MockInputDispositionHandler());
    input_router_.reset(new InputRouterImpl(
        client_.get(), disposition_handler_.get(), config_));

    client_->set_input_router(input_router());
    disposition_handler_->set_input_router(input_router());
  }

  void TearDown() override {
    // Process all pending tasks to avoid leaks.
    base::RunLoop().RunUntilIdle();

    input_router_.reset();
    client_.reset();
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

  InputRouterImpl* input_router() const { return input_router_.get(); }

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

  DispatchedEvents GetAndResetDispatchedEvents() {
    return client_->GetAndResetDispatchedEvents();
  }

  static void RunTasksAndWait(base::TimeDelta delay) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(), delay);
    base::RunLoop().Run();
  }

  void OverscrollDispatch();

  InputRouter::Config config_;
  std::unique_ptr<MockInputRouterImplClient> client_;
  std::unique_ptr<InputRouterImpl> input_router_;
  std::unique_ptr<MockInputDispositionHandler> disposition_handler_;
  bool wheel_scroll_latching_enabled_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  SyntheticWebTouchEvent touch_event_;

  base::test::ScopedFeatureList feature_list_;
};

class InputRouterImplRafAlignedTouchDisabledTest : public InputRouterImplTest {
 public:
  InputRouterImplRafAlignedTouchDisabledTest()
      : InputRouterImplTest(false, kWheelScrollingModeNone) {}
};

class InputRouterImplWheelScrollLatchingDisabledTest
    : public InputRouterImplTest {
 public:
  InputRouterImplWheelScrollLatchingDisabledTest()
      : InputRouterImplTest(true, kWheelScrollingModeNone) {}
};

class InputRouterImplAsyncWheelEventEnabledTest : public InputRouterImplTest {
 public:
  InputRouterImplAsyncWheelEventEnabledTest()
      : InputRouterImplTest(true, kAsyncWheelEvents) {}
};

TEST_F(InputRouterImplTest, HandledInputEvent) {
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure no input event is sent to the renderer.
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(0u, dispatched_events.size());

  // OnKeyboardEventAck should be triggered without actual ack.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
}

TEST_F(InputRouterImplTest, ClientCanceledKeyboardEvent) {
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  // Simulate a keyboard event that has no consumer.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure no input event is sent to the renderer.
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(0u, dispatched_events.size());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // Simulate a keyboard event that should be dropped.
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_UNKNOWN);
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure no input event is sent to the renderer, and no ack is sent.
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(0u, dispatched_events.size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
}

// Tests ported from RenderWidgetHostTest --------------------------------------

TEST_F(InputRouterImplTest, HandleKeyEventsWeSent) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure we sent the input event to the renderer.
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1u, dispatched_events.size());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            disposition_handler_->acked_keyboard_event().GetType());
}

TEST_F(InputRouterImplTest, CoalescesWheelEvents) {
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
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1u, dispatched_events.size());
  ASSERT_EQ(WebInputEvent::kMouseWheel,
            dispatched_events.at(0).event_->web_event->GetType());
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(
          dispatched_events.at(0).event_->web_event.get());
  EXPECT_EQ(0, wheel_event->delta_x);
  EXPECT_EQ(-5, wheel_event->delta_y);

  // Check that the ACK sends the second message immediately.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  // The coalesced events can queue up a delayed ack
  // so that additional input events can be processed before
  // we turn off coalescing.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1u, dispatched_events.size());
  ASSERT_EQ(WebInputEvent::kMouseWheel,
            dispatched_events.at(0).event_->web_event->GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      dispatched_events.at(0).event_->web_event.get());
  EXPECT_EQ(8, wheel_event->delta_x);
  EXPECT_EQ(-10 + -6, wheel_event->delta_y);  // coalesced

  // Ack the second event (which had the third coalesced into it).
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1u, dispatched_events.size());
  ASSERT_EQ(WebInputEvent::kMouseWheel,
            dispatched_events.at(0).event_->web_event->GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      dispatched_events.at(0).event_->web_event.get());
  EXPECT_EQ(9, wheel_event->delta_x);
  EXPECT_EQ(-7, wheel_event->delta_y);

  // Ack the fourth event.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1u, dispatched_events.size());
  ASSERT_EQ(WebInputEvent::kMouseWheel,
            dispatched_events.at(0).event_->web_event->GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      dispatched_events.at(0).event_->web_event.get());
  EXPECT_EQ(0, wheel_event->delta_x);
  EXPECT_EQ(-10, wheel_event->delta_y);

  // Ack the fifth event.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1u, dispatched_events.size());
  ASSERT_EQ(WebInputEvent::kMouseWheel,
            dispatched_events.at(0).event_->web_event->GetType());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      dispatched_events.at(0).event_->web_event.get());
  EXPECT_EQ(0, wheel_event->delta_x);
  EXPECT_EQ(0, wheel_event->delta_y);
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_event->phase);

  // After the final ack, the queue should be empty.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(0u, dispatched_events.size());
}

// Tests that touch-events are queued properly.
TEST_F(InputRouterImplRafAlignedTouchDisabledTest, TouchEventQueue) {
  OnHasTouchEventHandlers(true);

  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(client_->GetAndResetFilterEventCalled());
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1u, dispatched_events.size());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // The second touch should not be sent since one is already in queue.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  EXPECT_FALSE(client_->GetAndResetFilterEventCalled());
  EXPECT_EQ(0u, GetAndResetDispatchedEvents().size());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // Receive an ACK for the first touch-event.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kTouchStart,
            disposition_handler_->acked_touch_event().event.GetType());
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());

  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kTouchMove,
            disposition_handler_->acked_touch_event().event.GetType());
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
}

// Tests that touch-events are sent properly.
TEST_F(InputRouterImplTest, TouchEventQueue) {
  OnHasTouchEventHandlers(true);

  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(client_->GetAndResetFilterEventCalled());
  DispatchedEvents touch_start_event = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, touch_start_event.size());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // The second touch should be sent right away.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  DispatchedEvents touch_move_event = GetAndResetDispatchedEvents();
  EXPECT_TRUE(client_->GetAndResetFilterEventCalled());
  EXPECT_EQ(1U, touch_move_event.size());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // Receive an ACK for the first touch-event.
  CallCallback(std::move(touch_start_event.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kTouchStart,
            disposition_handler_->acked_touch_event().event.GetType());
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());

  CallCallback(std::move(touch_move_event.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::kTouchMove,
            disposition_handler_->acked_touch_event().event.GetType());
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
}

// Tests that the touch-queue is emptied after a page stops listening for touch
// events and the outstanding ack is received.
TEST_F(InputRouterImplTest, TouchEventQueueFlush) {
  OnHasTouchEventHandlers(true);
  EXPECT_TRUE(client_->has_touch_handler());
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_TRUE(TouchEventQueueEmpty());

  // Send a touch-press event.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  MoveTouchPoint(0, 2, 2);
  MoveTouchPoint(0, 3, 3);
  EXPECT_FALSE(TouchEventQueueEmpty());
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());

  // The page stops listening for touch-events. Note that flushing is deferred
  // until the outstanding ack is received.
  OnHasTouchEventHandlers(false);
  EXPECT_FALSE(client_->has_touch_handler());
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // After the ack, the touch-event queue should be empty, and none of the
  // flushed touch-events should have been sent to the renderer.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_TRUE(TouchEventQueueEmpty());
}

TEST_F(InputRouterImplTest, UnhandledWheelEvent) {
  // Simulate wheel events.
  SimulateWheelEventPossiblyIncludingPhase(!wheel_scroll_latching_enabled_, 0,
                                           0, 0, -5, 0, false,
                                           WebMouseWheelEvent::kPhaseBegan);
  SimulateWheelEventPossiblyIncludingPhase(!wheel_scroll_latching_enabled_, 0,
                                           0, 0, -10, 0, false,
                                           WebMouseWheelEvent::kPhaseChanged);

  // Check that only the first event was sent.
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());

  // Indicate that the wheel event was unhandled.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  dispatched_events = GetAndResetDispatchedEvents();

  // There should be a ScrollBegin, MouseWheel sent.
  EXPECT_EQ(2U, dispatched_events.size());

  ASSERT_EQ(WebInputEvent::kGestureScrollBegin,
            dispatched_events.at(0).event_->web_event->GetType());
  ASSERT_EQ(WebInputEvent::kMouseWheel,
            dispatched_events.at(1).event_->web_event->GetType());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the ack for ScrollBegin, MouseWheel were
  // processed.
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(disposition_handler_->acked_wheel_event().delta_y, -5);

  DispatchedEvents gesture_scroll_update = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, gesture_scroll_update.size());
  CallCallback(std::move(gesture_scroll_update.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // Ack the mouse wheel event.
  CallCallback(std::move(dispatched_events.at(1).callback_),
               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  ASSERT_EQ(WebInputEvent::kGestureScrollUpdate,
            dispatched_events.at(0).event_->web_event->GetType());

  // Check that the correct unhandled wheel event was received.
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
            disposition_handler_->acked_wheel_event_state());
  EXPECT_EQ(disposition_handler_->acked_wheel_event().delta_y, -10);

  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the ack for ScrollUpdate were processed.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
}
TEST_F(InputRouterImplWheelScrollLatchingDisabledTest, UnhandledWheelEvent) {
  // Simulate wheel events.
  SimulateWheelEventPossiblyIncludingPhase(!wheel_scroll_latching_enabled_, 0,
                                           0, 0, -5, 0, false,
                                           WebMouseWheelEvent::kPhaseBegan);
  SimulateWheelEventPossiblyIncludingPhase(!wheel_scroll_latching_enabled_, 0,
                                           0, 0, -10, 0, false,
                                           WebMouseWheelEvent::kPhaseChanged);

  // Check that only the first event was sent.
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());

  // Indicate that the wheel event was unhandled.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  dispatched_events = GetAndResetDispatchedEvents();

  // Check that the ack for MouseWheel and GestureScrollBegin
  // was processed.
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(disposition_handler_->acked_wheel_event().delta_y, -5);

  // There should be a ScrollBegin, ScrollUpdate and MouseWheel sent.
  EXPECT_EQ(3U, dispatched_events.size());

  ASSERT_EQ(WebInputEvent::kGestureScrollBegin,
            dispatched_events.at(0).event_->web_event->GetType());
  ASSERT_EQ(WebInputEvent::kGestureScrollUpdate,
            dispatched_events.at(1).event_->web_event->GetType());
  ASSERT_EQ(WebInputEvent::kMouseWheel,
            dispatched_events.at(2).event_->web_event->GetType());

  // Ack the ScrollUpdate
  CallCallback(std::move(dispatched_events.at(1).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the ack for ScrollBegin, ScrollUpdate were
  // processed.
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());

  // The GestureScrollUpdate ACK releases the GestureScrollEnd.
  EXPECT_EQ(1U, GetAndResetDispatchedEvents().size());

  // Ack the MouseWheel.
  CallCallback(std::move(dispatched_events.at(2).callback_),
               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());

  // There should be a ScrollBegin and ScrollUpdate sent.
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(2U, dispatched_events.size());
  ASSERT_EQ(WebInputEvent::kGestureScrollBegin,
            dispatched_events.at(0).event_->web_event->GetType());
  ASSERT_EQ(WebInputEvent::kGestureScrollUpdate,
            dispatched_events.at(1).event_->web_event->GetType());

  // Check that the correct unhandled wheel event was received.
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
            disposition_handler_->acked_wheel_event_state());
  EXPECT_EQ(disposition_handler_->acked_wheel_event().delta_y, -10);

  CallCallback(std::move(dispatched_events.at(1).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  // The GestureScrollUpdate ACK releases the GestureScrollEnd.
  EXPECT_EQ(1U, GetAndResetDispatchedEvents().size());

  // Check that the ack for the ScrollUpdate and ScrollEnd
  // were processed.
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
}

TEST_F(InputRouterImplAsyncWheelEventEnabledTest, UnhandledWheelEvent) {
  // Simulate wheel events.
  SimulateWheelEventWithPhase(0, 0, 0, -5, 0, false,
                              WebMouseWheelEvent::kPhaseBegan);
  SimulateWheelEventWithPhase(0, 0, 0, -10, 0, false,
                              WebMouseWheelEvent::kPhaseChanged);

  // Check that only the first event was sent.
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());

  // Indicate that the wheel event was unhandled.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // There should be a ScrollBegin and second MouseWheel sent.
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(2U, dispatched_events.size());
  ASSERT_EQ(WebInputEvent::kGestureScrollBegin,
            dispatched_events.at(0).event_->web_event->GetType());
  ASSERT_EQ(WebInputEvent::kMouseWheel,
            dispatched_events.at(1).event_->web_event->GetType());

  // Indicate that the GestureScrollBegin event was consumed.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the ack for the first MouseWheel, ScrollBegin, and the second
  // MouseWheel were processed.
  EXPECT_EQ(3U, disposition_handler_->GetAndResetAckCount());

  // There should be a ScrollUpdate sent.
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());

  // The last acked wheel event should be the second one since the input router
  // has already sent the immediate ack for the second wheel event.
  EXPECT_EQ(disposition_handler_->acked_wheel_event().delta_y, -10);
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_IGNORED,
            disposition_handler_->acked_wheel_event_state());

  // Ack the gesture scroll update.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the ack for the coalesced ScrollUpdate were processed.
  EXPECT_EQ(
      -15,
      disposition_handler_->acked_gesture_event().data.scroll_update.delta_y);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
}

TEST_F(InputRouterImplTest, TouchTypesIgnoringAck) {
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
  SendTouchEvent();
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  ASSERT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  ASSERT_EQ(0, client_->in_flight_event_count());

  // The TouchCancel has no callback.
  CancelTouchPoint(0);
  SendTouchEvent();
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());
  EXPECT_FALSE(HasPendingEvents());
  EXPECT_FALSE(dispatched_events.at(0).callback_);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_FALSE(HasPendingEvents());
}

TEST_F(InputRouterImplTest, GestureTypesIgnoringAck) {
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
      DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();

      if (type == WebInputEvent::kGestureScrollUpdate)
        EXPECT_EQ(2U, dispatched_events.size());
      else
        EXPECT_EQ(1U, dispatched_events.size());
      EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
      EXPECT_EQ(1, client_->in_flight_event_count());
      EXPECT_TRUE(HasPendingEvents());

      CallCallback(
          std::move(
              dispatched_events.at(dispatched_events.size() - 1).callback_),
          INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

      EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
      EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
      EXPECT_EQ(0, client_->in_flight_event_count());
      EXPECT_FALSE(HasPendingEvents());
      continue;
    }

    SimulateGestureEvent(type, blink::kWebGestureDeviceTouchscreen);
    EXPECT_EQ(1U, GetAndResetDispatchedEvents().size());
    EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
    EXPECT_EQ(0, client_->in_flight_event_count());
    EXPECT_FALSE(HasPendingEvents());
  }
}

TEST_F(InputRouterImplTest, MouseTypesIgnoringAck) {
  int start_type = static_cast<int>(WebInputEvent::kMouseDown);
  int end_type = static_cast<int>(WebInputEvent::kContextMenu);
  ASSERT_LT(start_type, end_type);
  for (int i = start_type; i <= end_type; ++i) {
    WebInputEvent::Type type = static_cast<WebInputEvent::Type>(i);

    SimulateMouseEvent(type, 0, 0);
    DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
    EXPECT_EQ(1U, dispatched_events.size());

    if (ShouldBlockEventStream(GetEventWithType(type))) {
      EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
      EXPECT_EQ(1, client_->in_flight_event_count());

      CallCallback(std::move(dispatched_events.at(0).callback_),
                   INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
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
TEST_F(InputRouterImplTest, RequiredEventAckTypes) {
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
// Test that GestureShowPress, GestureTapDown and GestureTapCancel events don't
// wait for ACKs.
TEST_F(InputRouterImplTest, GestureTypesIgnoringAckInterleaved) {
  // Interleave a few events that do and do not ignore acks, ensuring that
  // ack-ignoring events aren't dispatched until all prior events which observe
  // their ack disposition have been dispatched.

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(2U, dispatched_events.size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::kGestureShowPress,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::kGestureTapCancel,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  // Now ack each ack-respecting event. Ack-ignoring events should not be
  // dispatched until all prior events which observe ack disposition have been
  // fired, at which point they should be sent immediately.  They should also
  // have no effect on the in-flight event count.
  CallCallback(std::move(dispatched_events.at(1).callback_),
               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(2U, dispatched_events.size());
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Ack the GestureScrollUpdate
  CallCallback(std::move(dispatched_events.at(1).callback_),
               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(2U, dispatched_events.size());
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Ack the GestureScrollUpdate
  CallCallback(std::move(dispatched_events.at(1).callback_),
               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(2U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());
}

// Test that GestureShowPress events don't get out of order due to
// ignoring their acks.
TEST_F(InputRouterImplTest, GestureShowPressIsInOrder) {
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // GesturePinchBegin ignores its ack.
  SimulateGestureEvent(WebInputEvent::kGesturePinchBegin,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // GesturePinchUpdate waits for an ack.
  // This also verifies that GesturePinchUpdates for touchscreen are sent
  // to the renderer (in contrast to the TrackpadPinchUpdate test).
  SimulateGestureEvent(WebInputEvent::kGesturePinchUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::kGestureShowPress,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  // The ShowPress, though it ignores ack, is still stuck in the queue
  // behind the PinchUpdate which requires an ack.
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::kGestureShowPress,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  // ShowPress has entered the queue.
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  // Now that the Tap has been ACKed, the ShowPress events should receive
  // synthetic acks, and fire immediately.
  EXPECT_EQ(2U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(3U, disposition_handler_->GetAndResetAckCount());
}

// Test that touch ack timeout behavior is properly configured for
// mobile-optimized sites and allowed touch actions.
TEST_F(InputRouterImplTest, TouchAckTimeoutConfigured) {
  const int kDesktopTimeoutMs = 1;
  const int kMobileTimeoutMs = 0;
  SetUpForTouchAckTimeoutTest(kDesktopTimeoutMs, kMobileTimeoutMs);
  ASSERT_TRUE(TouchEventTimeoutEnabled());

  // Verify that the touch ack timeout fires upon the delayed ack.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(kDesktopTimeoutMs + 1));

  // The timed-out event should have been ack'ed.
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());

  // Ack'ing the timed-out event should fire a TouchCancel.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetAndResetDispatchedEvents().size());

  // The remainder of the touch sequence should be dropped.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
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
  SendTouchEvent();
  DispatchedEvents touch_press_event2 = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, touch_press_event2.size());
  OnSetTouchAction(cc::kTouchActionPanY);
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  ReleaseTouchPoint(0);
  SendTouchEvent();
  DispatchedEvents touch_release_event2 = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, touch_release_event2.size());
  CallCallback(std::move(touch_press_event2.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  CallCallback(std::move(touch_release_event2.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);

  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedEvents touch_press_event3 = GetAndResetDispatchedEvents();
  OnSetTouchAction(cc::kTouchActionNone);
  EXPECT_FALSE(TouchEventTimeoutEnabled());
  ReleaseTouchPoint(0);
  SendTouchEvent();
  DispatchedEvents touch_release_event3 = GetAndResetDispatchedEvents();
  CallCallback(std::move(touch_press_event3.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  CallCallback(std::move(touch_release_event3.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);

  // As the touch-action is reset by a new touch sequence, the timeout behavior
  // should be restored.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(TouchEventTimeoutEnabled());
}

// Test that a touch sequenced preceded by kTouchActionNone is not affected by
// the touch timeout.
TEST_F(InputRouterImplTest,
       TouchAckTimeoutDisabledForTouchSequenceAfterTouchActionNone) {
  const int kDesktopTimeoutMs = 1;
  const int kMobileTimeoutMs = 2;
  SetUpForTouchAckTimeoutTest(kDesktopTimeoutMs, kMobileTimeoutMs);
  ASSERT_TRUE(TouchEventTimeoutEnabled());
  OnHasTouchEventHandlers(true);

  // Start a touch sequence.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());

  // kTouchActionNone should disable the timeout.
  OnSetTouchAction(cc::kTouchActionNone);
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_FALSE(TouchEventTimeoutEnabled());

  MoveTouchPoint(0, 1, 2);
  SendTouchEvent();
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_FALSE(TouchEventTimeoutEnabled());
  EXPECT_EQ(1U, dispatched_events.size());

  // Delay the move ack. The timeout should not fire.
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(kDesktopTimeoutMs + 1));
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());

  // End the touch sequence.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  disposition_handler_->GetAndResetAckCount();
  GetAndResetDispatchedEvents();

  // Start another touch sequence.  This should restore the touch timeout.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  EXPECT_EQ(1U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());

  // Wait for the touch ack timeout to fire.
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(kDesktopTimeoutMs + 1));
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
}

// Test that TouchActionFilter::ResetTouchAction is called before the
// first touch event for a touch sequence reaches the renderer.
TEST_F(InputRouterImplTest, TouchActionResetBeforeEventReachesRenderer) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedEvents touch_press_event1 = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, touch_press_event1.size());
  OnSetTouchAction(cc::kTouchActionNone);
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  DispatchedEvents touch_move_event1 = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, touch_move_event1.size());
  ReleaseTouchPoint(0);
  SendTouchEvent();
  DispatchedEvents touch_release_event1 = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, touch_release_event1.size());

  // Sequence 2.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedEvents touch_press_event2 = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, touch_press_event2.size());
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  DispatchedEvents touch_move_event2 = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, touch_move_event2.size());
  ReleaseTouchPoint(0);
  SendTouchEvent();
  DispatchedEvents touch_release_event2 = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, touch_release_event2.size());

  CallCallback(std::move(touch_press_event1.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  CallCallback(std::move(touch_move_event1.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);

  // Ensure touch action is still none, as the next touch start hasn't been
  // acked yet. ScrollBegin and ScrollEnd don't require acks.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());

  // This allows the next touch sequence to start.
  CallCallback(std::move(touch_release_event1.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);

  // Ensure touch action has been set to auto, as a new touch sequence has
  // started.
  CallCallback(std::move(touch_press_event2.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  CallCallback(std::move(touch_move_event2.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  DispatchedEvents gesture_scroll_begin = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, gesture_scroll_begin.size());
  CallCallback(std::move(gesture_scroll_begin.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedEvents().size());
  CallCallback(std::move(touch_release_event2.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
}

// Test that TouchActionFilter::ResetTouchAction is called when a new touch
// sequence has no consumer.
TEST_F(InputRouterImplTest, TouchActionResetWhenTouchHasNoConsumer) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedEvents touch_press_event1 = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, touch_press_event1.size());
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  DispatchedEvents touch_move_event1 = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, touch_move_event1.size());
  OnSetTouchAction(cc::kTouchActionNone);
  CallCallback(std::move(touch_press_event1.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  CallCallback(std::move(touch_move_event1.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);

  ReleaseTouchPoint(0);
  SendTouchEvent();
  DispatchedEvents touch_release_event1 = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, touch_move_event1.size());

  // Sequence 2
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedEvents touch_press_event2 = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, touch_press_event2.size());
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(2U, GetAndResetDispatchedEvents().size());

  // Ensure we have touch-action:none. ScrollBegin and ScrollEnd don't require
  // acks.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());

  CallCallback(std::move(touch_release_event1.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  CallCallback(std::move(touch_press_event2.at(0).callback_),
               INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  // Ensure touch action has been set to auto, as the touch had no consumer.
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedEvents().size());
}

// Test that TouchActionFilter::ResetTouchAction is called when the touch
// handler is removed.
TEST_F(InputRouterImplTest, TouchActionResetWhenTouchHandlerRemoved) {
  // Touch sequence with touch handler.
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);
  SendTouchEvent();
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  OnSetTouchAction(cc::kTouchActionNone);
  ReleaseTouchPoint(0);
  SendTouchEvent();
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(3U, dispatched_events.size());

  // Ensure we have touch-action:none, suppressing scroll events.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  CallCallback(std::move(dispatched_events.at(1).callback_),
               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());

  CallCallback(std::move(dispatched_events.at(2).callback_),
               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());

  // Sequence without a touch handler. Note that in this case, the view may not
  // necessarily forward touches to the router (as no touch handler exists).
  OnHasTouchEventHandlers(false);

  // Ensure touch action has been set to auto, as the touch handler has been
  // removed.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedEvents().size());
}

// Tests that async touch-moves are ack'd from the browser side.
TEST_F(InputRouterImplTest, AsyncTouchMoveAckedImmediately) {
  OnHasTouchEventHandlers(true);

  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(client_->GetAndResetFilterEventCalled());
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // Receive an ACK for the first touch-event.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(2U, GetAndResetDispatchedEvents().size());

  // Now send an async move.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetAndResetDispatchedEvents().size());
}

// Test that the double tap gesture depends on the touch action of the first
// tap.
TEST_F(InputRouterImplTest, DoubleTapGestureDependsOnFirstTap) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  OnSetTouchAction(cc::kTouchActionNone);
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);

  ReleaseTouchPoint(0);
  SendTouchEvent();

  // Sequence 2
  PressTouchPoint(1, 1);
  SendTouchEvent();

  // First tap.
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);

  // The GestureTapUnconfirmed is converted into a tap, as the touch action is
  // none.
  SimulateGestureEvent(WebInputEvent::kGestureTapUnconfirmed,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(4U, dispatched_events.size());
  // This test will become invalid if GestureTap stops requiring an ack.
  ASSERT_TRUE(
      ShouldBlockEventStream(GetEventWithType(WebInputEvent::kGestureTap)));
  EXPECT_EQ(3, client_->in_flight_event_count());

  CallCallback(std::move(dispatched_events.at(3).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2, client_->in_flight_event_count());

  // This tap gesture is dropped, since the GestureTapUnconfirmed was turned
  // into a tap.
  SimulateGestureEvent(WebInputEvent::kGestureTap,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());

  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  CallCallback(std::move(dispatched_events.at(1).callback_),
               INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  // Second Tap.
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());

  // Although the touch-action is now auto, the double tap still won't be
  // dispatched, because the first tap occured when the touch-action was none.
  SimulateGestureEvent(WebInputEvent::kGestureDoubleTap,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  // This test will become invalid if GestureDoubleTap stops requiring an ack.
  ASSERT_TRUE(ShouldBlockEventStream(
      GetEventWithType(WebInputEvent::kGestureDoubleTap)));
  EXPECT_EQ(1, client_->in_flight_event_count());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0, client_->in_flight_event_count());
}

// Test that the double tap gesture depends on the touch action of the first
// tap.
TEST_F(InputRouterImplRafAlignedTouchDisabledTest,
       DoubleTapGestureDependsOnFirstTap) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  OnSetTouchAction(cc::kTouchActionNone);
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);

  ReleaseTouchPoint(0);
  SendTouchEvent();

  // Sequence 2
  PressTouchPoint(1, 1);
  SendTouchEvent();

  // First tap.
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);

  // The GestureTapUnconfirmed is converted into a tap, as the touch action is
  // none.
  SimulateGestureEvent(WebInputEvent::kGestureTapUnconfirmed,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(3U, dispatched_events.size());
  // This test will become invalid if GestureTap stops requiring an ack.
  ASSERT_TRUE(
      ShouldBlockEventStream(GetEventWithType(WebInputEvent::kGestureTap)));
  EXPECT_EQ(2, client_->in_flight_event_count());
  CallCallback(std::move(dispatched_events.at(2).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);

  EXPECT_EQ(1, client_->in_flight_event_count());

  // This tap gesture is dropped, since the GestureTapUnconfirmed was turned
  // into a tap.
  SimulateGestureEvent(WebInputEvent::kGestureTap,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedEvents().size());

  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);

  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  ASSERT_EQ(WebInputEvent::kTouchStart,
            dispatched_events.at(0).event_->web_event->GetType());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  // Second Tap.
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetDispatchedEvents().size());

  // Although the touch-action is now auto, the double tap still won't be
  // dispatched, because the first tap occured when the touch-action was none.
  SimulateGestureEvent(WebInputEvent::kGestureDoubleTap,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  // This test will become invalid if GestureDoubleTap stops requiring an ack.
  ASSERT_TRUE(ShouldBlockEventStream(
      GetEventWithType(WebInputEvent::kGestureDoubleTap)));
  EXPECT_EQ(1, client_->in_flight_event_count());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0, client_->in_flight_event_count());
}

// Test that GesturePinchUpdate is handled specially for trackpad
TEST_F(InputRouterImplTest, TouchpadPinchUpdate) {
  // GesturePinchUpdate for trackpad sends synthetic wheel events.
  // Note that the Touchscreen case is verified as NOT doing this as
  // part of the ShowPressIsInOrder test.

  SimulateGesturePinchUpdateEvent(1.5f, 20, 25, 0,
                                  blink::kWebGestureDeviceTouchpad);

  // Verify we actually sent a special wheel event to the renderer.
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  const WebInputEvent* input_event =
      dispatched_events.at(0).event_->web_event.get();
  ASSERT_EQ(WebInputEvent::kGesturePinchUpdate, input_event->GetType());
  const WebGestureEvent* gesture_event =
      static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(20, gesture_event->x);
  EXPECT_EQ(25, gesture_event->y);
  EXPECT_EQ(20, gesture_event->global_x);
  EXPECT_EQ(25, gesture_event->global_y);

  CallCallback(std::move(dispatched_events.at(0).callback_),
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
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  input_event = dispatched_events.at(0).event_->web_event.get();
  ASSERT_EQ(WebInputEvent::kGesturePinchUpdate, input_event->GetType());
  gesture_event = static_cast<const WebGestureEvent*>(input_event);

  CallCallback(std::move(dispatched_events.at(0).callback_),
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
TEST_F(InputRouterImplTest, TouchpadPinchAndScrollUpdate) {
  // The first scroll should be sent immediately.
  SimulateGestureScrollUpdateEvent(1.5f, 0.f, 0,
                                   blink::kWebGestureDeviceTouchpad);
  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchpad);
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Subsequent scroll and pinch events should remain queued, coalescing as
  // more trackpad events arrive.
  SimulateGesturePinchUpdateEvent(1.5f, 20, 25, 0,
                                  blink::kWebGestureDeviceTouchpad);
  ASSERT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGestureScrollUpdateEvent(1.5f, 1.5f, 0,
                                   blink::kWebGestureDeviceTouchpad);
  ASSERT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGesturePinchUpdateEvent(1.5f, 20, 25, 0,
                                  blink::kWebGestureDeviceTouchpad);
  ASSERT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGestureScrollUpdateEvent(0.f, 1.5f, 0,
                                   blink::kWebGestureDeviceTouchpad);
  ASSERT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Ack'ing the first scroll should trigger both the coalesced scroll and the
  // coalesced pinch events (which is sent to the renderer as a wheel event).
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(2U, dispatched_events.size());
  EXPECT_EQ(2, client_->in_flight_event_count());

  // Ack the second scroll.
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  ASSERT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Ack the wheel event.
  CallCallback(std::move(dispatched_events.at(1).callback_),
               INPUT_EVENT_ACK_STATE_CONSUMED);
  ASSERT_EQ(0U, GetAndResetDispatchedEvents().size());
  EXPECT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());
}

// Test proper routing of overscroll notifications received either from
// event acks or from |DidOverscroll| IPC messages.
void InputRouterImplTest::OverscrollDispatch() {
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
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());

  std::move(dispatched_events.at(0).callback_)
      .Run(InputEventAckSource::COMPOSITOR_THREAD, ui::LatencyInfo(),
           INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
           DidOverscrollParams(wheel_overscroll), base::nullopt);

  client_overscroll = client_->GetAndResetOverscroll();
  EXPECT_EQ(wheel_overscroll.accumulated_overscroll,
            client_overscroll.accumulated_overscroll);
  EXPECT_EQ(wheel_overscroll.latest_overscroll_delta,
            client_overscroll.latest_overscroll_delta);
  EXPECT_EQ(wheel_overscroll.current_fling_velocity,
            client_overscroll.current_fling_velocity);
}

TEST_F(InputRouterImplTest, OverscrollDispatch) {
  OverscrollDispatch();
}
TEST_F(InputRouterImplWheelScrollLatchingDisabledTest, OverscrollDispatch) {
  OverscrollDispatch();
}
TEST_F(InputRouterImplAsyncWheelEventEnabledTest, OverscrollDispatch) {
  OverscrollDispatch();
}

// Test proper routing of whitelisted touch action notifications received from
// |SetWhiteListedTouchAction| IPC messages.
TEST_F(InputRouterImplTest, OnSetWhiteListedTouchAction) {
  cc::TouchAction touch_action = cc::kTouchActionPanY;
  OnSetWhiteListedTouchAction(touch_action, 0,
                              INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  cc::TouchAction white_listed_touch_action =
      client_->GetAndResetWhiteListedTouchAction();
  EXPECT_EQ(touch_action, white_listed_touch_action);
}

// Tests that touch event stream validation passes when events are filtered
// out. See crbug.com/581231 for details.
TEST_F(InputRouterImplTest, TouchValidationPassesWithFilteredInputEvents) {
  // Touch sequence with touch handler.
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  PressTouchPoint(1, 1);
  SendTouchEvent();
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  // This event will be filtered out, since no consumer exists.
  ReleaseTouchPoint(1);
  SendTouchEvent();
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(0U, dispatched_events.size());

  // If the validator didn't see the filtered out release event, it will crash
  // now, upon seeing a press for a touch which it believes to be still pressed.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  CallCallback(std::move(dispatched_events.at(0).callback_),
               INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
}

TEST_F(InputRouterImplTest, TouchActionInCallback) {
  OnHasTouchEventHandlers(true);

  // Send a touchstart
  PressTouchPoint(1, 1);
  SendTouchEvent();
  DispatchedEvents dispatched_events = GetAndResetDispatchedEvents();
  EXPECT_EQ(1U, dispatched_events.size());
  CallCallbackWithTouchAction(std::move(dispatched_events.at(0).callback_),
                              INPUT_EVENT_ACK_STATE_CONSUMED,
                              cc::TouchAction::kTouchActionNone);
  ASSERT_EQ(1U, disposition_handler_->GetAndResetAckCount());
  EXPECT_EQ(cc::TouchAction::kTouchActionNone,
            input_router_->AllowedTouchAction());
}

namespace {

class InputRouterImplScaleEventTest : public InputRouterImplTest {
 public:
  InputRouterImplScaleEventTest() {}

  void SetUp() override {
    InputRouterImplTest::SetUp();
    input_router_->SetDeviceScaleFactor(2.f);
  }

  template <typename T>
  const T* GetSentWebInputEvent() {
    EXPECT_EQ(1u, dispatched_events_.size());

    return static_cast<const T*>(
        dispatched_events_.at(0).event_->web_event.get());
  }

  template <typename T>
  const T* GetFilterWebInputEvent() const {
    return static_cast<const T*>(client_->last_filter_event());
  }

  void UpdateDispatchedEvents() {
    dispatched_events_ = GetAndResetDispatchedEvents();
  }

 protected:
  DispatchedEvents dispatched_events_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputRouterImplScaleEventTest);
};

class InputRouterImplScaleMouseEventTest
    : public InputRouterImplScaleEventTest {
 public:
  InputRouterImplScaleMouseEventTest() {}

  void RunMouseEventTest(const std::string& name, WebInputEvent::Type type) {
    SCOPED_TRACE(name);
    SimulateMouseEvent(type, 10, 10);
    UpdateDispatchedEvents();
    const WebMouseEvent* sent_event = GetSentWebInputEvent<WebMouseEvent>();
    EXPECT_EQ(20, sent_event->PositionInWidget().x);
    EXPECT_EQ(20, sent_event->PositionInWidget().y);

    const WebMouseEvent* filter_event = GetFilterWebInputEvent<WebMouseEvent>();
    EXPECT_EQ(10, filter_event->PositionInWidget().x);
    EXPECT_EQ(10, filter_event->PositionInWidget().y);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputRouterImplScaleMouseEventTest);
};

}  // namespace

TEST_F(InputRouterImplScaleMouseEventTest, ScaleMouseEventTest) {
  RunMouseEventTest("Enter", WebInputEvent::kMouseEnter);
  RunMouseEventTest("Down", WebInputEvent::kMouseDown);
  RunMouseEventTest("Move", WebInputEvent::kMouseMove);
  RunMouseEventTest("Up", WebInputEvent::kMouseUp);
}

TEST_F(InputRouterImplScaleEventTest, ScaleMouseWheelEventTest) {
  SimulateWheelEventWithPhase(5, 5, 10, 10, 0, false,
                              WebMouseWheelEvent::kPhaseBegan);
  UpdateDispatchedEvents();

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

class InputRouterImplScaleTouchEventTest
    : public InputRouterImplScaleEventTest {
 public:
  InputRouterImplScaleTouchEventTest() {}

  // Test tests if two finger touch event at (10, 20) and (100, 200) are
  // properly scaled. The touch event must be generated ans flushed into
  // the message sink prior to this method.
  void RunTouchEventTest(const std::string& name, WebTouchPoint::State state) {
    SCOPED_TRACE(name);
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
    SendTouchEvent();
    UpdateDispatchedEvents();
    EXPECT_EQ(1u, dispatched_events_.size());
    if (dispatched_events_.at(0).callback_) {
      CallCallback(std::move(dispatched_events_.at(0).callback_),
                   INPUT_EVENT_ACK_STATE_CONSUMED);
    }
    ASSERT_TRUE(TouchEventQueueEmpty());
  }

  void ReleaseTouchPointAndAck(int index) {
    ReleaseTouchPoint(index);
    SendTouchEvent();
    UpdateDispatchedEvents();
    EXPECT_EQ(1u, dispatched_events_.size());
    CallCallback(std::move(dispatched_events_.at(0).callback_),
                 INPUT_EVENT_ACK_STATE_CONSUMED);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputRouterImplScaleTouchEventTest);
};

}  // namespace

TEST_F(InputRouterImplScaleTouchEventTest, ScaleTouchEventTest) {
  // Press
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::kTouchStart);

  RunTouchEventTest("Press", WebTouchPoint::kStatePressed);
  ReleaseTouchPointAndAck(1);
  ReleaseTouchPointAndAck(0);

  // Move
  PressTouchPoint(0, 0);
  PressTouchPoint(0, 0);
  FlushTouchEvent(WebInputEvent::kTouchStart);

  MoveTouchPoint(0, 10, 20);
  MoveTouchPoint(1, 100, 200);
  FlushTouchEvent(WebInputEvent::kTouchMove);
  RunTouchEventTest("Move", WebTouchPoint::kStateMoved);
  ReleaseTouchPointAndAck(1);
  ReleaseTouchPointAndAck(0);

  // Release
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::kTouchMove);

  ReleaseTouchPoint(0);
  ReleaseTouchPoint(1);
  FlushTouchEvent(WebInputEvent::kTouchEnd);
  RunTouchEventTest("Release", WebTouchPoint::kStateReleased);

  // Cancel
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::kTouchStart);

  CancelTouchPoint(0);
  CancelTouchPoint(1);
  FlushTouchEvent(WebInputEvent::kTouchCancel);
  RunTouchEventTest("Cancel", WebTouchPoint::kStateCancelled);
}

namespace {

class InputRouterImplScaleGestureEventTest
    : public InputRouterImplScaleEventTest {
 public:
  InputRouterImplScaleGestureEventTest() {}

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
  }

  void FlushGestureEvent(WebInputEvent::Type type) {
    UpdateDispatchedEvents();
    EXPECT_EQ(1u, dispatched_events_.size());
    if (dispatched_events_.at(0).callback_) {
      CallCallback(std::move(dispatched_events_.at(0).callback_),
                   INPUT_EVENT_ACK_STATE_CONSUMED);
    }
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
  DISALLOW_COPY_AND_ASSIGN(InputRouterImplScaleGestureEventTest);
};

}  // namespace

TEST_F(InputRouterImplScaleGestureEventTest, GestureScrollUpdate) {
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

TEST_F(InputRouterImplScaleGestureEventTest, GestureScrollBegin) {
  SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollBegin(
      10.f, 20.f, blink::kWebGestureDeviceTouchscreen));
  FlushGestureEvent(WebInputEvent::kGestureScrollBegin);

  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  EXPECT_EQ(20.f, sent_event->data.scroll_begin.delta_x_hint);
  EXPECT_EQ(40.f, sent_event->data.scroll_begin.delta_y_hint);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  EXPECT_EQ(10.f, filter_event->data.scroll_begin.delta_x_hint);
  EXPECT_EQ(20.f, filter_event->data.scroll_begin.delta_y_hint);
}

TEST_F(InputRouterImplScaleGestureEventTest, GesturePinchUpdate) {
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

TEST_F(InputRouterImplScaleGestureEventTest, GestureTapDown) {
  const gfx::Point orig(10, 20), scaled(20, 40);
  WebGestureEvent event =
      BuildGestureEvent(WebInputEvent::kGestureTapDown, orig);
  event.data.tap_down.width = 30;
  event.data.tap_down.height = 40;
  SimulateGestureEvent(event);
  FlushGestureEvent(WebInputEvent::kGestureTapDown);
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

TEST_F(InputRouterImplScaleGestureEventTest, GestureTapOthers) {
  TestTap("GestureDoubleTap", WebInputEvent::kGestureDoubleTap);
  TestTap("GestureTap", WebInputEvent::kGestureTap);
  TestTap("GestureTapUnconfirmed", WebInputEvent::kGestureTapUnconfirmed);
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureShowPress) {
  const gfx::Point orig(10, 20), scaled(20, 40);
  WebGestureEvent event =
      BuildGestureEvent(WebInputEvent::kGestureShowPress, orig);
  event.data.show_press.width = 30;
  event.data.show_press.height = 40;
  SimulateGestureEvent(event);
  FlushGestureEvent(WebInputEvent::kGestureShowPress);

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

TEST_F(InputRouterImplScaleGestureEventTest, GestureLongPress) {
  TestLongPress("LongPress", WebInputEvent::kGestureLongPress);
  TestLongPress("LongPap", WebInputEvent::kGestureLongTap);
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureTwoFingerTap) {
  WebGestureEvent event = BuildGestureEvent(WebInputEvent::kGestureTwoFingerTap,
                                            gfx::Point(10, 20));
  event.data.two_finger_tap.first_finger_width = 30;
  event.data.two_finger_tap.first_finger_height = 40;
  SimulateGestureEvent(event);
  FlushGestureEvent(WebInputEvent::kGestureTwoFingerTap);

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

TEST_F(InputRouterImplScaleGestureEventTest, GestureFlingStart) {
  const gfx::Point orig(10, 20), scaled(20, 40);
  WebGestureEvent event =
      BuildGestureEvent(WebInputEvent::kGestureFlingStart, orig);
  event.data.fling_start.velocity_x = 30;
  event.data.fling_start.velocity_y = 40;
  SimulateGestureEvent(event);
  FlushGestureEvent(WebInputEvent::kGestureFlingStart);

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
