// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/input/legacy_input_router_impl.h"
#include "content/browser/renderer_host/input/touch_emulator.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/edit_command.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/render_frame_metadata.mojom.h"
#include "content/common/resize_params.h"
#include "content/common/view_messages.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/fake_renderer_compositor_frame_sink.h"
#include "content/test/mock_widget_impl.h"
#include "content/test/mock_widget_input_handler.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"

#if defined(OS_ANDROID)
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "ui/android/screen_android.h"
#endif

#if defined(USE_AURA) || defined(OS_MACOSX)
#include "content/browser/compositor/test/test_image_transport_factory.h"
#endif

#if defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/aura/test/test_screen.h"
#include "ui/events/event.h"
#endif

using base::TimeDelta;
using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

// MockInputRouter -------------------------------------------------------------

class MockInputRouter : public InputRouter {
 public:
  explicit MockInputRouter(InputRouterClient* client)
      : sent_mouse_event_(false),
        sent_wheel_event_(false),
        sent_keyboard_event_(false),
        sent_gesture_event_(false),
        send_touch_event_not_cancelled_(false),
        message_received_(false),
        client_(client) {}
  ~MockInputRouter() override {}

  // InputRouter
  void SendMouseEvent(const MouseEventWithLatencyInfo& mouse_event) override {
    sent_mouse_event_ = true;
  }
  void SendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override {
    sent_wheel_event_ = true;
  }
  void SendKeyboardEvent(
      const NativeWebKeyboardEventWithLatencyInfo& key_event) override {
    sent_keyboard_event_ = true;
  }
  void SendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) override {
    sent_gesture_event_ = true;
  }
  void SendTouchEvent(const TouchEventWithLatencyInfo& touch_event) override {
    send_touch_event_not_cancelled_ =
        client_->FilterInputEvent(touch_event.event, touch_event.latency) ==
        INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  }
  void NotifySiteIsMobileOptimized(bool is_mobile_optimized) override {}
  bool HasPendingEvents() const override { return false; }
  void SetDeviceScaleFactor(float device_scale_factor) override {}
  void SetFrameTreeNodeId(int frameTreeNodeId) override {}
  cc::TouchAction AllowedTouchAction() override { return cc::kTouchActionAuto; }
  void SetForceEnableZoom(bool enabled) override {}
  void BindHost(mojom::WidgetInputHandlerHostRequest request,
                bool frame_handler) override {}
  void ProgressFling(base::TimeTicks time) override {}
  void StopFling() override {}
  bool FlingCancellationIsDeferred() override { return false; }

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& message) override {
    message_received_ = true;
    return false;
  }

  bool sent_mouse_event_;
  bool sent_wheel_event_;
  bool sent_keyboard_event_;
  bool sent_gesture_event_;
  bool send_touch_event_not_cancelled_;
  bool message_received_;

 private:
  InputRouterClient* client_;

  DISALLOW_COPY_AND_ASSIGN(MockInputRouter);
};

// TestFrameTokenMessageQueue ----------------------------------------------

class TestFrameTokenMessageQueue : public FrameTokenMessageQueue {
 public:
  TestFrameTokenMessageQueue(FrameTokenMessageQueue::Client* client)
      : FrameTokenMessageQueue(client) {}
  ~TestFrameTokenMessageQueue() override {}

  uint32_t processed_frame_messages_count() {
    return processed_frame_messages_count_;
  }

 protected:
  void ProcessSwapMessages(std::vector<IPC::Message> messages) override {
    processed_frame_messages_count_++;
  }

 private:
  uint32_t processed_frame_messages_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestFrameTokenMessageQueue);
};

// MockRenderWidgetHost ----------------------------------------------------

class MockRenderWidgetHost : public RenderWidgetHostImpl {
 public:

  // Allow poking at a few private members.
  using RenderWidgetHostImpl::GetResizeParams;
  using RenderWidgetHostImpl::OnResizeOrRepaintACK;
  using RenderWidgetHostImpl::RendererExited;
  using RenderWidgetHostImpl::SetInitialRenderSizeParams;
  using RenderWidgetHostImpl::old_resize_params_;
  using RenderWidgetHostImpl::is_hidden_;
  using RenderWidgetHostImpl::resize_ack_pending_;
  using RenderWidgetHostImpl::input_router_;
  using RenderWidgetHostImpl::frame_token_message_queue_;

  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       InputEventAckSource ack_source,
                       InputEventAckState ack_result) override {
    // Sniff touch acks.
    acked_touch_event_type_ = event.event.GetType();
    RenderWidgetHostImpl::OnTouchEventAck(event, ack_source, ack_result);
  }

  void reset_new_content_rendering_timeout_fired() {
    new_content_rendering_timeout_fired_ = false;
  }

  bool new_content_rendering_timeout_fired() const {
    return new_content_rendering_timeout_fired_;
  }

  void DisableGestureDebounce() {
    if (base::FeatureList::IsEnabled(features::kMojoInputMessages)) {
      input_router_.reset(
          new InputRouterImpl(this, this, InputRouter::Config()));
      legacy_widget_input_handler_ = nullptr;
    } else {
      input_router_.reset(new LegacyInputRouterImpl(
          process_, this, this, routing_id_, InputRouter::Config()));
      legacy_widget_input_handler_ =
          std::make_unique<LegacyIPCWidgetInputHandler>(
              static_cast<LegacyInputRouterImpl*>(input_router_.get()));
    }
  }

  void ExpectForceEnableZoom(bool enable) {
    EXPECT_EQ(enable, force_enable_zoom_);

    if (base::FeatureList::IsEnabled(features::kMojoInputMessages)) {
      InputRouterImpl* input_router =
          static_cast<InputRouterImpl*>(input_router_.get());
      EXPECT_EQ(enable, input_router->touch_action_filter_.force_enable_zoom_);
    } else {
      LegacyInputRouterImpl* input_router =
          static_cast<LegacyInputRouterImpl*>(input_router_.get());
      EXPECT_EQ(enable, input_router->touch_action_filter_.force_enable_zoom_);
    }
  }

  WebInputEvent::Type acked_touch_event_type() const {
    return acked_touch_event_type_;
  }

  // Mocks out |renderer_compositor_frame_sink_| with a
  // CompositorFrameSinkClientPtr bound to
  // |mock_renderer_compositor_frame_sink|.
  void SetMockRendererCompositorFrameSink(
      viz::MockCompositorFrameSinkClient* mock_renderer_compositor_frame_sink) {
    renderer_compositor_frame_sink_ =
        mock_renderer_compositor_frame_sink->BindInterfacePtr();
  }

  void SetupForInputRouterTest() {
    input_router_.reset(new MockInputRouter(this));
    legacy_widget_input_handler_ = nullptr;
  }

  MockInputRouter* mock_input_router() {
    return static_cast<MockInputRouter*>(input_router_.get());
  }

  uint32_t processed_frame_messages_count() {
    CHECK(frame_token_message_queue_);
    return static_cast<TestFrameTokenMessageQueue*>(
               frame_token_message_queue_.get())
        ->processed_frame_messages_count();
  }

  static MockRenderWidgetHost* Create(RenderWidgetHostDelegate* delegate,
                                      RenderProcessHost* process,
                                      int32_t routing_id) {
    mojom::WidgetPtr widget;
    std::unique_ptr<MockWidgetImpl> widget_impl =
        std::make_unique<MockWidgetImpl>(mojo::MakeRequest(&widget));

    return new MockRenderWidgetHost(delegate, process, routing_id,
                                    std::move(widget_impl), std::move(widget));
  }

  mojom::WidgetInputHandler* GetWidgetInputHandler() override {
    if (base::FeatureList::IsEnabled(features::kMojoInputMessages)) {
      return &mock_widget_input_handler_;
    }
    return RenderWidgetHostImpl::GetWidgetInputHandler();
  }

  MockWidgetInputHandler mock_widget_input_handler_;

 protected:
  void NotifyNewContentRenderingTimeoutForTesting() override {
    new_content_rendering_timeout_fired_ = true;
  }

  bool new_content_rendering_timeout_fired_;
  WebInputEvent::Type acked_touch_event_type_;

 private:
  MockRenderWidgetHost(RenderWidgetHostDelegate* delegate,
                       RenderProcessHost* process,
                       int routing_id,
                       std::unique_ptr<MockWidgetImpl> widget_impl,
                       mojom::WidgetPtr widget)
      : RenderWidgetHostImpl(delegate,
                             process,
                             routing_id,
                             std::move(widget),
                             false),
        new_content_rendering_timeout_fired_(false),
        widget_impl_(std::move(widget_impl)) {
    acked_touch_event_type_ = blink::WebInputEvent::kUndefined;
    frame_token_message_queue_.reset(new TestFrameTokenMessageQueue(this));
  }

  std::unique_ptr<MockWidgetImpl> widget_impl_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderWidgetHost);
};

namespace  {

// RenderWidgetHostProcess -----------------------------------------------------

class RenderWidgetHostProcess : public MockRenderProcessHost {
 public:
  explicit RenderWidgetHostProcess(BrowserContext* browser_context)
      : MockRenderProcessHost(browser_context) {
  }
  ~RenderWidgetHostProcess() override {}

  bool HasConnection() const override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostProcess);
};

// TestView --------------------------------------------------------------------

// This test view allows us to specify the size, and keep track of acked
// touch-events.
class TestView : public TestRenderWidgetHostView {
 public:
  explicit TestView(RenderWidgetHostImpl* rwh)
      : TestRenderWidgetHostView(rwh),
        unhandled_wheel_event_count_(0),
        acked_event_count_(0),
        gesture_event_type_(-1),
        use_fake_compositor_viewport_pixel_size_(false),
        ack_result_(INPUT_EVENT_ACK_STATE_UNKNOWN),
        top_controls_height_(0.f),
        bottom_controls_height_(0.f) {}

  // Sets the bounds returned by GetViewBounds.
  void SetBounds(const gfx::Rect& bounds) override {
    if (bounds_ == bounds)
      return;
    bounds_ = bounds;
    local_surface_id_ = local_surface_id_allocator_.GenerateId();
  }

  void SetScreenInfo(const ScreenInfo& screen_info) {
    screen_info_ = screen_info;
  }

  void GetScreenInfo(ScreenInfo* screen_info) const override {
    *screen_info = screen_info_;
  }

  void set_top_controls_height(float top_controls_height) {
    top_controls_height_ = top_controls_height;
  }

  void set_bottom_controls_height(float bottom_controls_height) {
    bottom_controls_height_ = bottom_controls_height;
  }

  const WebTouchEvent& acked_event() const { return acked_event_; }
  int acked_event_count() const { return acked_event_count_; }
  void ClearAckedEvent() {
    acked_event_.SetType(blink::WebInputEvent::kUndefined);
    acked_event_count_ = 0;
  }

  const WebMouseWheelEvent& unhandled_wheel_event() const {
    return unhandled_wheel_event_;
  }
  int unhandled_wheel_event_count() const {
    return unhandled_wheel_event_count_;
  }
  int gesture_event_type() const { return gesture_event_type_; }
  InputEventAckState ack_result() const { return ack_result_; }

  void SetMockCompositorViewportPixelSize(
      const gfx::Size& mock_compositor_viewport_pixel_size) {
    use_fake_compositor_viewport_pixel_size_ = true;
    mock_compositor_viewport_pixel_size_ = mock_compositor_viewport_pixel_size;
  }
  void ClearMockCompositorViewportPixelSize() {
    use_fake_compositor_viewport_pixel_size_ = false;
  }

  const viz::BeginFrameAck& last_did_not_produce_frame_ack() {
    return last_did_not_produce_frame_ack_;
  }

  // RenderWidgetHostView override.
  gfx::Rect GetViewBounds() const override { return bounds_; }
  float GetTopControlsHeight() const override { return top_controls_height_; }
  float GetBottomControlsHeight() const override {
    return bottom_controls_height_;
  }
  viz::LocalSurfaceId GetLocalSurfaceId() const override {
    return local_surface_id_;
  }

  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                              InputEventAckState ack_result) override {
    acked_event_ = touch.event;
    ++acked_event_count_;
  }
  void WheelEventAck(const WebMouseWheelEvent& event,
                     InputEventAckState ack_result) override {
    if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
      return;
    unhandled_wheel_event_count_++;
    unhandled_wheel_event_ = event;
  }
  void GestureEventAck(const WebGestureEvent& event,
                       InputEventAckState ack_result) override {
    gesture_event_type_ = event.GetType();
    ack_result_ = ack_result;
  }
  gfx::Size GetCompositorViewportPixelSize() const override {
    if (use_fake_compositor_viewport_pixel_size_)
      return mock_compositor_viewport_pixel_size_;
    return TestRenderWidgetHostView::GetCompositorViewportPixelSize();
  }
  void OnDidNotProduceFrame(const viz::BeginFrameAck& ack) override {
    last_did_not_produce_frame_ack_ = ack;
  }

 protected:
  WebMouseWheelEvent unhandled_wheel_event_;
  int unhandled_wheel_event_count_;
  WebTouchEvent acked_event_;
  int acked_event_count_;
  int gesture_event_type_;
  gfx::Rect bounds_;
  bool use_fake_compositor_viewport_pixel_size_;
  gfx::Size mock_compositor_viewport_pixel_size_;
  InputEventAckState ack_result_;
  float top_controls_height_;
  float bottom_controls_height_;
  viz::BeginFrameAck last_did_not_produce_frame_ack_;
  viz::LocalSurfaceId local_surface_id_;
  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;
  ScreenInfo screen_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestView);
};

// MockRenderViewHostDelegateView ------------------------------------------
class MockRenderViewHostDelegateView : public RenderViewHostDelegateView {
 public:
  MockRenderViewHostDelegateView() = default;
  ~MockRenderViewHostDelegateView() override = default;

  int start_dragging_count() const { return start_dragging_count_; }

  // RenderViewHostDelegateView:
  void StartDragging(const DropData& drop_data,
                     blink::WebDragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const DragEventSourceInfo& event_info,
                     RenderWidgetHostImpl* source_rwh) override {
    ++start_dragging_count_;
  }

 private:
  int start_dragging_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MockRenderViewHostDelegateView);
};

// FakeRenderFrameMetadataObserver -----------------------------------------

// Fake out the renderer side of mojom::RenderFrameMetadataObserver, allowing
// for RenderWidgetHostImpl to be created.
//
// All methods are no-opts, the provided mojo request and info are held, but
// never bound.
class FakeRenderFrameMetadataObserver
    : public mojom::RenderFrameMetadataObserver {
 public:
  FakeRenderFrameMetadataObserver(
      mojom::RenderFrameMetadataObserverRequest request,
      mojom::RenderFrameMetadataObserverClientPtrInfo client_info);
  ~FakeRenderFrameMetadataObserver() override {}

  void ReportAllFrameSubmissionsForTesting(bool enabled) override {}

 private:
  mojom::RenderFrameMetadataObserverRequest request_;
  mojom::RenderFrameMetadataObserverClientPtrInfo client_info_;
  DISALLOW_COPY_AND_ASSIGN(FakeRenderFrameMetadataObserver);
};

FakeRenderFrameMetadataObserver::FakeRenderFrameMetadataObserver(
    mojom::RenderFrameMetadataObserverRequest request,
    mojom::RenderFrameMetadataObserverClientPtrInfo client_info)
    : request_(std::move(request)), client_info_(std::move(client_info)) {}

// MockRenderWidgetHostDelegate --------------------------------------------

class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate()
      : prehandle_keyboard_event_(false),
        prehandle_keyboard_event_is_shortcut_(false),
        prehandle_keyboard_event_called_(false),
        prehandle_keyboard_event_type_(WebInputEvent::kUndefined),
        unhandled_keyboard_event_called_(false),
        unhandled_keyboard_event_type_(WebInputEvent::kUndefined),
        handle_wheel_event_(false),
        handle_wheel_event_called_(false),
        unresponsive_timer_fired_(false),
        render_view_host_delegate_view_(new MockRenderViewHostDelegateView()) {}
  ~MockRenderWidgetHostDelegate() override {}

  // Tests that make sure we ignore keyboard event acknowledgments to events we
  // didn't send work by making sure we didn't call UnhandledKeyboardEvent().
  bool unhandled_keyboard_event_called() const {
    return unhandled_keyboard_event_called_;
  }

  WebInputEvent::Type unhandled_keyboard_event_type() const {
    return unhandled_keyboard_event_type_;
  }

  bool prehandle_keyboard_event_called() const {
    return prehandle_keyboard_event_called_;
  }

  WebInputEvent::Type prehandle_keyboard_event_type() const {
    return prehandle_keyboard_event_type_;
  }

  void set_prehandle_keyboard_event(bool handle) {
    prehandle_keyboard_event_ = handle;
  }

  void set_handle_wheel_event(bool handle) {
    handle_wheel_event_ = handle;
  }

  void set_prehandle_keyboard_event_is_shortcut(bool is_shortcut) {
    prehandle_keyboard_event_is_shortcut_ = is_shortcut;
  }

  bool handle_wheel_event_called() const { return handle_wheel_event_called_; }

  bool unresponsive_timer_fired() const { return unresponsive_timer_fired_; }

  MockRenderViewHostDelegateView* mock_delegate_view() {
    return render_view_host_delegate_view_.get();
  }

  RenderViewHostDelegateView* GetDelegateView() override {
    return mock_delegate_view();
  }

 protected:
  KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) override {
    prehandle_keyboard_event_type_ = event.GetType();
    prehandle_keyboard_event_called_ = true;
    if (prehandle_keyboard_event_)
      return KeyboardEventProcessingResult::HANDLED;
    return prehandle_keyboard_event_is_shortcut_
               ? KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT
               : KeyboardEventProcessingResult::NOT_HANDLED;
  }

  void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) override {
    unhandled_keyboard_event_type_ = event.GetType();
    unhandled_keyboard_event_called_ = true;
  }

  bool HandleWheelEvent(const blink::WebMouseWheelEvent& event) override {
    handle_wheel_event_called_ = true;
    return handle_wheel_event_;
  }

  void RendererUnresponsive(RenderWidgetHostImpl* render_widget_host) override {
    unresponsive_timer_fired_ = true;
  }

  void ExecuteEditCommand(
      const std::string& command,
      const base::Optional<base::string16>& value) override {}

  void Cut() override {}
  void Copy() override {}
  void Paste() override {}
  void SelectAll() override {}

 private:
  bool prehandle_keyboard_event_;
  bool prehandle_keyboard_event_is_shortcut_;
  bool prehandle_keyboard_event_called_;
  WebInputEvent::Type prehandle_keyboard_event_type_;

  bool unhandled_keyboard_event_called_;
  WebInputEvent::Type unhandled_keyboard_event_type_;

  bool handle_wheel_event_;
  bool handle_wheel_event_called_;

  bool unresponsive_timer_fired_;

  std::unique_ptr<MockRenderViewHostDelegateView>
      render_view_host_delegate_view_;
};

enum WheelScrollingMode {
  kWheelScrollingModeNone,
  kWheelScrollLatching,
  kAsyncWheelEvents,
};

enum class UseMojoInputMessages { kEnabled, kDisabled };

// RenderWidgetHostTest --------------------------------------------------------

class RenderWidgetHostTest : public testing::Test {
 public:
  RenderWidgetHostTest(
      UseMojoInputMessages input_messages_mode = UseMojoInputMessages::kEnabled,
      WheelScrollingMode wheel_scrolling_mode = kWheelScrollLatching)
      : process_(nullptr),
        handle_key_press_event_(false),
        handle_mouse_event_(false),
        simulated_event_time_delta_seconds_(0),
        wheel_scroll_latching_enabled_(wheel_scrolling_mode !=
                                       kWheelScrollingModeNone) {
    std::vector<base::StringPiece> features;
    std::vector<base::StringPiece> disabled_features;
    if (input_messages_mode == UseMojoInputMessages::kEnabled) {
      features.push_back(features::kMojoInputMessages.name);
    } else {
      disabled_features.push_back(features::kMojoInputMessages.name);
    }

    switch (wheel_scrolling_mode) {
      case kWheelScrollingModeNone:
        disabled_features.push_back(
            features::kTouchpadAndWheelScrollLatching.name);
        disabled_features.push_back(features::kAsyncWheelEvents.name);
        break;
      case kWheelScrollLatching:
        features.push_back(features::kTouchpadAndWheelScrollLatching.name);
        disabled_features.push_back(features::kAsyncWheelEvents.name);
        break;
      case kAsyncWheelEvents:
        features.push_back(features::kTouchpadAndWheelScrollLatching.name);
        features.push_back(features::kAsyncWheelEvents.name);
        break;
    }

    features.push_back(features::kVsyncAlignedInputEvents.name);

    feature_list_.InitFromCommandLine(base::JoinString(features, ","),
                                      base::JoinString(disabled_features, ","));

    last_simulated_event_time_seconds_ =
        ui::EventTimeStampToSeconds(ui::EventTimeForNow());
  }
  ~RenderWidgetHostTest() override {}

  bool KeyPressEventCallback(const NativeWebKeyboardEvent& /* event */) {
    return handle_key_press_event_;
  }
  bool MouseEventCallback(const blink::WebMouseEvent& /* event */) {
    return handle_mouse_event_;
  }

 protected:
  // testing::Test
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kValidateInputEventStream);
    browser_context_.reset(new TestBrowserContext());
    delegate_.reset(new MockRenderWidgetHostDelegate());
    process_ = new RenderWidgetHostProcess(browser_context_.get());
    sink_ = &process_->sink();
#if defined(USE_AURA) || defined(OS_MACOSX)
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
#endif
#if defined(OS_ANDROID)
    ui::SetScreenAndroid();  // calls display::Screen::SetScreenInstance().
#endif
#if defined(USE_AURA)
    screen_.reset(aura::TestScreen::Create(gfx::Size()));
    display::Screen::SetScreenInstance(screen_.get());
#endif
    host_.reset(MockRenderWidgetHost::Create(delegate_.get(), process_,
                                             process_->GetNextRoutingID()));
    view_.reset(new TestView(host_.get()));
    ConfigureView(view_.get());
    host_->SetView(view_.get());
    SetInitialRenderSizeParams();
    host_->Init();
    host_->DisableGestureDebounce();

    viz::mojom::CompositorFrameSinkPtr sink;
    viz::mojom::CompositorFrameSinkRequest sink_request =
        mojo::MakeRequest(&sink);
    viz::mojom::CompositorFrameSinkClientRequest client_request =
        mojo::MakeRequest(&renderer_compositor_frame_sink_ptr_);
    renderer_compositor_frame_sink_ =
        std::make_unique<FakeRendererCompositorFrameSink>(
            std::move(sink), std::move(client_request));

    mojom::RenderFrameMetadataObserverPtr
        renderer_render_frame_metadata_observer_ptr;
    mojom::RenderFrameMetadataObserverRequest
        render_frame_metadata_observer_request =
            mojo::MakeRequest(&renderer_render_frame_metadata_observer_ptr);
    mojom::RenderFrameMetadataObserverClientPtrInfo
        render_frame_metadata_observer_client_info;
    mojom::RenderFrameMetadataObserverClientRequest
        render_frame_metadata_observer_client_request =
            mojo::MakeRequest(&render_frame_metadata_observer_client_info);
    renderer_render_frame_metadata_observer_ =
        std::make_unique<FakeRenderFrameMetadataObserver>(
            std::move(render_frame_metadata_observer_request),
            std::move(render_frame_metadata_observer_client_info));

    host_->RequestCompositorFrameSink(
        std::move(sink_request), std::move(renderer_compositor_frame_sink_ptr_),
        std::move(render_frame_metadata_observer_client_request),
        std::move(renderer_render_frame_metadata_observer_ptr));
  }

  void TearDown() override {
    view_.reset();
    host_.reset();
    delegate_.reset();
    process_ = nullptr;
    browser_context_.reset();

#if defined(USE_AURA)
    display::Screen::SetScreenInstance(nullptr);
    screen_.reset();
#endif
#if defined(USE_AURA) || defined(OS_MACOSX)
    ImageTransportFactory::Terminate();
#endif
#if defined(OS_ANDROID)
    display::Screen::SetScreenInstance(nullptr);
#endif

    // Process all pending tasks to avoid leaks.
    base::RunLoop().RunUntilIdle();
  }

  void SetInitialRenderSizeParams() {
    ResizeParams render_size_params;
    host_->GetResizeParams(&render_size_params);
    host_->SetInitialRenderSizeParams(render_size_params);
  }

  virtual void ConfigureView(TestView* view) {
  }

  int64_t GetLatencyComponentId() { return host_->GetLatencyComponentId(); }

  void SendInputEventACK(WebInputEvent::Type type,
                         InputEventAckState ack_result) {
    DCHECK(!WebInputEvent::IsTouchEventType(type));
    InputEventAck ack(InputEventAckSource::COMPOSITOR_THREAD, type, ack_result);
    host_->OnMessageReceived(InputHostMsg_HandleInputEvent_ACK(0, ack));
  }

  void SendScrollBeginAckIfneeded(InputEventAckState ack_result) {
    if (wheel_scroll_latching_enabled_) {
      // GSB events are blocking, send the ack.
      SendInputEventACK(WebInputEvent::kGestureScrollBegin, ack_result);
    }
  }

  double GetNextSimulatedEventTimeSeconds() {
    last_simulated_event_time_seconds_ += simulated_event_time_delta_seconds_;
    return last_simulated_event_time_seconds_;
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type) {
    SimulateKeyboardEvent(type, 0);
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type, int modifiers) {
    NativeWebKeyboardEvent native_event(type, modifiers,
                                        GetNextSimulatedEventTimeSeconds());
    host_->ForwardKeyboardEvent(native_event);
  }

  void SimulateKeyboardEventWithCommands(WebInputEvent::Type type) {
    NativeWebKeyboardEvent native_event(type, 0,
                                        GetNextSimulatedEventTimeSeconds());
    EditCommands commands;
    commands.emplace_back("name", "value");
    host_->ForwardKeyboardEventWithCommands(native_event, ui::LatencyInfo(),
                                            &commands, nullptr);
  }

  void SimulateMouseEvent(WebInputEvent::Type type) {
    host_->ForwardMouseEvent(SyntheticWebMouseEventBuilder::Build(type));
  }

  void SimulateMouseEventWithLatencyInfo(WebInputEvent::Type type,
                                         const ui::LatencyInfo& ui_latency) {
    host_->ForwardMouseEventWithLatencyInfo(
        SyntheticWebMouseEventBuilder::Build(type),
        ui_latency);
  }

  void SimulateWheelEvent(float dX, float dY, int modifiers, bool precise) {
    host_->ForwardWheelEvent(SyntheticWebMouseWheelEventBuilder::Build(
        0, 0, dX, dY, modifiers, precise));
  }

  void SimulateWheelEventPossiblyIncludingPhase(
      float dX,
      float dY,
      int modifiers,
      bool precise,
      WebMouseWheelEvent::Phase phase) {
    WebMouseWheelEvent wheel_event = SyntheticWebMouseWheelEventBuilder::Build(
        0, 0, dX, dY, modifiers, precise);
    if (wheel_scroll_latching_enabled_)
      wheel_event.phase = phase;
    host_->ForwardWheelEvent(wheel_event);
  }

  void SimulateWheelEventWithLatencyInfo(float dX,
                                         float dY,
                                         int modifiers,
                                         bool precise,
                                         const ui::LatencyInfo& ui_latency) {
    host_->ForwardWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(0, 0, dX, dY, modifiers,
                                                  precise),
        ui_latency);
  }

  void SimulateWheelEventWithLatencyInfoAndPossiblyPhase(
      float dX,
      float dY,
      int modifiers,
      bool precise,
      const ui::LatencyInfo& ui_latency,
      WebMouseWheelEvent::Phase phase) {
    WebMouseWheelEvent wheel_event = SyntheticWebMouseWheelEventBuilder::Build(
        0, 0, dX, dY, modifiers, precise);
    if (wheel_scroll_latching_enabled_)
      wheel_event.phase = phase;
    host_->ForwardWheelEventWithLatencyInfo(wheel_event, ui_latency);
  }

  void SimulateMouseMove(int x, int y, int modifiers) {
    SimulateMouseEvent(WebInputEvent::kMouseMove, x, y, modifiers, false);
  }

  void SimulateMouseEvent(
      WebInputEvent::Type type, int x, int y, int modifiers, bool pressed) {
    WebMouseEvent event =
        SyntheticWebMouseEventBuilder::Build(type, x, y, modifiers);
    if (pressed)
      event.button = WebMouseEvent::Button::kLeft;
    event.SetTimeStampSeconds(GetNextSimulatedEventTimeSeconds());
    host_->ForwardMouseEvent(event);
  }

  // Inject simple synthetic WebGestureEvent instances.
  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureDevice sourceDevice) {
    host_->ForwardGestureEvent(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
  }

  void SimulateGestureEventWithLatencyInfo(WebInputEvent::Type type,
                                           WebGestureDevice sourceDevice,
                                           const ui::LatencyInfo& ui_latency) {
    host_->ForwardGestureEventWithLatencyInfo(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice),
        ui_latency);
  }

  // Set the timestamp for the touch-event.
  void SetTouchTimestamp(base::TimeTicks timestamp) {
    touch_event_.SetTimestamp(timestamp);
  }

  // Sends a touch event (irrespective of whether the page has a touch-event
  // handler or not).
  uint32_t SendTouchEvent() {
    uint32_t touch_event_id = touch_event_.unique_touch_event_id;
    host_->ForwardTouchEventWithLatencyInfo(touch_event_, ui::LatencyInfo());

    touch_event_.ResetPoints();
    return touch_event_id;
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

  const WebInputEvent* GetInputEventFromMessage(const IPC::Message& message) {
    base::PickleIterator iter(message);
    const char* data;
    int data_length;
    if (!iter.ReadData(&data, &data_length))
      return nullptr;
    return reinterpret_cast<const WebInputEvent*>(data);
  }

  void UnhandledWheelEvent();
  void UnhandledWheelEventMojoInputDisabled();
  void HandleWheelEvent();
  void HandleWheelEventMojoInputDisabled();
  void InputEventRWHLatencyComponent();
  void InputEventRWHLatencyComponentMojoInputDisabled();

  std::unique_ptr<TestBrowserContext> browser_context_;
  RenderWidgetHostProcess* process_;  // Deleted automatically by the widget.
  std::unique_ptr<MockRenderWidgetHostDelegate> delegate_;
  std::unique_ptr<MockRenderWidgetHost> host_;
  std::unique_ptr<TestView> view_;
  std::unique_ptr<display::Screen> screen_;
  bool handle_key_press_event_;
  bool handle_mouse_event_;
  double last_simulated_event_time_seconds_;
  double simulated_event_time_delta_seconds_;
  IPC::TestSink* sink_;
  std::unique_ptr<FakeRendererCompositorFrameSink>
      renderer_compositor_frame_sink_;
  std::unique_ptr<FakeRenderFrameMetadataObserver>
      renderer_render_frame_metadata_observer_;
  bool wheel_scroll_latching_enabled_;

 private:
  SyntheticWebTouchEvent touch_event_;

  TestBrowserThreadBundle thread_bundle_;
  base::test::ScopedFeatureList feature_list_;
  viz::mojom::CompositorFrameSinkClientPtr renderer_compositor_frame_sink_ptr_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostTest);
};

class RenderWidgetHostMojoInputDisabledTest : public RenderWidgetHostTest {
 public:
  RenderWidgetHostMojoInputDisabledTest()
      : RenderWidgetHostTest(UseMojoInputMessages::kDisabled) {}
};

class RenderWidgetHostWheelScrollLatchingDisabledTest
    : public RenderWidgetHostTest {
 public:
  RenderWidgetHostWheelScrollLatchingDisabledTest()
      : RenderWidgetHostTest(UseMojoInputMessages::kEnabled,
                             kWheelScrollingModeNone) {}
};

class RenderWidgetHostAsyncWheelEventsEnabledTest
    : public RenderWidgetHostTest {
 public:
  RenderWidgetHostAsyncWheelEventsEnabledTest()
      : RenderWidgetHostTest(UseMojoInputMessages::kEnabled,
                             kAsyncWheelEvents) {}
};

class RenderWidgetHostWheelScrollLatchingMojoInputDisabledTest
    : public RenderWidgetHostTest {
 public:
  RenderWidgetHostWheelScrollLatchingMojoInputDisabledTest()
      : RenderWidgetHostTest(UseMojoInputMessages::kDisabled,
                             kWheelScrollingModeNone) {}
};

class RenderWidgetHostAsyncWheelEventsEnabledMojoInputDisabledTest
    : public RenderWidgetHostTest {
 public:
  RenderWidgetHostAsyncWheelEventsEnabledMojoInputDisabledTest()
      : RenderWidgetHostTest(UseMojoInputMessages::kDisabled,
                             kAsyncWheelEvents) {}
};

// RenderWidgetHostWithSourceTest ----------------------------------------------

// This is for tests that are to be run for all source devices.
class RenderWidgetHostWithSourceTest
    : public RenderWidgetHostTest,
      public testing::WithParamInterface<WebGestureDevice> {};

}  // namespace

// -----------------------------------------------------------------------------

TEST_F(RenderWidgetHostTest, Resize) {
  // The initial bounds is the empty rect, so setting it to the same thing
  // shouldn't send the resize message.
  view_->SetBounds(gfx::Rect());
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // No resize ack if the physical backing gets set, but the view bounds are
  // zero.
  view_->SetMockCompositorViewportPixelSize(gfx::Size(200, 200));
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);

  // Setting the view bounds to nonzero should send out the notification.
  // but should not expect ack for empty physical backing size.
  gfx::Rect original_size(0, 0, 100, 100);
  process_->sink().ClearMessages();
  view_->SetBounds(original_size);
  view_->SetMockCompositorViewportPixelSize(gfx::Size());
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->old_resize_params_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Setting the bounds and physical backing size to nonzero should send out
  // the notification and expect an ack.
  process_->sink().ClearMessages();
  view_->ClearMockCompositorViewportPixelSize();
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->old_resize_params_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
  ViewHostMsg_ResizeOrRepaint_ACK_Params params;
  params.flags = ViewHostMsg_ResizeOrRepaint_ACK_Flags::IS_RESIZE_ACK;
  params.view_size = original_size.size();
  host_->OnResizeOrRepaintACK(params);
  EXPECT_FALSE(host_->resize_ack_pending_);

  // Send out a update that's not a resize ack after setting resize ack pending
  // flag. This should not clean the resize ack pending flag.
  process_->sink().ClearMessages();
  gfx::Rect second_size(0, 0, 110, 110);
  EXPECT_FALSE(host_->resize_ack_pending_);
  view_->SetBounds(second_size);
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  params.flags = 0;
  params.view_size = gfx::Size(100, 100);
  host_->OnResizeOrRepaintACK(params);
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(second_size.size(), host_->old_resize_params_->new_size);

  // Sending out a new notification should NOT send out a new IPC message since
  // a resize ACK is pending.
  gfx::Rect third_size(0, 0, 120, 120);
  process_->sink().ClearMessages();
  view_->SetBounds(third_size);
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(second_size.size(), host_->old_resize_params_->new_size);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send a update that's a resize ack, but for the original_size we sent. Since
  // this isn't the second_size, the message handler should immediately send
  // a new resize message for the new size to the renderer.
  process_->sink().ClearMessages();
  params.flags = ViewHostMsg_ResizeOrRepaint_ACK_Flags::IS_RESIZE_ACK;
  params.view_size = original_size.size();
  host_->OnResizeOrRepaintACK(params);
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(third_size.size(), host_->old_resize_params_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send the resize ack for the latest size.
  process_->sink().ClearMessages();
  params.flags = ViewHostMsg_ResizeOrRepaint_ACK_Flags::IS_RESIZE_ACK;
  params.view_size = third_size.size();
  host_->OnResizeOrRepaintACK(params);
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(third_size.size(), host_->old_resize_params_->new_size);
  EXPECT_FALSE(process_->sink().GetFirstMessageMatching(ViewMsg_Resize::ID));

  // Now clearing the bounds should send out a notification but we shouldn't
  // expect a resize ack (since the renderer won't ack empty sizes). The message
  // should contain the new size (0x0) and not the previous one that we skipped
  process_->sink().ClearMessages();
  view_->SetBounds(gfx::Rect());
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->old_resize_params_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send a rect that has no area but has either width or height set.
  process_->sink().ClearMessages();
  view_->SetBounds(gfx::Rect(0, 0, 0, 30));
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 30), host_->old_resize_params_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Set the same size again. It should not be sent again.
  process_->sink().ClearMessages();
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 30), host_->old_resize_params_->new_size);
  EXPECT_FALSE(process_->sink().GetFirstMessageMatching(ViewMsg_Resize::ID));

  // A different size should be sent again, however.
  view_->SetBounds(gfx::Rect(0, 0, 0, 31));
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 31), host_->old_resize_params_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
}

// Test that a resize event is sent if WasResized() is called after a
// ScreenInfo change.
TEST_F(RenderWidgetHostTest, ResizeScreenInfo) {
  ScreenInfo screen_info;
  screen_info.device_scale_factor = 1.f;
  screen_info.rect = blink::WebRect(0, 0, 800, 600);
  screen_info.available_rect = blink::WebRect(0, 0, 800, 600);
  screen_info.orientation_angle = 0;
  screen_info.orientation_type = SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;

  view_->SetScreenInfo(screen_info);
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
  process_->sink().ClearMessages();

  screen_info.orientation_angle = 180;
  screen_info.orientation_type = SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY;

  view_->SetScreenInfo(screen_info);
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
  process_->sink().ClearMessages();

  screen_info.device_scale_factor = 2.f;

  view_->SetScreenInfo(screen_info);
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
  process_->sink().ClearMessages();

  // No screen change.
  view_->SetScreenInfo(screen_info);
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
}

// Test for crbug.com/25097.  If a renderer crashes between a resize and the
// corresponding update message, we must be sure to clear the resize ack logic.
TEST_F(RenderWidgetHostTest, ResizeThenCrash) {
  // Clear the first Resize message that carried screen info.
  process_->sink().ClearMessages();

  // Setting the bounds to a "real" rect should send out the notification.
  gfx::Rect original_size(0, 0, 100, 100);
  view_->SetBounds(original_size);
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->old_resize_params_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Simulate a renderer crash before the update message.  Ensure all the
  // resize ack logic is cleared.  Must clear the view first so it doesn't get
  // deleted.
  host_->SetView(nullptr);
  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->old_resize_params_->new_size);

  // Reset the view so we can exit the test cleanly.
  host_->SetView(view_.get());
}

// Unable to include render_widget_host_view_mac.h and compile.
#if !defined(OS_MACOSX)
// Tests setting background transparency.
TEST_F(RenderWidgetHostTest, Background) {
  std::unique_ptr<RenderWidgetHostViewBase> view;
#if defined(USE_AURA)
  view.reset(new RenderWidgetHostViewAura(
      host_.get(), false, false /* is_mus_browser_plugin_guest */));
  // TODO(derat): Call this on all platforms: http://crbug.com/102450.
  view->InitAsChild(nullptr);
#elif defined(OS_ANDROID)
  view.reset(new RenderWidgetHostViewAndroid(host_.get(), NULL));
#endif
  host_->SetView(view.get());

  EXPECT_NE(static_cast<unsigned>(SK_ColorTRANSPARENT),
            view->background_color());
  view->SetBackgroundColor(SK_ColorTRANSPARENT);
  EXPECT_EQ(static_cast<unsigned>(SK_ColorTRANSPARENT),
            view->background_color());

  const IPC::Message* set_background =
      process_->sink().GetUniqueMessageMatching(
          ViewMsg_SetBackgroundOpaque::ID);
  ASSERT_TRUE(set_background);
  std::tuple<bool> sent_background;
  ViewMsg_SetBackgroundOpaque::Read(set_background, &sent_background);
  EXPECT_FALSE(std::get<0>(sent_background));

  host_->SetView(nullptr);
  static_cast<RenderWidgetHostViewBase*>(view.release())->Destroy();
}
#endif

// Test that we don't paint when we're hidden, but we still send the ACK. Most
// of the rest of the painting is tested in the GetBackingStore* ones.
TEST_F(RenderWidgetHostTest, HiddenPaint) {
  // Hide the widget, it should have sent out a message to the renderer.
  EXPECT_FALSE(host_->is_hidden_);
  host_->WasHidden();
  EXPECT_TRUE(host_->is_hidden_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_WasHidden::ID));

  // Send it an update as from the renderer.
  process_->sink().ClearMessages();
  ViewHostMsg_ResizeOrRepaint_ACK_Params params;
  params.view_size = gfx::Size(100, 100);
  host_->OnResizeOrRepaintACK(params);

  // Now unhide.
  process_->sink().ClearMessages();
  host_->WasShown(ui::LatencyInfo());
  EXPECT_FALSE(host_->is_hidden_);

  // It should have sent out a restored message with a request to paint.
  const IPC::Message* restored = process_->sink().GetUniqueMessageMatching(
      ViewMsg_WasShown::ID);
  ASSERT_TRUE(restored);
  std::tuple<bool, ui::LatencyInfo> needs_repaint;
  ViewMsg_WasShown::Read(restored, &needs_repaint);
  EXPECT_TRUE(std::get<0>(needs_repaint));
}

TEST_F(RenderWidgetHostMojoInputDisabledTest,
       IgnoreKeyEventsHandledByRenderer) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::kRawKeyDown, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(delegate_->unhandled_keyboard_event_called());
}

TEST_F(RenderWidgetHostTest, IgnoreKeyEventsHandledByRenderer) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure we sent the input event to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(delegate_->unhandled_keyboard_event_called());
}

TEST_F(RenderWidgetHostMojoInputDisabledTest, SendEditCommandsBeforeKeyEvent) {
  // Clear any messages unrelated to this test.
  process_->sink().ClearMessages();
  EXPECT_EQ(0U, process_->sink().message_count());

  // Simulate a keyboard event.
  SimulateKeyboardEventWithCommands(WebInputEvent::kRawKeyDown);

  // Make sure we sent commands and key event to the renderer.
  EXPECT_EQ(2U, process_->sink().message_count());
  EXPECT_EQ(static_cast<uint32_t>(InputMsg_SetEditCommandsForNextKeyEvent::ID),
            process_->sink().GetMessageAt(0)->type());
  EXPECT_EQ(static_cast<uint32_t>(InputMsg_HandleInputEvent::ID),
            process_->sink().GetMessageAt(1)->type());
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::kRawKeyDown, INPUT_EVENT_ACK_STATE_CONSUMED);
}

TEST_F(RenderWidgetHostTest, SendEditCommandsBeforeKeyEvent) {
  // Simulate a keyboard event.
  SimulateKeyboardEventWithCommands(WebInputEvent::kRawKeyDown);

  // Make sure we sent commands and key event to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(2u, dispatched_events.size());

  ASSERT_TRUE(dispatched_events[0]->ToEditCommand());
  ASSERT_TRUE(dispatched_events[1]->ToEvent());
  // Send the simulated response from the renderer back.
  dispatched_events[1]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
}

TEST_F(RenderWidgetHostMojoInputDisabledTest, PreHandleRawKeyDownEvent) {
  // Simulate the situation that the browser handled the key down event during
  // pre-handle phrase.
  delegate_->set_prehandle_keyboard_event(true);
  process_->sink().ClearMessages();

  // Simulate a keyboard event.
  SimulateKeyboardEventWithCommands(WebInputEvent::kRawKeyDown);

  EXPECT_TRUE(delegate_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            delegate_->prehandle_keyboard_event_type());

  // Make sure the commands and key event are not sent to the renderer.
  EXPECT_EQ(0U, process_->sink().message_count());

  // The browser won't pre-handle a Char event.
  delegate_->set_prehandle_keyboard_event(false);

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::kChar);

  // Make sure the Char event is suppressed.
  EXPECT_EQ(0U, process_->sink().message_count());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::kKeyUp);

  // Make sure the KeyUp event is suppressed.
  EXPECT_EQ(0U, process_->sink().message_count());

  // Simulate a new RawKeyDown event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(static_cast<uint32_t>(InputMsg_HandleInputEvent::ID),
            process_->sink().GetMessageAt(0)->type());
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::kRawKeyDown,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_TRUE(delegate_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            delegate_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, PreHandleRawKeyDownEvent) {
  // Simulate the situation that the browser handled the key down event during
  // pre-handle phrase.
  delegate_->set_prehandle_keyboard_event(true);

  // Simulate a keyboard event.
  SimulateKeyboardEventWithCommands(WebInputEvent::kRawKeyDown);

  EXPECT_TRUE(delegate_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            delegate_->prehandle_keyboard_event_type());

  // Make sure the commands and key event are not sent to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // The browser won't pre-handle a Char event.
  delegate_->set_prehandle_keyboard_event(false);

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::kChar);

  // Make sure the Char event is suppressed.
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::kKeyUp);

  // Make sure the KeyUp event is suppressed.
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // Simulate a new RawKeyDown event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_TRUE(delegate_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            delegate_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostMojoInputDisabledTest, RawKeyDownShortcutEvent) {
  // Simulate the situation that the browser marks the key down as a keyboard
  // shortcut, but doesn't consume it in the pre-handle phase.
  delegate_->set_prehandle_keyboard_event_is_shortcut(true);
  process_->sink().ClearMessages();

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  EXPECT_TRUE(delegate_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            delegate_->prehandle_keyboard_event_type());

  // Make sure the RawKeyDown event is sent to the renderer.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ("RawKeyDown", GetInputMessageTypes(process_));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::kRawKeyDown,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            delegate_->unhandled_keyboard_event_type());

  // The browser won't pre-handle a Char event.
  delegate_->set_prehandle_keyboard_event_is_shortcut(false);

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::kChar);

  // The Char event is not suppressed; the renderer will ignore it
  // if the preceding RawKeyDown shortcut goes unhandled.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ("Char", GetInputMessageTypes(process_));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::kChar, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::kChar, delegate_->unhandled_keyboard_event_type());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::kKeyUp);

  // Make sure only KeyUp was sent to the renderer.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ("KeyUp", GetInputMessageTypes(process_));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::kKeyUp, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::kKeyUp, delegate_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, RawKeyDownShortcutEvent) {
  // Simulate the situation that the browser marks the key down as a keyboard
  // shortcut, but doesn't consume it in the pre-handle phase.
  delegate_->set_prehandle_keyboard_event_is_shortcut(true);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  EXPECT_TRUE(delegate_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            delegate_->prehandle_keyboard_event_type());

  // Make sure the RawKeyDown event is sent to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            delegate_->unhandled_keyboard_event_type());

  // The browser won't pre-handle a Char event.
  delegate_->set_prehandle_keyboard_event_is_shortcut(false);

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::kChar);

  // The Char event is not suppressed; the renderer will ignore it
  // if the preceding RawKeyDown shortcut goes unhandled.
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kChar,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::kChar, delegate_->unhandled_keyboard_event_type());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::kKeyUp);

  // Make sure only KeyUp was sent to the renderer.
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kKeyUp,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::kKeyUp, delegate_->unhandled_keyboard_event_type());
}

void RenderWidgetHostTest::UnhandledWheelEventMojoInputDisabled() {
  SimulateWheelEventPossiblyIncludingPhase(-5, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseBegan);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(delegate_->handle_wheel_event_called());
  EXPECT_EQ(1, view_->unhandled_wheel_event_count());
  EXPECT_EQ(-5, view_->unhandled_wheel_event().delta_x);
}
TEST_F(RenderWidgetHostMojoInputDisabledTest, UnhandledWheelEvent) {
  UnhandledWheelEventMojoInputDisabled();
}
TEST_F(RenderWidgetHostWheelScrollLatchingMojoInputDisabledTest,
       UnhandledWheelEvent) {
  UnhandledWheelEventMojoInputDisabled();
}
TEST_F(RenderWidgetHostAsyncWheelEventsEnabledMojoInputDisabledTest,
       UnhandledWheelEvent) {
  UnhandledWheelEventMojoInputDisabled();
}

void RenderWidgetHostTest::UnhandledWheelEvent() {
  SimulateWheelEventPossiblyIncludingPhase(-5, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseBegan);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kMouseWheel,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_TRUE(delegate_->handle_wheel_event_called());
  EXPECT_EQ(1, view_->unhandled_wheel_event_count());
  EXPECT_EQ(-5, view_->unhandled_wheel_event().delta_x);
}
TEST_F(RenderWidgetHostTest, UnhandledWheelEvent) {
  UnhandledWheelEvent();
}
TEST_F(RenderWidgetHostWheelScrollLatchingDisabledTest, UnhandledWheelEvent) {
  UnhandledWheelEvent();
}
TEST_F(RenderWidgetHostAsyncWheelEventsEnabledTest, UnhandledWheelEvent) {
  UnhandledWheelEvent();
}

void RenderWidgetHostTest::HandleWheelEventMojoInputDisabled() {
  // Indicate that we're going to handle this wheel event
  delegate_->set_handle_wheel_event(true);

  SimulateWheelEventPossiblyIncludingPhase(-5, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseBegan);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // ensure the wheel event handler was invoked
  EXPECT_TRUE(delegate_->handle_wheel_event_called());

  // and that it suppressed the unhandled wheel event handler.
  EXPECT_EQ(0, view_->unhandled_wheel_event_count());
}
TEST_F(RenderWidgetHostMojoInputDisabledTest, HandleWheelEvent) {
  HandleWheelEventMojoInputDisabled();
}
TEST_F(RenderWidgetHostWheelScrollLatchingMojoInputDisabledTest,
       HandleWheelEvent) {
  HandleWheelEventMojoInputDisabled();
}
TEST_F(RenderWidgetHostAsyncWheelEventsEnabledMojoInputDisabledTest,
       HandleWheelEvent) {
  HandleWheelEventMojoInputDisabled();
}

void RenderWidgetHostTest::HandleWheelEvent() {
  // Indicate that we're going to handle this wheel event
  delegate_->set_handle_wheel_event(true);

  SimulateWheelEventPossiblyIncludingPhase(-5, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseBegan);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kMouseWheel,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // ensure the wheel event handler was invoked
  EXPECT_TRUE(delegate_->handle_wheel_event_called());

  // and that it suppressed the unhandled wheel event handler.
  EXPECT_EQ(0, view_->unhandled_wheel_event_count());
}
TEST_F(RenderWidgetHostTest, HandleWheelEvent) {
  HandleWheelEvent();
}
TEST_F(RenderWidgetHostWheelScrollLatchingDisabledTest, HandleWheelEvent) {
  HandleWheelEvent();
}
TEST_F(RenderWidgetHostAsyncWheelEventsEnabledTest, HandleWheelEvent) {
  HandleWheelEvent();
}

TEST_F(RenderWidgetHostMojoInputDisabledTest, UnhandledGestureEvent) {
  SimulateGestureEvent(WebInputEvent::kGestureTwoFingerTap,
                       blink::kWebGestureDeviceTouchscreen);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::kGestureTwoFingerTap,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGestureTwoFingerTap, view_->gesture_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, view_->ack_result());
}

TEST_F(RenderWidgetHostTest, UnhandledGestureEvent) {
  SimulateGestureEvent(WebInputEvent::kGestureTwoFingerTap,
                       blink::kWebGestureDeviceTouchscreen);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGestureTwoFingerTap,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(WebInputEvent::kGestureTwoFingerTap, view_->gesture_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, view_->ack_result());
}

// Test that the hang monitor timer expires properly if a new timer is started
// while one is in progress (see crbug.com/11007).
TEST_F(RenderWidgetHostTest, DontPostponeHangMonitorTimeout) {
  // Start with a short timeout.
  host_->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));

  // Immediately try to add a long 30 second timeout.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  host_->StartHangMonitorTimeout(TimeDelta::FromSeconds(30));

  // Wait long enough for first timeout and see if it fired.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMilliseconds(10));
  base::RunLoop().Run();
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor timer expires properly if it is started, stopped,
// and then started again.
TEST_F(RenderWidgetHostTest, StopAndStartHangMonitorTimeout) {
  // Start with a short timeout, then stop it.
  host_->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));
  host_->StopHangMonitorTimeout();

  // Start it again to ensure it still works.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  host_->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));

  // Wait long enough for first timeout and see if it fired.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMilliseconds(40));
  base::RunLoop().Run();
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor timer expires properly if it is started, then
// updated to a shorter duration.
TEST_F(RenderWidgetHostTest, ShorterDelayHangMonitorTimeout) {
  // Start with a timeout.
  host_->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(100));

  // Start it again with shorter delay.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  host_->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(20));

  // Wait long enough for the second timeout and see if it fired.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMilliseconds(25));
  base::RunLoop().Run();
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor timer is effectively disabled when the widget is
// hidden.
TEST_F(RenderWidgetHostTest, HangMonitorTimeoutDisabledForInputWhenHidden) {
  host_->set_hung_renderer_delay(base::TimeDelta::FromMicroseconds(1));
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 10, 0, false);

  // Hiding the widget should deactivate the timeout.
  host_->WasHidden();

  // The timeout should not fire.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(2));
  base::RunLoop().Run();
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());

  // The timeout should never reactivate while hidden.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 10, 0, false);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(2));
  base::RunLoop().Run();
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());

  // Showing the widget should restore the timeout, as the events have
  // not yet been ack'ed.
  host_->WasShown(ui::LatencyInfo());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(2));
  base::RunLoop().Run();
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor catches two input events but only one ack.
// This can happen if the second input event causes the renderer to hang.
// This test will catch a regression of crbug.com/111185.
TEST_F(RenderWidgetHostTest, MultipleInputEvents) {
  // Configure the host to wait 10ms before considering
  // the renderer hung.
  host_->set_hung_renderer_delay(base::TimeDelta::FromMicroseconds(10));

  // Send two events but only one ack.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);
  SendInputEventACK(WebInputEvent::kRawKeyDown, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Wait long enough for first timeout and see if it fired.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(20));
  base::RunLoop().Run();
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the rendering timeout for newly loaded content fires when enough
// time passes without receiving a new compositor frame. This test assumes
// Surface Synchronization is off.
TEST_F(RenderWidgetHostTest, NewContentRenderingTimeoutWithoutSurfaceSync) {
  // If Surface Synchronization is on, we have a separate code path for
  // cancelling new content rendering timeout that is tested separately.
  if (features::IsSurfaceSynchronizationEnabled())
    return;

  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());

  // Mocking |renderer_compositor_frame_sink_| to prevent crashes in
  // renderer_compositor_frame_sink_->DidReceiveCompositorFrameAck(resources).
  std::unique_ptr<viz::MockCompositorFrameSinkClient>
      mock_compositor_frame_sink_client =
          std::make_unique<viz::MockCompositorFrameSinkClient>();
  host_->SetMockRendererCompositorFrameSink(
      mock_compositor_frame_sink_client.get());

  host_->set_new_content_rendering_delay_for_testing(
      base::TimeDelta::FromMicroseconds(10));

  // Start the timer and immediately send a CompositorFrame with the
  // content_source_id of the new page. The timeout shouldn't fire.
  host_->DidNavigate(5);
  auto frame = viz::CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetContentSourceId(5)
                   .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr, 0);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(20));
  base::RunLoop().Run();

  EXPECT_FALSE(host_->new_content_rendering_timeout_fired());
  host_->reset_new_content_rendering_timeout_fired();

  // Start the timer but receive frames only from the old page. The timer
  // should fire.
  host_->DidNavigate(10);
  frame = viz::CompositorFrameBuilder()
              .AddDefaultRenderPass()
              .SetContentSourceId(9)
              .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr, 0);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(20));
  base::RunLoop().Run();

  EXPECT_TRUE(host_->new_content_rendering_timeout_fired());
  host_->reset_new_content_rendering_timeout_fired();

  // Send a CompositorFrame with content_source_id of the new page before we
  // attempt to start the timer. The timer shouldn't fire.
  frame = viz::CompositorFrameBuilder()
              .AddDefaultRenderPass()
              .SetContentSourceId(7)
              .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr, 0);
  host_->DidNavigate(7);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(20));
  base::RunLoop().Run();

  EXPECT_FALSE(host_->new_content_rendering_timeout_fired());
  host_->reset_new_content_rendering_timeout_fired();

  // Don't send any frames after the timer starts. The timer should fire.
  host_->DidNavigate(20);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(20));
  base::RunLoop().Run();
  EXPECT_TRUE(host_->new_content_rendering_timeout_fired());
  host_->reset_new_content_rendering_timeout_fired();
}

// This tests that a compositor frame received with a stale content source ID
// in its metadata is properly discarded.
TEST_F(RenderWidgetHostTest, SwapCompositorFrameWithBadSourceId) {
  // If Surface Synchronization is on, we don't keep track of content_source_id
  // in CompositorFrameMetadata.
  if (features::IsSurfaceSynchronizationEnabled())
    return;
  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());

  host_->DidNavigate(100);
  host_->set_new_content_rendering_delay_for_testing(
      base::TimeDelta::FromMicroseconds(9999));

  {
    // First swap a frame with an invalid ID.
    auto frame = viz::CompositorFrameBuilder()
                     .AddDefaultRenderPass()
                     .SetBeginFrameAck(viz::BeginFrameAck(0, 1, true))
                     .SetContentSourceId(99)
                     .Build();

    // Mocking |renderer_compositor_frame_sink_| to prevent crashes in
    // renderer_compositor_frame_sink_->DidReceiveCompositorFrameAck(resources).
    std::unique_ptr<viz::MockCompositorFrameSinkClient>
        mock_compositor_frame_sink_client =
            std::make_unique<viz::MockCompositorFrameSinkClient>();
    host_->SetMockRendererCompositorFrameSink(
        mock_compositor_frame_sink_client.get());

    host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr,
                                 0);
    EXPECT_FALSE(
        static_cast<TestView*>(host_->GetView())->did_swap_compositor_frame());
    EXPECT_EQ(viz::BeginFrameAck(0, 1, false),
              static_cast<TestView*>(host_->GetView())
                  ->last_did_not_produce_frame_ack());
    static_cast<TestView*>(host_->GetView())->reset_did_swap_compositor_frame();
  }

  {
    // Test with a valid content ID as a control.
    auto frame = viz::CompositorFrameBuilder()
                     .AddDefaultRenderPass()
                     .SetContentSourceId(100)
                     .Build();
    host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr,
                                 0);
    EXPECT_TRUE(
        static_cast<TestView*>(host_->GetView())->did_swap_compositor_frame());
    static_cast<TestView*>(host_->GetView())->reset_did_swap_compositor_frame();
  }

  {
    // We also accept frames with higher content IDs, to cover the case where
    // the browser process receives a compositor frame for a new page before
    // the corresponding DidCommitProvisionalLoad (it's a race).
    auto frame = viz::CompositorFrameBuilder()
                     .AddDefaultRenderPass()
                     .SetContentSourceId(101)
                     .Build();
    host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr,
                                 0);
    EXPECT_TRUE(
        static_cast<TestView*>(host_->GetView())->did_swap_compositor_frame());
  }
}

TEST_F(RenderWidgetHostMojoInputDisabledTest, TouchEmulator) {
  simulated_event_time_delta_seconds_ = 0.1;
  // Immediately ack all touches instead of sending them to the renderer.
  host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, false));
  host_->GetTouchEmulator()->Enable(
      TouchEmulator::Mode::kEmulatingTouchFromMouse,
      ui::GestureProviderConfigType::GENERIC_MOBILE);
  process_->sink().ClearMessages();
  view_->SetBounds(gfx::Rect(0, 0, 400, 200));
  view_->Show();

  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 10, 0, false);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Mouse press becomes touch start which in turn becomes tap.
  SimulateMouseEvent(WebInputEvent::kMouseDown, 10, 10, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchStart, host_->acked_touch_event_type());
  EXPECT_EQ("GestureTapDown", GetInputMessageTypes(process_));

  // Mouse drag generates touch move, cancels tap and starts scroll.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 30, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  SendScrollBeginAckIfneeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(
      "GestureTapCancel GestureScrollBegin TouchScrollStarted "
      "GestureScrollUpdate",
      GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Mouse drag with shift becomes pinch.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 40,
                     WebInputEvent::kShiftKey, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  EXPECT_EQ("GesturePinchBegin",
            GetInputMessageTypes(process_));
  EXPECT_EQ(0U, process_->sink().message_count());

  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 50,
                     WebInputEvent::kShiftKey, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  EXPECT_EQ("GesturePinchUpdate",
            GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Mouse drag without shift becomes scroll again.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 60, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  EXPECT_EQ("GesturePinchEnd GestureScrollUpdate",
            GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());

  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 70, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  EXPECT_EQ("GestureScrollUpdate",
            GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());

  SimulateMouseEvent(WebInputEvent::kMouseUp, 10, 70, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchEnd, host_->acked_touch_event_type());
  EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes(process_));
  EXPECT_EQ(0U, process_->sink().message_count());

  // Mouse move does nothing.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 80, 0, false);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Another mouse down continues scroll.
  SimulateMouseEvent(WebInputEvent::kMouseDown, 10, 80, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchStart, host_->acked_touch_event_type());
  EXPECT_EQ("GestureTapDown", GetInputMessageTypes(process_));
  EXPECT_EQ(0U, process_->sink().message_count());

  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 100, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  SendScrollBeginAckIfneeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(
      "GestureTapCancel GestureScrollBegin TouchScrollStarted "
      "GestureScrollUpdate",
      GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Another pinch.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 110,
                     WebInputEvent::kShiftKey, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  EXPECT_EQ("GesturePinchBegin",
            GetInputMessageTypes(process_));
  EXPECT_EQ(0U, process_->sink().message_count());

  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 120,
                     WebInputEvent::kShiftKey, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  EXPECT_EQ("GesturePinchUpdate",
            GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Turn off emulation during a pinch.
  host_->GetTouchEmulator()->Disable();
  EXPECT_EQ(WebInputEvent::kTouchCancel, host_->acked_touch_event_type());
  EXPECT_EQ("GesturePinchEnd GestureScrollEnd",
            GetInputMessageTypes(process_));
  EXPECT_EQ(0U, process_->sink().message_count());

  // Mouse event should pass untouched.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 10,
                     WebInputEvent::kShiftKey, true);
  EXPECT_EQ("MouseMove", GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::kMouseMove, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Turn on emulation.
  host_->GetTouchEmulator()->Enable(
      TouchEmulator::Mode::kEmulatingTouchFromMouse,
      ui::GestureProviderConfigType::GENERIC_MOBILE);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Another touch.
  SimulateMouseEvent(WebInputEvent::kMouseDown, 10, 10, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchStart, host_->acked_touch_event_type());
  EXPECT_EQ("GestureTapDown", GetInputMessageTypes(process_));
  EXPECT_EQ(0U, process_->sink().message_count());

  // Scroll.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 30, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  SendScrollBeginAckIfneeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(
      "GestureTapCancel GestureScrollBegin TouchScrollStarted "
      "GestureScrollUpdate",
      GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Turn off emulation during a scroll.
  host_->GetTouchEmulator()->Disable();
  EXPECT_EQ(WebInputEvent::kTouchCancel, host_->acked_touch_event_type());

  EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes(process_));
  EXPECT_EQ(0U, process_->sink().message_count());
}

TEST_F(RenderWidgetHostTest, TouchEmulator) {
  simulated_event_time_delta_seconds_ = 0.1;
  // Immediately ack all touches instead of sending them to the renderer.
  host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, false));
  host_->GetTouchEmulator()->Enable(
      TouchEmulator::Mode::kEmulatingTouchFromMouse,
      ui::GestureProviderConfigType::GENERIC_MOBILE);
  process_->sink().ClearMessages();
  view_->SetBounds(gfx::Rect(0, 0, 400, 200));
  view_->Show();

  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 10, 0, false);
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // Mouse press becomes touch start which in turn becomes tap.
  SimulateMouseEvent(WebInputEvent::kMouseDown, 10, 10, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchStart, host_->acked_touch_event_type());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGestureTapDown,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Mouse drag generates touch move, cancels tap and starts scroll.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 30, 0, true);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(4u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  ASSERT_TRUE(dispatched_events[1]->ToEvent());
  ASSERT_TRUE(dispatched_events[2]->ToEvent());
  ASSERT_TRUE(dispatched_events[3]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGestureTapCancel,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());
  EXPECT_EQ(WebInputEvent::kGestureScrollBegin,
            dispatched_events[1]->ToEvent()->Event()->web_event->GetType());
  EXPECT_EQ(WebInputEvent::kTouchScrollStarted,
            dispatched_events[2]->ToEvent()->Event()->web_event->GetType());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            dispatched_events[3]->ToEvent()->Event()->web_event->GetType());
  dispatched_events[1]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  EXPECT_EQ(
      0u,
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages().size());
  dispatched_events[3]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
  // Mouse drag with shift becomes pinch.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 40,
                     WebInputEvent::kShiftKey, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());

  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGesturePinchBegin,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 50,
                     WebInputEvent::kShiftKey, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());

  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  dispatched_events[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Mouse drag without shift becomes scroll again.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 60, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());

  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(2u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  ASSERT_TRUE(dispatched_events[1]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGesturePinchEnd,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            dispatched_events[1]->ToEvent()->Event()->web_event->GetType());
  dispatched_events[1]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);

  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 70, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());
  dispatched_events[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);

  SimulateMouseEvent(WebInputEvent::kMouseUp, 10, 70, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchEnd, host_->acked_touch_event_type());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Mouse move does nothing.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 80, 0, false);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // Another mouse down continues scroll.
  SimulateMouseEvent(WebInputEvent::kMouseDown, 10, 80, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchStart, host_->acked_touch_event_type());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGestureTapDown,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 100, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(4u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  ASSERT_TRUE(dispatched_events[1]->ToEvent());
  ASSERT_TRUE(dispatched_events[2]->ToEvent());
  ASSERT_TRUE(dispatched_events[3]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGestureTapCancel,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());
  EXPECT_EQ(WebInputEvent::kGestureScrollBegin,
            dispatched_events[1]->ToEvent()->Event()->web_event->GetType());
  EXPECT_EQ(WebInputEvent::kTouchScrollStarted,
            dispatched_events[2]->ToEvent()->Event()->web_event->GetType());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            dispatched_events[3]->ToEvent()->Event()->web_event->GetType());
  dispatched_events[1]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(
      0u,
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages().size());
  dispatched_events[3]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
  // Another pinch.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 110,
                     WebInputEvent::kShiftKey, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_EQ(1u, dispatched_events.size());
  EXPECT_EQ(WebInputEvent::kGesturePinchBegin,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 120,
                     WebInputEvent::kShiftKey, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_EQ(1u, dispatched_events.size());
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());
  dispatched_events[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Turn off emulation during a pinch.
  host_->GetTouchEmulator()->Disable();
  EXPECT_EQ(WebInputEvent::kTouchCancel, host_->acked_touch_event_type());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(2u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  ASSERT_TRUE(dispatched_events[1]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGesturePinchEnd,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd,
            dispatched_events[1]->ToEvent()->Event()->web_event->GetType());

  // Mouse event should pass untouched.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 10,
                     WebInputEvent::kShiftKey, true);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kMouseMove,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());
  dispatched_events[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Turn on emulation.
  host_->GetTouchEmulator()->Enable(
      TouchEmulator::Mode::kEmulatingTouchFromMouse,
      ui::GestureProviderConfigType::GENERIC_MOBILE);

  // Another touch.
  SimulateMouseEvent(WebInputEvent::kMouseDown, 10, 10, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchStart, host_->acked_touch_event_type());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGestureTapDown,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Scroll.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 30, 0, true);
  EXPECT_EQ(WebInputEvent::kTouchMove, host_->acked_touch_event_type());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(4u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  ASSERT_TRUE(dispatched_events[1]->ToEvent());
  ASSERT_TRUE(dispatched_events[2]->ToEvent());
  ASSERT_TRUE(dispatched_events[3]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGestureTapCancel,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());
  EXPECT_EQ(WebInputEvent::kGestureScrollBegin,
            dispatched_events[1]->ToEvent()->Event()->web_event->GetType());
  EXPECT_EQ(WebInputEvent::kTouchScrollStarted,
            dispatched_events[2]->ToEvent()->Event()->web_event->GetType());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            dispatched_events[3]->ToEvent()->Event()->web_event->GetType());
  dispatched_events[1]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(
      0u,
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages().size());
  dispatched_events[3]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Turn off emulation during a scroll.
  host_->GetTouchEmulator()->Disable();
  EXPECT_EQ(WebInputEvent::kTouchCancel, host_->acked_touch_event_type());

  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());
}

TEST_F(RenderWidgetHostTest, IgnoreInputEvent) {
  host_->SetupForInputRouterTest();

  host_->SetIgnoreInputEvents(true);

  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  SimulateMouseEvent(WebInputEvent::kMouseMove);
  EXPECT_FALSE(host_->mock_input_router()->sent_mouse_event_);

  SimulateWheelEvent(0, 100, 0, true);
  EXPECT_FALSE(host_->mock_input_router()->sent_wheel_event_);

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchpad);
  EXPECT_FALSE(host_->mock_input_router()->sent_gesture_event_);

  PressTouchPoint(100, 100);
  SendTouchEvent();
  EXPECT_FALSE(host_->mock_input_router()->send_touch_event_not_cancelled_);
}

TEST_F(RenderWidgetHostTest, KeyboardListenerIgnoresEvent) {
  host_->SetupForInputRouterTest();
  host_->AddKeyPressEventCallback(
      base::Bind(&RenderWidgetHostTest::KeyPressEventCallback,
                 base::Unretained(this)));
  handle_key_press_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);
}

TEST_F(RenderWidgetHostTest, KeyboardListenerSuppressFollowingEvents) {
  host_->SetupForInputRouterTest();

  host_->AddKeyPressEventCallback(
      base::Bind(&RenderWidgetHostTest::KeyPressEventCallback,
                 base::Unretained(this)));

  // The callback handles the first event
  handle_key_press_event_ = true;
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  // Following Char events should be suppressed
  handle_key_press_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::kChar);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);
  SimulateKeyboardEvent(WebInputEvent::kChar);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  // Sending RawKeyDown event should stop suppression
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);
  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);

  host_->mock_input_router()->sent_keyboard_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::kChar);
  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);
}

TEST_F(RenderWidgetHostTest, MouseEventCallbackCanHandleEvent) {
  host_->SetupForInputRouterTest();

  host_->AddMouseEventCallback(
      base::Bind(&RenderWidgetHostTest::MouseEventCallback,
                 base::Unretained(this)));

  handle_mouse_event_ = true;
  SimulateMouseEvent(WebInputEvent::kMouseDown);

  EXPECT_FALSE(host_->mock_input_router()->sent_mouse_event_);

  handle_mouse_event_ = false;
  SimulateMouseEvent(WebInputEvent::kMouseDown);

  EXPECT_TRUE(host_->mock_input_router()->sent_mouse_event_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesHandleInputEvent_ACK) {
  host_->SetupForInputRouterTest();

  SendInputEventACK(WebInputEvent::kRawKeyDown, INPUT_EVENT_ACK_STATE_CONSUMED);

  EXPECT_TRUE(host_->mock_input_router()->message_received_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesMoveCaret_ACK) {
  host_->SetupForInputRouterTest();

  host_->OnMessageReceived(InputHostMsg_MoveCaret_ACK(0));

  EXPECT_TRUE(host_->mock_input_router()->message_received_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesSelectRange_ACK) {
  host_->SetupForInputRouterTest();

  host_->OnMessageReceived(InputHostMsg_SelectRange_ACK(0));

  EXPECT_TRUE(host_->mock_input_router()->message_received_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesHasTouchEventHandlers) {
  host_->SetupForInputRouterTest();

  host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));

  EXPECT_TRUE(host_->mock_input_router()->message_received_);
}

void CheckLatencyInfoComponentInMessage(RenderWidgetHostProcess* process,
                                        int64_t component_id,
                                        WebInputEvent::Type expected_type) {
  EXPECT_EQ(process->sink().message_count(), 1U);

  const IPC::Message* message = process->sink().GetMessageAt(0);
  EXPECT_EQ(static_cast<uint32_t>(InputMsg_HandleInputEvent::ID),
            message->type());
  InputMsg_HandleInputEvent::Param params;
  EXPECT_TRUE(InputMsg_HandleInputEvent::Read(message, &params));

  const WebInputEvent* event = std::get<0>(params);
  ui::LatencyInfo latency_info = std::get<2>(params);

  EXPECT_TRUE(event->GetType() == expected_type);
  EXPECT_TRUE(latency_info.FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, component_id, nullptr));

  process->sink().ClearMessages();
}

void CheckLatencyInfoComponentInGestureScrollUpdate(
    RenderWidgetHostProcess* process,
    int64_t component_id) {
  EXPECT_EQ(process->sink().message_count(), 2U);
  const IPC::Message* message = process->sink().GetMessageAt(0);
  EXPECT_EQ(static_cast<uint32_t>(InputMsg_HandleInputEvent::ID),
            message->type());
  InputMsg_HandleInputEvent::Param params;
  EXPECT_TRUE(InputMsg_HandleInputEvent::Read(message, &params));

  const WebInputEvent* event = std::get<0>(params);
  ui::LatencyInfo latency_info = std::get<2>(params);

  EXPECT_TRUE(event->GetType() == WebInputEvent::kTouchScrollStarted);

  message = process->sink().GetMessageAt(1);
  EXPECT_EQ(static_cast<uint32_t>(InputMsg_HandleInputEvent::ID),
            message->type());
  EXPECT_TRUE(InputMsg_HandleInputEvent::Read(message, &params));

  event = std::get<0>(params);
  latency_info = std::get<2>(params);

  EXPECT_TRUE(event->GetType() == WebInputEvent::kGestureScrollUpdate);
  EXPECT_TRUE(latency_info.FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, component_id, nullptr));

  process->sink().ClearMessages();
}

// Tests that after input event passes through RWHI through ForwardXXXEvent()
// or ForwardXXXEventWithLatencyInfo(), LatencyInfo component
// ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT will always present in the
// event's LatencyInfo.
void RenderWidgetHostTest::InputEventRWHLatencyComponentMojoInputDisabled() {
  host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  process_->sink().ClearMessages();

  // Tests RWHI::ForwardWheelEvent().
  SimulateWheelEventPossiblyIncludingPhase(-5, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseBegan);
  CheckLatencyInfoComponentInMessage(process_, GetLatencyComponentId(),
                                     WebInputEvent::kMouseWheel);
  SendInputEventACK(WebInputEvent::kMouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardWheelEventWithLatencyInfo().
  SimulateWheelEventWithLatencyInfoAndPossiblyPhase(
      -5, 0, 0, true, ui::LatencyInfo(), WebMouseWheelEvent::kPhaseChanged);
  CheckLatencyInfoComponentInMessage(process_, GetLatencyComponentId(),
                                     WebInputEvent::kMouseWheel);
  SendInputEventACK(WebInputEvent::kMouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardMouseEvent().
  SimulateMouseEvent(WebInputEvent::kMouseMove);
  CheckLatencyInfoComponentInMessage(process_, GetLatencyComponentId(),
                                     WebInputEvent::kMouseMove);
  SendInputEventACK(WebInputEvent::kMouseMove, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardMouseEventWithLatencyInfo().
  SimulateMouseEventWithLatencyInfo(WebInputEvent::kMouseMove,
                                    ui::LatencyInfo());
  CheckLatencyInfoComponentInMessage(process_, GetLatencyComponentId(),
                                     WebInputEvent::kMouseMove);
  SendInputEventACK(WebInputEvent::kMouseMove, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardGestureEvent().
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfneeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  CheckLatencyInfoComponentInMessage(process_, GetLatencyComponentId(),
                                     WebInputEvent::kGestureScrollBegin);

  // Tests RWHI::ForwardGestureEventWithLatencyInfo().
  SimulateGestureEventWithLatencyInfo(WebInputEvent::kGestureScrollUpdate,
                                      blink::kWebGestureDeviceTouchscreen,
                                      ui::LatencyInfo());
  CheckLatencyInfoComponentInGestureScrollUpdate(process_,
                                                 GetLatencyComponentId());
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardTouchEventWithLatencyInfo().
  PressTouchPoint(0, 1);
  uint32_t touch_event_id = SendTouchEvent();
  InputEventAck ack(InputEventAckSource::COMPOSITOR_THREAD,
                    WebInputEvent::kTouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_event_id);
  host_->OnMessageReceived(InputHostMsg_HandleInputEvent_ACK(0, ack));
  CheckLatencyInfoComponentInMessage(process_, GetLatencyComponentId(),
                                     WebInputEvent::kTouchStart);
}
TEST_F(RenderWidgetHostMojoInputDisabledTest, InputEventRWHLatencyComponent) {
  InputEventRWHLatencyComponentMojoInputDisabled();
}
TEST_F(RenderWidgetHostWheelScrollLatchingMojoInputDisabledTest,
       InputEventRWHLatencyComponent) {
  InputEventRWHLatencyComponentMojoInputDisabled();
}
TEST_F(RenderWidgetHostAsyncWheelEventsEnabledMojoInputDisabledTest,
       InputEventRWHLatencyComponent) {
  InputEventRWHLatencyComponentMojoInputDisabled();
}

void CheckLatencyInfoComponentInMessage(
    MockWidgetInputHandler::MessageVector& dispatched_events,
    int64_t component_id,
    WebInputEvent::Type expected_type) {
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());

  EXPECT_TRUE(dispatched_events[0]->ToEvent()->Event()->web_event->GetType() ==
              expected_type);
  EXPECT_TRUE(
      dispatched_events[0]->ToEvent()->Event()->latency_info.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, component_id, nullptr));
  dispatched_events[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
}

void CheckLatencyInfoComponentInGestureScrollUpdate(
    MockWidgetInputHandler::MessageVector& dispatched_events,
    int64_t component_id) {
  ASSERT_EQ(2u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  ASSERT_TRUE(dispatched_events[1]->ToEvent());
  EXPECT_EQ(WebInputEvent::kTouchScrollStarted,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            dispatched_events[1]->ToEvent()->Event()->web_event->GetType());
  EXPECT_TRUE(
      dispatched_events[1]->ToEvent()->Event()->latency_info.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, component_id, nullptr));
  dispatched_events[1]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
}

// Tests that after input event passes through RWHI through ForwardXXXEvent()
// or ForwardXXXEventWithLatencyInfo(), LatencyInfo component
// ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT will always present in the
// event's LatencyInfo.
void RenderWidgetHostTest::InputEventRWHLatencyComponent() {
  host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));

  // Tests RWHI::ForwardWheelEvent().
  SimulateWheelEventPossiblyIncludingPhase(-5, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseBegan);
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events, GetLatencyComponentId(),
                                     WebInputEvent::kMouseWheel);

  // Tests RWHI::ForwardWheelEventWithLatencyInfo().
  SimulateWheelEventWithLatencyInfoAndPossiblyPhase(
      -5, 0, 0, true, ui::LatencyInfo(), WebMouseWheelEvent::kPhaseChanged);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events, GetLatencyComponentId(),
                                     WebInputEvent::kMouseWheel);

  // Tests RWHI::ForwardMouseEvent().
  SimulateMouseEvent(WebInputEvent::kMouseMove);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events, GetLatencyComponentId(),
                                     WebInputEvent::kMouseMove);

  // Tests RWHI::ForwardMouseEventWithLatencyInfo().
  SimulateMouseEventWithLatencyInfo(WebInputEvent::kMouseMove,
                                    ui::LatencyInfo());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events, GetLatencyComponentId(),
                                     WebInputEvent::kMouseMove);

  // Tests RWHI::ForwardGestureEvent().
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events, GetLatencyComponentId(),
                                     WebInputEvent::kGestureScrollBegin);

  // Tests RWHI::ForwardGestureEventWithLatencyInfo().
  SimulateGestureEventWithLatencyInfo(WebInputEvent::kGestureScrollUpdate,
                                      blink::kWebGestureDeviceTouchscreen,
                                      ui::LatencyInfo());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInGestureScrollUpdate(dispatched_events,
                                                 GetLatencyComponentId());

  // Tests RWHI::ForwardTouchEventWithLatencyInfo().
  PressTouchPoint(0, 1);
  SendTouchEvent();
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events, GetLatencyComponentId(),
                                     WebInputEvent::kTouchStart);
}
TEST_F(RenderWidgetHostTest, InputEventRWHLatencyComponent) {
  InputEventRWHLatencyComponent();
}
TEST_F(RenderWidgetHostWheelScrollLatchingDisabledTest,
       InputEventRWHLatencyComponent) {
  InputEventRWHLatencyComponent();
}
TEST_F(RenderWidgetHostAsyncWheelEventsEnabledTest,
       InputEventRWHLatencyComponent) {
  InputEventRWHLatencyComponent();
}

TEST_F(RenderWidgetHostTest, RendererExitedResetsInputRouter) {
  // RendererExited will delete the view.
  host_->SetView(new TestView(host_.get()));
  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);

  // Make sure the input router is in a fresh state.
  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
}

// Regression test for http://crbug.com/401859 and http://crbug.com/522795.
TEST_F(RenderWidgetHostTest, RendererExitedResetsIsHidden) {
  // RendererExited will delete the view.
  host_->SetView(new TestView(host_.get()));
  host_->WasShown(ui::LatencyInfo());

  ASSERT_FALSE(host_->is_hidden());
  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  ASSERT_TRUE(host_->is_hidden());

  // Make sure the input router is in a fresh state.
  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
}

TEST_F(RenderWidgetHostTest, ResizeParams) {
  gfx::Rect bounds(0, 0, 100, 100);
  gfx::Size compositor_viewport_pixel_size(40, 50);
  view_->SetBounds(bounds);
  view_->SetMockCompositorViewportPixelSize(compositor_viewport_pixel_size);

  ResizeParams resize_params;
  host_->GetResizeParams(&resize_params);
  EXPECT_EQ(bounds.size(), resize_params.new_size);
  EXPECT_EQ(compositor_viewport_pixel_size,
            resize_params.compositor_viewport_pixel_size);
}

TEST_F(RenderWidgetHostTest, ResizeParamsDeviceScale) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kEnableUseZoomForDSF, "true");

  float device_scale = 3.5f;
  ScreenInfo screen_info;
  screen_info.device_scale_factor = device_scale;

  view_->SetScreenInfo(screen_info);
  host_->WasResized();

  float top_controls_height = 10.0f;
  float bottom_controls_height = 20.0f;
  view_->set_top_controls_height(top_controls_height);
  view_->set_bottom_controls_height(bottom_controls_height);

  ResizeParams resize_params;
  host_->GetResizeParams(&resize_params);
  EXPECT_EQ(top_controls_height * device_scale,
            resize_params.top_controls_height);
  EXPECT_EQ(bottom_controls_height * device_scale,
            resize_params.bottom_controls_height);
}

// Make sure no dragging occurs after renderer exited. See crbug.com/704832.
TEST_F(RenderWidgetHostTest, RendererExitedNoDrag) {
  host_->SetView(new TestView(host_.get()));

  EXPECT_EQ(delegate_->mock_delegate_view()->start_dragging_count(), 0);

  GURL http_url = GURL("http://www.domain.com/index.html");
  DropData drop_data;
  drop_data.url = http_url;
  drop_data.html_base_url = http_url;
  blink::WebDragOperationsMask drag_operation = blink::kWebDragOperationEvery;
  DragEventSourceInfo event_info;
  host_->OnStartDragging(drop_data, drag_operation, SkBitmap(), gfx::Vector2d(),
                         event_info);
  EXPECT_EQ(delegate_->mock_delegate_view()->start_dragging_count(), 1);

  // Simulate that renderer exited due navigation to the next page.
  host_->RendererExited(base::TERMINATION_STATUS_NORMAL_TERMINATION, 0);
  EXPECT_FALSE(host_->GetView());
  host_->OnStartDragging(drop_data, drag_operation, SkBitmap(), gfx::Vector2d(),
                         event_info);
  EXPECT_EQ(delegate_->mock_delegate_view()->start_dragging_count(), 1);
}

class RenderWidgetHostInitialSizeTest : public RenderWidgetHostTest {
 public:
  RenderWidgetHostInitialSizeTest()
      : RenderWidgetHostTest(), initial_size_(200, 100) {}

  void ConfigureView(TestView* view) override {
    view->SetBounds(gfx::Rect(initial_size_));
  }

 protected:
  gfx::Size initial_size_;
};

TEST_F(RenderWidgetHostInitialSizeTest, InitialSize) {
  // Having an initial size set means that the size information had been sent
  // with the reqiest to new up the RenderView and so subsequent WasResized
  // calls should not result in new IPC (unless the size has actually changed).
  host_->WasResized();
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
  EXPECT_EQ(initial_size_, host_->old_resize_params_->new_size);
  EXPECT_TRUE(host_->resize_ack_pending_);
}

// Tests that event dispatch after the delegate has been detached doesn't cause
// a crash. See crbug.com/563237.
TEST_F(RenderWidgetHostTest, EventDispatchPostDetach) {
  host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  process_->sink().ClearMessages();

  host_->DetachDelegate();

  // Tests RWHI::ForwardGestureEventWithLatencyInfo().
  SimulateGestureEventWithLatencyInfo(WebInputEvent::kGestureScrollUpdate,
                                      blink::kWebGestureDeviceTouchscreen,
                                      ui::LatencyInfo());

  // Tests RWHI::ForwardWheelEventWithLatencyInfo().
  SimulateWheelEventWithLatencyInfo(-5, 0, 0, true, ui::LatencyInfo());

  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
}

// Check that if messages of a frame arrive earlier than the frame itself, we
// queue the messages until the frame arrives and then process them.
TEST_F(RenderWidgetHostTest, FrameToken_MessageThenFrame) {
  const uint32_t frame_token = 99;
  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());
  std::vector<IPC::Message> messages;
  messages.push_back(ViewHostMsg_DidFirstVisuallyNonEmptyPaint(5));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      ViewHostMsg_FrameSwapMessages(0, frame_token, messages));
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  auto frame = viz::CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetFrameToken(frame_token)
                   .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(1u, host_->processed_frame_messages_count());
}

// Check that if a frame arrives earlier than its messages, we process the
// messages immedtiately.
TEST_F(RenderWidgetHostTest, FrameToken_FrameThenMessage) {
  const uint32_t frame_token = 99;
  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());
  std::vector<IPC::Message> messages;
  messages.push_back(ViewHostMsg_DidFirstVisuallyNonEmptyPaint(5));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  auto frame = viz::CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetFrameToken(frame_token)
                   .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      ViewHostMsg_FrameSwapMessages(0, frame_token, messages));
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(1u, host_->processed_frame_messages_count());
}

// Check that if messages of multiple frames arrive before the frames, we
// process each message once it frame arrives.
TEST_F(RenderWidgetHostTest, FrameToken_MultipleMessagesThenTokens) {
  const uint32_t frame_token1 = 99;
  const uint32_t frame_token2 = 100;
  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());
  std::vector<IPC::Message> messages1;
  std::vector<IPC::Message> messages2;
  messages1.push_back(ViewHostMsg_DidFirstVisuallyNonEmptyPaint(5));
  messages2.push_back(ViewHostMsg_DidFirstVisuallyNonEmptyPaint(6));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      ViewHostMsg_FrameSwapMessages(0, frame_token1, messages1));
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      ViewHostMsg_FrameSwapMessages(0, frame_token2, messages2));
  EXPECT_EQ(2u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  auto frame = viz::CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetFrameToken(frame_token1)
                   .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr, 0);
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(1u, host_->processed_frame_messages_count());

  frame = viz::CompositorFrameBuilder()
              .AddDefaultRenderPass()
              .SetFrameToken(frame_token2)
              .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(2u, host_->processed_frame_messages_count());
}

// Check that if multiple frames arrive before their messages, each message is
// processed immediately as soon as it arrives.
TEST_F(RenderWidgetHostTest, FrameToken_MultipleTokensThenMessages) {
  const uint32_t frame_token1 = 99;
  const uint32_t frame_token2 = 100;
  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());
  std::vector<IPC::Message> messages1;
  std::vector<IPC::Message> messages2;
  messages1.push_back(ViewHostMsg_DidFirstVisuallyNonEmptyPaint(5));
  messages2.push_back(ViewHostMsg_DidFirstVisuallyNonEmptyPaint(6));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  auto frame = viz::CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetFrameToken(frame_token1)
                   .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  frame = viz::CompositorFrameBuilder()
              .AddDefaultRenderPass()
              .SetFrameToken(frame_token2)
              .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      ViewHostMsg_FrameSwapMessages(0, frame_token1, messages1));
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(1u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      ViewHostMsg_FrameSwapMessages(0, frame_token2, messages2));
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(2u, host_->processed_frame_messages_count());
}

// Check that if one frame is lost but its messages arrive, we process the
// messages on the arrival of the next frame.
TEST_F(RenderWidgetHostTest, FrameToken_DroppedFrame) {
  const uint32_t frame_token1 = 99;
  const uint32_t frame_token2 = 100;
  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());
  std::vector<IPC::Message> messages1;
  std::vector<IPC::Message> messages2;
  messages1.push_back(ViewHostMsg_DidFirstVisuallyNonEmptyPaint(5));
  messages2.push_back(ViewHostMsg_DidFirstVisuallyNonEmptyPaint(6));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      ViewHostMsg_FrameSwapMessages(0, frame_token1, messages1));
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      ViewHostMsg_FrameSwapMessages(0, frame_token2, messages2));
  EXPECT_EQ(2u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  auto frame = viz::CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetFrameToken(frame_token2)
                   .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(2u, host_->processed_frame_messages_count());
}

// Check that if the renderer crashes, we drop all queued messages and allow
// smaller frame tokens to be sent by the renderer.
TEST_F(RenderWidgetHostTest, FrameToken_RendererCrash) {
  const uint32_t frame_token1 = 99;
  const uint32_t frame_token2 = 50;
  const uint32_t frame_token3 = 30;
  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());
  std::vector<IPC::Message> messages1;
  std::vector<IPC::Message> messages3;
  messages1.push_back(ViewHostMsg_DidFirstVisuallyNonEmptyPaint(5));
  messages3.push_back(ViewHostMsg_DidFirstVisuallyNonEmptyPaint(6));

  // Mocking |renderer_compositor_frame_sink_| to prevent crashes in
  // renderer_compositor_frame_sink_->DidReceiveCompositorFrameAck(resources).
  std::unique_ptr<viz::MockCompositorFrameSinkClient>
      mock_compositor_frame_sink_client =
          std::make_unique<viz::MockCompositorFrameSinkClient>();
  host_->SetMockRendererCompositorFrameSink(
      mock_compositor_frame_sink_client.get());

  // If we don't do this, then RWHI destroys the view in RendererExited and
  // then a crash occurs when we attempt to destroy it again in TearDown().
  host_->SetView(nullptr);

  host_->OnMessageReceived(
      ViewHostMsg_FrameSwapMessages(0, frame_token1, messages1));
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());
  host_->Init();

  auto frame = viz::CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetFrameToken(frame_token2)
                   .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());
  host_->SetView(view_.get());
  host_->Init();

  host_->OnMessageReceived(
      ViewHostMsg_FrameSwapMessages(0, frame_token3, messages3));
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  frame = viz::CompositorFrameBuilder()
              .AddDefaultRenderPass()
              .SetFrameToken(frame_token3)
              .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame), nullptr, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(1u, host_->processed_frame_messages_count());
}

TEST_F(RenderWidgetHostTest, InflightEventCountResetsAfterRebind) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  EXPECT_EQ(1u, host_->in_flight_event_count());
  mojom::WidgetPtr widget;
  std::unique_ptr<MockWidgetImpl> widget_impl =
      std::make_unique<MockWidgetImpl>(mojo::MakeRequest(&widget));
  host_->SetWidget(std::move(widget));
  EXPECT_EQ(0u, host_->in_flight_event_count());
}

TEST_F(RenderWidgetHostTest, ForceEnableZoomShouldUpdateAfterRebind) {
  SCOPED_TRACE("force_enable_zoom is false at start.");
  host_->ExpectForceEnableZoom(false);

  // Set force_enable_zoom true.
  host_->SetForceEnableZoom(true);

  SCOPED_TRACE("force_enable_zoom is true after set.");
  host_->ExpectForceEnableZoom(true);

  // Rebind should also update to the latest force_enable_zoom state.
  mojom::WidgetPtr widget;
  std::unique_ptr<MockWidgetImpl> widget_impl =
      std::make_unique<MockWidgetImpl>(mojo::MakeRequest(&widget));
  host_->SetWidget(std::move(widget));

  SCOPED_TRACE("force_enable_zoom is true after rebind.");
  host_->ExpectForceEnableZoom(true);
}

TEST_F(RenderWidgetHostTest, RenderWidgetSurfaceProperties) {
  RenderWidgetSurfaceProperties prop1;
  prop1.size = gfx::Size(200, 200);
  prop1.device_scale_factor = 1.f;
  RenderWidgetSurfaceProperties prop2;
  prop2.size = gfx::Size(300, 300);
  prop2.device_scale_factor = 2.f;

  EXPECT_EQ(
      "RenderWidgetSurfaceProperties(size(this: 200x200, other: 300x300), "
      "device_scale_factor(this: 1, other: 2))",
      prop1.ToDiffString(prop2));
  EXPECT_EQ(
      "RenderWidgetSurfaceProperties(size(this: 300x300, other: 200x200), "
      "device_scale_factor(this: 2, other: 1))",
      prop2.ToDiffString(prop1));
  EXPECT_EQ("", prop1.ToDiffString(prop1));
  EXPECT_EQ("", prop2.ToDiffString(prop2));
}

// If a navigation happens while the widget is hidden, we shouldn't show
// contents of the previous page when we become visible.
TEST_F(RenderWidgetHostTest, NavigateInBackgroundShowsBlank) {
  // When visible, navigation does not immediately call into
  // ClearDisplayedGraphics.
  host_->WasShown(ui::LatencyInfo());
  host_->DidNavigate(5);
  EXPECT_FALSE(host_->new_content_rendering_timeout_fired());

  // Hide then show. ClearDisplayedGraphics must be called.
  host_->WasHidden();
  host_->WasShown(ui::LatencyInfo());
  EXPECT_TRUE(host_->new_content_rendering_timeout_fired());
  host_->reset_new_content_rendering_timeout_fired();

  // Hide, navigate, then show. ClearDisplayedGraphics must be called.
  host_->WasHidden();
  host_->DidNavigate(6);
  host_->WasShown(ui::LatencyInfo());
  EXPECT_TRUE(host_->new_content_rendering_timeout_fired());
}

TEST_F(RenderWidgetHostTest, RendererHangRecordsMetrics) {
  base::SimpleTestTickClock clock;
  host_->set_clock_for_testing(&clock);
  base::HistogramTester tester;

  // RenderWidgetHost makes private the methods it overrides from
  // InputRouterClient. Call them through the base class.
  InputRouterClient* input_router_client = host_.get();

  // Do a 3s hang. This shouldn't affect metrics.
  input_router_client->IncrementInFlightEventCount();
  clock.Advance(base::TimeDelta::FromSeconds(3));
  input_router_client->DecrementInFlightEventCount(
      InputEventAckSource::UNKNOWN);
  tester.ExpectTotalCount("Renderer.Hung.Duration", 0u);

  // Do a 17s hang. This should affect metrics.
  input_router_client->IncrementInFlightEventCount();
  clock.Advance(base::TimeDelta::FromSeconds(17));
  input_router_client->DecrementInFlightEventCount(
      InputEventAckSource::UNKNOWN);
  tester.ExpectTotalCount("Renderer.Hung.Duration", 1u);
  tester.ExpectUniqueSample("Renderer.Hung.Duration", 17000, 1);
}

}  // namespace content
