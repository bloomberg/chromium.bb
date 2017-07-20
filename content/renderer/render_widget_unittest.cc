// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include <tuple>
#include <vector>

#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/resize_params.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/devtools/render_widget_screen_metrics_emulator.h"
#include "content/test/fake_compositor_dependencies.h"
#include "content/test/mock_render_process.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebCoalescedInputEvent.h"
#include "third_party/WebKit/public/web/WebDeviceEmulationParams.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/rect.h"

using testing::_;

namespace content {

namespace {

const char* EVENT_LISTENER_RESULT_HISTOGRAM = "Event.PassiveListeners";

// Keep in sync with enum defined in
// RenderWidgetInputHandler::LogPassiveEventListenersUma.
enum {
  PASSIVE_LISTENER_UMA_ENUM_PASSIVE,
  PASSIVE_LISTENER_UMA_ENUM_UNCANCELABLE,
  PASSIVE_LISTENER_UMA_ENUM_SUPPRESSED,
  PASSIVE_LISTENER_UMA_ENUM_CANCELABLE,
  PASSIVE_LISTENER_UMA_ENUM_CANCELABLE_AND_CANCELED,
  PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING,
  PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_MAIN_THREAD_RESPONSIVENESS,
  PASSIVE_LISTENER_UMA_ENUM_COUNT
};

bool ShouldBlockEventStream(const blink::WebInputEvent& event) {
  return ui::WebInputEventTraits::ShouldBlockEventStream(
      event,
      base::FeatureList::IsEnabled(features::kRafAlignedTouchInputEvents),
      base::FeatureList::IsEnabled(features::kTouchpadAndWheelScrollLatching));
}

class MockWebWidget : public blink::WebWidget {
 public:
  MOCK_METHOD1(
      HandleInputEvent,
      blink::WebInputEventResult(const blink::WebCoalescedInputEvent&));
};

}  // namespace

class InteractiveRenderWidget : public RenderWidget {
 public:
  explicit InteractiveRenderWidget(CompositorDependencies* compositor_deps)
      : RenderWidget(++next_routing_id_,
                     compositor_deps,
                     blink::kWebPopupTypeNone,
                     ScreenInfo(),
                     false,
                     false,
                     false),
        always_overscroll_(false) {
    Init(RenderWidget::ShowCallback(), mock_webwidget());
  }

  void SetTouchRegion(const std::vector<gfx::Rect>& rects) {
    rects_ = rects;
  }

  void SendInputEvent(const blink::WebInputEvent& event) {
    OnHandleInputEvent(
        &event, std::vector<const blink::WebInputEvent*>(), ui::LatencyInfo(),
        ShouldBlockEventStream(event)
            ? InputEventDispatchType::DISPATCH_TYPE_BLOCKING
            : InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING);
  }

  void set_always_overscroll(bool overscroll) {
    always_overscroll_ = overscroll;
  }

  IPC::TestSink* sink() { return &sink_; }

  MockWebWidget* mock_webwidget() { return &mock_webwidget_; }

 protected:
  ~InteractiveRenderWidget() override { webwidget_internal_ = nullptr; }

  // Overridden from RenderWidget:
  bool HasTouchEventHandlersAt(const gfx::Point& point) const override {
    for (std::vector<gfx::Rect>::const_iterator iter = rects_.begin();
         iter != rects_.end(); ++iter) {
      if ((*iter).Contains(point))
        return true;
    }
    return false;
  }

  bool WillHandleGestureEvent(const blink::WebGestureEvent& event) override {
    if (always_overscroll_ &&
        event.GetType() == blink::WebInputEvent::kGestureScrollUpdate) {
      DidOverscroll(blink::WebFloatSize(event.data.scroll_update.delta_x,
                                        event.data.scroll_update.delta_y),
                    blink::WebFloatSize(event.data.scroll_update.delta_x,
                                        event.data.scroll_update.delta_y),
                    blink::WebFloatPoint(event.x, event.y),
                    blink::WebFloatSize(event.data.scroll_update.velocity_x,
                                        event.data.scroll_update.velocity_y));
      return true;
    }

    return false;
  }

  bool Send(IPC::Message* msg) override {
    sink_.OnMessageReceived(*msg);
    delete msg;
    return true;
  }

 private:
  std::vector<gfx::Rect> rects_;
  IPC::TestSink sink_;
  bool always_overscroll_;
  MockWebWidget mock_webwidget_;
  static int next_routing_id_;

  DISALLOW_COPY_AND_ASSIGN(InteractiveRenderWidget);
};

int InteractiveRenderWidget::next_routing_id_ = 0;

class RenderWidgetUnittest : public testing::Test {
 public:
  RenderWidgetUnittest()
      : widget_(new InteractiveRenderWidget(&compositor_deps_)) {
    // RenderWidget::Init does an AddRef that's balanced by a browser-initiated
    // Close IPC. That Close will never happen in this test, so do a Release
    // here to ensure |widget_| is properly freed.
    widget_->Release();
    DCHECK(widget_->HasOneRef());
  }

  ~RenderWidgetUnittest() override {}

  InteractiveRenderWidget* widget() const { return widget_.get(); }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  MockRenderProcess render_process_;
  MockRenderThread render_thread_;
  FakeCompositorDependencies compositor_deps_;
  scoped_refptr<InteractiveRenderWidget> widget_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetUnittest);
};

TEST_F(RenderWidgetUnittest, TouchHitTestSinglePoint) {
  SyntheticWebTouchEvent touch;
  touch.PressPoint(10, 10);

  EXPECT_CALL(*widget()->mock_webwidget(), HandleInputEvent(_))
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::kNotHandled));

  widget()->SendInputEvent(touch);
  ASSERT_EQ(1u, widget()->sink()->message_count());

  // Since there's currently no touch-event handling region, the response should
  // be 'no consumer exists'.
  const IPC::Message* message = widget()->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Param params;
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  InputEventAckState ack_state = std::get<0>(params).state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, ack_state);
  widget()->sink()->ClearMessages();

  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(0, 0, 20, 20));
  rects.push_back(gfx::Rect(25, 0, 10, 10));
  widget()->SetTouchRegion(rects);

  EXPECT_CALL(*widget()->mock_webwidget(), HandleInputEvent(_))
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::kNotHandled));

  widget()->SendInputEvent(touch);
  ASSERT_EQ(1u, widget()->sink()->message_count());
  message = widget()->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  ack_state = std::get<0>(params).state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, ack_state);
  widget()->sink()->ClearMessages();
}

TEST_F(RenderWidgetUnittest, TouchHitTestMultiplePoints) {
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(0, 0, 20, 20));
  rects.push_back(gfx::Rect(25, 0, 10, 10));
  widget()->SetTouchRegion(rects);

  SyntheticWebTouchEvent touch;
  touch.PressPoint(25, 25);

  EXPECT_CALL(*widget()->mock_webwidget(), HandleInputEvent(_))
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::kNotHandled));

  widget()->SendInputEvent(touch);
  ASSERT_EQ(1u, widget()->sink()->message_count());

  // Since there's currently no touch-event handling region, the response should
  // be 'no consumer exists'.
  const IPC::Message* message = widget()->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Param params;
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  InputEventAckState ack_state = std::get<0>(params).state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, ack_state);
  widget()->sink()->ClearMessages();

  // Press a second touch point. This time, on a touch-handling region.
  touch.PressPoint(10, 10);
  widget()->SendInputEvent(touch);
  ASSERT_EQ(1u, widget()->sink()->message_count());
  message = widget()->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  ack_state = std::get<0>(params).state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, ack_state);
  widget()->sink()->ClearMessages();
}

TEST_F(RenderWidgetUnittest, EventOverscroll) {
  widget()->set_always_overscroll(true);

  EXPECT_CALL(*widget()->mock_webwidget(), HandleInputEvent(_))
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::kNotHandled));

  blink::WebGestureEvent scroll(
      blink::WebInputEvent::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  scroll.x = -10;
  scroll.data.scroll_update.delta_y = 10;
  widget()->SendInputEvent(scroll);

  // Overscroll notifications received while handling an input event should
  // be bundled with the event ack IPC.
  ASSERT_EQ(1u, widget()->sink()->message_count());
  const IPC::Message* message = widget()->sink()->GetMessageAt(0);
  ASSERT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Param params;
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  const InputEventAck& ack = std::get<0>(params);
  ASSERT_EQ(ack.type, scroll.GetType());
  ASSERT_TRUE(ack.overscroll);
  EXPECT_EQ(gfx::Vector2dF(0, 10), ack.overscroll->accumulated_overscroll);
  EXPECT_EQ(gfx::Vector2dF(0, 10), ack.overscroll->latest_overscroll_delta);
  EXPECT_EQ(gfx::Vector2dF(), ack.overscroll->current_fling_velocity);
  EXPECT_EQ(gfx::PointF(-10, 0), ack.overscroll->causal_event_viewport_point);
  widget()->sink()->ClearMessages();
}

TEST_F(RenderWidgetUnittest, FlingOverscroll) {
  // Overscroll notifications received outside of handling an input event should
  // be sent as a separate IPC.
  widget()->DidOverscroll(blink::WebFloatSize(10, 5), blink::WebFloatSize(5, 5),
                          blink::WebFloatPoint(1, 1),
                          blink::WebFloatSize(10, 5));
  ASSERT_EQ(1u, widget()->sink()->message_count());
  const IPC::Message* message = widget()->sink()->GetMessageAt(0);
  ASSERT_EQ(InputHostMsg_DidOverscroll::ID, message->type());
  InputHostMsg_DidOverscroll::Param params;
  InputHostMsg_DidOverscroll::Read(message, &params);
  const ui::DidOverscrollParams& overscroll = std::get<0>(params);
  EXPECT_EQ(gfx::Vector2dF(10, 5), overscroll.latest_overscroll_delta);
  EXPECT_EQ(gfx::Vector2dF(5, 5), overscroll.accumulated_overscroll);
  EXPECT_EQ(gfx::PointF(1, 1), overscroll.causal_event_viewport_point);
  EXPECT_EQ(gfx::Vector2dF(10, 5), overscroll.current_fling_velocity);
  widget()->sink()->ClearMessages();
}

TEST_F(RenderWidgetUnittest, RenderWidgetInputEventUmaMetrics) {
  SyntheticWebTouchEvent touch;
  touch.PressPoint(10, 10);
  touch.touch_start_or_first_touch_move = true;

  EXPECT_CALL(*widget()->mock_webwidget(), HandleInputEvent(_))
      .Times(7)
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::kNotHandled));

  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_CANCELABLE, 1);

  touch.dispatch_type = blink::WebInputEvent::DispatchType::kEventNonBlocking;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_UNCANCELABLE,
                                       1);

  touch.dispatch_type =
      blink::WebInputEvent::DispatchType::kListenersNonBlockingPassive;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_PASSIVE, 1);

  touch.dispatch_type =
      blink::WebInputEvent::DispatchType::kListenersForcedNonBlockingDueToFling;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING, 1);

  touch.MovePoint(0, 10, 10);
  touch.touch_start_or_first_touch_move = true;
  touch.dispatch_type =
      blink::WebInputEvent::DispatchType::kListenersForcedNonBlockingDueToFling;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING, 2);

  touch.dispatch_type = blink::WebInputEvent::DispatchType::
      kListenersForcedNonBlockingDueToMainThreadResponsiveness;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_MAIN_THREAD_RESPONSIVENESS,
      1);

  touch.MovePoint(0, 10, 10);
  touch.touch_start_or_first_touch_move = true;
  touch.dispatch_type = blink::WebInputEvent::DispatchType::
      kListenersForcedNonBlockingDueToMainThreadResponsiveness;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_MAIN_THREAD_RESPONSIVENESS,
      2);

  EXPECT_CALL(*widget()->mock_webwidget(), HandleInputEvent(_))
      .WillOnce(
          ::testing::Return(blink::WebInputEventResult::kHandledSuppressed));
  touch.dispatch_type = blink::WebInputEvent::DispatchType::kBlocking;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_SUPPRESSED, 1);

  EXPECT_CALL(*widget()->mock_webwidget(), HandleInputEvent(_))
      .WillOnce(
          ::testing::Return(blink::WebInputEventResult::kHandledApplication));
  touch.dispatch_type = blink::WebInputEvent::DispatchType::kBlocking;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_CANCELABLE_AND_CANCELED, 1);
}

TEST_F(RenderWidgetUnittest, TouchDuringOrOutsideFlingUmaMetrics) {
  EXPECT_CALL(*widget()->mock_webwidget(), HandleInputEvent(_))
      .Times(3)
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::kNotHandled));

  SyntheticWebTouchEvent touch;
  touch.PressPoint(10, 10);
  touch.dispatch_type = blink::WebInputEvent::DispatchType::kBlocking;
  touch.touch_start_or_first_touch_move = true;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectTotalCount("Event.Touch.TouchLatencyOutsideFling",
                                      1);

  touch.MovePoint(0, 10, 10);
  touch.touch_start_or_first_touch_move = true;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectTotalCount("Event.Touch.TouchLatencyOutsideFling",
                                      2);

  touch.MovePoint(0, 30, 30);
  touch.touch_start_or_first_touch_move = false;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectTotalCount("Event.Touch.TouchLatencyOutsideFling",
                                      2);
}

class PopupRenderWidget : public RenderWidget {
 public:
  explicit PopupRenderWidget(CompositorDependencies* compositor_deps)
      : RenderWidget(routing_id_++,
                     compositor_deps,
                     blink::kWebPopupTypePage,
                     ScreenInfo(),
                     false,
                     false,
                     false) {
    Init(RenderWidget::ShowCallback(), mock_webwidget());
    did_show_ = true;
  }

  IPC::TestSink* sink() { return &sink_; }

  MockWebWidget* mock_webwidget() { return &mock_webwidget_; }

  void SetScreenMetricsEmulationParameters(
      bool,
      const blink::WebDeviceEmulationParams&) override {}

 protected:
  ~PopupRenderWidget() override { webwidget_internal_ = nullptr; }

  bool Send(IPC::Message* msg) override {
    sink_.OnMessageReceived(*msg);
    delete msg;
    return true;
  }

 private:
  IPC::TestSink sink_;
  MockWebWidget mock_webwidget_;
  static int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(PopupRenderWidget);
};

int PopupRenderWidget::routing_id_ = 1;

class RenderWidgetPopupUnittest : public testing::Test {
 public:
  RenderWidgetPopupUnittest()
      : widget_(new PopupRenderWidget(&compositor_deps_)) {
    // RenderWidget::Init does an AddRef that's balanced by a browser-initiated
    // Close IPC. That Close will never happen in this test, so do a Release
    // here to ensure |widget_| is properly freed.
    widget_->Release();
    DCHECK(widget_->HasOneRef());
  }
  ~RenderWidgetPopupUnittest() override {}

  PopupRenderWidget* widget() const { return widget_.get(); }
  FakeCompositorDependencies compositor_deps_;

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  MockRenderProcess render_process_;
  MockRenderThread render_thread_;
  scoped_refptr<PopupRenderWidget> widget_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetPopupUnittest);
};

TEST_F(RenderWidgetPopupUnittest, EmulatingPopupRect) {
  blink::WebRect popup_screen_rect(200, 250, 100, 400);
  widget()->SetWindowRect(popup_screen_rect);

  // The view and window rect on a popup type RenderWidget should be
  // immediately set, without requiring an ACK.
  EXPECT_EQ(popup_screen_rect.x, widget()->WindowRect().x);
  EXPECT_EQ(popup_screen_rect.y, widget()->WindowRect().y);

  EXPECT_EQ(popup_screen_rect.x, widget()->ViewRect().x);
  EXPECT_EQ(popup_screen_rect.y, widget()->ViewRect().y);

  gfx::Rect emulated_window_rect(0, 0, 980, 1200);

  blink::WebDeviceEmulationParams emulation_params;
  emulation_params.screen_position = blink::WebDeviceEmulationParams::kMobile;
  emulation_params.view_size = emulated_window_rect.size();
  emulation_params.view_position = blink::WebPoint(150, 160);

  gfx::Rect parent_window_rect = gfx::Rect(0, 0, 800, 600);

  ResizeParams resize_params;
  resize_params.new_size = parent_window_rect.size();

  scoped_refptr<PopupRenderWidget> parent_widget(
      new PopupRenderWidget(&compositor_deps_));
  parent_widget->Release();  // Balance Init().
  RenderWidgetScreenMetricsEmulator emulator(
      parent_widget.get(), emulation_params, resize_params, parent_window_rect,
      parent_window_rect);
  emulator.Apply();

  widget()->SetPopupOriginAdjustmentsForEmulation(&emulator);

  // Position of the popup as seen by the emulated widget.
  gfx::Point emulated_position(
      emulation_params.view_position.x + popup_screen_rect.x,
      emulation_params.view_position.y + popup_screen_rect.y);

  // Both the window and view rects as read from the accessors should have the
  // emulation parameters applied.
  EXPECT_EQ(emulated_position.x(), widget()->WindowRect().x);
  EXPECT_EQ(emulated_position.y(), widget()->WindowRect().y);
  EXPECT_EQ(emulated_position.x(), widget()->ViewRect().x);
  EXPECT_EQ(emulated_position.y(), widget()->ViewRect().y);

  // Setting a new window rect while emulated should remove the emulation
  // transformation from the given rect so that getting the rect, which applies
  // the transformation to the raw rect, should result in the same value.
  blink::WebRect popup_emulated_rect(130, 170, 100, 400);
  widget()->SetWindowRect(popup_emulated_rect);

  EXPECT_EQ(popup_emulated_rect.x, widget()->WindowRect().x);
  EXPECT_EQ(popup_emulated_rect.y, widget()->WindowRect().y);
  EXPECT_EQ(popup_emulated_rect.x, widget()->ViewRect().x);
  EXPECT_EQ(popup_emulated_rect.y, widget()->ViewRect().y);
}

}  // namespace content
