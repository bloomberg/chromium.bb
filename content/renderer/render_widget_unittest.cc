// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include <tuple>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/common/visual_properties.h"
#include "content/common/widget_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/compositor/layer_tree_view.h"
#include "content/renderer/devtools/render_widget_screen_metrics_emulator.h"
#include "content/renderer/input/widget_input_handler_manager.h"
#include "content/renderer/render_widget_delegate.h"
#include "content/test/fake_compositor_dependencies.h"
#include "content/test/mock_render_process.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/web/web_device_emulation_params.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/rect.h"

using testing::_;

namespace ui {

bool operator==(const ui::DidOverscrollParams& lhs,
                const ui::DidOverscrollParams& rhs) {
  return lhs.accumulated_overscroll == rhs.accumulated_overscroll &&
         lhs.latest_overscroll_delta == rhs.latest_overscroll_delta &&
         lhs.current_fling_velocity == rhs.current_fling_velocity &&
         lhs.causal_event_viewport_point == rhs.causal_event_viewport_point &&
         lhs.overscroll_behavior == rhs.overscroll_behavior;
}

}  // namespace ui

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
  PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_MAIN_THREAD_RESPONSIVENESS_DEPRECATED,
  PASSIVE_LISTENER_UMA_ENUM_COUNT
};

class MockWidgetInputHandlerHost : public mojom::WidgetInputHandlerHost {
 public:
  MockWidgetInputHandlerHost(
      mojo::InterfaceRequest<mojom::WidgetInputHandlerHost> request)
      : binding_(this, std::move(request)) {}
  MOCK_METHOD1(SetTouchActionFromMain, void(cc::TouchAction));

  MOCK_METHOD3(SetWhiteListedTouchAction,
               void(cc::TouchAction, uint32_t, content::InputEventAckState));

  MOCK_METHOD1(DidOverscroll, void(const ui::DidOverscrollParams&));

  MOCK_METHOD0(DidStopFlinging, void());

  MOCK_METHOD0(DidStartScrollingViewport, void());

  MOCK_METHOD0(ImeCancelComposition, void());

  MOCK_METHOD2(ImeCompositionRangeChanged,
               void(const gfx::Range&, const std::vector<gfx::Rect>&));

  MOCK_METHOD1(SetMouseCapture, void(bool));

 private:
  mojo::Binding<mojom::WidgetInputHandlerHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockWidgetInputHandlerHost);
};

// Since std::unique_ptr isn't copyable we can't use the
// MockCallback template.
class MockHandledEventCallback {
 public:
  MockHandledEventCallback() = default;
  MOCK_METHOD4_T(Run,
                 void(InputEventAckState,
                      const ui::LatencyInfo&,
                      std::unique_ptr<ui::DidOverscrollParams>&,
                      base::Optional<cc::TouchAction>));

  HandledEventCallback GetCallback() {
    return base::BindOnce(&MockHandledEventCallback::HandleCallback,
                          base::Unretained(this));
  }

 private:
  void HandleCallback(InputEventAckState ack_state,
                      const ui::LatencyInfo& latency_info,
                      std::unique_ptr<ui::DidOverscrollParams> overscroll,
                      base::Optional<cc::TouchAction> touch_action) {
    Run(ack_state, latency_info, overscroll, touch_action);
  }

  DISALLOW_COPY_AND_ASSIGN(MockHandledEventCallback);
};

class StubWebPagePopup : public blink::WebPagePopup {
 public:
  // WebWidget implementation.
  void SetLayerTreeView(blink::WebLayerTreeView*) override {}
  blink::WebURL GetURLForDebugTrace() override { return {}; }
  blink::WebHitTestResult HitTestResultAt(const gfx::Point&) override {
    return {};
  }

  // WebPagePopup implementation.
  blink::WebPoint PositionRelativeToOwner() override { return {}; }
};

class MockWebPagePopup : public StubWebPagePopup {
 public:
  MOCK_METHOD0(DispatchBufferedTouchEvents, blink::WebInputEventResult());
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
                     ScreenInfo(),
                     blink::kWebDisplayModeUndefined,
                     false,
                     false,
                     false),
        always_overscroll_(false) {
    InitForPopup(base::NullCallback(), &mock_page_popup_);

    mojom::WidgetInputHandlerHostPtr widget_input_handler;
    mock_input_handler_host_ = std::make_unique<MockWidgetInputHandlerHost>(
        mojo::MakeRequest(&widget_input_handler));

    widget_input_handler_manager_->AddInterface(
        nullptr, std::move(widget_input_handler));
  }

  void SendInputEvent(const blink::WebInputEvent& event,
                      HandledEventCallback callback) {
    HandleInputEvent(blink::WebCoalescedInputEvent(
                         event, std::vector<const blink::WebInputEvent*>(),
                         std::vector<const blink::WebInputEvent*>()),
                     ui::LatencyInfo(), std::move(callback));
  }

  void set_always_overscroll(bool overscroll) {
    always_overscroll_ = overscroll;
  }

  IPC::TestSink* sink() { return &sink_; }

  MockWebPagePopup* mock_webwidget() { return &mock_page_popup_; }

  MockWidgetInputHandlerHost* mock_input_handler_host() {
    return mock_input_handler_host_.get();
  }

  const viz::LocalSurfaceIdAllocation& local_surface_id_allocation_from_parent()
      const {
    return local_surface_id_allocation_from_parent_;
  }

 protected:
  ~InteractiveRenderWidget() override {
    Close();
    webwidget_internal_ = nullptr;
  }

  // Overridden from RenderWidget:
  bool WillHandleGestureEvent(const blink::WebGestureEvent& event) override {
    if (always_overscroll_ &&
        event.GetType() == blink::WebInputEvent::kGestureScrollUpdate) {
      DidOverscroll(blink::WebFloatSize(event.data.scroll_update.delta_x,
                                        event.data.scroll_update.delta_y),
                    blink::WebFloatSize(event.data.scroll_update.delta_x,
                                        event.data.scroll_update.delta_y),
                    event.PositionInWidget(),
                    blink::WebFloatSize(event.data.scroll_update.velocity_x,
                                        event.data.scroll_update.velocity_y),
                    cc::OverscrollBehavior());
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
  IPC::TestSink sink_;
  bool always_overscroll_;
  MockWebPagePopup mock_page_popup_;
  std::unique_ptr<MockWidgetInputHandlerHost> mock_input_handler_host_;
  static int next_routing_id_;

  DISALLOW_COPY_AND_ASSIGN(InteractiveRenderWidget);
};

int InteractiveRenderWidget::next_routing_id_ = 0;

class RenderWidgetUnittest : public testing::Test {
 public:
  RenderWidgetUnittest() = default;
  ~RenderWidgetUnittest() override = default;

  // testing::Test implementation.
  void SetUp() override {
    widget_ = new InteractiveRenderWidget(&compositor_deps_);
    // RenderWidget::Init does an AddRef that's balanced by a browser-initiated
    // Close IPC. That Close will never happen in this test, so do a Release
    // here to ensure |widget_| is properly freed.
    widget_->Release();
    DCHECK(widget_->HasOneRef());
  }

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

TEST_F(RenderWidgetUnittest, CursorChange) {
  blink::WebCursorInfo cursor_info;
  cursor_info.type = blink::WebCursorInfo::Type::kTypePointer;

  widget()->DidChangeCursor(cursor_info);
  EXPECT_EQ(widget()->sink()->message_count(), 1U);
  EXPECT_EQ(widget()->sink()->GetMessageAt(0)->type(),
            WidgetHostMsg_SetCursor::ID);
  widget()->sink()->ClearMessages();

  widget()->DidChangeCursor(cursor_info);
  EXPECT_EQ(widget()->sink()->message_count(), 0U);

  widget()->SendInputEvent(SyntheticWebMouseEventBuilder::Build(
                               blink::WebInputEvent::Type::kMouseLeave),
                           HandledEventCallback());
  EXPECT_EQ(widget()->sink()->message_count(), 0U);

  widget()->DidChangeCursor(cursor_info);
  EXPECT_EQ(widget()->sink()->message_count(), 1U);
  EXPECT_EQ(widget()->sink()->GetMessageAt(0)->type(),
            WidgetHostMsg_SetCursor::ID);
}

TEST_F(RenderWidgetUnittest, EventOverscroll) {
  widget()->set_always_overscroll(true);

  EXPECT_CALL(*widget()->mock_webwidget(), HandleInputEvent(_))
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::kNotHandled));

  blink::WebGestureEvent scroll(blink::WebInputEvent::kGestureScrollUpdate,
                                blink::WebInputEvent::kNoModifiers,
                                ui::EventTimeForNow());
  scroll.SetPositionInWidget(gfx::PointF(-10, 0));
  scroll.data.scroll_update.delta_y = 10;
  MockHandledEventCallback handled_event;

  ui::DidOverscrollParams expected_overscroll;
  expected_overscroll.latest_overscroll_delta = gfx::Vector2dF(0, 10);
  expected_overscroll.accumulated_overscroll = gfx::Vector2dF(0, 10);
  expected_overscroll.causal_event_viewport_point = gfx::PointF(-10, 0);
  expected_overscroll.current_fling_velocity = gfx::Vector2dF();

  // Overscroll notifications received while handling an input event should
  // be bundled with the event ack IPC.
  EXPECT_CALL(handled_event, Run(INPUT_EVENT_ACK_STATE_CONSUMED, _,
                                 testing::Pointee(expected_overscroll), _))
      .Times(1);

  widget()->SendInputEvent(scroll, handled_event.GetCallback());
}

TEST_F(RenderWidgetUnittest, RenderWidgetInputEventUmaMetrics) {
  SyntheticWebTouchEvent touch;
  touch.PressPoint(10, 10);
  touch.touch_start_or_first_touch_move = true;

  EXPECT_CALL(*widget()->mock_webwidget(), HandleInputEvent(_))
      .Times(5)
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::kNotHandled));

  EXPECT_CALL(*widget()->mock_webwidget(), DispatchBufferedTouchEvents())
      .Times(5)
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::kNotHandled));

  widget()->SendInputEvent(touch, HandledEventCallback());
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_CANCELABLE, 1);

  touch.dispatch_type = blink::WebInputEvent::DispatchType::kEventNonBlocking;
  widget()->SendInputEvent(touch, HandledEventCallback());
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_UNCANCELABLE,
                                       1);

  touch.dispatch_type =
      blink::WebInputEvent::DispatchType::kListenersNonBlockingPassive;
  widget()->SendInputEvent(touch, HandledEventCallback());
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_PASSIVE, 1);

  touch.dispatch_type =
      blink::WebInputEvent::DispatchType::kListenersForcedNonBlockingDueToFling;
  widget()->SendInputEvent(touch, HandledEventCallback());
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING, 1);

  touch.MovePoint(0, 10, 10);
  touch.touch_start_or_first_touch_move = true;
  touch.dispatch_type =
      blink::WebInputEvent::DispatchType::kListenersForcedNonBlockingDueToFling;
  widget()->SendInputEvent(touch, HandledEventCallback());
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING, 2);

  EXPECT_CALL(*widget()->mock_webwidget(), HandleInputEvent(_))
      .WillOnce(::testing::Return(blink::WebInputEventResult::kNotHandled));
  EXPECT_CALL(*widget()->mock_webwidget(), DispatchBufferedTouchEvents())
      .WillOnce(
          ::testing::Return(blink::WebInputEventResult::kHandledSuppressed));
  touch.dispatch_type = blink::WebInputEvent::DispatchType::kBlocking;
  widget()->SendInputEvent(touch, HandledEventCallback());
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_SUPPRESSED, 1);

  EXPECT_CALL(*widget()->mock_webwidget(), HandleInputEvent(_))
      .WillOnce(::testing::Return(blink::WebInputEventResult::kNotHandled));
  EXPECT_CALL(*widget()->mock_webwidget(), DispatchBufferedTouchEvents())
      .WillOnce(
          ::testing::Return(blink::WebInputEventResult::kHandledApplication));
  touch.dispatch_type = blink::WebInputEvent::DispatchType::kBlocking;
  widget()->SendInputEvent(touch, HandledEventCallback());
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_CANCELABLE_AND_CANCELED, 1);
}

// Tests that if a RenderWidget is auto-resized, it requests a new
// viz::LocalSurfaceId to be allocated on the impl thread.
TEST_F(RenderWidgetUnittest, AutoResizeAllocatedLocalSurfaceId) {
  viz::ParentLocalSurfaceIdAllocator allocator;

  // Enable auto-resize.
  content::VisualProperties visual_properties;
  visual_properties.auto_resize_enabled = true;
  visual_properties.min_size_for_auto_resize = gfx::Size(100, 100);
  visual_properties.max_size_for_auto_resize = gfx::Size(200, 200);
  allocator.GenerateId();
  visual_properties.local_surface_id_allocation =
      allocator.GetCurrentLocalSurfaceIdAllocation();
  widget()->SynchronizeVisualProperties(visual_properties);
  EXPECT_EQ(allocator.GetCurrentLocalSurfaceIdAllocation(),
            widget()->local_surface_id_allocation_from_parent());
  EXPECT_FALSE(widget()
                   ->layer_tree_view()
                   ->layer_tree_host()
                   ->new_local_surface_id_request_for_testing());

  constexpr gfx::Size size(200, 200);
  widget()->DidAutoResize(size);
  EXPECT_EQ(allocator.GetCurrentLocalSurfaceIdAllocation(),
            widget()->local_surface_id_allocation_from_parent());
  EXPECT_TRUE(widget()
                  ->layer_tree_view()
                  ->layer_tree_host()
                  ->new_local_surface_id_request_for_testing());
}

class PopupRenderWidget : public RenderWidget {
 public:
  explicit PopupRenderWidget(CompositorDependencies* compositor_deps)
      : RenderWidget(routing_id_++,
                     compositor_deps,
                     ScreenInfo(),
                     blink::kWebDisplayModeUndefined,
                     false,
                     false,
                     false) {
    InitForPopup(RenderWidget::ShowCallback(), &stub_page_popup_);
  }

  IPC::TestSink* sink() { return &sink_; }

  void SetScreenMetricsEmulationParameters(
      bool,
      const blink::WebDeviceEmulationParams&) override {}

  // Shuts down the metrics emulator, the compositor, and destroys the internal
  // WebWidget. Should be called before destroying the object.
  void Shutdown() {
    RenderWidget::Close();
    shutdown_ = true;
  }

 protected:
  ~PopupRenderWidget() override { DCHECK(shutdown_); }

  bool Send(IPC::Message* msg) override {
    sink_.OnMessageReceived(*msg);
    delete msg;
    return true;
  }

 private:
  bool shutdown_ = false;
  IPC::TestSink sink_;
  StubWebPagePopup stub_page_popup_;
  static int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(PopupRenderWidget);
};

int PopupRenderWidget::routing_id_ = 1;

class RenderWidgetPopupUnittest : public testing::Test {
 public:
  RenderWidgetPopupUnittest() = default;
  ~RenderWidgetPopupUnittest() override { widget_->Shutdown(); }

  // testing::Test implementation.
  void SetUp() override {
    widget_ = new PopupRenderWidget(&compositor_deps_);
    // RenderWidget::Init does an AddRef that's balanced by a browser-initiated
    // Close IPC. That Close will never happen in this test, so do a Release
    // here to ensure |widget_| is properly freed.
    widget_->Release();
    DCHECK(widget_->HasOneRef());
  }

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

class StubRenderWidgetDelegate : public RenderWidgetDelegate {
 public:
  blink::WebWidget* GetWebWidgetForWidget() const override { return nullptr; }
  blink::WebWidgetClient* GetWebWidgetClientForWidget() override {
    return nullptr;
  }
  bool RenderWidgetWillHandleMouseEventForWidget(
      const blink::WebMouseEvent& event) override {
    return false;
  }
  void SetActiveForWidget(bool active) override {}
  bool SupportsMultipleWindowsForWidget() override { return true; }
  void DidHandleGestureEventForWidget(
      const blink::WebGestureEvent& event) override {}
  void DidCloseWidget() override {}
  void CancelPagePopupForWidget() override {}
  void ApplyNewDisplayModeForWidget(
      const blink::WebDisplayMode& new_display_mode) override {}
  void ApplyAutoResizeLimitsForWidget(const gfx::Size& min_size,
                                      const gfx::Size& max_size) override {}
  void DisableAutoResizeForWidget() override {}
  void ScrollFocusedNodeIntoViewForWidget() override {}
  void DidReceiveSetFocusEventForWidget() override {}
  void DidChangeFocusForWidget() override {}
  void DidCommitCompositorFrameForWidget() override {}
  void DidCompletePageScaleAnimationForWidget() override {}
  void ResizeWebWidgetForWidget(
      const gfx::Size& size,
      float top_controls_height,
      float bottom_controls_height,
      bool browser_controls_shrink_blink_size) override {}
  void SetScreenMetricsEmulationParametersForWidget(
      bool enabled,
      const blink::WebDeviceEmulationParams& params) override {}
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

  VisualProperties visual_properties;
  visual_properties.new_size = parent_window_rect.size();

  scoped_refptr<PopupRenderWidget> parent_widget(
      new PopupRenderWidget(&compositor_deps_));
  parent_widget->Release();  // Balance Init().

  // Emulation only happens for RenderWidgets with a delegate.
  parent_widget->set_delegate(std::make_unique<StubRenderWidgetDelegate>());

  // Setup emulation on the |parent_widget|.
  parent_widget->OnSynchronizeVisualProperties(visual_properties);
  parent_widget->OnEnableDeviceEmulation(emulation_params);
  // Then use it for the popup widget under test.
  widget()->ApplyEmulatedScreenMetricsForPopupWidget(parent_widget.get());

  // Position of the popup as seen by the emulated widget.
  gfx::Point emulated_position(
      emulation_params.view_position->x + popup_screen_rect.x,
      emulation_params.view_position->y + popup_screen_rect.y);

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

  parent_widget->Shutdown();
}

// Verify desktop memory limit calculations.
#if !defined(OS_ANDROID)
TEST(RenderWidgetTest, IgnoreGivenMemoryPolicy) {
  auto policy = RenderWidget::GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                                                 gfx::Size(), 1.f);
  EXPECT_EQ(512u * 1024u * 1024u, policy.bytes_limit_when_visible);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
            policy.priority_cutoff_when_visible);
}

TEST(RenderWidgetTest, LargeScreensUseMoreMemory) {
  auto policy = RenderWidget::GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                                                 gfx::Size(4096, 2160), 1.f);
  EXPECT_EQ(2u * 512u * 1024u * 1024u, policy.bytes_limit_when_visible);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
            policy.priority_cutoff_when_visible);

  policy = RenderWidget::GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                                            gfx::Size(2048, 1080), 2.f);
  EXPECT_EQ(2u * 512u * 1024u * 1024u, policy.bytes_limit_when_visible);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
            policy.priority_cutoff_when_visible);
}
#endif

#if defined(OS_ANDROID)
class RenderWidgetSurfaceSyncUnittest : public RenderWidgetUnittest {
 public:
  RenderWidgetSurfaceSyncUnittest() = default;
  ~RenderWidgetSurfaceSyncUnittest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kEnableSurfaceSynchronization);
    RenderWidgetUnittest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(RenderWidgetSurfaceSyncUnittest, ForceSendMetadataOnInput) {
  auto* layer_tree_host = widget()->layer_tree_view()->layer_tree_host();
  // We should not have any force send metadata requests at start.
  EXPECT_FALSE(layer_tree_host->TakeForceSendMetadataRequest());
  // ShowVirtualKeyboard will trigger a text input state update.
  widget()->ShowVirtualKeyboard();
  // We should now have a force send metadata request.
  EXPECT_TRUE(layer_tree_host->TakeForceSendMetadataRequest());
}
#endif  // !defined(OS_ANDROID)

}  // namespace content
