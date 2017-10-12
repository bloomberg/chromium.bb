// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include <stddef.h>
#include <stdint.h>

#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/gl_helper.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "components/viz/test/fake_surface_observer.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/compositor/test/no_transport_image_transport_factory.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/delegated_frame_host_client_aura.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/mouse_wheel_event_queue.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_event_handler.h"
#include "content/browser/renderer_host/render_widget_host_view_frame_subscriber.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/browser/web_contents/web_contents_view_aura.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/text_input_state.h"
#include "content/common/view_messages.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/fake_renderer_compositor_frame_sink.h"
#include "content/test/mock_widget_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gestures/motion_event_aura.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/selection_bound.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/default_screen_position_client.h"
#include "ui/wm/core/window_util.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/input_method.h"
#endif

using testing::_;

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::WebInputEventTraits;
using viz::FrameEvictionManager;

namespace content {

void InstallDelegatedFrameHostClient(
    RenderWidgetHostViewAura* render_widget_host_view,
    std::unique_ptr<DelegatedFrameHostClient> delegated_frame_host_client);

namespace {

const viz::LocalSurfaceId kArbitraryLocalSurfaceId(
    1,
    base::UnguessableToken::Deserialize(2, 3));

class TestOverscrollDelegate : public OverscrollControllerDelegate {
 public:
  explicit TestOverscrollDelegate(RenderWidgetHostView* view)
      : view_(view),
        current_mode_(OVERSCROLL_NONE),
        completed_mode_(OVERSCROLL_NONE),
        delta_x_(0.f),
        delta_y_(0.f) {}

  ~TestOverscrollDelegate() override {}

  void set_delta_cap(float delta_cap) { delta_cap_ = delta_cap; }

  OverscrollMode current_mode() const { return current_mode_; }
  OverscrollMode completed_mode() const { return completed_mode_; }
  const std::vector<OverscrollMode>& historical_modes() const {
    return historical_modes_;
  }
  float delta_x() const { return delta_x_; }
  float delta_y() const { return delta_y_; }

  void Reset() {
    current_mode_ = OVERSCROLL_NONE;
    completed_mode_ = OVERSCROLL_NONE;
    historical_modes_.clear();
    delta_x_ = delta_y_ = 0.f;
  }

 private:
  // Overridden from OverscrollControllerDelegate:
  gfx::Size GetDisplaySize() const override {
    return display::Screen::GetScreen()
        ->GetDisplayNearestView(view_->GetNativeView())
        .size();
  }

  bool OnOverscrollUpdate(float delta_x, float delta_y) override {
    delta_x_ = delta_x;
    delta_y_ = delta_y;
    return true;
  }

  void OnOverscrollComplete(OverscrollMode overscroll_mode) override {
    EXPECT_EQ(current_mode_, overscroll_mode);
    completed_mode_ = overscroll_mode;
    current_mode_ = OVERSCROLL_NONE;
  }

  void OnOverscrollModeChange(OverscrollMode old_mode,
                              OverscrollMode new_mode,
                              OverscrollSource source) override {
    EXPECT_EQ(current_mode_, old_mode);
    current_mode_ = new_mode;
    historical_modes_.push_back(new_mode);
    delta_x_ = delta_y_ = 0.f;
  }

  base::Optional<float> GetMaxOverscrollDelta() const override {
    return delta_cap_;
  }

  RenderWidgetHostView* view_;
  base::Optional<float> delta_cap_;
  OverscrollMode current_mode_;
  OverscrollMode completed_mode_;
  std::vector<OverscrollMode> historical_modes_;

  float delta_x_;
  float delta_y_;

  DISALLOW_COPY_AND_ASSIGN(TestOverscrollDelegate);
};

class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate()
      : rwh_(nullptr),
        is_fullscreen_(false),
        focused_widget_(nullptr),
        last_device_scale_factor_(0.0) {}
  ~MockRenderWidgetHostDelegate() override {}
  const NativeWebKeyboardEvent* last_event() const { return last_event_.get(); }
  void set_widget_host(RenderWidgetHostImpl* rwh) { rwh_ = rwh; }
  void set_is_fullscreen(bool is_fullscreen) { is_fullscreen_ = is_fullscreen; }
  TextInputManager* GetTextInputManager() override {
    return &text_input_manager_;
  }
  RenderWidgetHostImpl* GetFocusedRenderWidgetHost(
      RenderWidgetHostImpl* widget_host) override {
    return !!focused_widget_ ? focused_widget_ : widget_host;
  }
  void set_focused_widget(RenderWidgetHostImpl* focused_widget) {
    focused_widget_ = focused_widget;
  }

  double get_last_device_scale_factor() { return last_device_scale_factor_; }
  void UpdateDeviceScaleFactor(double device_scale_factor) override {
    last_device_scale_factor_ = device_scale_factor;
  }
  void set_pre_handle_keyboard_event_result(
      KeyboardEventProcessingResult result) {
    pre_handle_keyboard_event_result_ = result;
  }

 protected:
  // RenderWidgetHostDelegate:
  KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) override {
    last_event_.reset(new NativeWebKeyboardEvent(event));
    return pre_handle_keyboard_event_result_;
  }
  void ExecuteEditCommand(
      const std::string& command,
      const base::Optional<base::string16>& value) override {}
  void Cut() override {}
  void Copy() override {}
  void Paste() override {}
  void SelectAll() override {}
  void SendScreenRects() override {
    if (rwh_)
      rwh_->SendScreenRects();
  }
  bool IsFullscreenForCurrentTab() const override { return is_fullscreen_; }

 private:
  std::unique_ptr<NativeWebKeyboardEvent> last_event_;
  RenderWidgetHostImpl* rwh_;
  bool is_fullscreen_;
  TextInputManager text_input_manager_;
  RenderWidgetHostImpl* focused_widget_;
  double last_device_scale_factor_;
  KeyboardEventProcessingResult pre_handle_keyboard_event_result_ =
      KeyboardEventProcessingResult::HANDLED;

  DISALLOW_COPY_AND_ASSIGN(MockRenderWidgetHostDelegate);
};

// Simple observer that keeps track of changes to a window for tests.
class TestWindowObserver : public aura::WindowObserver {
 public:
  explicit TestWindowObserver(aura::Window* window_to_observe)
      : window_(window_to_observe) {
    window_->AddObserver(this);
  }
  ~TestWindowObserver() override {
    if (window_)
      window_->RemoveObserver(this);
  }

  bool destroyed() const { return destroyed_; }

  // aura::WindowObserver overrides:
  void OnWindowDestroyed(aura::Window* window) override {
    CHECK_EQ(window, window_);
    destroyed_ = true;
    window_ = nullptr;
  }

 private:
  // Window that we're observing, or nullptr if it's been destroyed.
  aura::Window* window_;

  // Was |window_| destroyed?
  bool destroyed_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowObserver);
};

class FakeFrameSubscriber : public RenderWidgetHostViewFrameSubscriber {
 public:
  FakeFrameSubscriber(gfx::Size size, base::Callback<void(bool)> callback)
      : size_(size),
        callback_(callback),
        should_capture_(true),
        source_id_for_copy_request_(base::UnguessableToken::Create()) {}

  bool ShouldCaptureFrame(const gfx::Rect& damage_rect,
                          base::TimeTicks present_time,
                          scoped_refptr<media::VideoFrame>* storage,
                          DeliverFrameCallback* callback) override {
    if (!should_capture_)
      return false;
    last_present_time_ = present_time;
    *storage = media::VideoFrame::CreateFrame(media::PIXEL_FORMAT_YV12, size_,
                                              gfx::Rect(size_), size_,
                                              base::TimeDelta());
    *callback = base::Bind(&FakeFrameSubscriber::CallbackMethod, callback_);
    return true;
  }

  const base::UnguessableToken& GetSourceIdForCopyRequest() override {
    return source_id_for_copy_request_;
  }

  base::TimeTicks last_present_time() const { return last_present_time_; }

  void set_should_capture(bool should_capture) {
    should_capture_ = should_capture;
  }

  static void CallbackMethod(base::Callback<void(bool)> callback,
                             base::TimeTicks present_time,
                             const gfx::Rect& region_in_frame,
                             bool success) {
    callback.Run(success);
  }

 private:
  gfx::Size size_;
  base::Callback<void(bool)> callback_;
  base::TimeTicks last_present_time_;
  bool should_capture_;
  base::UnguessableToken source_id_for_copy_request_;
};

class FakeWindowEventDispatcher : public aura::WindowEventDispatcher {
 public:
  FakeWindowEventDispatcher(aura::WindowTreeHost* host)
      : WindowEventDispatcher(host),
        processed_touch_event_count_(0) {}

  void ProcessedTouchEvent(
      uint32_t unique_event_id,
      aura::Window* window,
      ui::EventResult result,
      bool is_source_touch_event_set_non_blocking) override {
    WindowEventDispatcher::ProcessedTouchEvent(
        unique_event_id, window, result,
        is_source_touch_event_set_non_blocking);
    processed_touch_event_count_++;
  }

  size_t GetAndResetProcessedTouchEventCount() {
    size_t count = processed_touch_event_count_;
    processed_touch_event_count_ = 0;
    return count;
  }

 private:
  size_t processed_touch_event_count_;
};

class FakeDelegatedFrameHostClientAura : public DelegatedFrameHostClientAura,
                                         public ui::CompositorLockDelegate {
 public:
  explicit FakeDelegatedFrameHostClientAura(
      RenderWidgetHostViewAura* render_widget_host_view)
      : DelegatedFrameHostClientAura(render_widget_host_view),
        weak_ptr_factory_(this) {}
  ~FakeDelegatedFrameHostClientAura() override = default;

  void DisableResizeLock() { can_create_resize_lock_ = false; }

  bool resize_locked() const { return resize_locked_; }
  bool compositor_locked() const { return compositor_locked_; }

 private:
  // DelegatedFrameHostClientAura implementation.
  bool DelegatedFrameCanCreateResizeLock() const override {
    return can_create_resize_lock_;
  }

  // CompositorResizeLockClient implemention. Overrides from
  // DelegatedFrameHostClientAura, to prevent the lock from timing out.
  std::unique_ptr<ui::CompositorLock> GetCompositorLock(
      ui::CompositorLockClient* client) override {
    resize_locked_ = compositor_locked_ = true;
    return base::MakeUnique<ui::CompositorLock>(nullptr,
                                                weak_ptr_factory_.GetWeakPtr());
  }
  // CompositorResizeLockClient implemention. Overrides from
  // // DelegatedFrameHostClientAura.
  void CompositorResizeLockEnded() override {
    resize_locked_ = false;
    DelegatedFrameHostClientAura::CompositorResizeLockEnded();
  }

  // ui::CompositorLockDelegate implemention.
  void RemoveCompositorLock(ui::CompositorLock*) override {
    compositor_locked_ = false;
  }

  bool can_create_resize_lock_ = true;
  bool resize_locked_ = false;
  bool compositor_locked_ = false;
  base::WeakPtrFactory<FakeDelegatedFrameHostClientAura> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeDelegatedFrameHostClientAura);
};

class FakeRenderWidgetHostViewAura : public RenderWidgetHostViewAura {
 public:
  FakeRenderWidgetHostViewAura(RenderWidgetHost* widget,
                               bool is_guest_view_hack,
                               bool enable_surface_synchronization)
      : RenderWidgetHostViewAura(widget,
                                 is_guest_view_hack,
                                 enable_surface_synchronization),
        delegated_frame_host_client_(
            new FakeDelegatedFrameHostClientAura(this)) {
    InstallDelegatedFrameHostClient(
        this, base::WrapUnique(delegated_frame_host_client_));
    CreateNewRendererCompositorFrameSink();
  }

  ~FakeRenderWidgetHostViewAura() override {}

  void CreateNewRendererCompositorFrameSink() {
    viz::mojom::CompositorFrameSinkPtr sink;
    viz::mojom::CompositorFrameSinkRequest sink_request =
        mojo::MakeRequest(&sink);
    viz::mojom::CompositorFrameSinkClientRequest client_request =
        mojo::MakeRequest(&renderer_compositor_frame_sink_ptr_);
    renderer_compositor_frame_sink_ =
        base::MakeUnique<FakeRendererCompositorFrameSink>(
            std::move(sink), std::move(client_request));
    DidCreateNewRendererCompositorFrameSink(
        renderer_compositor_frame_sink_ptr_.get());
  }

  void DisableResizeLock() {
    delegated_frame_host_client_->DisableResizeLock();
  }

  void UseFakeDispatcher() {
    dispatcher_ = new FakeWindowEventDispatcher(window()->GetHost());
    std::unique_ptr<aura::WindowEventDispatcher> dispatcher(dispatcher_);
    aura::test::SetHostDispatcher(window()->GetHost(), std::move(dispatcher));
  }

  void RunOnCompositingDidCommit() {
    GetDelegatedFrameHost()->OnCompositingDidCommitForTesting(
        window()->GetHost()->compositor());
  }

  void InterceptCopyOfOutput(std::unique_ptr<viz::CopyOutputRequest> request) {
    last_copy_request_ = std::move(request);
    if (last_copy_request_->has_texture_mailbox()) {
      // Give the resulting texture a size.
      viz::GLHelper* gl_helper =
          ImageTransportFactory::GetInstance()->GetGLHelper();
      GLuint texture = gl_helper->ConsumeMailboxToTexture(
          last_copy_request_->texture_mailbox().mailbox(),
          last_copy_request_->texture_mailbox().sync_token());
      gl_helper->ResizeTexture(texture, window()->bounds().size());
      gl_helper->DeleteTexture(texture);
    }
  }

  viz::SurfaceId surface_id() const {
    return GetDelegatedFrameHost()->SurfaceIdForTesting();
  }

  bool HasPrimarySurface() const {
    return GetDelegatedFrameHost()->HasPrimarySurfaceForTesting();
  }

  bool released_front_lock_active() const {
    return GetDelegatedFrameHost()->ReleasedFrontLockActiveForTesting();
  }

  void ReclaimResources(const std::vector<viz::ReturnedResource>& resources) {
    GetDelegatedFrameHost()->ReclaimResources(resources);
  }

  void ResetCompositor() { GetDelegatedFrameHost()->ResetCompositor(); }

  const ui::MotionEventAura& pointer_state() {
    return event_handler()->pointer_state();
  }

  bool resize_locked() const {
    return delegated_frame_host_client_->resize_locked();
  }
  bool compositor_locked() const {
    return delegated_frame_host_client_->compositor_locked();
  }

  gfx::Size last_frame_size_;
  std::unique_ptr<viz::CopyOutputRequest> last_copy_request_;
  FakeWindowEventDispatcher* dispatcher_;
  std::unique_ptr<FakeRendererCompositorFrameSink>
      renderer_compositor_frame_sink_;

 private:
  FakeDelegatedFrameHostClientAura* delegated_frame_host_client_;
  viz::mojom::CompositorFrameSinkClientPtr renderer_compositor_frame_sink_ptr_;

  DISALLOW_COPY_AND_ASSIGN(FakeRenderWidgetHostViewAura);
};

// A layout manager that always resizes a child to the root window size.
class FullscreenLayoutManager : public aura::LayoutManager {
 public:
  explicit FullscreenLayoutManager(aura::Window* owner) : owner_(owner) {}
  ~FullscreenLayoutManager() override {}

  // Overridden from aura::LayoutManager:
  void OnWindowResized() override {
    aura::Window::Windows::const_iterator i;
    for (i = owner_->children().begin(); i != owner_->children().end(); ++i) {
      (*i)->SetBounds(gfx::Rect());
    }
  }
  void OnWindowAddedToLayout(aura::Window* child) override {
    child->SetBounds(gfx::Rect());
  }
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    SetChildBoundsDirect(child, gfx::Rect(owner_->bounds().size()));
  }

 private:
  aura::Window* owner_;
  DISALLOW_COPY_AND_ASSIGN(FullscreenLayoutManager);
};

class MockWindowObserver : public aura::WindowObserver {
 public:
  MOCK_METHOD2(OnDelegatedFrameDamage, void(aura::Window*, const gfx::Rect&));
};

class MockRenderWidgetHostImpl : public RenderWidgetHostImpl {
 public:
  ~MockRenderWidgetHostImpl() override {}

  // Extracts |latency_info| for wheel event, and stores it in
  // |lastWheelOrTouchEventLatencyInfo|.
  void ForwardWheelEventWithLatencyInfo(
      const blink::WebMouseWheelEvent& wheel_event,
      const ui::LatencyInfo& ui_latency) override {
    RenderWidgetHostImpl::ForwardWheelEventWithLatencyInfo(wheel_event,
                                                           ui_latency);
    lastWheelOrTouchEventLatencyInfo = ui::LatencyInfo(ui_latency);
  }

  // Extracts |latency_info| for touch event, and stores it in
  // |lastWheelOrTouchEventLatencyInfo|.
  void ForwardTouchEventWithLatencyInfo(
      const blink::WebTouchEvent& touch_event,
      const ui::LatencyInfo& ui_latency) override {
    RenderWidgetHostImpl::ForwardTouchEventWithLatencyInfo(touch_event,
                                                           ui_latency);
    lastWheelOrTouchEventLatencyInfo = ui::LatencyInfo(ui_latency);
  }

  static MockRenderWidgetHostImpl* Create(RenderWidgetHostDelegate* delegate,
                                          RenderProcessHost* process,
                                          int32_t routing_id) {
    mojom::WidgetPtr widget;
    std::unique_ptr<MockWidgetImpl> widget_impl =
        base::MakeUnique<MockWidgetImpl>(mojo::MakeRequest(&widget));

    return new MockRenderWidgetHostImpl(delegate, process, routing_id,
                                        std::move(widget_impl),
                                        std::move(widget));
  }
  ui::LatencyInfo lastWheelOrTouchEventLatencyInfo;

 private:
  MockRenderWidgetHostImpl(RenderWidgetHostDelegate* delegate,
                           RenderProcessHost* process,
                           int32_t routing_id,
                           std::unique_ptr<MockWidgetImpl> widget_impl,
                           mojom::WidgetPtr widget)
      : RenderWidgetHostImpl(delegate,
                             process,
                             routing_id,
                             std::move(widget),
                             false),
        widget_impl_(std::move(widget_impl)) {
    lastWheelOrTouchEventLatencyInfo = ui::LatencyInfo();
  }

  std::unique_ptr<MockWidgetImpl> widget_impl_;
};

const WebInputEvent* GetInputEventFromMessage(const IPC::Message& message) {
  base::PickleIterator iter(message);
  const char* data;
  int data_length;
  if (!iter.ReadData(&data, &data_length))
    return nullptr;
  return reinterpret_cast<const WebInputEvent*>(data);
}

enum WheelScrollingMode {
  kWheelScrollingModeNone,
  kWheelScrollLatching,
  kAsyncWheelEvents,
};

}  // namespace

class RenderWidgetHostViewAuraTest : public testing::Test {
 public:
  RenderWidgetHostViewAuraTest()
      : widget_host_uses_shutdown_to_destroy_(false),
        is_guest_view_hack_(false) {}

  static void InstallDelegatedFrameHostClient(
      RenderWidgetHostViewAura* render_widget_host_view,
      std::unique_ptr<DelegatedFrameHostClient> delegated_frame_host_client) {
    render_widget_host_view->delegated_frame_host_client_ =
        std::move(delegated_frame_host_client);
  }

  void SetUpEnvironment(bool enable_surface_synchronization) {
    ImageTransportFactory::SetFactory(
        std::make_unique<NoTransportImageTransportFactory>());
    aura_test_helper_.reset(new aura::test::AuraTestHelper());
    aura_test_helper_->SetUp(
        ImageTransportFactory::GetInstance()->GetContextFactory(),
        ImageTransportFactory::GetInstance()->GetContextFactoryPrivate());
    new wm::DefaultActivationClient(aura_test_helper_->root_window());

    browser_context_.reset(new TestBrowserContext);
    process_host_ = new MockRenderProcessHost(browser_context_.get());
    process_host_->Init();

    sink_ = &process_host_->sink();

    int32_t routing_id = process_host_->GetNextRoutingID();
    delegates_.push_back(base::WrapUnique(new MockRenderWidgetHostDelegate));
    parent_host_ = MockRenderWidgetHostImpl::Create(delegates_.back().get(),
                                                    process_host_, routing_id);
    delegates_.back()->set_widget_host(parent_host_);
    parent_view_ = new RenderWidgetHostViewAura(
        parent_host_, is_guest_view_hack_, enable_surface_synchronization);
    parent_view_->InitAsChild(nullptr);
    aura::client::ParentWindowWithContext(parent_view_->GetNativeView(),
                                          aura_test_helper_->root_window(),
                                          gfx::Rect());

    routing_id = process_host_->GetNextRoutingID();
    delegates_.push_back(base::WrapUnique(new MockRenderWidgetHostDelegate));
    widget_host_ = MockRenderWidgetHostImpl::Create(delegates_.back().get(),
                                                    process_host_, routing_id);
    delegates_.back()->set_widget_host(widget_host_);
    widget_host_->Init();
    view_ = new FakeRenderWidgetHostViewAura(widget_host_, is_guest_view_hack_,
                                             enable_surface_synchronization);
  }

  void TearDownEnvironment() {
    sink_ = nullptr;
    process_host_ = nullptr;
    if (view_) {
      // For guest-views, |view_| is not the view used by |widget_host_|.
      if (!is_guest_view_hack_) {
        EXPECT_EQ(view_, widget_host_->GetView());
      }
      view_->Destroy();
      if (!is_guest_view_hack_) {
        EXPECT_EQ(nullptr, widget_host_->GetView());
      }
    }

    if (widget_host_uses_shutdown_to_destroy_)
      widget_host_->ShutdownAndDestroyWidget(true);
    else
      delete widget_host_;

    parent_view_->Destroy();
    delete parent_host_;

    browser_context_.reset();
    aura_test_helper_->TearDown();

    base::RunLoop().RunUntilIdle();
    ImageTransportFactory::Terminate();
  }

  void SetUp() override {
    SetUpEnvironment(false /* enable_surface_synchronization */);
  }

  void TearDown() override { TearDownEnvironment(); }

  void set_widget_host_uses_shutdown_to_destroy(bool use) {
    widget_host_uses_shutdown_to_destroy_ = use;
  }

  void SimulateMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level) {
    // Here should be base::MemoryPressureListener::NotifyMemoryPressure, but
    // since the FrameEvictionManager is installing a MemoryPressureListener
    // which uses base::ObserverListThreadSafe, which furthermore remembers the
    // message loop for the thread it was created in. Between tests, the
    // FrameEvictionManager singleton survives and and the MessageLoop gets
    // destroyed. The correct fix would be to have base::ObserverListThreadSafe
    // look
    // up the proper message loop every time (see crbug.com/443824.)
    FrameEvictionManager::GetInstance()->OnMemoryPressure(level);
  }

  void SendInputEventACK(WebInputEvent::Type type,
      InputEventAckState ack_result) {
    DCHECK(!WebInputEvent::IsTouchEventType(type));
    InputEventAck ack(InputEventAckSource::COMPOSITOR_THREAD, type, ack_result);
    InputHostMsg_HandleInputEvent_ACK response(0, ack);
    widget_host_->OnMessageReceived(response);
  }

  void SendTouchEventACK(WebInputEvent::Type type,
                         InputEventAckState ack_result,
                         uint32_t event_id) {
    DCHECK(WebInputEvent::IsTouchEventType(type));
    InputEventAck ack(InputEventAckSource::COMPOSITOR_THREAD, type, ack_result,
                      event_id);
    InputHostMsg_HandleInputEvent_ACK response(0, ack);
    widget_host_->OnMessageReceived(response);
  }

  std::string GetInputMessageTypes() {
    return content::GetInputMessageTypes(process_host_);
  }

  size_t GetSentMessageCountAndResetSink() {
    size_t count = sink_->message_count();
    sink_->ClearMessages();
    return count;
  }

  void AckLastSentInputEventIfNecessary(InputEventAckState ack_result) {
    if (!sink_->message_count())
      return;

    InputMsg_HandleInputEvent::Param params;
    if (!InputMsg_HandleInputEvent::Read(
            sink_->GetMessageAt(sink_->message_count() - 1), &params)) {
      return;
    }

    InputEventDispatchType dispatch_type = std::get<3>(params);
    if (dispatch_type == InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING)
      return;

    const blink::WebInputEvent* event = std::get<0>(params);
    SendTouchEventACK(event->GetType(), ack_result,
                      WebInputEventTraits::GetUniqueTouchEventId(*event));
  }

  const ui::MotionEventAura& pointer_state() { return view_->pointer_state(); }

  void EnableRafAlignedTouchInput() {
    feature_list_.InitFromCommandLine(
        features::kRafAlignedTouchInputEvents.name, "");
  }

  void DisableRafAlignedTouchInput() {
    feature_list_.InitFromCommandLine(
        "", features::kRafAlignedTouchInputEvents.name);
  }

  void EnableWheelScrollLatching() {
    feature_list_.InitFromCommandLine(
        features::kTouchpadAndWheelScrollLatching.name,
        features::kMojoInputMessages.name);
  }

  void DisableWheelScrollLatching() {
    feature_list_.InitFromCommandLine(
        "", features::kTouchpadAndWheelScrollLatching.name);
  }

  void SetFeatureList(
      bool raf_aligned_touch,
      WheelScrollingMode wheel_scrolling_mode = kWheelScrollLatching) {
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

    vsync_feature_list_.InitAndEnableFeature(
        features::kVsyncAlignedInputEvents);
  }

 protected:
  BrowserContext* browser_context() { return browser_context_.get(); }

  MockRenderWidgetHostDelegate* render_widget_host_delegate() const {
    return delegates_.back().get();
  }

  // Sets the |view| active in TextInputManager with the given |type|. |type|
  // cannot be ui::TEXT_INPUT_TYPE_NONE.
  // Must not be called in the destruction path of |view|.
  void ActivateViewForTextInputManager(RenderWidgetHostViewBase* view,
                                       ui::TextInputType type) {
    DCHECK_NE(ui::TEXT_INPUT_TYPE_NONE, type);
    // First mock-focus the widget if not already.
    if (render_widget_host_delegate()->GetFocusedRenderWidgetHost(
            widget_host_) != view->GetRenderWidgetHost()) {
      render_widget_host_delegate()->set_focused_widget(
          RenderWidgetHostImpl::From(view->GetRenderWidgetHost()));
    }

    TextInputManager* manager =
        static_cast<RenderWidgetHostImpl*>(view->GetRenderWidgetHost())
            ->delegate()
            ->GetTextInputManager();
    if (manager->GetActiveWidget()) {
      manager->active_view_for_testing()->TextInputStateChanged(
          TextInputState());
    }

    if (!view)
      return;

    TextInputState state_with_type_text;
    state_with_type_text.type = type;
    view->TextInputStateChanged(state_with_type_text);
  }

  // If true, then calls RWH::Shutdown() instead of deleting RWH.
  bool widget_host_uses_shutdown_to_destroy_;

  bool is_guest_view_hack_;

  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  std::unique_ptr<BrowserContext> browser_context_;
  std::vector<std::unique_ptr<MockRenderWidgetHostDelegate>> delegates_;
  MockRenderProcessHost* process_host_;

  // Tests should set these to nullptr if they've already triggered their
  // destruction.
  RenderWidgetHostImpl* parent_host_;
  RenderWidgetHostViewAura* parent_view_;

  // Tests should set these to nullptr if they've already triggered their
  // destruction.
  MockRenderWidgetHostImpl* widget_host_;
  FakeRenderWidgetHostViewAura* view_;

  IPC::TestSink* sink_;
  base::test::ScopedFeatureList vsync_feature_list_;
  base::test::ScopedFeatureList feature_list_;

  viz::LocalSurfaceIdAllocator local_surface_id_allocator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAuraTest);
};

class RenderWidgetHostViewAuraRafAlignedTouchEnabledTest
    : public RenderWidgetHostViewAuraTest {
 public:
  void SetUp() override {
    EnableRafAlignedTouchInput();
    RenderWidgetHostViewAuraTest::SetUp();
  }
};

class RenderWidgetHostViewAuraSurfaceSynchronizationTest
    : public RenderWidgetHostViewAuraTest {
  void SetUp() override {
    SetUpEnvironment(true /* enable_surface_synchronization */);
  }
};

class RenderWidgetHostViewAuraRafAlignedTouchDisabledTest
    : public RenderWidgetHostViewAuraTest {
 public:
  void SetUp() override {
    DisableRafAlignedTouchInput();
    RenderWidgetHostViewAuraTest::SetUp();
  }
};

class RenderWidgetHostViewAuraWheelScrollLatchingEnabledTest
    : public RenderWidgetHostViewAuraTest {
 public:
  void SetUp() override {
    EnableWheelScrollLatching();
    ui::GestureConfiguration::GetInstance()->set_scroll_debounce_interval_in_ms(
        0);
    RenderWidgetHostViewAuraTest::SetUp();
  }
};

void InstallDelegatedFrameHostClient(
    RenderWidgetHostViewAura* render_widget_host_view,
    std::unique_ptr<DelegatedFrameHostClient> delegated_frame_host_client) {
  RenderWidgetHostViewAuraTest::InstallDelegatedFrameHostClient(
      render_widget_host_view, std::move(delegated_frame_host_client));
}

// Helper class to instantiate RenderWidgetHostViewGuest which is backed
// by an aura platform view.
class RenderWidgetHostViewGuestAuraTest : public RenderWidgetHostViewAuraTest {
 public:
  RenderWidgetHostViewGuestAuraTest() {
    // Use RWH::Shutdown to destroy RWH, instead of deleting.
    // This will ensure that the RenderWidgetHostViewGuest is not leaked and
    // is deleted properly upon RWH going away.
    set_widget_host_uses_shutdown_to_destroy(true);
  }

  // We explicitly invoke SetUp to allow gesture debounce customization.
  void SetUp() override {
    is_guest_view_hack_ = true;

    RenderWidgetHostViewAuraTest::SetUp();

    guest_view_weak_ = (RenderWidgetHostViewGuest::Create(widget_host_, nullptr,
                                                          view_->GetWeakPtr()))
                           ->GetWeakPtr();
  }

  void TearDown() override {
    // Internal override to do nothing, we clean up ourselves in the test body.
    // This helps us test that |guest_view_weak_| does not leak.
  }

 protected:
  base::WeakPtr<RenderWidgetHostViewBase> guest_view_weak_;

 private:

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewGuestAuraTest);
};

class RenderWidgetHostViewAuraOverscrollTest
    : public RenderWidgetHostViewAuraTest {
 public:
  RenderWidgetHostViewAuraOverscrollTest(
      WheelScrollingMode wheel_scrolling_mode = kWheelScrollLatching)
      : wheel_scroll_latching_enabled_(wheel_scrolling_mode !=
                                       kWheelScrollingModeNone),
        wheel_scrolling_mode_(wheel_scrolling_mode) {}

  // We explicitly invoke SetUp to allow gesture debounce customization.
  void SetUp() override {}

  void SendScrollBeginAckIfNeeded(InputEventAckState ack_result) {
    if (wheel_scroll_latching_enabled_) {
      // GSB events are blocking, send the ack.
      SendInputEventACK(WebInputEvent::kGestureScrollBegin, ack_result);
    }
  }

 protected:
  void SetUpOverscrollEnvironmentWithDebounce(int debounce_interval_in_ms) {
    SetUpOverscrollEnvironmentImpl(debounce_interval_in_ms);
  }

  void SetUpOverscrollEnvironment() { SetUpOverscrollEnvironmentImpl(0); }

  void SetUpOverscrollEnvironmentImpl(int debounce_interval_in_ms) {
    SetFeatureList(true, wheel_scrolling_mode_);
    ui::GestureConfiguration::GetInstance()->set_scroll_debounce_interval_in_ms(
        debounce_interval_in_ms);

    RenderWidgetHostViewAuraTest::SetUp();

    view_->SetOverscrollControllerEnabled(true);
    overscroll_delegate_.reset(new TestOverscrollDelegate(view_));
    view_->overscroll_controller()->set_delegate(overscroll_delegate_.get());

    view_->InitAsChild(nullptr);
    view_->SetBounds(gfx::Rect(0, 0, 400, 200));
    view_->Show();

    sink_->ClearMessages();
  }

  // TODO(jdduke): Simulate ui::Events, injecting through the view.
  void SimulateMouseEvent(WebInputEvent::Type type) {
    widget_host_->ForwardMouseEvent(SyntheticWebMouseEventBuilder::Build(type));
  }

  void SimulateMouseEventWithLatencyInfo(WebInputEvent::Type type,
                                         const ui::LatencyInfo& ui_latency) {
    widget_host_->ForwardMouseEventWithLatencyInfo(
        SyntheticWebMouseEventBuilder::Build(type), ui_latency);
  }

  void SimulateWheelEvent(float dX, float dY, int modifiers, bool precise) {
    widget_host_->ForwardWheelEvent(SyntheticWebMouseWheelEventBuilder::Build(
        0, 0, dX, dY, modifiers, precise));
  }

  void SimulateWheelEventWithPhase(float dX,
                                   float dY,
                                   int modifiers,
                                   bool precise,
                                   WebMouseWheelEvent::Phase phase) {
    WebMouseWheelEvent wheel_event = SyntheticWebMouseWheelEventBuilder::Build(
        0, 0, dX, dY, modifiers, precise);
    wheel_event.phase = phase;
    widget_host_->ForwardWheelEvent(wheel_event);
  }

  void SimulateWheelEventPossiblyIncludingPhase(
      float dX,
      float dY,
      int modifiers,
      bool precise,
      WebMouseWheelEvent::Phase phase) {
    if (wheel_scroll_latching_enabled_)
      SimulateWheelEventWithPhase(dX, dY, modifiers, precise, phase);
    else
      SimulateWheelEvent(dX, dY, modifiers, precise);
  }

  void SimulateWheelEventWithLatencyInfo(float dX,
                                         float dY,
                                         int modifiers,
                                         bool precise,
                                         const ui::LatencyInfo& ui_latency) {
    widget_host_->ForwardWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(0, 0, dX, dY, modifiers,
                                                  precise),
        ui_latency);
  }

  void SimulateMouseMove(int x, int y, int modifiers) {
    SimulateMouseEvent(WebInputEvent::kMouseMove, x, y, modifiers, false);
  }

  void SimulateMouseEvent(WebInputEvent::Type type,
                          int x,
                          int y,
                          int modifiers,
                          bool pressed) {
    WebMouseEvent event =
        SyntheticWebMouseEventBuilder::Build(type, x, y, modifiers);
    if (pressed)
      event.button = WebMouseEvent::Button::kLeft;
    widget_host_->ForwardMouseEvent(event);
  }

  void SimulateWheelEventWithPhase(WebMouseWheelEvent::Phase phase) {
    widget_host_->ForwardWheelEvent(
        SyntheticWebMouseWheelEventBuilder::Build(phase));
  }

  // Inject provided synthetic WebGestureEvent instance.
  void SimulateGestureEventCore(const WebGestureEvent& gesture_event) {
    widget_host_->ForwardGestureEvent(gesture_event);
  }

  void SimulateGestureEventCoreWithLatencyInfo(
      const WebGestureEvent& gesture_event,
      const ui::LatencyInfo& ui_latency) {
    widget_host_->ForwardGestureEventWithLatencyInfo(gesture_event, ui_latency);
  }

  // Inject simple synthetic WebGestureEvent instances.
  void SimulateGestureEvent(WebInputEvent::Type type,
                            blink::WebGestureDevice sourceDevice) {
    SimulateGestureEventCore(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
  }

  void SimulateGestureEventWithLatencyInfo(WebInputEvent::Type type,
                                           blink::WebGestureDevice sourceDevice,
                                           const ui::LatencyInfo& ui_latency) {
    SimulateGestureEventCoreWithLatencyInfo(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice), ui_latency);
  }

  void SimulateGestureScrollUpdateEvent(float dX, float dY, int modifiers) {
    SimulateGestureEventCore(SyntheticWebGestureEventBuilder::BuildScrollUpdate(
        dX, dY, modifiers, blink::kWebGestureDeviceTouchscreen));
  }

  void SimulateGesturePinchUpdateEvent(float scale,
                                       float anchorX,
                                       float anchorY,
                                       int modifiers) {
    SimulateGestureEventCore(SyntheticWebGestureEventBuilder::BuildPinchUpdate(
        scale, anchorX, anchorY, modifiers,
        blink::kWebGestureDeviceTouchscreen));
  }

  // Inject synthetic GestureFlingStart events.
  void SimulateGestureFlingStartEvent(float velocityX,
                                      float velocityY,
                                      blink::WebGestureDevice sourceDevice) {
    SimulateGestureEventCore(SyntheticWebGestureEventBuilder::BuildFling(
        velocityX, velocityY, sourceDevice));
  }

  bool ScrollStateIsContentScrolling() const {
    return scroll_state() == OverscrollController::STATE_CONTENT_SCROLLING;
  }

  bool ScrollStateIsOverscrolling() const {
    return scroll_state() == OverscrollController::STATE_OVERSCROLLING;
  }

  bool ScrollStateIsUnknown() const {
    return scroll_state() == OverscrollController::STATE_UNKNOWN;
  }

  OverscrollController::ScrollState scroll_state() const {
    return view_->overscroll_controller()->scroll_state_;
  }

  OverscrollMode overscroll_mode() const {
    return view_->overscroll_controller()->overscroll_mode_;
  }

  OverscrollSource overscroll_source() const {
    return view_->overscroll_controller()->overscroll_source_;
  }

  float overscroll_delta_x() const {
    return view_->overscroll_controller()->overscroll_delta_x_;
  }

  float overscroll_delta_y() const {
    return view_->overscroll_controller()->overscroll_delta_y_;
  }

  TestOverscrollDelegate* overscroll_delegate() {
    return overscroll_delegate_.get();
  }

  uint32_t SendTouchEvent() {
    uint32_t touch_event_id = touch_event_.unique_touch_event_id;
    widget_host_->ForwardTouchEventWithLatencyInfo(touch_event_,
                                                   ui::LatencyInfo());
    touch_event_.ResetPoints();
    return touch_event_id;
  }

  void PressTouchPoint(int x, int y) {
    touch_event_.PressPoint(x, y);
  }

  void MoveTouchPoint(int index, int x, int y) {
    touch_event_.MovePoint(index, x, y);
  }

  void ReleaseTouchPoint(int index) {
    touch_event_.ReleasePoint(index);
  }

  void ExpectGestureScrollEndForWheelScrolling(bool is_last) {
    if (!wheel_scroll_latching_enabled_) {
      // Already handled in |ExpectGestureScrollEventsAfterMouseWheelACK()|.
      EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
      return;
    }
    if (is_last) {
      // Scroll latching will have one GestureScrollEnd at the end.
      EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes());
    }
    // No GestureScrollEnd during the scroll.
    EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  }

  void ExpectGestureScrollEventsAfterMouseWheelACK(
      bool is_first_ack,
      size_t enqueued_wheel_event_count) {
    std::string expected_events;
    if (wheel_scrolling_mode_ == kAsyncWheelEvents) {
      ASSERT_TRUE(wheel_scroll_latching_enabled_);
      // If the ack for the first sent event is not consumed,
      // MouseWheelEventQueue(MWEQ) sends the rest of the wheel events in the
      // current scrolling sequence as non-blocking events. Since MWEQ
      // receives the ack for non-blocking events asynchronously, it sends the
      // next queued wheel event immediately and this continues till the queue
      // is empty.
      // Expecting a GSB+GSU for ACKing the first MouseWheel, plus an additional
      // MouseWheel+GSU per enqueued wheel event. Note that GestureEventQueue
      // allows multiple in-flight events.
      if (is_first_ack)
        expected_events += "GestureScrollBegin GestureScrollUpdate ";
      for (size_t i = 0; i < enqueued_wheel_event_count; ++i)
        expected_events += "MouseWheel GestureScrollUpdate ";
    } else if (wheel_scrolling_mode_ == kWheelScrollLatching) {
      ASSERT_TRUE(wheel_scroll_latching_enabled_);
      // Since the MWEQ must wait for ack of the sent event before sending the
      // next queued event, when wheel events are blocking only one queued
      // event will be sent regardless of the number of the queued wheel
      // events.
      // Expecting a GSU or GSB+GSU for ACKing the previous MouseWheel, plus an
      // additional MouseWheel if the queue is not empty. GSE will be delayed by
      // scroll latching.
      if (is_first_ack)
        expected_events += "GestureScrollBegin ";
      expected_events += "GestureScrollUpdate ";
      if (enqueued_wheel_event_count != 0)
        expected_events += "MouseWheel";
    } else {
      // The MWEQ must wait for ack of the sent event before sending the
      // next queued event.
      // Expecting a GSB+GSU+GSE for ACKing the previous MouseWheel, plus an
      // additional MouseWheel if the queue is not empty.
      expected_events +=
          "GestureScrollBegin GestureScrollUpdate GestureScrollEnd ";
      if (enqueued_wheel_event_count != 0)
        expected_events += "MouseWheel";
    }

    EXPECT_EQ(base::TrimWhitespaceASCII(expected_events, base::TRIM_TRAILING),
              GetInputMessageTypes());
  }

  void ExpectGestureScrollUpdateAfterNonBlockingMouseWheelACK(
      bool wheel_was_queued) {
    size_t gesture_scroll_update_index;
    InputMsg_HandleInputEvent::Param params;
    if (wheel_was_queued) {
      // The queued wheel event is already sent.
      gesture_scroll_update_index = 0;
    } else {
      // The first sent must be the wheel event and the second one must be
      // GestureScrollUpdate since the ack for the wheel event is non-blocking.
      if (InputMsg_HandleInputEvent::Read(sink_->GetMessageAt(0), &params)) {
        EXPECT_EQ(WebInputEvent::kMouseWheel, (std::get<0>(params))->GetType());
      }
      gesture_scroll_update_index = 1;
    }
    if (InputMsg_HandleInputEvent::Read(
            sink_->GetMessageAt(gesture_scroll_update_index), &params)) {
      EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
                (std::get<0>(params))->GetType());
    }
    EXPECT_EQ(gesture_scroll_update_index + 1,
              GetSentMessageCountAndResetSink());
  }

  void ExpectGestureScrollEventsAfterMouseWheelAckWhenOverscrolling() {
    // Wheel event ack either:
    //  - does not generate a gesture scroll update (async wheel event); or
    //  - generates a gesture scroll update that is consumed by the overscroll
    //    controller to prevent content scroll (non-async wheel event).
    if (wheel_scroll_latching_enabled_) {
      EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
    } else {
      // Wheel event ack generates a gesture scroll begin, a gesture scroll
      // update, and a gesture scroll end; of which the gesture scroll update is
      // consumed by the overscroll controller to prevent content scroll.
      EXPECT_EQ("GestureScrollBegin GestureScrollEnd", GetInputMessageTypes());
    }
  }

  void WheelNotPreciseScrollEvent();
  void WheelScrollOverscrollToggle();
  void OverscrollMouseMoveCompletion();
  void WheelScrollEventOverscrolls();
  void WheelScrollConsumedDoNotHorizOverscroll();
  void ScrollEventsOverscrollWithFling();
  void OverscrollDirectionChangeMouseWheel();
  void OverscrollStateResetsAfterScroll();
  void ScrollDeltasResetOnEnd();
  void ScrollEventsOverscrollWithZeroFling();

  SyntheticWebTouchEvent touch_event_;

  std::unique_ptr<TestOverscrollDelegate> overscroll_delegate_;

  bool wheel_scroll_latching_enabled_;
  WheelScrollingMode wheel_scrolling_mode_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAuraOverscrollTest);
};

class RenderWidgetHostViewAuraOverscrollWithoutWheelScrollLatchingTest
    : public RenderWidgetHostViewAuraOverscrollTest {
 public:
  RenderWidgetHostViewAuraOverscrollWithoutWheelScrollLatchingTest()
      : RenderWidgetHostViewAuraOverscrollTest(kWheelScrollingModeNone) {}
};

class RenderWidgetHostViewAuraOverScrollAsyncWheelEventsEnabledTest
    : public RenderWidgetHostViewAuraOverscrollTest {
 public:
  RenderWidgetHostViewAuraOverScrollAsyncWheelEventsEnabledTest()
      : RenderWidgetHostViewAuraOverscrollTest(kAsyncWheelEvents) {}
};

class RenderWidgetHostViewAuraShutdownTest
    : public RenderWidgetHostViewAuraTest {
 public:
  RenderWidgetHostViewAuraShutdownTest() {}

  void TearDown() override {
    // No TearDownEnvironment here, we do this explicitly during the test.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAuraShutdownTest);
};

// Checks that RenderWidgetHostViewAura can be destroyed before it is properly
// initialized.
TEST_F(RenderWidgetHostViewAuraTest, DestructionBeforeProperInitialization) {
  // Terminate the test without initializing |view_|.
}

// Checks that a fullscreen view has the correct show-state and receives the
// focus.
TEST_F(RenderWidgetHostViewAuraTest, FocusFullscreen) {
  view_->InitAsFullscreen(parent_view_);
  aura::Window* window = view_->GetNativeView();
  ASSERT_TRUE(window != nullptr);
  EXPECT_EQ(ui::SHOW_STATE_FULLSCREEN,
            window->GetProperty(aura::client::kShowStateKey));

  // Check that we requested and received the focus.
  EXPECT_TRUE(window->HasFocus());

  // Check that we'll also say it's okay to activate the window when there's an
  // ActivationClient defined.
  EXPECT_TRUE(view_->ShouldActivate());
}

// Checks that a popup is positioned correctly relative to its parent using
// screen coordinates.
TEST_F(RenderWidgetHostViewAuraTest, PositionChildPopup) {
  wm::DefaultScreenPositionClient screen_position_client;

  aura::Window* window = parent_view_->GetNativeView();
  aura::Window* root = window->GetRootWindow();
  aura::client::SetScreenPositionClient(root, &screen_position_client);

  parent_view_->SetBounds(gfx::Rect(10, 10, 800, 600));
  gfx::Rect bounds_in_screen = parent_view_->GetViewBounds();
  int horiz = bounds_in_screen.width() / 4;
  int vert = bounds_in_screen.height() / 4;
  bounds_in_screen.Inset(horiz, vert);

  // Verify that when the popup is initialized for the first time, it correctly
  // treats the input bounds as screen coordinates.
  view_->InitAsPopup(parent_view_, bounds_in_screen);

  gfx::Rect final_bounds_in_screen = view_->GetViewBounds();
  EXPECT_EQ(final_bounds_in_screen.ToString(), bounds_in_screen.ToString());

  // Verify that directly setting the bounds via SetBounds() treats the input
  // as screen coordinates.
  bounds_in_screen = gfx::Rect(60, 60, 100, 100);
  view_->SetBounds(bounds_in_screen);
  final_bounds_in_screen = view_->GetViewBounds();
  EXPECT_EQ(final_bounds_in_screen.ToString(), bounds_in_screen.ToString());

  // Verify that setting the size does not alter the origin.
  gfx::Point original_origin = window->bounds().origin();
  view_->SetSize(gfx::Size(120, 120));
  gfx::Point new_origin = window->bounds().origin();
  EXPECT_EQ(original_origin.ToString(), new_origin.ToString());

  aura::client::SetScreenPositionClient(root, nullptr);
}

// Checks that moving parent sends new screen bounds.
TEST_F(RenderWidgetHostViewAuraTest, ParentMovementUpdatesScreenRect) {
  view_->InitAsChild(nullptr);

  aura::Window* root = parent_view_->GetNativeView()->GetRootWindow();

  aura::test::TestWindowDelegate delegate1, delegate2;
  std::unique_ptr<aura::Window> parent1(new aura::Window(&delegate1));
  parent1->Init(ui::LAYER_TEXTURED);
  parent1->Show();
  std::unique_ptr<aura::Window> parent2(new aura::Window(&delegate2));
  parent2->Init(ui::LAYER_TEXTURED);
  parent2->Show();

  root->AddChild(parent1.get());
  parent1->AddChild(parent2.get());
  parent2->AddChild(view_->GetNativeView());

  root->SetBounds(gfx::Rect(0, 0, 800, 600));
  parent1->SetBounds(gfx::Rect(1, 1, 300, 300));
  parent2->SetBounds(gfx::Rect(2, 2, 200, 200));
  view_->SetBounds(gfx::Rect(3, 3, 100, 100));
  // view_ will be destroyed when parent is destroyed.
  view_ = nullptr;

  // Flush the state after initial setup is done.
  widget_host_->OnMessageReceived(
      ViewHostMsg_UpdateScreenRects_ACK(widget_host_->GetRoutingID()));
  widget_host_->OnMessageReceived(
      ViewHostMsg_UpdateScreenRects_ACK(widget_host_->GetRoutingID()));
  sink_->ClearMessages();

  // Move parents.
  parent2->SetBounds(gfx::Rect(20, 20, 200, 200));
  ASSERT_EQ(1U, sink_->message_count());
  const IPC::Message* msg = sink_->GetMessageAt(0);
  ASSERT_EQ(ViewMsg_UpdateScreenRects::ID, msg->type());
  ViewMsg_UpdateScreenRects::Param params;
  ViewMsg_UpdateScreenRects::Read(msg, &params);
  EXPECT_EQ(gfx::Rect(24, 24, 100, 100), std::get<0>(params));
  EXPECT_EQ(gfx::Rect(1, 1, 300, 300), std::get<1>(params));
  sink_->ClearMessages();
  widget_host_->OnMessageReceived(
      ViewHostMsg_UpdateScreenRects_ACK(widget_host_->GetRoutingID()));
  // There should not be any pending update.
  EXPECT_EQ(0U, sink_->message_count());

  parent1->SetBounds(gfx::Rect(10, 10, 300, 300));
  ASSERT_EQ(1U, sink_->message_count());
  msg = sink_->GetMessageAt(0);
  ASSERT_EQ(ViewMsg_UpdateScreenRects::ID, msg->type());
  ViewMsg_UpdateScreenRects::Read(msg, &params);
  EXPECT_EQ(gfx::Rect(33, 33, 100, 100), std::get<0>(params));
  EXPECT_EQ(gfx::Rect(10, 10, 300, 300), std::get<1>(params));
  sink_->ClearMessages();
  widget_host_->OnMessageReceived(
      ViewHostMsg_UpdateScreenRects_ACK(widget_host_->GetRoutingID()));
  // There should not be any pending update.
  EXPECT_EQ(0U, sink_->message_count());
}

// Checks that a fullscreen view is destroyed when it loses the focus.
TEST_F(RenderWidgetHostViewAuraTest, DestroyFullscreenOnBlur) {
  view_->InitAsFullscreen(parent_view_);
  aura::Window* window = view_->GetNativeView();
  ASSERT_TRUE(window != nullptr);
  ASSERT_TRUE(window->HasFocus());

  // After we create and focus another window, the RWHVA's window should be
  // destroyed.
  TestWindowObserver observer(window);
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> sibling(new aura::Window(&delegate));
  sibling->Init(ui::LAYER_TEXTURED);
  sibling->Show();
  window->parent()->AddChild(sibling.get());
  sibling->Focus();
  ASSERT_TRUE(sibling->HasFocus());
  ASSERT_TRUE(observer.destroyed());

  widget_host_ = nullptr;
  view_ = nullptr;
}

// Checks that a popup view is destroyed when a user clicks outside of the popup
// view and focus does not change. This is the case when the user clicks on the
// desktop background on Chrome OS.
TEST_F(RenderWidgetHostViewAuraTest, DestroyPopupClickOutsidePopup) {
  parent_view_->SetBounds(gfx::Rect(10, 10, 400, 400));
  parent_view_->Focus();
  EXPECT_TRUE(parent_view_->HasFocus());

  view_->InitAsPopup(parent_view_, gfx::Rect(10, 10, 100, 100));
  aura::Window* window = view_->GetNativeView();
  ASSERT_TRUE(window != nullptr);

  gfx::Point click_point;
  EXPECT_FALSE(window->GetBoundsInRootWindow().Contains(click_point));
  aura::Window* parent_window = parent_view_->GetNativeView();
  EXPECT_FALSE(parent_window->GetBoundsInRootWindow().Contains(click_point));

  TestWindowObserver observer(window);
  ui::test::EventGenerator generator(window->GetRootWindow(), click_point);
  generator.ClickLeftButton();
  ASSERT_TRUE(parent_view_->HasFocus());
  ASSERT_TRUE(observer.destroyed());

  widget_host_ = nullptr;
  view_ = nullptr;
}

// Checks that a popup view is destroyed when a user taps outside of the popup
// view and focus does not change. This is the case when the user taps the
// desktop background on Chrome OS.
TEST_F(RenderWidgetHostViewAuraTest, DestroyPopupTapOutsidePopup) {
  parent_view_->SetBounds(gfx::Rect(10, 10, 400, 400));
  parent_view_->Focus();
  EXPECT_TRUE(parent_view_->HasFocus());

  view_->InitAsPopup(parent_view_, gfx::Rect(10, 10, 100, 100));
  aura::Window* window = view_->GetNativeView();
  ASSERT_TRUE(window != nullptr);

  gfx::Point tap_point;
  EXPECT_FALSE(window->GetBoundsInRootWindow().Contains(tap_point));
  aura::Window* parent_window = parent_view_->GetNativeView();
  EXPECT_FALSE(parent_window->GetBoundsInRootWindow().Contains(tap_point));

  TestWindowObserver observer(window);
  ui::test::EventGenerator generator(window->GetRootWindow(), tap_point);
  generator.GestureTapAt(tap_point);
  ASSERT_TRUE(parent_view_->HasFocus());
  ASSERT_TRUE(observer.destroyed());

  widget_host_ = nullptr;
  view_ = nullptr;
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)

// On Desktop Linux, select boxes need mouse capture in order to work. Test that
// when a select box is opened via a mouse press that it retains mouse capture
// after the mouse is released.
TEST_F(RenderWidgetHostViewAuraTest, PopupRetainsCaptureAfterMouseRelease) {
  parent_view_->SetBounds(gfx::Rect(10, 10, 400, 400));
  parent_view_->Focus();
  EXPECT_TRUE(parent_view_->HasFocus());

  ui::test::EventGenerator generator(
      parent_view_->GetNativeView()->GetRootWindow(), gfx::Point(300, 300));
  generator.PressLeftButton();

  view_->SetPopupType(blink::kWebPopupTypePage);
  view_->InitAsPopup(parent_view_, gfx::Rect(10, 10, 100, 100));
  ASSERT_TRUE(view_->NeedsMouseCapture());
  aura::Window* window = view_->GetNativeView();
  EXPECT_TRUE(window->HasCapture());

  generator.ReleaseLeftButton();
  EXPECT_TRUE(window->HasCapture());
}
#endif

// Test that select boxes close when their parent window loses focus (e.g. due
// to an alert or system modal dialog).
TEST_F(RenderWidgetHostViewAuraTest, PopupClosesWhenParentLosesFocus) {
  parent_view_->SetBounds(gfx::Rect(10, 10, 400, 400));
  parent_view_->Focus();
  EXPECT_TRUE(parent_view_->HasFocus());

  view_->SetPopupType(blink::kWebPopupTypePage);
  view_->InitAsPopup(parent_view_, gfx::Rect(10, 10, 100, 100));

  aura::Window* popup_window = view_->GetNativeView();
  TestWindowObserver observer(popup_window);

  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> dialog_window(new aura::Window(&delegate));
  dialog_window->Init(ui::LAYER_TEXTURED);
  aura::client::ParentWindowWithContext(
      dialog_window.get(), popup_window, gfx::Rect());
  dialog_window->Show();
  wm::ActivateWindow(dialog_window.get());
  dialog_window->Focus();

  ASSERT_TRUE(wm::IsActiveWindow(dialog_window.get()));
  EXPECT_TRUE(observer.destroyed());

  widget_host_ = nullptr;
  view_ = nullptr;
}

// Checks that IME-composition-event state is maintained correctly.
TEST_F(RenderWidgetHostViewAuraTest, SetCompositionText) {
  view_->InitAsChild(nullptr);
  view_->Show();
  ActivateViewForTextInputManager(view_, ui::TEXT_INPUT_TYPE_TEXT);

  ui::CompositionText composition_text;
  composition_text.text = base::ASCIIToUTF16("|a|b");

  // Focused segment
  composition_text.ime_text_spans.push_back(ui::ImeTextSpan(
      ui::ImeTextSpan::Type::kComposition, 0, 3, 0xff000000, true, 0x78563412));

  // Non-focused segment, with different background color.
  composition_text.ime_text_spans.push_back(
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 3, 4, 0xff000000,
                      false, 0xefcdab90));

  const ui::ImeTextSpans& ime_text_spans = composition_text.ime_text_spans;

  // Caret is at the end. (This emulates Japanese MSIME 2007 and later)
  composition_text.selection = gfx::Range(4);

  sink_->ClearMessages();
  view_->SetCompositionText(composition_text);
  EXPECT_TRUE(view_->has_composition_text_);
  {
    const IPC::Message* msg =
      sink_->GetFirstMessageMatching(InputMsg_ImeSetComposition::ID);
    ASSERT_TRUE(msg != nullptr);

    InputMsg_ImeSetComposition::Param params;
    InputMsg_ImeSetComposition::Read(msg, &params);
    // composition text
    EXPECT_EQ(composition_text.text, std::get<0>(params));
    // ime spans
    ASSERT_EQ(ime_text_spans.size(), std::get<1>(params).size());
    for (size_t i = 0; i < ime_text_spans.size(); ++i) {
      EXPECT_EQ(ime_text_spans[i].start_offset,
                std::get<1>(params)[i].start_offset);
      EXPECT_EQ(ime_text_spans[i].end_offset,
                std::get<1>(params)[i].end_offset);
      EXPECT_EQ(ime_text_spans[i].underline_color,
                std::get<1>(params)[i].underline_color);
      EXPECT_EQ(ime_text_spans[i].thick, std::get<1>(params)[i].thick);
      EXPECT_EQ(ime_text_spans[i].background_color,
                std::get<1>(params)[i].background_color);
    }
    EXPECT_EQ(gfx::Range::InvalidRange(), std::get<2>(params));
    // highlighted range
    EXPECT_EQ(4, std::get<3>(params)) << "Should be the same to the caret pos";
    EXPECT_EQ(4, std::get<4>(params)) << "Should be the same to the caret pos";
  }

  view_->ImeCancelComposition();
  EXPECT_FALSE(view_->has_composition_text_);
}

// Checks that sequence of IME-composition-event and mouse-event when mouse
// clicking to cancel the composition.
TEST_F(RenderWidgetHostViewAuraTest, FinishCompositionByMouse) {
  view_->InitAsChild(nullptr);
  view_->Show();
  ActivateViewForTextInputManager(view_, ui::TEXT_INPUT_TYPE_TEXT);

  ui::CompositionText composition_text;
  composition_text.text = base::ASCIIToUTF16("|a|b");

  // Focused segment
  composition_text.ime_text_spans.push_back(ui::ImeTextSpan(
      ui::ImeTextSpan::Type::kComposition, 0, 3, 0xff000000, true, 0x78563412));

  // Non-focused segment, with different background color.
  composition_text.ime_text_spans.push_back(
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 3, 4, 0xff000000,
                      false, 0xefcdab90));

  // Caret is at the end. (This emulates Japanese MSIME 2007 and later)
  composition_text.selection = gfx::Range(4);

  view_->SetCompositionText(composition_text);
  EXPECT_TRUE(view_->has_composition_text_);
  sink_->ClearMessages();

  // Simulates the mouse press.
  ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             0);
  view_->OnMouseEvent(&mouse_event);

  EXPECT_FALSE(view_->has_composition_text_);

  ASSERT_EQ(2U, sink_->message_count());

  // Verify mouse event happens after the finish composing text event.
  EXPECT_EQ(InputMsg_ImeFinishComposingText::ID,
            sink_->GetMessageAt(0)->type());
  EXPECT_EQ(InputMsg_HandleInputEvent::ID, sink_->GetMessageAt(1)->type());
}

// Checks that WasOcculded/WasUnOccluded notifies RenderWidgetHostImpl.
TEST_F(RenderWidgetHostViewAuraTest, WasOccluded) {
  view_->InitAsChild(nullptr);
  view_->Show();
  EXPECT_FALSE(widget_host_->is_hidden());

  // Verifies WasOccluded sets RenderWidgetHostImpl as hidden and WasUnOccluded
  // resets the state.
  view_->WasOccluded();
  EXPECT_TRUE(widget_host_->is_hidden());
  view_->WasUnOccluded();
  EXPECT_FALSE(widget_host_->is_hidden());

  // Verifies WasOccluded sets RenderWidgetHostImpl as hidden and Show resets
  // the state.
  view_->WasOccluded();
  EXPECT_TRUE(widget_host_->is_hidden());
  view_->Show();
  EXPECT_FALSE(widget_host_->is_hidden());

  // WasOccluded and WasUnOccluded are not in pairs. The last one dictates
  // the final state.
  for (int i = 0; i < 2; ++i) {
    view_->WasOccluded();
    EXPECT_TRUE(widget_host_->is_hidden());
  }
  view_->WasUnOccluded();
  EXPECT_FALSE(widget_host_->is_hidden());

  for (int i = 0; i < 4; ++i) {
    view_->WasUnOccluded();
    EXPECT_FALSE(widget_host_->is_hidden());
  }
  view_->WasOccluded();
  EXPECT_TRUE(widget_host_->is_hidden());
}

// Checks that touch-event state is maintained correctly.
TEST_F(RenderWidgetHostViewAuraRafAlignedTouchDisabledTest, TouchEventState) {
  view_->InitAsChild(nullptr);
  view_->Show();
  GetSentMessageCountAndResetSink();

  // Start with no touch-event handler in the renderer.
  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, false));

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(30, 30), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(20, 20), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(20, 20), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));

  // The touch events should get forwarded from the view, but they should not
  // reach the renderer.
  view_->OnTouchEvent(&press);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_DOWN, pointer_state().GetAction());

  view_->OnTouchEvent(&move);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  view_->OnTouchEvent(&release);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(0U, pointer_state().GetPointerCount());

  // Now install some touch-event handlers and do the same steps. The touch
  // events should now be consumed. However, the touch-event state should be
  // updated as before.
  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));

  view_->OnTouchEvent(&press);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_DOWN, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  view_->OnTouchEvent(&move);
  EXPECT_TRUE(move.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());
  view_->OnTouchEvent(&release);
  EXPECT_TRUE(release.synchronous_handling_disabled());
  EXPECT_EQ(0U, pointer_state().GetPointerCount());

  // Now start a touch event, and remove the event-handlers before the release.
  view_->OnTouchEvent(&press);
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_DOWN, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, false));

  // Ack'ing the outstanding event should flush the pending touch queue.
  InputEventAck ack(
      InputEventAckSource::COMPOSITOR_THREAD, blink::WebInputEvent::kTouchStart,
      INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, press.unique_event_id());
  widget_host_->OnMessageReceived(InputHostMsg_HandleInputEvent_ACK(0, ack));
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(20, 20), base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  view_->OnTouchEvent(&move2);
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  ui::TouchEvent release2(
      ui::ET_TOUCH_RELEASED, gfx::Point(20, 20), base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  view_->OnTouchEvent(&release2);
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(0U, pointer_state().GetPointerCount());
}

// Checks that touch-event state is maintained correctly.
TEST_F(RenderWidgetHostViewAuraRafAlignedTouchEnabledTest, TouchEventState) {
  view_->InitAsChild(nullptr);
  view_->Show();
  GetSentMessageCountAndResetSink();

  // Start with no touch-event handler in the renderer.
  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, false));

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(30, 30), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(20, 20), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(20, 20), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));

  // The touch events should get forwarded from the view, but they should not
  // reach the renderer.
  view_->OnTouchEvent(&press);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_DOWN, pointer_state().GetAction());

  view_->OnTouchEvent(&move);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  view_->OnTouchEvent(&release);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(0U, pointer_state().GetPointerCount());

  // Now install some touch-event handlers and do the same steps. The touch
  // events should now be consumed. However, the touch-event state should be
  // updated as before.
  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));

  view_->OnTouchEvent(&press);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_DOWN, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  view_->OnTouchEvent(&move);
  EXPECT_TRUE(move.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());
  view_->OnTouchEvent(&release);
  EXPECT_TRUE(release.synchronous_handling_disabled());
  EXPECT_EQ(0U, pointer_state().GetPointerCount());

  // Now start a touch event, and remove the event-handlers before the release.
  view_->OnTouchEvent(&press);
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_DOWN, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());
  EXPECT_EQ(3U, GetSentMessageCountAndResetSink());

  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, false));

  // All outstanding events should have already been sent but no new events
  // should get sent.
  InputEventAck ack(
      InputEventAckSource::COMPOSITOR_THREAD, blink::WebInputEvent::kTouchStart,
      INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, press.unique_event_id());
  widget_host_->OnMessageReceived(InputHostMsg_HandleInputEvent_ACK(0, ack));
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(20, 20), base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  view_->OnTouchEvent(&move2);
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  ui::TouchEvent release2(
      ui::ET_TOUCH_RELEASED, gfx::Point(20, 20), base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  view_->OnTouchEvent(&release2);
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(0U, pointer_state().GetPointerCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
}

TEST_F(RenderWidgetHostViewAuraWheelScrollLatchingEnabledTest,
       TimerBasedWheelEventPhaseInfo) {
  view_->InitAsChild(nullptr);
  view_->Show();
  sink_->ClearMessages();

  ui::MouseWheelEvent event(gfx::Vector2d(0, 5), gfx::Point(2, 2),
                            gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&event);
  const WebInputEvent* input_event =
      GetInputEventFromMessage(*sink_->GetMessageAt(0));
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
  SendInputEventACK(blink::WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(1));
  const WebGestureEvent* gesture_event =
      static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(WebInputEvent::kGestureScrollBegin, gesture_event->GetType());
  SendInputEventACK(WebInputEvent::kGestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(2));
  gesture_event = static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, gesture_event->GetType());
  EXPECT_EQ(0U, gesture_event->data.scroll_update.delta_x);
  EXPECT_EQ(5U, gesture_event->data.scroll_update.delta_y);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  sink_->ClearMessages();

  // Send a ui::ScrollEvent instead of ui::MouseWheel event, the timer based
  // phase info doesn't diffrentiate between the two types of events.
  ui::ScrollEvent scroll1(ui::ET_SCROLL, gfx::Point(2, 2),
                          ui::EventTimeForNow(), 0, 0, 2, 0, 2, 2);
  view_->OnScrollEvent(&scroll1);
  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(0));
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  base::TimeTicks wheel_event_timestamp =
      ui::EventTimeStampFromSeconds(wheel_event->TimeStampSeconds());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged, wheel_event->phase);
  SendInputEventACK(blink::WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(1));
  gesture_event = static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, gesture_event->GetType());
  EXPECT_EQ(0U, gesture_event->data.scroll_update.delta_x);
  EXPECT_EQ(2U, gesture_event->data.scroll_update.delta_y);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  sink_->ClearMessages();

  // Let the MouseWheelPhaseHandler::mouse_wheel_end_dispatch_timer_ fire. A
  // synthetic wheel event with zero deltas and kPhaseEnded will be sent.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      base::TimeDelta::FromMilliseconds(
          kDefaultMouseWheelLatchingTransactionMs));
  base::RunLoop().Run();

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(0));
  const WebMouseWheelEvent* wheel_end_event =
      static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_end_event->phase);
  EXPECT_EQ(0U, wheel_end_event->delta_x);
  EXPECT_EQ(0U, wheel_end_event->delta_y);
  EXPECT_EQ(0U, wheel_end_event->wheel_ticks_x);
  EXPECT_EQ(0U, wheel_end_event->wheel_ticks_y);
  EXPECT_GT(ui::EventTimeStampFromSeconds(wheel_end_event->TimeStampSeconds()),
            wheel_event_timestamp);
  SendInputEventACK(blink::WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(1));
  gesture_event = static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd, gesture_event->GetType());
  sink_->ClearMessages();
}

// Tests that a gesture fling start with touchpad source stops the
// RenderWidgetHostViewEventHandler::mouse_wheel_phase_timer_ and no synthetic
// wheel event will be sent.
TEST_F(RenderWidgetHostViewAuraWheelScrollLatchingEnabledTest,
       TouchpadFlingStartStopsWheelPhaseTimer) {
  ui::ScrollEvent scroll0(ui::ET_SCROLL, gfx::Point(2, 2),
                          ui::EventTimeForNow(), 0, 0, 5, 0, 5, 2);
  view_->OnScrollEvent(&scroll0);
  const WebInputEvent* input_event =
      GetInputEventFromMessage(*sink_->GetMessageAt(0));
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
  SendInputEventACK(blink::WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(1));
  const WebGestureEvent* gesture_event =
      static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(WebInputEvent::kGestureScrollBegin, gesture_event->GetType());
  SendInputEventACK(WebInputEvent::kGestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(2));
  gesture_event = static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, gesture_event->GetType());
  EXPECT_EQ(0U, gesture_event->data.scroll_update.delta_x);
  EXPECT_EQ(5U, gesture_event->data.scroll_update.delta_y);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  sink_->ClearMessages();

  ui::ScrollEvent fling_start(ui::ET_SCROLL_FLING_START, gfx::Point(2, 2),
                              ui::EventTimeForNow(), 0, 0, 10, 0, 10, 2);
  view_->OnScrollEvent(&fling_start);
  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(0));
  gesture_event = static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(WebInputEvent::kGestureFlingStart, gesture_event->GetType());
  SendInputEventACK(WebInputEvent::kGestureFlingStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  sink_->ClearMessages();

  // Let the MouseWheelPhaseHandler::mouse_wheel_end_dispatch_timer_ fire. No
  // synthetic wheel event will be sent since the timer has stopped.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      base::TimeDelta::FromMilliseconds(
          kDefaultMouseWheelLatchingTransactionMs));
  base::RunLoop().Run();

  EXPECT_EQ(0U, sink_->message_count());

  // Handling the next ui::ET_SCROLL event will send a fling cancellation and a
  // mouse wheel with kPhaseBegan.
  ui::ScrollEvent scroll1(ui::ET_SCROLL, gfx::Point(2, 2),
                          ui::EventTimeForNow(), 0, 0, 15, 0, 15, 2);
  view_->OnScrollEvent(&scroll1);
  EXPECT_EQ(2U, sink_->message_count());
  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(0));
  gesture_event = static_cast<const WebGestureEvent*>(input_event);

  EXPECT_EQ(WebInputEvent::kGestureFlingCancel, gesture_event->GetType());
  SendInputEventACK(WebInputEvent::kGestureFlingCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(1));
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
  sink_->ClearMessages();
}

TEST_F(RenderWidgetHostViewAuraWheelScrollLatchingEnabledTest,
       GSBWithTouchSourceStopsWheelScrollSequence) {
  ui::ScrollEvent scroll0(ui::ET_SCROLL, gfx::Point(2, 2),
                          ui::EventTimeForNow(), 0, 0, 5, 0, 5, 2);
  view_->OnScrollEvent(&scroll0);
  const WebInputEvent* input_event =
      GetInputEventFromMessage(*sink_->GetMessageAt(0));
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
  SendInputEventACK(blink::WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(1));
  const WebGestureEvent* gesture_event =
      static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(WebInputEvent::kGestureScrollBegin, gesture_event->GetType());
  SendInputEventACK(WebInputEvent::kGestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(2));
  gesture_event = static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, gesture_event->GetType());
  EXPECT_EQ(0U, gesture_event->data.scroll_update.delta_x);
  EXPECT_EQ(5U, gesture_event->data.scroll_update.delta_y);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  sink_->ClearMessages();

  ui::GestureEventDetails event_details(ui::ET_GESTURE_SCROLL_BEGIN);
  event_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent scroll_begin(2, 2, 0, ui::EventTimeForNow(), event_details);
  view_->OnGestureEvent(&scroll_begin);
  EXPECT_EQ(3U, sink_->message_count());

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(0));
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_event->phase);
  EXPECT_EQ(0U, wheel_event->delta_x);
  EXPECT_EQ(0U, wheel_event->delta_y);

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(1));
  gesture_event = static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd, gesture_event->GetType());
  EXPECT_EQ(blink::kWebGestureDeviceTouchpad, gesture_event->source_device);

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(2));
  gesture_event = static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(WebInputEvent::kGestureScrollBegin, gesture_event->GetType());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, gesture_event->source_device);
  sink_->ClearMessages();
}

// Checks that touch-event state is maintained correctly for multiple touch
// points.
TEST_F(RenderWidgetHostViewAuraTest, MultiTouchPointsStates) {
  view_->InitAsFullscreen(parent_view_);
  view_->Show();
  view_->UseFakeDispatcher();
  GetSentMessageCountAndResetSink();

  ui::TouchEvent press0(
      ui::ET_TOUCH_PRESSED, gfx::Point(30, 30), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));

  view_->OnTouchEvent(&press0);
  SendTouchEventACK(blink::WebInputEvent::kTouchStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED, press0.unique_event_id());
  EXPECT_EQ(ui::MotionEvent::ACTION_DOWN, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());
  EXPECT_EQ(1U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());

  ui::TouchEvent move0(
      ui::ET_TOUCH_MOVED, gfx::Point(20, 20), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));

  view_->OnTouchEvent(&move0);
  SendTouchEventACK(blink::WebInputEvent::kTouchMove,
                    INPUT_EVENT_ACK_STATE_CONSUMED, move0.unique_event_id());
  EXPECT_EQ(ui::MotionEvent::ACTION_MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());
  EXPECT_EQ(1U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());

  // For the second touchstart, only the state of the second touch point is
  // StatePressed, the state of the first touch point is StateStationary.
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));

  view_->OnTouchEvent(&press1);
  SendTouchEventACK(blink::WebInputEvent::kTouchStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED, press1.unique_event_id());
  EXPECT_EQ(ui::MotionEvent::ACTION_POINTER_DOWN, pointer_state().GetAction());
  EXPECT_EQ(1, pointer_state().GetActionIndex());
  EXPECT_EQ(2U, pointer_state().GetPointerCount());
  EXPECT_EQ(1U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());

  // For the touchmove of second point, the state of the second touch point is
  // StateMoved, the state of the first touch point is StateStationary.
  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, gfx::Point(30, 30), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));

  view_->OnTouchEvent(&move1);
  SendTouchEventACK(blink::WebInputEvent::kTouchMove,
                    INPUT_EVENT_ACK_STATE_CONSUMED, move1.unique_event_id());
  EXPECT_EQ(ui::MotionEvent::ACTION_MOVE, pointer_state().GetAction());
  EXPECT_EQ(2U, pointer_state().GetPointerCount());
  EXPECT_EQ(1U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());

  // For the touchmove of first point, the state of the first touch point is
  // StateMoved, the state of the second touch point is StateStationary.
  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));

  view_->OnTouchEvent(&move2);
  SendTouchEventACK(blink::WebInputEvent::kTouchMove,
                    INPUT_EVENT_ACK_STATE_CONSUMED, move2.unique_event_id());
  EXPECT_EQ(ui::MotionEvent::ACTION_MOVE, pointer_state().GetAction());
  EXPECT_EQ(2U, pointer_state().GetPointerCount());
  EXPECT_EQ(1U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());

  ui::TouchEvent cancel0(
      ui::ET_TOUCH_CANCELLED, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));

  // For the touchcancel, only the state of the current touch point is
  // StateCancelled, the state of the other touch point is StateStationary.
  view_->OnTouchEvent(&cancel0);
  EXPECT_EQ(1U, pointer_state().GetPointerCount());
  EXPECT_EQ(1U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());

  ui::TouchEvent cancel1(
      ui::ET_TOUCH_CANCELLED, gfx::Point(30, 30), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));

  view_->OnTouchEvent(&cancel1);
  EXPECT_EQ(1U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());
  EXPECT_EQ(0U, pointer_state().GetPointerCount());
}

// Checks that touch-events are queued properly when there is a touch-event
// handler on the page.
TEST_F(RenderWidgetHostViewAuraTest, TouchEventSyncAsync) {
  view_->InitAsChild(nullptr);
  view_->Show();

  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(30, 30), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(20, 20), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(20, 20), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));

  view_->OnTouchEvent(&press);
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_DOWN, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  view_->OnTouchEvent(&move);
  EXPECT_TRUE(move.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  // Send the same move event. Since the point hasn't moved, it won't affect the
  // queue. However, the view should consume the event.
  view_->OnTouchEvent(&move);
  EXPECT_TRUE(move.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::ACTION_MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  view_->OnTouchEvent(&release);
  EXPECT_TRUE(release.synchronous_handling_disabled());
  EXPECT_EQ(0U, pointer_state().GetPointerCount());
}

TEST_F(RenderWidgetHostViewAuraTest, PhysicalBackingSizeWithScale) {
  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  sink_->ClearMessages();
  view_->SetSize(gfx::Size(100, 100));
  EXPECT_EQ("100x100", view_->GetPhysicalBackingSize().ToString());
  EXPECT_EQ(1u, sink_->message_count());
  EXPECT_EQ(ViewMsg_Resize::ID, sink_->GetMessageAt(0)->type());
  {
    const IPC::Message* msg = sink_->GetMessageAt(0);
    EXPECT_EQ(ViewMsg_Resize::ID, msg->type());
    ViewMsg_Resize::Param params;
    ViewMsg_Resize::Read(msg, &params);
    EXPECT_EQ("100x100", std::get<0>(params).new_size.ToString());  // dip size
    EXPECT_EQ(
        "100x100",
        std::get<0>(params).physical_backing_size.ToString());  // backing size
  }

  widget_host_->ResetSizeAndRepaintPendingFlags();
  sink_->ClearMessages();

  aura_test_helper_->test_screen()->SetDeviceScaleFactor(2.0f);
  EXPECT_EQ("200x200", view_->GetPhysicalBackingSize().ToString());
  // Extra ScreenInfoChanged message for |parent_view_|.
  // Changing the device scale factor triggers the
  // RenderWidgetHostViewAura::OnDisplayMetricsChanged() observer callback,
  // which sends a ViewMsg_Resize::ID message to the renderer.
  EXPECT_EQ(1u, sink_->message_count());
  EXPECT_EQ(ViewMsg_Resize::ID, sink_->GetMessageAt(0)->type());
  auto* view_delegate = static_cast<MockRenderWidgetHostDelegate*>(
      static_cast<RenderWidgetHostImpl*>(view_->GetRenderWidgetHost())
          ->delegate());
  EXPECT_EQ(2.0f, view_delegate->get_last_device_scale_factor());

  widget_host_->ResetSizeAndRepaintPendingFlags();
  sink_->ClearMessages();

  aura_test_helper_->test_screen()->SetDeviceScaleFactor(1.0f);
  // Extra ScreenInfoChanged message for |parent_view_|.
  EXPECT_EQ(1u, sink_->message_count());
  EXPECT_EQ(ViewMsg_Resize::ID, sink_->GetMessageAt(0)->type());
  EXPECT_EQ(1.0f, view_delegate->get_last_device_scale_factor());
  EXPECT_EQ("100x100", view_->GetPhysicalBackingSize().ToString());
}

// Checks that InputMsg_CursorVisibilityChange IPC messages are dispatched
// to the renderer at the correct times.
TEST_F(RenderWidgetHostViewAuraTest, CursorVisibilityChange) {
  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(gfx::Size(100, 100));

  aura::test::TestCursorClient cursor_client(
      parent_view_->GetNativeView()->GetRootWindow());

  cursor_client.AddObserver(view_);

  // Expect a message the first time the cursor is shown.
  view_->Show();
  sink_->ClearMessages();
  cursor_client.ShowCursor();
  EXPECT_EQ(1u, sink_->message_count());
  EXPECT_TRUE(sink_->GetUniqueMessageMatching(
      InputMsg_CursorVisibilityChange::ID));

  // No message expected if the renderer already knows the cursor is visible.
  sink_->ClearMessages();
  cursor_client.ShowCursor();
  EXPECT_EQ(0u, sink_->message_count());

  // Hiding the cursor should send a message.
  sink_->ClearMessages();
  cursor_client.HideCursor();
  EXPECT_EQ(1u, sink_->message_count());
  EXPECT_TRUE(sink_->GetUniqueMessageMatching(
      InputMsg_CursorVisibilityChange::ID));

  // No message expected if the renderer already knows the cursor is invisible.
  sink_->ClearMessages();
  cursor_client.HideCursor();
  EXPECT_EQ(0u, sink_->message_count());

  // No messages should be sent while the view is invisible.
  view_->Hide();
  sink_->ClearMessages();
  cursor_client.ShowCursor();
  EXPECT_EQ(0u, sink_->message_count());
  cursor_client.HideCursor();
  EXPECT_EQ(0u, sink_->message_count());

  // Show the view. Since the cursor was invisible when the view was hidden,
  // no message should be sent.
  sink_->ClearMessages();
  view_->Show();
  EXPECT_FALSE(sink_->GetUniqueMessageMatching(
      InputMsg_CursorVisibilityChange::ID));

  // No message expected if the renderer already knows the cursor is invisible.
  sink_->ClearMessages();
  cursor_client.HideCursor();
  EXPECT_EQ(0u, sink_->message_count());

  // Showing the cursor should send a message.
  sink_->ClearMessages();
  cursor_client.ShowCursor();
  EXPECT_EQ(1u, sink_->message_count());
  EXPECT_TRUE(sink_->GetUniqueMessageMatching(
      InputMsg_CursorVisibilityChange::ID));

  // No messages should be sent while the view is invisible.
  view_->Hide();
  sink_->ClearMessages();
  cursor_client.HideCursor();
  EXPECT_EQ(0u, sink_->message_count());

  // Show the view. Since the cursor was visible when the view was hidden,
  // a message is expected to be sent.
  sink_->ClearMessages();
  view_->Show();
  EXPECT_TRUE(sink_->GetUniqueMessageMatching(
      InputMsg_CursorVisibilityChange::ID));

  cursor_client.RemoveObserver(view_);
}

TEST_F(RenderWidgetHostViewAuraTest, UpdateCursorIfOverSelf) {
  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());

  // Note that all coordinates in this test are screen coordinates.
  view_->SetBounds(gfx::Rect(60, 60, 100, 100));
  view_->Show();

  aura::test::TestCursorClient cursor_client(
      parent_view_->GetNativeView()->GetRootWindow());

  // Cursor is in the middle of the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->set_last_mouse_location(gfx::Point(110, 110));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(1, cursor_client.calls_to_set_cursor());

  // Cursor is near the top of the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->set_last_mouse_location(gfx::Point(80, 65));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(1, cursor_client.calls_to_set_cursor());

  // Cursor is near the bottom of the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->set_last_mouse_location(gfx::Point(159, 159));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(1, cursor_client.calls_to_set_cursor());

  // Cursor is above the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->set_last_mouse_location(gfx::Point(67, 59));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(0, cursor_client.calls_to_set_cursor());

  // Cursor is below the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->set_last_mouse_location(gfx::Point(161, 161));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(0, cursor_client.calls_to_set_cursor());
}

viz::CompositorFrame MakeDelegatedFrame(float scale_factor,
                                        gfx::Size size,
                                        gfx::Rect damage) {
  viz::CompositorFrame frame;
  frame.metadata.device_scale_factor = scale_factor;
  frame.metadata.begin_frame_ack = viz::BeginFrameAck(0, 1, true);

  std::unique_ptr<viz::RenderPass> pass = viz::RenderPass::Create();
  pass->SetNew(1, gfx::Rect(size), damage, gfx::Transform());
  frame.render_pass_list.push_back(std::move(pass));
  if (!size.IsEmpty()) {
    viz::TransferableResource resource;
    resource.id = 1;
    frame.resource_list.push_back(std::move(resource));
  }
  return frame;
}

// This test verifies that returned resources do not require a pending ack.
TEST_F(RenderWidgetHostViewAuraTest, ReturnedResources) {
  gfx::Size view_size(100, 100);
  gfx::Rect view_rect(view_size);

  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(), parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(view_size);
  view_->Show();
  sink_->ClearMessages();

  // Accumulate some returned resources. This should trigger an IPC.
  std::vector<viz::ReturnedResource> resources;
  viz::ReturnedResource resource;
  resource.id = 1;
  resources.push_back(resource);
  view_->renderer_compositor_frame_sink_->Reset();
  view_->ReclaimResources(resources);
  view_->renderer_compositor_frame_sink_->Flush();
  EXPECT_FALSE(view_->renderer_compositor_frame_sink_->did_receive_ack());
  EXPECT_FALSE(
      view_->renderer_compositor_frame_sink_->last_reclaimed_resources()
          .empty());
}

// This test verifies that when the CompositorFrameSink changes, the old
// resources are not returned.
TEST_F(RenderWidgetHostViewAuraTest, TwoOutputSurfaces) {
  viz::FakeSurfaceObserver manager_observer;
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  viz::SurfaceManager* manager = factory->GetContextFactoryPrivate()
                                     ->GetFrameSinkManager()
                                     ->surface_manager();
  manager->AddObserver(&manager_observer);

  gfx::Size view_size(100, 100);
  gfx::Rect view_rect(view_size);

  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(), parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(view_size);
  view_->Show();
  sink_->ClearMessages();

  // Submit a frame with resources.
  viz::CompositorFrame frame = MakeDelegatedFrame(1.f, view_size, view_rect);
  viz::TransferableResource resource;
  resource.id = 1;
  frame.resource_list.push_back(resource);
  view_->SubmitCompositorFrame(kArbitraryLocalSurfaceId, std::move(frame));
  EXPECT_EQ(0u, sink_->message_count());

  // Signal that a new RendererCompositorFrameSink was created by the renderer.
  view_->CreateNewRendererCompositorFrameSink();

  // Submit another frame. The resources for the previous frame belong to the
  // old RendererCompositorFrameSink and should not be returned.
  view_->SubmitCompositorFrame(local_surface_id_allocator_.GenerateId(),
                               MakeDelegatedFrame(1.f, view_size, view_rect));
  EXPECT_EQ(0u, sink_->message_count());

  // Report that the surface is drawn to trigger an ACK.
  view_->renderer_compositor_frame_sink_->Reset();
  viz::Surface* surface = manager->GetSurfaceForId(view_->surface_id());
  EXPECT_TRUE(surface);
  surface->RunDrawCallback();
  view_->renderer_compositor_frame_sink_->Flush();
  EXPECT_TRUE(view_->renderer_compositor_frame_sink_->did_receive_ack());

  manager->RemoveObserver(&manager_observer);
}

// Resizing in fullscreen mode should send the up-to-date screen info.
// http://crbug.com/324350
TEST_F(RenderWidgetHostViewAuraTest, DISABLED_FullscreenResize) {
  aura::Window* root_window = aura_test_helper_->root_window();
  root_window->SetLayoutManager(new FullscreenLayoutManager(root_window));
  view_->InitAsFullscreen(parent_view_);
  view_->Show();
  widget_host_->ResetSizeAndRepaintPendingFlags();
  sink_->ClearMessages();

  // Call WasResized to flush the old screen info.
  view_->GetRenderWidgetHost()->WasResized();
  {
    // 0 is CreatingNew message.
    const IPC::Message* msg = sink_->GetMessageAt(0);
    EXPECT_EQ(ViewMsg_Resize::ID, msg->type());
    ViewMsg_Resize::Param params;
    ViewMsg_Resize::Read(msg, &params);
    EXPECT_EQ(
        "0,0 800x600",
        std::get<0>(params).screen_info.available_rect.ToString());
    EXPECT_EQ("800x600", std::get<0>(params).new_size.ToString());
    // Resizes are blocked until we swapped a frame of the correct size, and
    // we've committed it.
    view_->SubmitCompositorFrame(
        kArbitraryLocalSurfaceId,
        MakeDelegatedFrame(1.f, std::get<0>(params).new_size,
                           gfx::Rect(std::get<0>(params).new_size)));
    ui::DrawWaiterForTest::WaitForCommit(
        root_window->GetHost()->compositor());
  }

  widget_host_->ResetSizeAndRepaintPendingFlags();
  sink_->ClearMessages();

  // Make sure the corrent screen size is set along in the resize
  // request when the screen size has changed.
  aura_test_helper_->test_screen()->SetUIScale(0.5);
  EXPECT_EQ(1u, sink_->message_count());
  {
    const IPC::Message* msg = sink_->GetMessageAt(0);
    EXPECT_EQ(ViewMsg_Resize::ID, msg->type());
    ViewMsg_Resize::Param params;
    ViewMsg_Resize::Read(msg, &params);
    EXPECT_EQ(
        "0,0 1600x1200",
        std::get<0>(params).screen_info.available_rect.ToString());
    EXPECT_EQ("1600x1200", std::get<0>(params).new_size.ToString());
    view_->SubmitCompositorFrame(
        kArbitraryLocalSurfaceId,
        MakeDelegatedFrame(1.f, std::get<0>(params).new_size,
                           gfx::Rect(std::get<0>(params).new_size)));
    ui::DrawWaiterForTest::WaitForCommit(
        root_window->GetHost()->compositor());
  }
}

// Swapping a frame should notify the window.
TEST_F(RenderWidgetHostViewAuraTest, SwapNotifiesWindow) {
  gfx::Size view_size(100, 100);
  gfx::Rect view_rect(view_size);

  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(view_size);
  view_->Show();

  MockWindowObserver observer;
  view_->window_->AddObserver(&observer);

  // Delegated renderer path
  EXPECT_CALL(observer, OnDelegatedFrameDamage(view_->window_, view_rect));
  view_->SubmitCompositorFrame(kArbitraryLocalSurfaceId,
                               MakeDelegatedFrame(1.f, view_size, view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnDelegatedFrameDamage(view_->window_,
                                               gfx::Rect(5, 5, 5, 5)));
  view_->SubmitCompositorFrame(
      kArbitraryLocalSurfaceId,
      MakeDelegatedFrame(1.f, view_size, gfx::Rect(5, 5, 5, 5)));
  testing::Mock::VerifyAndClearExpectations(&observer);

  view_->window_->RemoveObserver(&observer);
}

// If the view size is larger than the compositor frame then extra layers
// should be created to fill the gap.
TEST_F(RenderWidgetHostViewAuraTest, DelegatedFrameGutter) {
  gfx::Size large_size(100, 100);
  gfx::Size small_size(40, 45);
  gfx::Size medium_size(40, 95);
  viz::LocalSurfaceId small_id = local_surface_id_allocator_.GenerateId();
  viz::LocalSurfaceId medium_id = local_surface_id_allocator_.GenerateId();

  // Prevent the DelegatedFrameHost from skipping frames.
  view_->DisableResizeLock();

  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(), parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(large_size);
  view_->Show();
  viz::CompositorFrame frame =
      MakeDelegatedFrame(1.f, small_size, gfx::Rect(small_size));
  frame.metadata.root_background_color = SK_ColorRED;
  view_->SubmitCompositorFrame(small_id, std::move(frame));

  ui::Layer* parent_layer = view_->GetNativeView()->layer();

  ASSERT_EQ(2u, parent_layer->children().size());
  EXPECT_EQ(gfx::Rect(40, 0, 60, 100), parent_layer->children()[0]->bounds());
  EXPECT_EQ(SK_ColorRED, parent_layer->children()[0]->background_color());
  EXPECT_EQ(gfx::Rect(0, 45, 40, 55), parent_layer->children()[1]->bounds());
  EXPECT_EQ(SK_ColorRED, parent_layer->children()[1]->background_color());

  delegates_.back()->set_is_fullscreen(true);
  view_->SetSize(medium_size);

  // Right gutter is unnecessary.
  ASSERT_EQ(1u, parent_layer->children().size());
  EXPECT_EQ(gfx::Rect(0, 45, 40, 50), parent_layer->children()[0]->bounds());

  // RWH is fullscreen, so gutters should be black.
  EXPECT_EQ(SK_ColorBLACK, parent_layer->children()[0]->background_color());

  frame = MakeDelegatedFrame(1.f, medium_size, gfx::Rect(medium_size));
  view_->SubmitCompositorFrame(medium_id, std::move(frame));
  EXPECT_EQ(0u, parent_layer->children().size());

  view_->SetSize(large_size);
  ASSERT_EQ(2u, parent_layer->children().size());

  // This should evict the frame and remove the gutter layers.
  view_->Hide();
  view_->SetSize(small_size);
  ASSERT_EQ(0u, parent_layer->children().size());
}

TEST_F(RenderWidgetHostViewAuraTest, Resize) {
  gfx::Size size1(100, 100);
  gfx::Size size2(200, 200);
  gfx::Size size3(300, 300);
  viz::LocalSurfaceId id1 = local_surface_id_allocator_.GenerateId();
  viz::LocalSurfaceId id2 = local_surface_id_allocator_.GenerateId();
  viz::LocalSurfaceId id3 = local_surface_id_allocator_.GenerateId();

  aura::Window* root_window = parent_view_->GetNativeView()->GetRootWindow();
  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(), root_window, gfx::Rect(size1));
  view_->Show();
  view_->SetSize(size1);
  view_->SubmitCompositorFrame(
      id1, MakeDelegatedFrame(1.f, size1, gfx::Rect(size1)));
  ui::DrawWaiterForTest::WaitForCommit(
      root_window->GetHost()->compositor());
  ViewHostMsg_ResizeOrRepaint_ACK_Params update_params;
  update_params.view_size = size1;
  update_params.flags = ViewHostMsg_ResizeOrRepaint_ACK_Flags::IS_RESIZE_ACK;
  widget_host_->OnMessageReceived(ViewHostMsg_ResizeOrRepaint_ACK(
      widget_host_->GetRoutingID(), update_params));
  sink_->ClearMessages();
  // Resize logic is idle (no pending resize, no pending commit).
  EXPECT_EQ(size1.ToString(), view_->GetRequestedRendererSize().ToString());

  // Resize renderer, should produce a Resize message
  view_->SetSize(size2);
  EXPECT_EQ(size2.ToString(), view_->GetRequestedRendererSize().ToString());
  EXPECT_EQ(1u, sink_->message_count());
  {
    const IPC::Message* msg = sink_->GetMessageAt(0);
    EXPECT_EQ(ViewMsg_Resize::ID, msg->type());
    ViewMsg_Resize::Param params;
    ViewMsg_Resize::Read(msg, &params);
    EXPECT_EQ(size2.ToString(), std::get<0>(params).new_size.ToString());
  }
  // Send resize ack to observe new Resize messages.
  update_params.view_size = size2;
  widget_host_->OnMessageReceived(ViewHostMsg_ResizeOrRepaint_ACK(
      widget_host_->GetRoutingID(), update_params));
  sink_->ClearMessages();

  // Resize renderer again, before receiving a frame. Should not produce a
  // Resize message.
  view_->SetSize(size3);
  EXPECT_EQ(size2.ToString(), view_->GetRequestedRendererSize().ToString());
  EXPECT_EQ(0u, sink_->message_count());

  // Receive a frame of the new size, should be skipped and not produce a Resize
  // message.
  view_->renderer_compositor_frame_sink_->Reset();
  view_->SubmitCompositorFrame(
      id3, MakeDelegatedFrame(1.f, size3, gfx::Rect(size3)));
  view_->renderer_compositor_frame_sink_->Flush();
  // Expect the frame ack;
  EXPECT_TRUE(view_->renderer_compositor_frame_sink_->did_receive_ack());
  EXPECT_EQ(size2.ToString(), view_->GetRequestedRendererSize().ToString());

  // Receive a frame of the correct size, should not be skipped and, and should
  // produce a Resize message after the commit.
  view_->renderer_compositor_frame_sink_->Reset();
  view_->SubmitCompositorFrame(
      id2, MakeDelegatedFrame(1.f, size2, gfx::Rect(size2)));
  view_->renderer_compositor_frame_sink_->Flush();
  viz::SurfaceId surface_id = view_->surface_id();
  if (!surface_id.is_valid()) {
    // No frame ack yet.
    EXPECT_FALSE(view_->renderer_compositor_frame_sink_->did_receive_ack());
  } else {
    // Frame isn't desired size, so early ack.
    EXPECT_TRUE(view_->renderer_compositor_frame_sink_->did_receive_ack());
  }
  EXPECT_EQ(size2.ToString(), view_->GetRequestedRendererSize().ToString());

  // Wait for commit, then we should unlock the compositor and send a Resize
  // message (and a frame ack)
  ui::DrawWaiterForTest::WaitForCommit(
      root_window->GetHost()->compositor());

  bool has_resize = false;
  for (uint32_t i = 0; i < sink_->message_count(); ++i) {
    const IPC::Message* msg = sink_->GetMessageAt(i);
    switch (msg->type()) {
      case InputMsg_HandleInputEvent::ID: {
        // On some platforms, the call to view_->Show() causes a posted task to
        // call
        // ui::WindowEventDispatcher::SynthesizeMouseMoveAfterChangeToWindow,
        // which the above WaitForCommit may cause to be picked up. Be robust
        // to this extra IPC coming in.
        InputMsg_HandleInputEvent::Param params;
        InputMsg_HandleInputEvent::Read(msg, &params);
        const blink::WebInputEvent* event = std::get<0>(params);
        EXPECT_EQ(blink::WebInputEvent::kMouseMove, event->GetType());
        break;
      }
      case ViewMsg_Resize::ID: {
        EXPECT_FALSE(has_resize);
        ViewMsg_Resize::Param params;
        ViewMsg_Resize::Read(msg, &params);
        EXPECT_EQ(size3.ToString(), std::get<0>(params).new_size.ToString());
        has_resize = true;
        break;
      }
      default:
        ADD_FAILURE() << "Unexpected message " << msg->type();
        break;
    }
  }
  EXPECT_TRUE(has_resize);
  update_params.view_size = size3;
  widget_host_->OnMessageReceived(ViewHostMsg_ResizeOrRepaint_ACK(
      widget_host_->GetRoutingID(), update_params));
  sink_->ClearMessages();
}

// Skipped frames should not drop their damage.
TEST_F(RenderWidgetHostViewAuraTest, SkippedDelegatedFrames) {
  gfx::Rect view_rect(100, 100);
  gfx::Size frame_size = view_rect.size();
  viz::LocalSurfaceId local_surface_id = kArbitraryLocalSurfaceId;

  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(view_rect.size());

  MockWindowObserver observer;
  view_->window_->AddObserver(&observer);

  // A full frame of damage.
  EXPECT_CALL(observer, OnDelegatedFrameDamage(view_->window_, view_rect));
  view_->SubmitCompositorFrame(local_surface_id,
                               MakeDelegatedFrame(1.f, frame_size, view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  // A partial damage frame.
  gfx::Rect partial_view_rect(30, 30, 20, 20);
  EXPECT_CALL(observer,
              OnDelegatedFrameDamage(view_->window_, partial_view_rect));
  view_->SubmitCompositorFrame(
      local_surface_id, MakeDelegatedFrame(1.f, frame_size, partial_view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  EXPECT_FALSE(view_->resize_locked());
  EXPECT_FALSE(view_->compositor_locked());

  // Lock the compositor. Now we should drop frames.
  view_rect = gfx::Rect(150, 150);
  view_->SetSize(view_rect.size());
  EXPECT_TRUE(view_->resize_locked());
  EXPECT_TRUE(view_->compositor_locked());

  // This frame is dropped.
  gfx::Rect dropped_damage_rect_1(10, 20, 30, 40);
  EXPECT_CALL(observer, OnDelegatedFrameDamage(_, _)).Times(0);
  view_->SubmitCompositorFrame(
      local_surface_id,
      MakeDelegatedFrame(1.f, frame_size, dropped_damage_rect_1));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  gfx::Rect dropped_damage_rect_2(40, 50, 10, 20);
  EXPECT_CALL(observer, OnDelegatedFrameDamage(_, _)).Times(0);
  view_->SubmitCompositorFrame(
      local_surface_id,
      MakeDelegatedFrame(1.f, frame_size, dropped_damage_rect_2));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  EXPECT_TRUE(view_->resize_locked());
  EXPECT_TRUE(view_->compositor_locked());

  // Unlock the compositor. This frame should damage everything.
  frame_size = view_rect.size();
  local_surface_id = local_surface_id_allocator_.GenerateId();

  gfx::Rect new_damage_rect(5, 6, 10, 10);
  EXPECT_CALL(observer,
              OnDelegatedFrameDamage(view_->window_, view_rect));
  view_->SubmitCompositorFrame(
      local_surface_id, MakeDelegatedFrame(1.f, frame_size, new_damage_rect));
  // The swap unlocks the compositor.
  EXPECT_TRUE(view_->resize_locked());
  EXPECT_FALSE(view_->compositor_locked());
  testing::Mock::VerifyAndClearExpectations(&observer);
  // The UI commit unlocks for further resize.
  view_->RunOnCompositingDidCommit();
  EXPECT_FALSE(view_->resize_locked());
  EXPECT_FALSE(view_->compositor_locked());

  // A partial damage frame, this should not be dropped.
  EXPECT_CALL(observer,
              OnDelegatedFrameDamage(view_->window_, partial_view_rect));
  view_->SubmitCompositorFrame(
      local_surface_id, MakeDelegatedFrame(1.f, frame_size, partial_view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();
  EXPECT_FALSE(view_->resize_locked());
  EXPECT_FALSE(view_->compositor_locked());

  // Resize to something empty. This doesn't lock anything since it's not
  // visible anymore anyways.
  view_rect = gfx::Rect(100, 0);
  view_->SetSize(view_rect.size());
  EXPECT_FALSE(view_->resize_locked());
  EXPECT_FALSE(view_->compositor_locked());

  // We're never expecting empty frames, resize to something non-empty.
  view_rect = gfx::Rect(100, 100);
  frame_size = view_rect.size();
  local_surface_id = local_surface_id_allocator_.GenerateId();
  view_->SetSize(view_rect.size());
  EXPECT_TRUE(view_->resize_locked());
  EXPECT_TRUE(view_->compositor_locked());

  // This frame should not be dropped.
  EXPECT_CALL(observer, OnDelegatedFrameDamage(view_->window_, view_rect));
  view_->SubmitCompositorFrame(local_surface_id,
                               MakeDelegatedFrame(1.f, frame_size, view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(view_->resize_locked());
  EXPECT_FALSE(view_->compositor_locked());
  view_->RunOnCompositingDidCommit();
  EXPECT_FALSE(view_->resize_locked());
  EXPECT_FALSE(view_->compositor_locked());

  view_->window_->RemoveObserver(&observer);
}

// If resize races with a renderer frame, we should lock for the right size.
TEST_F(RenderWidgetHostViewAuraTest, ResizeAfterReceivingFrame) {
  gfx::Rect view_rect(100, 100);
  gfx::Size frame_size = view_rect.size();
  viz::LocalSurfaceId local_surface_id = kArbitraryLocalSurfaceId;

  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(), parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(view_rect.size());

  MockWindowObserver observer;
  view_->window_->AddObserver(&observer);

  // A frame of initial size.
  EXPECT_CALL(observer, OnDelegatedFrameDamage(view_->window_, view_rect));
  view_->SubmitCompositorFrame(
      local_surface_id,
      MakeDelegatedFrame(1.f, frame_size, gfx::Rect(frame_size)));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  // A frame of initial size arrives, but we don't commit in the UI yet.
  EXPECT_CALL(observer, OnDelegatedFrameDamage(view_->window_, _));
  view_->SubmitCompositorFrame(
      local_surface_id,
      MakeDelegatedFrame(1.f, frame_size, gfx::Rect(frame_size)));
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_FALSE(view_->resize_locked());
  EXPECT_FALSE(view_->compositor_locked());

  // Resize, and lock the compositor. Now we should drop frames of the old size.
  view_rect = gfx::Rect(150, 150);
  view_->SetSize(view_rect.size());
  EXPECT_TRUE(view_->resize_locked());
  EXPECT_TRUE(view_->compositor_locked());

  EXPECT_CALL(observer, OnDelegatedFrameDamage(_, _)).Times(0);
  view_->SubmitCompositorFrame(
      local_surface_id,
      MakeDelegatedFrame(1.f, frame_size, gfx::Rect(frame_size)));
  testing::Mock::VerifyAndClearExpectations(&observer);

  // If the CompositorLock times out in the meantime, a commit would happen.
  // Verify that if a commit occurs, the lock remains and we reject frames
  // of the wrong size still.
  view_->RunOnCompositingDidCommit();

  EXPECT_TRUE(view_->resize_locked());
  // In this case we lied about it and the CompositorLock is still active.
  EXPECT_TRUE(view_->compositor_locked());

  EXPECT_CALL(observer, OnDelegatedFrameDamage(_, _)).Times(0);
  view_->SubmitCompositorFrame(
      local_surface_id,
      MakeDelegatedFrame(1.f, frame_size, gfx::Rect(frame_size)));
  testing::Mock::VerifyAndClearExpectations(&observer);

  // A frame arrives of the new size, which will be accepted.
  frame_size = view_rect.size();
  local_surface_id = local_surface_id_allocator_.GenerateId();
  EXPECT_CALL(observer, OnDelegatedFrameDamage(view_->window_, _));
  view_->SubmitCompositorFrame(
      local_surface_id,
      MakeDelegatedFrame(1.f, frame_size, gfx::Rect(frame_size)));
  // Receiving the frame unlocks the compositor so it can commit.
  EXPECT_TRUE(view_->resize_locked());
  EXPECT_FALSE(view_->compositor_locked());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // When the frame of the correct size is committed, the CompositorResizeLock
  // is released.
  view_->RunOnCompositingDidCommit();
  EXPECT_FALSE(view_->resize_locked());
  EXPECT_FALSE(view_->compositor_locked());

  view_->window_->RemoveObserver(&observer);
}

// When the DelegatedFrameHost does not have a frame from the renderer, it has
// no reason to lock the compositor as there can't be guttering around a
// renderer frame that doesn't exist.
TEST_F(RenderWidgetHostViewAuraTest, MissingFramesDontLock) {
  gfx::Rect view_rect(100, 100);
  gfx::Size frame_size = view_rect.size();

  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(), parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());

  // The view is resized before the first frame, which should not lock the
  // compositor as it's never received a frame to show yet.
  view_->SetSize(view_rect.size());

  EXPECT_FALSE(view_->resize_locked());
  EXPECT_FALSE(view_->compositor_locked());

  // Submit a frame of initial size to make a frame present in
  // DelegatedFrameHost, at which point locking becomes feasible if resized.
  view_->SubmitCompositorFrame(
      kArbitraryLocalSurfaceId,
      MakeDelegatedFrame(1.f, frame_size, gfx::Rect(frame_size)));
  view_->RunOnCompositingDidCommit();

  EXPECT_FALSE(view_->resize_locked());
  EXPECT_FALSE(view_->compositor_locked());

  // The view is resized and has its frame evicted, before a new frame arrives.
  // The resize will lock the compositor, but when evicted, it should no longer
  // be locked.
  view_rect.SetRect(0, 0, 150, 150);
  view_->SetSize(view_rect.size());
  EXPECT_TRUE(view_->resize_locked());
  EXPECT_TRUE(view_->compositor_locked());

  view_->ClearCompositorFrame();
  EXPECT_FALSE(view_->resize_locked());
  EXPECT_FALSE(view_->compositor_locked());

  // And future resizes after eviction should not lock the compositor since
  // there is no frame present.
  view_rect.SetRect(0, 0, 120, 120);
  view_->SetSize(view_rect.size());
  EXPECT_FALSE(view_->resize_locked());
  EXPECT_FALSE(view_->compositor_locked());
}

TEST_F(RenderWidgetHostViewAuraTest, OutputSurfaceIdChange) {
  gfx::Rect view_rect(100, 100);
  gfx::Size frame_size = view_rect.size();

  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(view_rect.size());

  MockWindowObserver observer;
  view_->window_->AddObserver(&observer);

  // Swap a frame.
  EXPECT_CALL(observer, OnDelegatedFrameDamage(view_->window_, view_rect));
  view_->SubmitCompositorFrame(kArbitraryLocalSurfaceId,
                               MakeDelegatedFrame(1.f, frame_size, view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  // Signal that a new RendererCompositorFrameSink was created.
  view_->CreateNewRendererCompositorFrameSink();

  // Submit a frame from the new RendererCompositorFrameSink.
  EXPECT_CALL(observer, OnDelegatedFrameDamage(view_->window_, view_rect));
  view_->SubmitCompositorFrame(local_surface_id_allocator_.GenerateId(),
                               MakeDelegatedFrame(1.f, frame_size, view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  // Signal that a new RendererCompositorFrameSink was created.
  view_->CreateNewRendererCompositorFrameSink();

  // Submit a frame from the new RendererCompositorFrameSink.
  view_->SubmitCompositorFrame(
      local_surface_id_allocator_.GenerateId(),
      MakeDelegatedFrame(1.f, gfx::Size(), gfx::Rect()));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  // Signal that a new RendererCompositorFrameSink was created.
  view_->CreateNewRendererCompositorFrameSink();

  // Swap another frame, with a different surface id.
  EXPECT_CALL(observer, OnDelegatedFrameDamage(view_->window_, view_rect));
  view_->SubmitCompositorFrame(local_surface_id_allocator_.GenerateId(),
                               MakeDelegatedFrame(1.f, frame_size, view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  view_->window_->RemoveObserver(&observer);
}

// This test verifies that the primary SurfaceInfo is populated on resize and
// the fallback SurfaceInfo is populated on SubmitCompositorFrame.
TEST_F(RenderWidgetHostViewAuraSurfaceSynchronizationTest, SurfaceChanges) {
  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(), parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());

  // Prevent the DelegatedFrameHost from skipping frames.
  view_->DisableResizeLock();
  EXPECT_FALSE(view_->HasPrimarySurface());

  view_->SetSize(gfx::Size(300, 300));
  ASSERT_TRUE(view_->HasPrimarySurface());
  EXPECT_EQ(gfx::Size(300, 300),
            view_->window_->layer()->GetPrimarySurfaceInfo()->size_in_pixels());
  EXPECT_FALSE(view_->window_->layer()->GetFallbackSurfaceInfo()->is_valid());

  // Resizing should update the primary SurfaceInfo.
  view_->SetSize(gfx::Size(400, 400));
  EXPECT_EQ(gfx::Size(400, 400),
            view_->window_->layer()->GetPrimarySurfaceInfo()->size_in_pixels());
  EXPECT_FALSE(view_->window_->layer()->GetFallbackSurfaceInfo()->is_valid());

  // Submitting a CompositorFrame should update the fallback SurfaceInfo
  view_->SubmitCompositorFrame(
      kArbitraryLocalSurfaceId,
      MakeDelegatedFrame(1.f, gfx::Size(400, 400), gfx::Rect(400, 400)));
  EXPECT_EQ(gfx::Size(400, 400),
            view_->window_->layer()->GetPrimarySurfaceInfo()->size_in_pixels());
  EXPECT_EQ(
      gfx::Size(400, 400),
      view_->window_->layer()->GetFallbackSurfaceInfo()->size_in_pixels());
}

// This test verifies that changing the CompositorFrameSink (and thus evicting
// the current surface) does not crash,
TEST_F(RenderWidgetHostViewAuraSurfaceSynchronizationTest,
       CompositorFrameSinkChange) {
  gfx::Rect view_rect(100, 100);
  gfx::Size frame_size = view_rect.size();

  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(), parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(view_rect.size());

  // Swap a frame.
  view_->SubmitCompositorFrame(kArbitraryLocalSurfaceId,
                               MakeDelegatedFrame(1.f, frame_size, view_rect));
  view_->RunOnCompositingDidCommit();

  // Signal that a new RendererCompositorFrameSink was created.
  view_->CreateNewRendererCompositorFrameSink();

  // Submit a frame from the new RendererCompositorFrameSink.
  view_->SubmitCompositorFrame(local_surface_id_allocator_.GenerateId(),
                               MakeDelegatedFrame(1.f, frame_size, view_rect));
  view_->RunOnCompositingDidCommit();
}

// This test verifies that frame eviction plays well with surface
// synchronizaton. This test is similar to
// RenderWidgetHostViewAuraTest.DiscardDelegatedFrame but resizes instead
// of submitting frame as that's when the primary surface is set when
// surface synchronization is on.
TEST_F(RenderWidgetHostViewAuraSurfaceSynchronizationTest,
       DiscardDelegatedFrames) {
  view_->InitAsChild(nullptr);

  size_t max_renderer_frames =
      FrameEvictionManager::GetInstance()->GetMaxNumberOfSavedFrames();
  ASSERT_LE(2u, max_renderer_frames);
  size_t renderer_count = max_renderer_frames + 1;

  std::unique_ptr<RenderWidgetHostImpl* []> hosts(
      new RenderWidgetHostImpl*[renderer_count]);
  std::unique_ptr<FakeRenderWidgetHostViewAura* []> views(
      new FakeRenderWidgetHostViewAura*[renderer_count]);

  // Create a bunch of renderers.
  for (size_t i = 0; i < renderer_count; ++i) {
    int32_t routing_id = process_host_->GetNextRoutingID();
    delegates_.push_back(base::WrapUnique(new MockRenderWidgetHostDelegate));
    hosts[i] = MockRenderWidgetHostImpl::Create(delegates_.back().get(),
                                                process_host_, routing_id);
    delegates_.back()->set_widget_host(hosts[i]);
    hosts[i]->Init();
    views[i] = new FakeRenderWidgetHostViewAura(
        hosts[i], false, true /* enable_surface_synchronization */);
    // Prevent frames from being skipped due to resize, this test does not
    // run a UI compositor so the DelegatedFrameHost doesn't get the chance
    // to release its resize lock once it receives a frame of the expected
    // size.
    views[i]->DisableResizeLock();
    views[i]->InitAsChild(nullptr);
    aura::client::ParentWindowWithContext(
        views[i]->GetNativeView(),
        parent_view_->GetNativeView()->GetRootWindow(), gfx::Rect());
    // Make each renderer visible, resize it, then make it invisible.
    views[i]->Show();
    views[i]->SetSize(gfx::Size(300, 300));
    EXPECT_TRUE(views[i]->HasPrimarySurface());
    views[i]->Hide();
  }

  // There should be max_renderer_frames with a frame in it, and one without it.
  // Since the logic is LRU eviction, the first one should be without.
  EXPECT_FALSE(views[0]->HasPrimarySurface());
  for (size_t i = 1; i < renderer_count; ++i)
    EXPECT_TRUE(views[i]->HasPrimarySurface());

  // LRU renderer is [0], make it visible, it shouldn't evict anything yet.
  views[0]->Show();
  EXPECT_FALSE(views[0]->HasPrimarySurface());
  EXPECT_TRUE(views[1]->HasPrimarySurface());

  // Resize [0], it should evict the next LRU [1].
  views[0]->SetSize(gfx::Size(300, 300));
  EXPECT_TRUE(views[0]->HasPrimarySurface());
  EXPECT_FALSE(views[1]->HasPrimarySurface());
  views[0]->Hide();

  // LRU renderer is [1], still hidden. Resize it, it should evict
  // the next LRU [2].
  views[1]->Show();
  views[1]->SetSize(gfx::Size(300, 300));
  views[1]->Hide();
  EXPECT_TRUE(views[0]->HasPrimarySurface());
  EXPECT_TRUE(views[1]->HasPrimarySurface());
  EXPECT_FALSE(views[2]->HasPrimarySurface());
  for (size_t i = 3; i < renderer_count; ++i)
    EXPECT_TRUE(views[i]->HasPrimarySurface());

  // Make all renderers but [0] visible and resize them, keep [0]
  // hidden, it becomes the LRU.
  for (size_t i = 1; i < renderer_count; ++i) {
    views[i]->Show();
    // The renderers who don't have a frame should be waiting. The ones that
    // have a frame should not.
    // In practice, [1] has a frame, but anything after has its frame evicted.
    EXPECT_EQ(!views[i]->HasPrimarySurface(),
              views[i]->released_front_lock_active());
    views[i]->SetSize(gfx::Size(300, 300));
    EXPECT_TRUE(views[i]->HasPrimarySurface());
  }
  EXPECT_FALSE(views[0]->HasPrimarySurface());

  // Resize [0], it should be evicted immediately.
  views[0]->SetSize(gfx::Size(300, 300));
  EXPECT_FALSE(views[0]->HasPrimarySurface());

  // Make [0] visible, and swap a frame on it. Nothing should be evicted
  // although we're above the limit.
  views[0]->Show();
  views[0]->SetSize(gfx::Size(300, 300));
  for (size_t i = 0; i < renderer_count; ++i)
    EXPECT_TRUE(views[i]->HasPrimarySurface());

  // Make [0] hidden, it should evict its frame.
  views[0]->Hide();
  EXPECT_FALSE(views[0]->HasPrimarySurface());

  for (size_t i = 0; i < renderer_count; ++i) {
    views[i]->Destroy();
    delete hosts[i];
  }
}

TEST_F(RenderWidgetHostViewAuraTest, DiscardDelegatedFrames) {
  view_->InitAsChild(nullptr);

  size_t max_renderer_frames =
      FrameEvictionManager::GetInstance()->GetMaxNumberOfSavedFrames();
  ASSERT_LE(2u, max_renderer_frames);
  size_t renderer_count = max_renderer_frames + 1;
  gfx::Rect view_rect(100, 100);
  gfx::Size frame_size = view_rect.size();

  std::unique_ptr<RenderWidgetHostImpl* []> hosts(
      new RenderWidgetHostImpl*[renderer_count]);
  std::unique_ptr<FakeRenderWidgetHostViewAura* []> views(
      new FakeRenderWidgetHostViewAura*[renderer_count]);

  // Create a bunch of renderers.
  for (size_t i = 0; i < renderer_count; ++i) {
    int32_t routing_id = process_host_->GetNextRoutingID();
    delegates_.push_back(base::WrapUnique(new MockRenderWidgetHostDelegate));
    hosts[i] = MockRenderWidgetHostImpl::Create(delegates_.back().get(),
                                                process_host_, routing_id);
    delegates_.back()->set_widget_host(hosts[i]);
    hosts[i]->Init();
    views[i] = new FakeRenderWidgetHostViewAura(
        hosts[i], false, false /* enable_surface_synchronization */);
    // Prevent frames from being skipped due to resize, this test does not
    // run a UI compositor so the DelegatedFrameHost doesn't get the chance
    // to release its resize lock once it receives a frame of the expected
    // size.
    views[i]->DisableResizeLock();
    views[i]->InitAsChild(nullptr);
    aura::client::ParentWindowWithContext(
        views[i]->GetNativeView(),
        parent_view_->GetNativeView()->GetRootWindow(),
        gfx::Rect());
    views[i]->SetSize(view_rect.size());
  }

  // Make each renderer visible, and swap a frame on it, then make it invisible.
  for (size_t i = 0; i < renderer_count; ++i) {
    views[i]->Show();
    views[i]->SubmitCompositorFrame(
        kArbitraryLocalSurfaceId,
        MakeDelegatedFrame(1.f, frame_size, view_rect));
    EXPECT_TRUE(views[i]->HasPrimarySurface());
    views[i]->Hide();
  }

  // There should be max_renderer_frames with a frame in it, and one without it.
  // Since the logic is LRU eviction, the first one should be without.
  EXPECT_FALSE(views[0]->HasPrimarySurface());
  for (size_t i = 1; i < renderer_count; ++i)
    EXPECT_TRUE(views[i]->HasPrimarySurface());

  // LRU renderer is [0], make it visible, it shouldn't evict anything yet.
  views[0]->Show();
  EXPECT_FALSE(views[0]->HasPrimarySurface());
  EXPECT_TRUE(views[1]->HasPrimarySurface());
  // Since [0] doesn't have a frame, it should be waiting for the renderer to
  // give it one.
  EXPECT_TRUE(views[0]->released_front_lock_active());

  // Swap a frame on it, it should evict the next LRU [1].
  views[0]->SubmitCompositorFrame(
      kArbitraryLocalSurfaceId, MakeDelegatedFrame(1.f, frame_size, view_rect));
  EXPECT_TRUE(views[0]->HasPrimarySurface());
  EXPECT_FALSE(views[1]->HasPrimarySurface());
  // Now that [0] got a frame, it shouldn't be waiting any more.
  EXPECT_FALSE(views[0]->released_front_lock_active());
  views[0]->Hide();

  // LRU renderer is [1], still hidden. Swap a frame on it, it should evict
  // the next LRU [2].
  views[1]->SubmitCompositorFrame(
      kArbitraryLocalSurfaceId, MakeDelegatedFrame(1.f, frame_size, view_rect));
  EXPECT_TRUE(views[0]->HasPrimarySurface());
  EXPECT_TRUE(views[1]->HasPrimarySurface());
  EXPECT_FALSE(views[2]->HasPrimarySurface());
  for (size_t i = 3; i < renderer_count; ++i)
    EXPECT_TRUE(views[i]->HasPrimarySurface());

  // Make all renderers but [0] visible and swap a frame on them, keep [0]
  // hidden, it becomes the LRU.
  for (size_t i = 1; i < renderer_count; ++i) {
    views[i]->Show();
    // The renderers who don't have a frame should be waiting. The ones that
    // have a frame should not.
    // In practice, [1] has a frame, but anything after has its frame evicted.
    EXPECT_EQ(!views[i]->HasPrimarySurface(),
              views[i]->released_front_lock_active());
    views[i]->SubmitCompositorFrame(
        kArbitraryLocalSurfaceId,
        MakeDelegatedFrame(1.f, frame_size, view_rect));
    // Now everyone has a frame.
    EXPECT_FALSE(views[i]->released_front_lock_active());
    EXPECT_TRUE(views[i]->HasPrimarySurface());
  }
  EXPECT_FALSE(views[0]->HasPrimarySurface());

  // Swap a frame on [0], it should be evicted immediately.
  views[0]->SubmitCompositorFrame(
      kArbitraryLocalSurfaceId, MakeDelegatedFrame(1.f, frame_size, view_rect));
  EXPECT_FALSE(views[0]->HasPrimarySurface());

  // Make [0] visible, and swap a frame on it. Nothing should be evicted
  // although we're above the limit.
  views[0]->Show();
  // We don't have a frame, wait.
  EXPECT_TRUE(views[0]->released_front_lock_active());
  views[0]->SubmitCompositorFrame(
      kArbitraryLocalSurfaceId, MakeDelegatedFrame(1.f, frame_size, view_rect));
  EXPECT_FALSE(views[0]->released_front_lock_active());
  for (size_t i = 0; i < renderer_count; ++i)
    EXPECT_TRUE(views[i]->HasPrimarySurface());

  // Make [0] hidden, it should evict its frame.
  views[0]->Hide();
  EXPECT_FALSE(views[0]->HasPrimarySurface());

  // Make [0] visible, don't give it a frame, it should be waiting.
  views[0]->Show();
  EXPECT_TRUE(views[0]->released_front_lock_active());
  // Make [0] hidden, it should stop waiting.
  views[0]->Hide();
  EXPECT_FALSE(views[0]->released_front_lock_active());

  // Make [1] hidden, resize it. It should drop its frame.
  views[1]->Hide();
  EXPECT_TRUE(views[1]->HasPrimarySurface());
  gfx::Size size2(200, 200);
  viz::LocalSurfaceId id2 = local_surface_id_allocator_.GenerateId();
  views[1]->SetSize(size2);
  EXPECT_FALSE(views[1]->HasPrimarySurface());
  // Show it, it should block until we give it a frame.
  views[1]->Show();
  EXPECT_TRUE(views[1]->released_front_lock_active());
  views[1]->SubmitCompositorFrame(
      id2, MakeDelegatedFrame(1.f, size2, gfx::Rect(size2)));
  EXPECT_FALSE(views[1]->released_front_lock_active());

  for (size_t i = 0; i < renderer_count; ++i) {
    views[i]->Destroy();
    delete hosts[i];
  }
}

TEST_F(RenderWidgetHostViewAuraTest, DiscardDelegatedFramesWithLocking) {
  view_->InitAsChild(nullptr);

  size_t max_renderer_frames =
      FrameEvictionManager::GetInstance()->GetMaxNumberOfSavedFrames();
  ASSERT_LE(2u, max_renderer_frames);
  size_t renderer_count = max_renderer_frames + 1;
  gfx::Rect view_rect(100, 100);
  gfx::Size frame_size = view_rect.size();

  std::unique_ptr<RenderWidgetHostImpl* []> hosts(
      new RenderWidgetHostImpl*[renderer_count]);
  std::unique_ptr<FakeRenderWidgetHostViewAura* []> views(
      new FakeRenderWidgetHostViewAura*[renderer_count]);

  // Create a bunch of renderers.
  for (size_t i = 0; i < renderer_count; ++i) {
    int32_t routing_id = process_host_->GetNextRoutingID();
    delegates_.push_back(base::WrapUnique(new MockRenderWidgetHostDelegate));
    hosts[i] = MockRenderWidgetHostImpl::Create(delegates_.back().get(),
                                                process_host_, routing_id);
    delegates_.back()->set_widget_host(hosts[i]);
    hosts[i]->Init();
    views[i] = new FakeRenderWidgetHostViewAura(
        hosts[i], false, false /* enable_surface_synchronization */);
    views[i]->InitAsChild(nullptr);
    aura::client::ParentWindowWithContext(
        views[i]->GetNativeView(),
        parent_view_->GetNativeView()->GetRootWindow(),
        gfx::Rect());
    views[i]->SetSize(view_rect.size());
  }

  // Make each renderer visible and swap a frame on it. No eviction should
  // occur because all frames are visible.
  for (size_t i = 0; i < renderer_count; ++i) {
    views[i]->Show();
    views[i]->SubmitCompositorFrame(
        i ? local_surface_id_allocator_.GenerateId() : kArbitraryLocalSurfaceId,
        MakeDelegatedFrame(1.f, frame_size, view_rect));
    EXPECT_TRUE(views[i]->HasPrimarySurface());
  }

  // If we hide [0], then [0] should be evicted.
  views[0]->Hide();
  EXPECT_FALSE(views[0]->HasPrimarySurface());

  // If we lock [0] before hiding it, then [0] should not be evicted.
  views[0]->Show();
  views[0]->SubmitCompositorFrame(
      kArbitraryLocalSurfaceId, MakeDelegatedFrame(1.f, frame_size, view_rect));
  EXPECT_TRUE(views[0]->HasPrimarySurface());
  views[0]->GetDelegatedFrameHost()->LockResources();
  views[0]->Hide();
  EXPECT_TRUE(views[0]->HasPrimarySurface());

  // If we unlock [0] now, then [0] should be evicted.
  views[0]->GetDelegatedFrameHost()->UnlockResources();
  EXPECT_FALSE(views[0]->HasPrimarySurface());

  for (size_t i = 0; i < renderer_count; ++i) {
    views[i]->Destroy();
    delete hosts[i];
  }
}

// Test that changing the memory pressure should delete saved frames. This test
// only applies to ChromeOS.
TEST_F(RenderWidgetHostViewAuraTest, DiscardDelegatedFramesWithMemoryPressure) {
  view_->InitAsChild(nullptr);

  // The test logic below relies on having max_renderer_frames > 2.  By default,
  // this value is calculated from total physical memory and causes the test to
  // fail when run on hardware with < 256MB of RAM.
  const size_t kMaxRendererFrames = 5;
  FrameEvictionManager::GetInstance()->set_max_number_of_saved_frames(
      kMaxRendererFrames);

  size_t renderer_count = kMaxRendererFrames;
  gfx::Rect view_rect(100, 100);
  gfx::Size frame_size = view_rect.size();

  std::unique_ptr<RenderWidgetHostImpl* []> hosts(
      new RenderWidgetHostImpl*[renderer_count]);
  std::unique_ptr<FakeRenderWidgetHostViewAura* []> views(
      new FakeRenderWidgetHostViewAura*[renderer_count]);

  // Create a bunch of renderers.
  for (size_t i = 0; i < renderer_count; ++i) {
    int32_t routing_id = process_host_->GetNextRoutingID();

    delegates_.push_back(base::WrapUnique(new MockRenderWidgetHostDelegate));
    hosts[i] = MockRenderWidgetHostImpl::Create(delegates_.back().get(),
                                                process_host_, routing_id);
    delegates_.back()->set_widget_host(hosts[i]);
    hosts[i]->Init();
    views[i] = new FakeRenderWidgetHostViewAura(
        hosts[i], false, false /* enable_surface_synchronization */);
    views[i]->InitAsChild(nullptr);
    aura::client::ParentWindowWithContext(
        views[i]->GetNativeView(),
        parent_view_->GetNativeView()->GetRootWindow(),
        gfx::Rect());
    views[i]->SetSize(view_rect.size());
  }

  // Make each renderer visible and swap a frame on it. No eviction should
  // occur because all frames are visible.
  for (size_t i = 0; i < renderer_count; ++i) {
    views[i]->Show();
    views[i]->SubmitCompositorFrame(
        kArbitraryLocalSurfaceId,
        MakeDelegatedFrame(1.f, frame_size, view_rect));
    EXPECT_TRUE(views[i]->HasPrimarySurface());
  }

  // If we hide one, it should not get evicted.
  views[0]->Hide();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(views[0]->HasPrimarySurface());
  // Using a lesser memory pressure event however, should evict.
  SimulateMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(views[0]->HasPrimarySurface());

  // Check the same for a higher pressure event.
  views[1]->Hide();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(views[1]->HasPrimarySurface());
  SimulateMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(views[1]->HasPrimarySurface());

  for (size_t i = 0; i < renderer_count; ++i) {
    views[i]->Destroy();
    delete hosts[i];
  }
}

TEST_F(RenderWidgetHostViewAuraTest, SourceEventTypeExistsInLatencyInfo) {
  // WHEEL source exists.
  ui::ScrollEvent scroll(ui::ET_SCROLL, gfx::Point(2, 2), ui::EventTimeForNow(),
                         0, 0, 0, 0, 0, 2);
  view_->OnScrollEvent(&scroll);
  EXPECT_EQ(widget_host_->lastWheelOrTouchEventLatencyInfo.source_event_type(),
            ui::SourceEventType::WHEEL);

  // TOUCH source exists.
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(30, 30), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(20, 20), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(20, 20), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  view_->OnTouchEvent(&press);
  view_->OnTouchEvent(&move);
  EXPECT_EQ(widget_host_->lastWheelOrTouchEventLatencyInfo.source_event_type(),
            ui::SourceEventType::TOUCH);
  view_->OnTouchEvent(&release);
}

// Tests that BeginFrameAcks are forwarded correctly from the
// SwapCompositorFrame and OnDidNotProduceFrame IPCs through DelegatedFrameHost
// and its CompositorFrameSinkSupport.
TEST_F(RenderWidgetHostViewAuraTest, ForwardsBeginFrameAcks) {
  gfx::Rect view_rect(100, 100);
  gfx::Size frame_size = view_rect.size();
  viz::LocalSurfaceId local_surface_id = kArbitraryLocalSurfaceId;

  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(), parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(view_rect.size());

  viz::FakeSurfaceObserver observer;
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  viz::SurfaceManager* surface_manager = factory->GetContextFactoryPrivate()
                                             ->GetFrameSinkManager()
                                             ->surface_manager();
  surface_manager->AddObserver(&observer);

  view_->SetNeedsBeginFrames(true);
  uint32_t source_id = 10;

  {
    // Ack from CompositorFrame is forwarded.
    viz::BeginFrameAck ack(source_id, 5, true);
    viz::CompositorFrame frame = MakeDelegatedFrame(1.f, frame_size, view_rect);
    frame.metadata.begin_frame_ack = ack;
    view_->SubmitCompositorFrame(local_surface_id, std::move(frame));
    view_->RunOnCompositingDidCommit();
    EXPECT_EQ(ack, observer.last_ack());
  }

  {
    // Explicit ack through OnDidNotProduceFrame is forwarded.
    viz::BeginFrameAck ack(source_id, 6, false);
    view_->OnDidNotProduceFrame(ack);
    EXPECT_EQ(ack, observer.last_ack());
  }

  surface_manager->RemoveObserver(&observer);
  view_->SetNeedsBeginFrames(false);
}

class RenderWidgetHostViewAuraCopyRequestTest
    : public RenderWidgetHostViewAuraShutdownTest {
 public:
  RenderWidgetHostViewAuraCopyRequestTest()
      : callback_count_(0),
        result_(false),
        frame_subscriber_(nullptr),
        tick_clock_(nullptr),
        view_rect_(100, 100) {}

  void CallbackMethod(bool result) {
    result_ = result;
    callback_count_++;
    quit_closure_.Run();
  }

  void RunLoopUntilCallback() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    // Temporarily ignore real draw requests.
    frame_subscriber_->set_should_capture(false);
    run_loop.Run();
    frame_subscriber_->set_should_capture(true);
  }

  void InitializeView() {
    view_->InitAsChild(nullptr);
    view_->GetDelegatedFrameHost()->SetRequestCopyOfOutputCallbackForTesting(
        base::Bind(&FakeRenderWidgetHostViewAura::InterceptCopyOfOutput,
                   base::Unretained(view_)));
    aura::client::ParentWindowWithContext(
        view_->GetNativeView(), parent_view_->GetNativeView()->GetRootWindow(),
        gfx::Rect());
    view_->SetSize(view_rect_.size());
    view_->Show();

    frame_subscriber_ = new FakeFrameSubscriber(
        view_rect_.size(),
        base::Bind(&RenderWidgetHostViewAuraCopyRequestTest::CallbackMethod,
                   base::Unretained(this)));
    view_->BeginFrameSubscription(base::WrapUnique(frame_subscriber_));
    ASSERT_EQ(0, callback_count_);
    ASSERT_FALSE(view_->last_copy_request_);
  }

  void InstallFakeTickClock() {
    // Create a fake tick clock and transfer ownership to the frame host.
    tick_clock_ = new base::SimpleTestTickClock();
    view_->GetDelegatedFrameHost()->tick_clock_ = base::WrapUnique(tick_clock_);
  }

  void SubmitCompositorFrame() {
    view_->SubmitCompositorFrame(
        kArbitraryLocalSurfaceId,
        MakeDelegatedFrame(1.f, view_rect_.size(), view_rect_));
    viz::SurfaceId surface_id =
        view_->GetDelegatedFrameHost()->SurfaceIdForTesting();
    if (surface_id.is_valid())
      view_->GetDelegatedFrameHost()->WillDrawSurface(
          surface_id.local_surface_id(), view_rect_);
    ASSERT_TRUE(view_->last_copy_request_);
  }

  void ReleaseSwappedFrame() {
    std::unique_ptr<viz::CopyOutputRequest> request =
        std::move(view_->last_copy_request_);
    request->SendResult(std::make_unique<viz::CopyOutputTextureResult>(
        view_rect_, request->texture_mailbox(),
        viz::SingleReleaseCallback::Create(
            base::Bind([](const gpu::SyncToken&, bool) {}))));
    RunLoopUntilCallback();
  }

  void SubmitCompositorFrameAndRelease() {
    SubmitCompositorFrame();
    ReleaseSwappedFrame();
  }

  void RunOnCompositingDidCommitAndReleaseFrame() {
    view_->RunOnCompositingDidCommit();
    ReleaseSwappedFrame();
  }

  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) {
    view_->GetDelegatedFrameHost()->OnUpdateVSyncParameters(timebase, interval);
  }

  base::TimeTicks vsync_timebase() {
    return view_->GetDelegatedFrameHost()->vsync_timebase_;
  }

  base::TimeDelta vsync_interval() {
    return view_->GetDelegatedFrameHost()->vsync_interval_;
  }

  int callback_count_;
  bool result_;
  FakeFrameSubscriber* frame_subscriber_;  // Owned by |view_|.
  base::SimpleTestTickClock* tick_clock_;  // Owned by DelegatedFrameHost.
  const gfx::Rect view_rect_;

 private:
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAuraCopyRequestTest);
};

// Tests that only one copy/readback request will be executed per one browser
// composite operation, even when multiple render frame swaps occur in between
// browser composites, and even if the frame subscriber desires more frames than
// the number of browser composites.
TEST_F(RenderWidgetHostViewAuraCopyRequestTest, DedupeFrameSubscriberRequests) {
  InitializeView();
  int expected_callback_count = 0;

  // Normal case: A browser composite executes for each render frame swap.
  for (int i = 0; i < 3; ++i) {
    // Renderer provides another frame and the Browser composites with the
    // frame, executing the copy request, and then the result is delivered.
    SubmitCompositorFrame();
    RunOnCompositingDidCommitAndReleaseFrame();

    // The callback should be run with success status.
    ++expected_callback_count;
    ASSERT_EQ(expected_callback_count, callback_count_);
    EXPECT_TRUE(result_);
  }

  // De-duping case: One browser composite executes per varied number of render
  // frame swaps.
  for (int i = 0; i < 3; ++i) {
    const int num_swaps = 1 + i % 3;

    // The renderer provides |num_swaps| frames.
    for (int j = 0; j < num_swaps; ++j) {
      SubmitCompositorFrame();
      if (j > 0) {
        ++expected_callback_count;
        ASSERT_EQ(expected_callback_count, callback_count_);
        EXPECT_FALSE(result_);  // The prior copy request was aborted.
      }
    }

    // Browser composites with the frame, executing the last copy request that
    // was made, and then the result is delivered.
    RunOnCompositingDidCommitAndReleaseFrame();

    // The final callback should be run with success status.
    ++expected_callback_count;
    ASSERT_EQ(expected_callback_count, callback_count_);
    EXPECT_TRUE(result_);
  }

  // Destroy the RenderWidgetHostViewAura and ImageTransportFactory.
  TearDownEnvironment();
}

TEST_F(RenderWidgetHostViewAuraCopyRequestTest, DestroyedAfterCopyRequest) {
  InitializeView();

  SubmitCompositorFrame();
  EXPECT_EQ(0, callback_count_);
  EXPECT_TRUE(view_->last_copy_request_);
  EXPECT_TRUE(view_->last_copy_request_->has_texture_mailbox());

  // Notify DelegatedFrameHost that the copy requests were moved to the
  // compositor thread by calling OnCompositingDidCommit().
  //
  // Send back the mailbox included in the request. There's no release callback
  // since the mailbox came from the RWHVA originally.
  RunOnCompositingDidCommitAndReleaseFrame();

  // The callback should succeed.
  EXPECT_EQ(1, callback_count_);
  EXPECT_TRUE(result_);

  SubmitCompositorFrame();
  EXPECT_EQ(1, callback_count_);
  std::unique_ptr<viz::CopyOutputRequest> request =
      std::move(view_->last_copy_request_);

  // Destroy the RenderWidgetHostViewAura and ImageTransportFactory.
  TearDownEnvironment();

  // Send the result after-the-fact.  It goes nowhere since DelegatedFrameHost
  // has been destroyed.  CopyOutputRequest auto-sends an empty result upon
  // destruction.
  request.reset();

  // Because the copy request callback may be holding state within it, that
  // state must handle the RWHVA and ImageTransportFactory going away before the
  // callback is called. This test passes if it does not crash as a result of
  // these things being destroyed.
  EXPECT_EQ(2, callback_count_);
  EXPECT_FALSE(result_);
}

TEST_F(RenderWidgetHostViewAuraCopyRequestTest, PresentTime) {
  InitializeView();
  InstallFakeTickClock();

  // Verify our initial state.
  EXPECT_EQ(base::TimeTicks(), frame_subscriber_->last_present_time());
  EXPECT_EQ(base::TimeTicks(), tick_clock_->NowTicks());

  // Start our fake clock from a non-zero, but not an even multiple of the
  // interval, value to differentiate it from our initialization state.
  const base::TimeDelta kDefaultInterval =
      viz::BeginFrameArgs::DefaultInterval();
  tick_clock_->Advance(kDefaultInterval / 3);

  // Swap the first frame without any vsync information.
  ASSERT_EQ(base::TimeTicks(), vsync_timebase());
  ASSERT_EQ(base::TimeDelta(), vsync_interval());

  // During this first call, there is no known vsync information, so while the
  // callback should succeed the present time is effectively just current time.
  SubmitCompositorFrameAndRelease();
  EXPECT_EQ(tick_clock_->NowTicks(), frame_subscriber_->last_present_time());

  // Now initialize the vsync parameters with a null timebase, but a known vsync
  // interval; which should give us slightly better frame time estimates.
  OnUpdateVSyncParameters(base::TimeTicks(), kDefaultInterval);
  ASSERT_EQ(base::TimeTicks(), vsync_timebase());
  ASSERT_EQ(kDefaultInterval, vsync_interval());

  // Now that we have a vsync interval, the presentation time estimate should be
  // the nearest presentation interval, which is just kDefaultInterval since our
  // tick clock is initialized to a time before that.
  SubmitCompositorFrameAndRelease();
  EXPECT_EQ(base::TimeTicks() + kDefaultInterval,
            frame_subscriber_->last_present_time());

  // Now initialize the vsync parameters with a valid timebase and a known vsync
  // interval; which should give us the best frame time estimates.
  const base::TimeTicks kBaseTime = tick_clock_->NowTicks();
  OnUpdateVSyncParameters(kBaseTime, kDefaultInterval);
  ASSERT_EQ(kBaseTime, vsync_timebase());
  ASSERT_EQ(kDefaultInterval, vsync_interval());

  // Now that we have a vsync interval and a timebase, the presentation time
  // should be based on the number of vsync intervals which have elapsed since
  // the vsync timebase.  Advance time by a non integer number of intervals to
  // verify.
  const double kElapsedIntervals = 2.5;
  tick_clock_->Advance(kDefaultInterval * kElapsedIntervals);
  SubmitCompositorFrameAndRelease();
  EXPECT_EQ(kBaseTime + kDefaultInterval * std::ceil(kElapsedIntervals),
            frame_subscriber_->last_present_time());

  // Destroy the RenderWidgetHostViewAura and ImageTransportFactory.
  TearDownEnvironment();
}

TEST_F(RenderWidgetHostViewAuraTest, VisibleViewportTest) {
  gfx::Rect view_rect(100, 100);

  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(view_rect.size());
  view_->Show();

  // Defaults to full height of the view.
  EXPECT_EQ(100, view_->GetVisibleViewportSize().height());

  widget_host_->ResetSizeAndRepaintPendingFlags();
  sink_->ClearMessages();
  view_->SetInsets(gfx::Insets(0, 0, 40, 0));

  EXPECT_EQ(60, view_->GetVisibleViewportSize().height());

  const IPC::Message *message = sink_->GetFirstMessageMatching(
      ViewMsg_Resize::ID);
  ASSERT_TRUE(message != nullptr);

  ViewMsg_Resize::Param params;
  ViewMsg_Resize::Read(message, &params);
  EXPECT_EQ(60, std::get<0>(params).visible_viewport_size.height());
}

// Ensures that touch event positions are never truncated to integers.
TEST_F(RenderWidgetHostViewAuraTest, TouchEventPositionsArentRounded) {
  const float kX = 30.58f;
  const float kY = 50.23f;

  view_->InitAsChild(nullptr);
  view_->Show();

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  press.set_location_f(gfx::PointF(kX, kY));
  press.set_root_location_f(gfx::PointF(kX, kY));

  view_->OnTouchEvent(&press);
  EXPECT_EQ(ui::MotionEvent::ACTION_DOWN, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());
  EXPECT_EQ(kX, pointer_state().GetX(0));
  EXPECT_EQ(kY, pointer_state().GetY(0));
}

void RenderWidgetHostViewAuraOverscrollTest::WheelNotPreciseScrollEvent() {
  SetUpOverscrollEnvironment();

  // Simulate wheel events.
  SimulateWheelEventPossiblyIncludingPhase(
      -5, 0, 0, false, WebMouseWheelEvent::kPhaseBegan);  // sent directly
  SimulateWheelEventPossiblyIncludingPhase(
      -60, 1, 0, false, WebMouseWheelEvent::kPhaseChanged);  // enqueued
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ("MouseWheel", GetInputMessageTypes());

  // Receive ACK the first wheel event as not processed.
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  ExpectGestureScrollEventsAfterMouseWheelACK(true, 1);

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  if (wheel_scrolling_mode_ != kAsyncWheelEvents) {
    ExpectGestureScrollEndForWheelScrolling(false);
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelACK(false, 0);
  }

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  if (wheel_scrolling_mode_ == kAsyncWheelEvents) {
    SimulateWheelEventWithPhase(0, 0, 0, true, WebMouseWheelEvent::kPhaseEnded);
    EXPECT_EQ("MouseWheel GestureScrollEnd", GetInputMessageTypes());
  } else if (wheel_scroll_latching_enabled_) {
    SimulateWheelEventWithPhase(0, 0, 0, true, WebMouseWheelEvent::kPhaseEnded);
    EXPECT_EQ("MouseWheel", GetInputMessageTypes());
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEndForWheelScrolling(true);
  } else {
    ExpectGestureScrollEndForWheelScrolling(true);
  }

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
}
TEST_F(RenderWidgetHostViewAuraOverscrollTest, WheelNotPreciseScrollEvent) {
  WheelNotPreciseScrollEvent();
}
TEST_F(RenderWidgetHostViewAuraOverscrollWithoutWheelScrollLatchingTest,
       WheelNotPreciseScrollEvent) {
  WheelNotPreciseScrollEvent();
}
TEST_F(RenderWidgetHostViewAuraOverScrollAsyncWheelEventsEnabledTest,
       WheelNotPreciseScrollEvent) {
  WheelNotPreciseScrollEvent();
}

void RenderWidgetHostViewAuraOverscrollTest::WheelScrollEventOverscrolls() {
  SetUpOverscrollEnvironment();

  // Simulate wheel events.
  SimulateWheelEventPossiblyIncludingPhase(
      -5, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);  // sent directly
  SimulateWheelEventPossiblyIncludingPhase(
      -1, 1, 0, true, WebMouseWheelEvent::kPhaseChanged);  // enqueued
  SimulateWheelEventPossiblyIncludingPhase(
      -10, -3, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEventPossiblyIncludingPhase(
      -15, -1, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEventPossiblyIncludingPhase(
      -30, -3, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEventPossiblyIncludingPhase(
      -20, 6, 1, true,
      WebMouseWheelEvent::kPhaseChanged);  // enqueued, different modifiers
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ("MouseWheel", GetInputMessageTypes());

  // Receive ACK the first wheel event as not processed.
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  ExpectGestureScrollEventsAfterMouseWheelACK(true, 2);

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  if (wheel_scrolling_mode_ != kAsyncWheelEvents) {
    ExpectGestureScrollEndForWheelScrolling(false);
    // Receive ACK for the second (coalesced) event as not processed. This will
    // start a back navigation. However, this will also cause the queued next
    // event to be sent to the renderer. But since overscroll navigation has
    // started, that event will also be included in the overscroll computation
    // instead of being sent to the renderer. So the result will be an
    // overscroll back navigation, and no ScrollUpdate event will be sent to the
    // renderer.
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelACK(false, 1);
  }
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ExpectGestureScrollEndForWheelScrolling(false);

  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ExpectGestureScrollEventsAfterMouseWheelAckWhenOverscrolling();

  EXPECT_EQ(OVERSCROLL_WEST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  EXPECT_EQ(OVERSCROLL_WEST, overscroll_delegate()->current_mode());
  EXPECT_EQ(-81.f, overscroll_delta_x());
  EXPECT_EQ(-31.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, sink_->message_count());

  // Send a mouse-move event. This should cancel the overscroll navigation.
  SimulateMouseMove(5, 10, 0);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(1U, sink_->message_count());
}
TEST_F(RenderWidgetHostViewAuraOverscrollTest, WheelScrollEventOverscrolls) {
  WheelScrollEventOverscrolls();
}
TEST_F(RenderWidgetHostViewAuraOverscrollWithoutWheelScrollLatchingTest,
       WheelScrollEventOverscrolls) {
  WheelScrollEventOverscrolls();
}
TEST_F(RenderWidgetHostViewAuraOverScrollAsyncWheelEventsEnabledTest,
       WheelScrollEventOverscrolls) {
  WheelScrollEventOverscrolls();
}

// Tests that if some scroll events are consumed towards the start, then
// subsequent scrolls do not horizontal overscroll.
void RenderWidgetHostViewAuraOverscrollTest::
    WheelScrollConsumedDoNotHorizOverscroll() {
  SetUpOverscrollEnvironment();

  // Simulate wheel events.
  SimulateWheelEventPossiblyIncludingPhase(
      -5, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);  // sent directly
  SimulateWheelEventPossiblyIncludingPhase(
      -1, -1, 0, true, WebMouseWheelEvent::kPhaseChanged);  // enqueued
  SimulateWheelEventPossiblyIncludingPhase(
      -10, -3, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEventPossiblyIncludingPhase(
      -15, -1, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEventPossiblyIncludingPhase(
      -30, -3, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEventPossiblyIncludingPhase(
      -20, 6, 1, true,
      WebMouseWheelEvent::kPhaseChanged);  // enqueued, different modifiers

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ("MouseWheel", GetInputMessageTypes());

  // Receive ACK the first wheel event as processed.
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  ExpectGestureScrollEventsAfterMouseWheelACK(true, 2);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  if (wheel_scrolling_mode_ != kAsyncWheelEvents) {
    SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_CONSUMED);
    ExpectGestureScrollEndForWheelScrolling(false);
    // Receive ACK for the second (coalesced) event as not processed. This
    // should not initiate overscroll, since the beginning of the scroll has
    // been consumed. The queued event with different modifiers should be sent
    // to the renderer.
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelACK(false, 1);
  }
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());

  if (wheel_scrolling_mode_ == kAsyncWheelEvents) {
    // The GSU events are coalesced. This is the ack for the coalesced event.
    // Since it is the first GSU, the ack should be consumed.
    SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_CONSUMED);
  } else {
    SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEndForWheelScrolling(false);
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelACK(false, 0);
    SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  }

  if (wheel_scrolling_mode_ == kAsyncWheelEvents) {
    SimulateWheelEventWithPhase(0, 0, 0, true, WebMouseWheelEvent::kPhaseEnded);
    EXPECT_EQ("MouseWheel GestureScrollEnd", GetInputMessageTypes());
  } else if (wheel_scroll_latching_enabled_) {
    SimulateWheelEventWithPhase(0, 0, 0, true, WebMouseWheelEvent::kPhaseEnded);
    EXPECT_EQ("MouseWheel", GetInputMessageTypes());
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEndForWheelScrolling(true);
  } else {
    ExpectGestureScrollEndForWheelScrolling(true);
  }
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
}
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       WheelScrollConsumedDoNotHorizOverscroll) {
  WheelScrollConsumedDoNotHorizOverscroll();
}
TEST_F(RenderWidgetHostViewAuraOverscrollWithoutWheelScrollLatchingTest,
       WheelScrollConsumedDoNotHorizOverscroll) {
  WheelScrollConsumedDoNotHorizOverscroll();
}
TEST_F(RenderWidgetHostViewAuraOverScrollAsyncWheelEventsEnabledTest,
       WheelScrollConsumedDoNotHorizOverscroll) {
  WheelScrollConsumedDoNotHorizOverscroll();
}

// Tests that wheel-scrolling correctly turns overscroll on and off.
void RenderWidgetHostViewAuraOverscrollTest::WheelScrollOverscrollToggle() {
  SetUpOverscrollEnvironment();

  // Send a wheel event. ACK the event as not processed. This should not
  // initiate an overscroll gesture since it doesn't cross the threshold yet.
  SimulateWheelEventPossiblyIncludingPhase(10, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseBegan);
  EXPECT_EQ("MouseWheel", GetInputMessageTypes());
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  ExpectGestureScrollEventsAfterMouseWheelACK(true, 0);

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Scroll some more so as to not overscroll.
  SimulateWheelEventPossiblyIncludingPhase(10, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseChanged);
  if (wheel_scrolling_mode_ == kAsyncWheelEvents) {
    ExpectGestureScrollUpdateAfterNonBlockingMouseWheelACK(false);
  } else {
    EXPECT_EQ("MouseWheel", GetInputMessageTypes());
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelACK(false, 0);
  }

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Scroll some more to initiate an overscroll.
  SimulateWheelEventPossiblyIncludingPhase(40, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseChanged);
  if (wheel_scrolling_mode_ == kAsyncWheelEvents) {
    ExpectGestureScrollUpdateAfterNonBlockingMouseWheelACK(false);
  } else {
    EXPECT_EQ("MouseWheel", GetInputMessageTypes());
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelACK(false, 0);
  }

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(60.f, overscroll_delta_x());
  EXPECT_EQ(10.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // Scroll in the reverse direction enough to abort the overscroll.
  SimulateWheelEventPossiblyIncludingPhase(-20, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseChanged);
  EXPECT_EQ("MouseWheel", GetInputMessageTypes());
  if (wheel_scrolling_mode_ != kAsyncWheelEvents) {
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelAckWhenOverscrolling();
  }
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Continue to scroll in the reverse direction.
  SimulateWheelEventPossiblyIncludingPhase(-20, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseChanged);

  EXPECT_EQ("MouseWheel", GetInputMessageTypes());
  if (wheel_scrolling_mode_ != kAsyncWheelEvents) {
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelAckWhenOverscrolling();
  }

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Continue to scroll in the reverse direction enough to initiate overscroll
  // in that direction. However, overscroll should not be initiated as the
  // overscroll mode is locked to east mode.
  SimulateWheelEventPossiblyIncludingPhase(-55, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseChanged);
  EXPECT_EQ("MouseWheel", GetInputMessageTypes());
  if (wheel_scrolling_mode_ != kAsyncWheelEvents) {
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelAckWhenOverscrolling();
  }

  if (wheel_scrolling_mode_ == kAsyncWheelEvents) {
    SimulateWheelEventWithPhase(0, 0, 0, true, WebMouseWheelEvent::kPhaseEnded);
    EXPECT_EQ("MouseWheel GestureScrollEnd", GetInputMessageTypes());
  } else if (wheel_scroll_latching_enabled_) {
    SimulateWheelEventWithPhase(0, 0, 0, true, WebMouseWheelEvent::kPhaseEnded);
    EXPECT_EQ("MouseWheel", GetInputMessageTypes());
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes());
  }

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(-95.f, overscroll_delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
}
TEST_F(RenderWidgetHostViewAuraOverscrollTest, WheelScrollOverscrollToggle) {
  WheelScrollOverscrollToggle();
}
TEST_F(RenderWidgetHostViewAuraOverscrollWithoutWheelScrollLatchingTest,
       WheelScrollOverscrollToggle) {
  WheelScrollOverscrollToggle();
}
TEST_F(RenderWidgetHostViewAuraOverScrollAsyncWheelEventsEnabledTest,
       WheelScrollOverscrollToggle) {
  WheelScrollOverscrollToggle();
}

void RenderWidgetHostViewAuraOverscrollTest::ScrollEventsOverscrollWithFling() {
  SetUpOverscrollEnvironment();

  // Send a wheel event. ACK the event as not processed. This should not
  // initiate an overscroll gesture since it doesn't cross the threshold yet.
  SimulateWheelEventPossiblyIncludingPhase(10, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseBegan);
  EXPECT_EQ("MouseWheel", GetInputMessageTypes());
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  ExpectGestureScrollEventsAfterMouseWheelACK(true, 0);

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Scroll some more so as to not overscroll.
  SimulateWheelEventPossiblyIncludingPhase(20, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseChanged);
  if (wheel_scrolling_mode_ == kAsyncWheelEvents) {
    ExpectGestureScrollUpdateAfterNonBlockingMouseWheelACK(false);
  } else {
    EXPECT_EQ("MouseWheel", GetInputMessageTypes());
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelACK(false, 0);
  }
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  sink_->ClearMessages();

  // Scroll some more to initiate an overscroll.
  SimulateWheelEventPossiblyIncludingPhase(30, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseChanged);
  if (wheel_scrolling_mode_ == kAsyncWheelEvents) {
    ExpectGestureScrollUpdateAfterNonBlockingMouseWheelACK(false);
  } else {
    EXPECT_EQ("MouseWheel", GetInputMessageTypes());
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelACK(false, 0);
  }
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());

  EXPECT_EQ(60.f, overscroll_delta_x());
  EXPECT_EQ(10.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // Send a fling start, but with a small velocity, so that the overscroll is
  // aborted. The fling should proceed to the renderer, through the gesture
  // event filter.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureFlingStartEvent(0.f, 0.1f, blink::kWebGestureDeviceTouchpad);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());

  EXPECT_EQ("GestureScrollBegin GestureFlingStart", GetInputMessageTypes());
}
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       ScrollEventsOverscrollWithFling) {
  ScrollEventsOverscrollWithFling();
}
TEST_F(RenderWidgetHostViewAuraOverscrollWithoutWheelScrollLatchingTest,
       ScrollEventsOverscrollWithFling) {
  ScrollEventsOverscrollWithFling();
}
TEST_F(RenderWidgetHostViewAuraOverScrollAsyncWheelEventsEnabledTest,
       ScrollEventsOverscrollWithFling) {
  ScrollEventsOverscrollWithFling();
}

// Same as ScrollEventsOverscrollWithFling, but with zero velocity. Checks that
// the zero-velocity fling does not reach the renderer.
void RenderWidgetHostViewAuraOverscrollTest::
    ScrollEventsOverscrollWithZeroFling() {
  SetUpOverscrollEnvironment();

  // Send a wheel event. ACK the event as not processed. This should not
  // initiate an overscroll gesture since it doesn't cross the threshold yet.
  SimulateWheelEventPossiblyIncludingPhase(10, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseBegan);
  EXPECT_EQ("MouseWheel", GetInputMessageTypes());
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  ExpectGestureScrollEventsAfterMouseWheelACK(true, 0);

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Scroll some more so as to not overscroll.
  SimulateWheelEventPossiblyIncludingPhase(20, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseChanged);
  if (wheel_scrolling_mode_ == kAsyncWheelEvents) {
    ExpectGestureScrollUpdateAfterNonBlockingMouseWheelACK(false);
  } else {
    EXPECT_EQ("MouseWheel", GetInputMessageTypes());
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelACK(false, 0);
  }

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Scroll some more to initiate an overscroll.
  SimulateWheelEventPossiblyIncludingPhase(30, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseChanged);
  if (wheel_scrolling_mode_ == kAsyncWheelEvents) {
    ExpectGestureScrollUpdateAfterNonBlockingMouseWheelACK(false);
  } else {
    EXPECT_EQ("MouseWheel", GetInputMessageTypes());
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelACK(false, 0);
  }

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());

  EXPECT_EQ(60.f, overscroll_delta_x());
  EXPECT_EQ(10.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // Send a fling start, but with a small velocity, so that the overscroll is
  // aborted. The fling should proceed to the renderer, through the gesture
  // event filter.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureFlingStartEvent(10.f, 0.f, blink::kWebGestureDeviceTouchpad);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());

  EXPECT_EQ("GestureScrollBegin GestureFlingStart", GetInputMessageTypes());
}
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       ScrollEventsOverscrollWithZeroFling) {
  ScrollEventsOverscrollWithZeroFling();
}
TEST_F(RenderWidgetHostViewAuraOverscrollWithoutWheelScrollLatchingTest,
       ScrollEventsOverscrollWithZeroFling) {
  ScrollEventsOverscrollWithZeroFling();
}
TEST_F(RenderWidgetHostViewAuraOverScrollAsyncWheelEventsEnabledTest,
       ScrollEventsOverscrollWithZeroFling) {
  ScrollEventsOverscrollWithZeroFling();
}

// Tests that a fling in the opposite direction of the overscroll cancels the
// overscroll nav instead of completing it.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, ReverseFlingCancelsOverscroll) {
  SetUpOverscrollEnvironment();

  {
    // Start and end a gesture in the same direction without processing the
    // gesture events in the renderer. This should initiate and complete an
    // overscroll navigation.
    SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                         blink::kWebGestureDeviceTouchscreen);
    SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
    SimulateGestureScrollUpdateEvent(300, -5, 0);
    SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
    EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
    EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
    sink_->ClearMessages();

    SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                         blink::kWebGestureDeviceTouchscreen);
    EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->completed_mode());
    EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
    EXPECT_EQ(1U, sink_->message_count());
  }

  {
    // Start over, except instead of ending the gesture with ScrollEnd, end it
    // with a FlingStart, with velocity in the reverse direction. This should
    // initiate an overscroll navigation, but it should be cancelled because of
    // the fling in the opposite direction.
    overscroll_delegate()->Reset();
    SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                         blink::kWebGestureDeviceTouchscreen);
    SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
    SimulateGestureScrollUpdateEvent(-300, -5, 0);
    SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(OVERSCROLL_WEST, overscroll_mode());
    EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
    EXPECT_EQ(OVERSCROLL_WEST, overscroll_delegate()->current_mode());
    sink_->ClearMessages();

    SimulateGestureFlingStartEvent(100, 0, blink::kWebGestureDeviceTouchscreen);
    EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->completed_mode());
    EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
    EXPECT_EQ(1U, sink_->message_count());
  }
}

// Tests that touch-scroll events are handled correctly by the overscroll
// controller. This also tests that the overscroll controller and the
// gesture-event filter play nice with each other.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, GestureScrollOverscrolls) {
  SetUpOverscrollEnvironment();

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ("GestureScrollBegin", GetInputMessageTypes());

  // Send another gesture event and ACK as not being processed. This should
  // initiate the navigation gesture.
  SimulateGestureScrollUpdateEvent(55, -5, 0);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, overscroll_delta_x());
  EXPECT_EQ(-5.f, overscroll_delta_y());
  EXPECT_EQ(5.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetInputMessageTypes());

  // Send another gesture update event. This event should be consumed by the
  // controller, and not be forwarded to the renderer. The gesture-event filter
  // should not also receive this event.
  SimulateGestureScrollUpdateEvent(10, -5, 0);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(65.f, overscroll_delta_x());
  EXPECT_EQ(-10.f, overscroll_delta_y());
  EXPECT_EQ(15.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, sink_->message_count());

  // Now send a scroll end. This should cancel the overscroll gesture, and send
  // the event to the renderer. The gesture-event filter should receive this
  // event.
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes());
}

// Tests that when a cap is set for overscroll delta, extra overscroll delta is
// ignored.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, OverscrollDeltaCap) {
  SetUpOverscrollEnvironment();

  // Set overscroll cap and start scrolling.
  overscroll_delegate()->set_delta_cap(50);
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ("GestureScrollBegin", GetInputMessageTypes());

  // Scroll enough to initiate the overscrolling.
  SimulateGestureScrollUpdateEvent(55, -5, 0);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, overscroll_delta_x());
  EXPECT_EQ(-5.f, overscroll_delta_y());
  EXPECT_EQ(5.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetInputMessageTypes());

  // Scroll beyond overscroll cap. Overscroll delta should not surpass the cap.
  SimulateGestureScrollUpdateEvent(75, -5, 0);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(100.f, overscroll_delta_x());
  EXPECT_EQ(-10.f, overscroll_delta_y());
  EXPECT_EQ(50.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, sink_->message_count());

  // Scroll back a bit. Since the extra scroll after cap in previous step is
  // ignored, scrolling back should immediately reduce overscroll delta.
  SimulateGestureScrollUpdateEvent(-10, -5, 0);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(90.f, overscroll_delta_x());
  EXPECT_EQ(-15.f, overscroll_delta_y());
  EXPECT_EQ(40.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, sink_->message_count());

  // End overscrolling.
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes());
}

// Tests that if the page is scrolled because of a scroll-gesture, then that
// particular scroll sequence never generates overscroll if the scroll direction
// is horizontal.
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       GestureScrollConsumedHorizontal) {
  SetUpOverscrollEnvironment();

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureScrollUpdateEvent(10, 0, 0);

  // Start scrolling on content. ACK both events as being processed.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  sink_->ClearMessages();

  // Send another gesture event and ACK as not being processed. This should
  // not initiate overscroll because the beginning of the scroll event did
  // scroll some content on the page. Since there was no overscroll, the event
  // should reach the renderer.
  SimulateGestureScrollUpdateEvent(55, 0, 0);
  EXPECT_EQ("GestureScrollUpdate", GetInputMessageTypes());
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
}

// Tests that the overscroll controller plays nice with touch-scrolls and the
// gesture event filter with debounce filtering turned on.
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       GestureScrollDebounceOverscrolls) {
  SetUpOverscrollEnvironmentWithDebounce(100);

  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ("GestureScrollBegin", GetInputMessageTypes());

  // Send update events.
  SimulateGestureScrollUpdateEvent(25, 0, 0);
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetInputMessageTypes());

  // Quickly end and restart the scroll gesture. These two events should get
  // discarded.
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, sink_->message_count());

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, sink_->message_count());

  // Send another update event. This should be sent right away.
  SimulateGestureScrollUpdateEvent(30, 0, 0);
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetInputMessageTypes());

  // Receive an ACK for the first scroll-update event as not being processed.
  // This will contribute to the overscroll gesture, but not enough for the
  // overscroll controller to start consuming gesture events.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  // The second GSU was already sent.
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // Send another update event. This should be forwarded immediately since
  // GestureEventQueue allows multiple in-flight events.
  SimulateGestureScrollUpdateEvent(10, 0, 0);
  EXPECT_EQ("GestureScrollUpdate", GetInputMessageTypes());

  // Receive an ACK for the second scroll-update event as not being processed.
  // This will now initiate an overscroll.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, overscroll_delta_x());
  EXPECT_EQ(5.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, sink_->message_count());

  // Receive an ACK for the last scroll-update event as not being processed.
  // This will be consumed by the overscroll controller.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(65.f, overscroll_delta_x());
  EXPECT_EQ(15.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, sink_->message_count());
}

// Tests that the gesture debounce timer plays nice with the overscroll
// controller.
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       GestureScrollDebounceTimerOverscroll) {
  SetUpOverscrollEnvironmentWithDebounce(10);

  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ("GestureScrollBegin", GetInputMessageTypes());

  // Send update events.
  SimulateGestureScrollUpdateEvent(55, 0, 0);
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetInputMessageTypes());

  // Send an end event. This should get in the debounce queue.
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, sink_->message_count());

  // Receive ACK for the scroll-update event.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, overscroll_delta_x());
  EXPECT_EQ(5.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, sink_->message_count());

  // Let the timer for the debounce queue fire. That should release the queued
  // scroll-end event. Since overscroll has started, but there hasn't been
  // enough overscroll to complete the gesture, the overscroll controller
  // will reset the state. The scroll-end should therefore be dispatched to the
  // renderer, and the gesture-event-filter should await an ACK for it.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      base::TimeDelta::FromMilliseconds(15));
  base::RunLoop().Run();

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes());
}

// Tests that when touch-events are dispatched to the renderer, the overscroll
// gesture deals with them correctly.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, OverscrollWithTouchEvents) {
  SetUpOverscrollEnvironmentWithDebounce(10);
  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  sink_->ClearMessages();

  // The test sends an intermingled sequence of touch and gesture events.
  PressTouchPoint(0, 1);
  uint32_t touch_press_event_id1 = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::kTouchStart,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED, touch_press_event_id1);
  EXPECT_EQ("TouchStart", GetInputMessageTypes());

  MoveTouchPoint(0, 20, 5);
  uint32_t touch_move_event_id1 = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::kTouchMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED, touch_move_event_id1);
  EXPECT_EQ("TouchMove", GetInputMessageTypes());

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ("GestureScrollBegin", GetInputMessageTypes());
  SimulateGestureScrollUpdateEvent(20, 0, 0);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetInputMessageTypes());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Another touch move event should reach the renderer since overscroll hasn't
  // started yet.  Note that touch events sent during the scroll period may
  // not require an ack (having been marked uncancelable).
  MoveTouchPoint(0, 65, 10);
  SendTouchEvent();
  AckLastSentInputEventIfNecessary(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ("TouchMove", GetInputMessageTypes());

  SimulateGestureScrollUpdateEvent(45, 0, 0);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(65.f, overscroll_delta_x());
  EXPECT_EQ(15.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  EXPECT_EQ("GestureScrollUpdate", GetInputMessageTypes());

  // Send another touch event. The page should get the touch-move event, even
  // though overscroll has started.
  MoveTouchPoint(0, 55, 5);
  SendTouchEvent();
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(65.f, overscroll_delta_x());
  EXPECT_EQ(15.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  AckLastSentInputEventIfNecessary(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ("TouchMove", GetInputMessageTypes());

  SimulateGestureScrollUpdateEvent(-10, 0, 0);
  EXPECT_EQ(0U, sink_->message_count());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, overscroll_delta_x());
  EXPECT_EQ(5.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  PressTouchPoint(255, 5);
  SendTouchEvent();
  AckLastSentInputEventIfNecessary(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ("TouchStart", GetInputMessageTypes());

  SimulateGestureScrollUpdateEvent(200, 0, 0);
  EXPECT_EQ(0U, sink_->message_count());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(255.f, overscroll_delta_x());
  EXPECT_EQ(205.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // The touch-end/cancel event should always reach the renderer if the page has
  // touch handlers.
  ReleaseTouchPoint(1);
  SendTouchEvent();
  AckLastSentInputEventIfNecessary(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ("TouchEnd", GetInputMessageTypes());
  ReleaseTouchPoint(0);
  SendTouchEvent();
  AckLastSentInputEventIfNecessary(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ("TouchEnd", GetInputMessageTypes());

  SimulateGestureEvent(blink::WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      base::TimeDelta::FromMilliseconds(10));
  base::RunLoop().Run();
  EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->completed_mode());
}

// Tests that touch-gesture end is dispatched to the renderer at the end of a
// touch-gesture initiated overscroll.
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       TouchGestureEndDispatchedAfterOverscrollComplete) {
  SetUpOverscrollEnvironmentWithDebounce(10);
  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  sink_->ClearMessages();

  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ("GestureScrollBegin", GetInputMessageTypes());
  // The scroll begin event will have received a synthetic ack from the input
  // router.
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Send update events.
  SimulateGestureScrollUpdateEvent(55, -5, 0);
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetInputMessageTypes());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, sink_->message_count());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, overscroll_delta_x());
  EXPECT_EQ(5.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // Send end event.
  SimulateGestureEvent(blink::WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, sink_->message_count());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      base::TimeDelta::FromMilliseconds(10));
  base::RunLoop().Run();
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->completed_mode());
  EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes());

  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ("GestureScrollBegin", GetInputMessageTypes());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Send update events.
  SimulateGestureScrollUpdateEvent(235, -5, 0);
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetInputMessageTypes());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, sink_->message_count());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(235.f, overscroll_delta_x());
  EXPECT_EQ(185.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // Send end event.
  SimulateGestureEvent(blink::WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, sink_->message_count());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      base::TimeDelta::FromMilliseconds(10));
  base::RunLoop().Run();
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->completed_mode());
  EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes());
}

TEST_F(RenderWidgetHostViewAuraOverscrollTest, OverscrollDirectionChange) {
  SetUpOverscrollEnvironmentWithDebounce(100);

  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ("GestureScrollBegin", GetInputMessageTypes());

  // Send update events and receive ack as not consumed.
  SimulateGestureScrollUpdateEvent(125, -5, 0);
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetInputMessageTypes());

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(0U, sink_->message_count());

  // Send another update event, but in the reverse direction. Although the
  // overscroll controller is not triggering gesture-nav, it will consume the
  // ScrollUpdate event to prevent content scroll.
  SimulateGestureScrollUpdateEvent(-260, 0, 0);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());

  // Although the overscroll mode has been reset, the next scroll update events
  // should be consumed by the overscroll controller to prevent content scroll.
  SimulateGestureScrollUpdateEvent(-20, 0, 0);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
}

TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       CompleteOverscrollOnGestureScrollEndAck) {
  SetUpOverscrollEnvironment();

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->completed_mode());
  EXPECT_EQ("GestureScrollBegin", GetInputMessageTypes());

  // Send GSU to trigger overscroll.
  SimulateGestureScrollUpdateEvent(300, -5, 0);
  // Send GSE immediately before ACKing GSU.
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);

  // Now ACK the GSU. Should see a completed overscroll.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->completed_mode());
}

TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       InterleavedScrollUpdateAckAndScrollEnd) {
  SetUpOverscrollEnvironment();

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Send the first GSU which shouldn't trigger overscroll.
  SimulateGestureScrollUpdateEvent(30, -5, 0);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(0U, overscroll_delegate()->historical_modes().size());

  // Send the second GSU which should be able to trigger overscroll if combined.
  SimulateGestureScrollUpdateEvent(30, -5, 0);

  // Send GSE immediately before ACKing GSU.
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);

  // Now ACK the second GSU, should see overscroll being triggered and cleared.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(2U, overscroll_delegate()->historical_modes().size());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->historical_modes().at(0));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->historical_modes().at(1));
}

void RenderWidgetHostViewAuraOverscrollTest::
    OverscrollDirectionChangeMouseWheel() {
  SetUpOverscrollEnvironment();

  // Send wheel event and receive ack as not consumed.
  SimulateWheelEventPossiblyIncludingPhase(125, -5, 0, true,
                                           WebMouseWheelEvent::kPhaseBegan);
  EXPECT_EQ("MouseWheel", GetInputMessageTypes());
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  ExpectGestureScrollEventsAfterMouseWheelACK(true, 0);

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());

  // Send another wheel event, but in the reverse direction. Although the
  // overscroll controller is not triggering gesture-nav, it will consume the
  // ScrollUpdate event to prevent content scroll.
  SimulateWheelEventPossiblyIncludingPhase(-260, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseChanged);

  EXPECT_EQ("MouseWheel", GetInputMessageTypes());
  if (wheel_scrolling_mode_ != kAsyncWheelEvents) {
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelAckWhenOverscrolling();
  }

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());

  // Although the overscroll controller consumes ScrollUpdate, it will not
  // initiate west overscroll as it is now locked in east mode.
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  SimulateWheelEventPossiblyIncludingPhase(-20, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseChanged);
  EXPECT_EQ("MouseWheel", GetInputMessageTypes());
  if (wheel_scrolling_mode_ != kAsyncWheelEvents) {
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

    ExpectGestureScrollEventsAfterMouseWheelAckWhenOverscrolling();
  }

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
}
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       OverscrollDirectionChangeMouseWheel) {
  OverscrollDirectionChangeMouseWheel();
}
TEST_F(RenderWidgetHostViewAuraOverscrollWithoutWheelScrollLatchingTest,
       OverscrollDirectionChangeMouseWheel) {
  OverscrollDirectionChangeMouseWheel();
}
TEST_F(RenderWidgetHostViewAuraOverScrollAsyncWheelEventsEnabledTest,
       OverscrollDirectionChangeMouseWheel) {
  OverscrollDirectionChangeMouseWheel();
}

void RenderWidgetHostViewAuraOverscrollTest::OverscrollMouseMoveCompletion() {
  SetUpOverscrollEnvironment();

  SimulateWheelEventPossiblyIncludingPhase(
      5, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);  // sent directly
  SimulateWheelEventPossiblyIncludingPhase(
      -1, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);  // enqueued
  SimulateWheelEventPossiblyIncludingPhase(
      -10, -3, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEventPossiblyIncludingPhase(
      -15, -1, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEventPossiblyIncludingPhase(
      -30, -3, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ("MouseWheel", GetInputMessageTypes());

  // Receive ACK the first wheel event as not processed.
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ExpectGestureScrollEventsAfterMouseWheelACK(true, 1);

  // Ack the first GSU, shouldn't be able to trigger overscroll.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  if (wheel_scrolling_mode_ != kAsyncWheelEvents) {
    ExpectGestureScrollEndForWheelScrolling(false);
    // Receive ACK for the second (coalesced) event as not processed. This will
    // start an overcroll gesture.
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelACK(false, 0);
  }

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_WEST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  EXPECT_EQ(OVERSCROLL_WEST, overscroll_delegate()->current_mode());

  // Send a mouse-move event. This should cancel the overscroll navigation
  // (since the amount overscrolled is not above the threshold), and so the
  // mouse-move should reach the renderer.
  SimulateMouseMove(5, 10, 0);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->completed_mode());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ("MouseMove", GetInputMessageTypes());

  SendInputEventACK(WebInputEvent::kMouseMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Moving the mouse more should continue to send the events to the renderer.
  SimulateMouseMove(5, 10, 0);
  SendInputEventACK(WebInputEvent::kMouseMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ("MouseMove", GetInputMessageTypes());

  // Now try with gestures.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureScrollUpdateEvent(300, -5, 0);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  sink_->ClearMessages();

  // Overscroll gesture is in progress. Send a mouse-move now. This should
  // complete the gesture (because the amount overscrolled is above the
  // threshold).
  SimulateMouseMove(5, 10, 0);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->completed_mode());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ("MouseMove", GetInputMessageTypes());
  SendInputEventACK(WebInputEvent::kMouseMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes());

  // Move mouse some more. The mouse-move events should reach the renderer.
  SimulateMouseMove(5, 10, 0);
  EXPECT_EQ("MouseMove", GetInputMessageTypes());

  SendInputEventACK(WebInputEvent::kMouseMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
}
TEST_F(RenderWidgetHostViewAuraOverscrollTest, OverscrollMouseMoveCompletion) {
  OverscrollMouseMoveCompletion();
}
TEST_F(RenderWidgetHostViewAuraOverscrollWithoutWheelScrollLatchingTest,
       OverscrollMouseMoveCompletion) {
  OverscrollMouseMoveCompletion();
}
TEST_F(RenderWidgetHostViewAuraOverScrollAsyncWheelEventsEnabledTest,
       OverscrollMouseMoveCompletion) {
  OverscrollMouseMoveCompletion();
}

// Tests that if a page scrolled, then the overscroll controller's states are
// reset after the end of the scroll.
void RenderWidgetHostViewAuraOverscrollTest::
    OverscrollStateResetsAfterScroll() {
  SetUpOverscrollEnvironment();

  SimulateWheelEventPossiblyIncludingPhase(
      0, 5, 0, true, WebMouseWheelEvent::kPhaseBegan);  // sent directly
  SimulateWheelEventPossiblyIncludingPhase(
      0, 30, 0, true, WebMouseWheelEvent::kPhaseChanged);  // enqueued
  SimulateWheelEventPossiblyIncludingPhase(
      0, 40, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEventPossiblyIncludingPhase(
      0, 10, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ("MouseWheel", GetInputMessageTypes());

  // The first wheel event is consumed. Dispatches the queued wheel event.
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  ExpectGestureScrollEventsAfterMouseWheelACK(true, 1);

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(ScrollStateIsContentScrolling());

  if (wheel_scrolling_mode_ != kAsyncWheelEvents) {
    ExpectGestureScrollEndForWheelScrolling(false);
    // The second wheel event is consumed.
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelACK(false, 0);
  }

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  if (wheel_scrolling_mode_ == kAsyncWheelEvents) {
    SimulateWheelEventWithPhase(0, 0, 0, true, WebMouseWheelEvent::kPhaseEnded);
    EXPECT_EQ("MouseWheel GestureScrollEnd", GetInputMessageTypes());
  } else if (wheel_scroll_latching_enabled_) {
    SimulateWheelEventWithPhase(0, 0, 0, true, WebMouseWheelEvent::kPhaseEnded);
    EXPECT_EQ("MouseWheel", GetInputMessageTypes());
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEndForWheelScrolling(true);
  } else {
    ExpectGestureScrollEndForWheelScrolling(true);
  }
  EXPECT_TRUE(ScrollStateIsContentScrolling());

  // Touchpad scroll can end with a zero-velocity fling which is not dispatched,
  // but it should still reset the overscroll controller state.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureFlingStartEvent(0.f, 0.f, blink::kWebGestureDeviceTouchpad);
  EXPECT_TRUE(ScrollStateIsUnknown());
  EXPECT_EQ("GestureScrollBegin", GetInputMessageTypes());

  // Dropped flings should neither propagate *nor* indicate that they were
  // consumed and have triggered a fling animation (as tracked by the router).
  EXPECT_FALSE(parent_host_->input_router()->HasPendingEvents());

  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes());

  SimulateWheelEventPossiblyIncludingPhase(
      -5, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);  // sent directly
  SimulateWheelEventPossiblyIncludingPhase(
      -60, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);  // enqueued
  SimulateWheelEventPossiblyIncludingPhase(
      -100, 0, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event

  EXPECT_TRUE(ScrollStateIsUnknown());
  EXPECT_EQ("MouseWheel", GetInputMessageTypes());

  // The first wheel scroll did not scroll content. Overscroll should not start
  // yet, since enough hasn't been scrolled.
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  ExpectGestureScrollEventsAfterMouseWheelACK(true, 1);

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_TRUE(ScrollStateIsUnknown());

  if (wheel_scrolling_mode_ != kAsyncWheelEvents) {
    ExpectGestureScrollEndForWheelScrolling(false);
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEventsAfterMouseWheelACK(false, 0);
  }

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  if (wheel_scrolling_mode_ == kAsyncWheelEvents) {
    SimulateWheelEventWithPhase(0, 0, 0, true, WebMouseWheelEvent::kPhaseEnded);
    EXPECT_EQ("MouseWheel GestureScrollEnd", GetInputMessageTypes());
  } else if (wheel_scroll_latching_enabled_) {
    SimulateWheelEventWithPhase(0, 0, 0, true, WebMouseWheelEvent::kPhaseEnded);
    EXPECT_EQ("MouseWheel", GetInputMessageTypes());
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    ExpectGestureScrollEndForWheelScrolling(true);
  } else {
    ExpectGestureScrollEndForWheelScrolling(true);
  }
  EXPECT_EQ(OVERSCROLL_WEST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  EXPECT_TRUE(ScrollStateIsOverscrolling());

  // The GestureScrollBegin will reset the delegate's mode, so check it here.
  EXPECT_EQ(OVERSCROLL_WEST, overscroll_delegate()->current_mode());
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureFlingStartEvent(0.f, 0.f, blink::kWebGestureDeviceTouchpad);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_TRUE(ScrollStateIsUnknown());
  EXPECT_EQ("GestureScrollBegin", GetInputMessageTypes());
  EXPECT_FALSE(parent_host_->input_router()->HasPendingEvents());
}
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       OverscrollStateResetsAfterScroll) {
  OverscrollStateResetsAfterScroll();
}
TEST_F(RenderWidgetHostViewAuraOverscrollWithoutWheelScrollLatchingTest,
       OverscrollStateResetsAfterScroll) {
  OverscrollStateResetsAfterScroll();
}
TEST_F(RenderWidgetHostViewAuraOverScrollAsyncWheelEventsEnabledTest,
       OverscrollStateResetsAfterScroll) {
  OverscrollStateResetsAfterScroll();
}

TEST_F(RenderWidgetHostViewAuraOverscrollTest, OverscrollResetsOnBlur) {
  SetUpOverscrollEnvironment();

  // Start an overscroll with gesture scroll. In the middle of the scroll, blur
  // the host.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureScrollUpdateEvent(300, -5, 0);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ("GestureScrollBegin TouchScrollStarted GestureScrollUpdate",
            GetInputMessageTypes());

  view_->OnWindowFocused(nullptr, view_->GetNativeView());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->completed_mode());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  sink_->ClearMessages();

  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes());

  // Start a scroll gesture again. This should correctly start the overscroll
  // after the threshold.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureScrollUpdateEvent(300, -5, 0);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->completed_mode());

  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->completed_mode());
  EXPECT_EQ(
      "GestureScrollBegin TouchScrollStarted GestureScrollUpdate "
      "GestureScrollEnd",
      GetInputMessageTypes());
}

#if defined(OS_CHROMEOS)
// Check that when accessibility virtual keyboard is enabled, windows are
// shifted up when focused and restored when focus is lost.
TEST_F(RenderWidgetHostViewAuraTest, VirtualKeyboardFocusEnsureCaretInRect) {
  // TODO (oshima): Test that overscroll occurs.

  view_->InitAsChild(nullptr);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(), parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  aura::Window* root_window = parent_view_->GetNativeView()->GetRootWindow();
  wm::DefaultScreenPositionClient screen_position_client;
  aura::client::SetScreenPositionClient(root_window, &screen_position_client);

  const gfx::Rect orig_view_bounds = gfx::Rect(0, 300, 400, 200);
  const gfx::Rect shifted_view_bounds = gfx::Rect(0, 200, 400, 200);
  const gfx::Rect root_bounds = root_window->bounds();
  const int keyboard_height = 200;
  const gfx::Rect keyboard_view_bounds =
      gfx::Rect(0, root_bounds.height() - keyboard_height, root_bounds.width(),
                keyboard_height);

  ui::InputMethod* input_method = root_window->GetHost()->GetInputMethod();

  // Focus the window.
  view_->SetBounds(orig_view_bounds);
  input_method->SetFocusedTextInputClient(view_);
  EXPECT_EQ(view_->GetNativeView()->bounds(), orig_view_bounds);

  // Simulate virtual keyboard.
  input_method->SetOnScreenKeyboardBounds(keyboard_view_bounds);

  // Window should be shifted.
  EXPECT_EQ(view_->GetNativeView()->bounds(), shifted_view_bounds);

  // Detach the RenderWidgetHostViewAura from the IME.
  view_->DetachFromInputMethod();

  // Window should be restored.
  EXPECT_EQ(view_->GetNativeView()->bounds(), orig_view_bounds);

  aura::client::SetScreenPositionClient(root_window, nullptr);
}
#endif  // defined(OS_CHROMEOS)

// Tests that when view initiated shutdown happens (i.e. RWHView is deleted
// before RWH), we clean up properly and don't leak the RWHVGuest.
TEST_F(RenderWidgetHostViewGuestAuraTest, GuestViewDoesNotLeak) {
  view_->InitAsChild(nullptr);
  TearDownEnvironment();
  ASSERT_FALSE(guest_view_weak_.get());
}

// Tests that invalid touch events are consumed and handled
// synchronously.
TEST_F(RenderWidgetHostViewAuraTest,
       InvalidEventsHaveSyncHandlingDisabled) {
  view_->InitAsChild(nullptr);
  view_->Show();
  GetSentMessageCountAndResetSink();

  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(30, 30), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));

  // Construct a move with a touch id which doesn't exist.
  ui::TouchEvent invalid_move(
      ui::ET_TOUCH_MOVED, gfx::Point(30, 30), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));

  // Valid press is handled asynchronously.
  view_->OnTouchEvent(&press);
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  AckLastSentInputEventIfNecessary(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Invalid move is handled synchronously, but is consumed. It should not
  // be forwarded to the renderer.
  view_->OnTouchEvent(&invalid_move);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_FALSE(invalid_move.synchronous_handling_disabled());
  EXPECT_TRUE(invalid_move.stopped_propagation());
}

// Checks key event codes.
TEST_F(RenderWidgetHostViewAuraTest, KeyEvent) {
  view_->InitAsChild(nullptr);
  view_->Show();

  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A,
                         ui::EF_NONE);
  view_->OnKeyEvent(&key_event);

  const NativeWebKeyboardEvent* event = delegates_.back()->last_event();
  ASSERT_TRUE(event);
  EXPECT_EQ(key_event.key_code(), event->windows_key_code);
  EXPECT_EQ(ui::KeycodeConverter::DomCodeToNativeKeycode(key_event.code()),
            event->native_key_code);
}

TEST_F(RenderWidgetHostViewAuraTest, KeyEventsHandled) {
  view_->InitAsChild(nullptr);
  view_->Show();

  ui::KeyEvent key_event1(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  view_->OnKeyEvent(&key_event1);
  // Normally event should be handled.
  EXPECT_TRUE(key_event1.handled());

  ASSERT_FALSE(delegates_.empty());
  // Make the delegate mark the event as not-handled.
  delegates_.back()->set_pre_handle_keyboard_event_result(
      KeyboardEventProcessingResult::HANDLED_DONT_UPDATE_EVENT);
  ui::KeyEvent key_event2(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  view_->OnKeyEvent(&key_event2);
  EXPECT_FALSE(key_event2.handled());
}

TEST_F(RenderWidgetHostViewAuraTest, SetCanScrollForWebMouseWheelEvent) {
  view_->InitAsChild(nullptr);
  view_->Show();

  sink_->ClearMessages();

  // Simulates the mouse wheel event with ctrl modifier applied.
  ui::MouseWheelEvent event(gfx::Vector2d(1, 1), gfx::Point(), gfx::Point(),
                            ui::EventTimeForNow(), ui::EF_CONTROL_DOWN, 0);
  view_->OnMouseEvent(&event);

  const WebInputEvent* input_event =
      GetInputEventFromMessage(*sink_->GetMessageAt(0));
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(input_event);
  // Check if scroll is caused when ctrl-scroll is generated from
  // mouse wheel event.
  EXPECT_FALSE(WebInputEventTraits::CanCauseScroll(*wheel_event));
  sink_->ClearMessages();

  // Ack'ing the outstanding event should flush the pending event queue.
  SendInputEventACK(blink::WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Simulates the mouse wheel event with no modifier applied.
  event = ui::MouseWheelEvent(gfx::Vector2d(1, 1), gfx::Point(), gfx::Point(),
                              ui::EventTimeForNow(), ui::EF_NONE, 0);

  view_->OnMouseEvent(&event);

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(0));
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  // Check if scroll is caused when no modifier is applied to the
  // mouse wheel event.
  EXPECT_TRUE(WebInputEventTraits::CanCauseScroll(*wheel_event));
  sink_->ClearMessages();

  SendInputEventACK(blink::WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Simulates the scroll event with ctrl modifier applied.
  ui::ScrollEvent scroll(ui::ET_SCROLL, gfx::Point(2, 2), ui::EventTimeForNow(),
                         ui::EF_CONTROL_DOWN, 0, 5, 0, 5, 2);
  view_->OnScrollEvent(&scroll);

  input_event = GetInputEventFromMessage(*sink_->GetMessageAt(0));
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  // Check if scroll is caused when ctrl-touchpad-scroll is generated
  // from scroll event.
  EXPECT_TRUE(WebInputEventTraits::CanCauseScroll(*wheel_event));
}

// Ensures that the mapping from ui::TouchEvent to blink::WebTouchEvent doesn't
// lose track of the number of acks required.
TEST_F(RenderWidgetHostViewAuraTest, CorrectNumberOfAcksAreDispatched) {
  view_->InitAsFullscreen(parent_view_);
  view_->Show();
  view_->UseFakeDispatcher();

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(30, 30), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));

  view_->OnTouchEvent(&press1);
  SendTouchEventACK(blink::WebInputEvent::kTouchStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED, press1.unique_event_id());

  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(20, 20), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));
  view_->OnTouchEvent(&press2);
  SendTouchEventACK(blink::WebInputEvent::kTouchStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED, press2.unique_event_id());

  EXPECT_EQ(2U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());
}

// Tests that the scroll deltas stored within the overscroll controller get
// reset at the end of the overscroll gesture even if the overscroll threshold
// isn't surpassed and the overscroll mode stays OVERSCROLL_NONE.
void RenderWidgetHostViewAuraOverscrollTest::ScrollDeltasResetOnEnd() {
  SetUpOverscrollEnvironment();
  // Wheel event scroll ending with mouse move.
  SimulateWheelEventPossiblyIncludingPhase(
      -30, -10, 0, true, WebMouseWheelEvent::kPhaseBegan);  // sent directly
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(-30.f, overscroll_delta_x());
  EXPECT_EQ(-10.f, overscroll_delta_y());
  SimulateMouseMove(5, 10, 0);
  EXPECT_EQ(0.f, overscroll_delta_x());
  EXPECT_EQ(0.f, overscroll_delta_y());

  // A wheel event with phase ended is sent before a GSB with touchscreen
  // device.
  SimulateWheelEventPossiblyIncludingPhase(0, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseEnded);
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Scroll gesture.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureScrollUpdateEvent(-30, -5, 0);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(-30.f, overscroll_delta_x());
  EXPECT_EQ(-5.f, overscroll_delta_y());
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0.f, overscroll_delta_x());
  EXPECT_EQ(0.f, overscroll_delta_y());

  // Wheel event scroll ending with a fling. This is the first wheel event after
  // touchscreen scrolling ends so it will have phase = kPhaseBegan.
  SimulateWheelEventPossiblyIncludingPhase(5, 0, 0, true,
                                           WebMouseWheelEvent::kPhaseBegan);
  SendInputEventACK(WebInputEvent::kMouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendScrollBeginAckIfNeeded(INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  SimulateWheelEventPossiblyIncludingPhase(10, -5, 0, true,
                                           WebMouseWheelEvent::kPhaseChanged);
  if (wheel_scrolling_mode_ != kAsyncWheelEvents) {
    SendInputEventACK(WebInputEvent::kMouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  }
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(15.f, overscroll_delta_x());
  EXPECT_EQ(-5.f, overscroll_delta_y());
  SimulateGestureFlingStartEvent(0.f, 0.1f, blink::kWebGestureDeviceTouchpad);
  EXPECT_EQ(0.f, overscroll_delta_x());
  EXPECT_EQ(0.f, overscroll_delta_y());
}
TEST_F(RenderWidgetHostViewAuraOverscrollTest, ScrollDeltasResetOnEnd) {
  ScrollDeltasResetOnEnd();
}
TEST_F(RenderWidgetHostViewAuraOverscrollWithoutWheelScrollLatchingTest,
       ScrollDeltasResetOnEnd) {
  ScrollDeltasResetOnEnd();
}
TEST_F(RenderWidgetHostViewAuraOverScrollAsyncWheelEventsEnabledTest,
       ScrollDeltasResetOnEnd) {
  ScrollDeltasResetOnEnd();
}

TEST_F(RenderWidgetHostViewAuraTest, ForwardMouseEvent) {
  aura::Window* root = parent_view_->GetNativeView()->GetRootWindow();

  // Set up test delegate and window hierarchy.
  aura::test::EventCountDelegate delegate;
  std::unique_ptr<aura::Window> parent(new aura::Window(&delegate));
  parent->Init(ui::LAYER_TEXTURED);
  root->AddChild(parent.get());
  view_->InitAsChild(parent.get());

  // Simulate mouse events, ensure they are forwarded to delegate.
  ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             0);
  view_->OnMouseEvent(&mouse_event);
  EXPECT_EQ("1 0", delegate.GetMouseButtonCountsAndReset());

  // Simulate mouse events, ensure they are forwarded to delegate.
  mouse_event = ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(1, 1),
                               gfx::Point(), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&mouse_event);
  EXPECT_EQ("0 1 0", delegate.GetMouseMotionCountsAndReset());

  // Lock the mouse, simulate, and ensure they are forwarded.
  view_->LockMouse();

  mouse_event =
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  view_->OnMouseEvent(&mouse_event);
  EXPECT_EQ("1 0", delegate.GetMouseButtonCountsAndReset());

  mouse_event = ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&mouse_event);
  EXPECT_EQ("0 1 0", delegate.GetMouseMotionCountsAndReset());

  view_->UnlockMouse();

  // view_ will be destroyed when parent is destroyed.
  view_ = nullptr;
}

TEST_F(RenderWidgetHostViewAuraTest, GestureTapFromStylusHasPointerType) {
  view_->InitAsFullscreen(parent_view_);
  view_->Show();

  aura::Window* root = view_->GetNativeView()->GetRootWindow();
  root->SetTargetHandler(view_);

  ui::test::EventGenerator generator(root, root->bounds().CenterPoint());

  // Simulate touch press and release to generate a GestureTap.
  sink_->ClearMessages();
  generator.EnterPenPointerMode();
  generator.PressTouch();
  AckLastSentInputEventIfNecessary(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  generator.ReleaseTouch();
  AckLastSentInputEventIfNecessary(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // GestureTap event should have correct pointer type.
  EXPECT_EQ(5U, sink_->message_count());
  const WebInputEvent* input_event = GetInputEventFromMessage(
      *sink_->GetMessageAt(sink_->message_count() - 1));
  EXPECT_EQ(WebInputEvent::kGestureTap, input_event->GetType());
  const WebGestureEvent* geture_event =
      static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(blink::WebPointerProperties::PointerType::kPen,
            geture_event->primary_pointer_type);

  sink_->ClearMessages();
}

// This class provides functionality to test a RenderWidgetHostViewAura
// instance which has been hooked up to a test RenderViewHost instance and
// a WebContents instance.
class RenderWidgetHostViewAuraWithViewHarnessTest
    : public RenderViewHostImplTestHarness {
 public:
   RenderWidgetHostViewAuraWithViewHarnessTest()
      : view_(nullptr) {}
   ~RenderWidgetHostViewAuraWithViewHarnessTest() override {}

 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    // Delete the current RenderWidgetHostView instance before setting
    // the RWHVA as the view.
    delete contents()->GetRenderViewHost()->GetWidget()->GetView();
    // This instance is destroyed in the TearDown method below.
    view_ = new RenderWidgetHostViewAura(
        contents()->GetRenderViewHost()->GetWidget(), false,
        false /* enable_surface_synchronization */);
  }

  void TearDown() override {
    view_->Destroy();
    RenderViewHostImplTestHarness::TearDown();
  }

  RenderWidgetHostViewAura* view() {
    return view_;
  }

 private:
  RenderWidgetHostViewAura* view_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAuraWithViewHarnessTest);
};

// Provides a mock implementation of the WebContentsViewDelegate class.
// Currently provides functionality to validate the ShowContextMenu
// callback.
class MockWebContentsViewDelegate : public WebContentsViewDelegate {
 public:
  MockWebContentsViewDelegate()
      : context_menu_request_received_(false) {}

  ~MockWebContentsViewDelegate() override {}

  bool context_menu_request_received() const {
    return context_menu_request_received_;
  }

  ui::MenuSourceType context_menu_source_type() const {
    return context_menu_params_.source_type;
  }

  // WebContentsViewDelegate overrides.
  void ShowContextMenu(RenderFrameHost* render_frame_host,
                       const ContextMenuParams& params) override {
    context_menu_request_received_ = true;
    context_menu_params_ = params;
  }

  void ClearState() {
    context_menu_request_received_ = false;
    context_menu_params_.source_type = ui::MENU_SOURCE_NONE;
  }

 private:
  bool context_menu_request_received_;
  ContextMenuParams context_menu_params_;

  DISALLOW_COPY_AND_ASSIGN(MockWebContentsViewDelegate);
};

// On Windows we don't want the context menu to be displayed in the context of
// a long press gesture. It should be displayed when the touch is released.
// On other platforms we should display the context menu in the long press
// gesture.
// This test validates this behavior.
TEST_F(RenderWidgetHostViewAuraWithViewHarnessTest,
       ContextMenuTest) {
  // This instance will be destroyed when the WebContents instance is
  // destroyed.
  MockWebContentsViewDelegate* delegate = new MockWebContentsViewDelegate;
  static_cast<WebContentsViewAura*>(
      contents()->GetView())->SetDelegateForTesting(delegate);

  RenderViewHostFactory::set_is_real_render_view_host(true);

  // A context menu request with the MENU_SOURCE_MOUSE source type should
  // result in the MockWebContentsViewDelegate::ShowContextMenu method
  // getting called. This means that the request worked correctly.
  ContextMenuParams context_menu_params;
  context_menu_params.source_type = ui::MENU_SOURCE_MOUSE;
  contents()->ShowContextMenu(contents()->GetRenderViewHost()->GetMainFrame(),
                              context_menu_params);
  EXPECT_TRUE(delegate->context_menu_request_received());
  EXPECT_EQ(delegate->context_menu_source_type(), ui::MENU_SOURCE_MOUSE);

  // A context menu request with the MENU_SOURCE_TOUCH source type should
  // result in the MockWebContentsViewDelegate::ShowContextMenu method
  // getting called on all platforms. This means that the request worked
  // correctly.
  delegate->ClearState();
  context_menu_params.source_type = ui::MENU_SOURCE_TOUCH;
  contents()->ShowContextMenu(contents()->GetRenderViewHost()->GetMainFrame(),
                              context_menu_params);
  EXPECT_TRUE(delegate->context_menu_request_received());

  // A context menu request with the MENU_SOURCE_LONG_TAP source type should
  // result in the MockWebContentsViewDelegate::ShowContextMenu method
  // getting called on all platforms. This means that the request worked
  // correctly.
  delegate->ClearState();
  context_menu_params.source_type = ui::MENU_SOURCE_LONG_TAP;
  contents()->ShowContextMenu(contents()->GetRenderViewHost()->GetMainFrame(),
                              context_menu_params);
  EXPECT_TRUE(delegate->context_menu_request_received());

  // A context menu request with the MENU_SOURCE_LONG_PRESS source type should
  // result in the MockWebContentsViewDelegate::ShowContextMenu method
  // getting called on non Windows platforms. This means that the request
  //  worked correctly. On Windows this should be blocked.
  delegate->ClearState();
  context_menu_params.source_type = ui::MENU_SOURCE_LONG_PRESS;
  contents()->ShowContextMenu(contents()->GetRenderViewHost()->GetMainFrame(),
                              context_menu_params);
#if defined(OS_WIN)
  EXPECT_FALSE(delegate->context_menu_request_received());
#else
  EXPECT_TRUE(delegate->context_menu_request_received());
#endif

#if defined(OS_WIN)
  // On Windows the context menu request blocked above should be received when
  // the ET_GESTURE_LONG_TAP gesture is sent to the RenderWidgetHostViewAura
  // instance. This means that the touch was released.
  delegate->ClearState();

  ui::GestureEventDetails event_details(ui::ET_GESTURE_LONG_TAP);
  ui::GestureEvent gesture_event(
      100, 100, 0, ui::EventTimeForNow(), event_details);
  view()->OnGestureEvent(&gesture_event);

  EXPECT_TRUE(delegate->context_menu_request_received());
  EXPECT_EQ(delegate->context_menu_source_type(), ui::MENU_SOURCE_TOUCH);
#endif

  RenderViewHostFactory::set_is_real_render_view_host(false);
}

// ----------------------------------------------------------------------------
// TextInputManager and IME-Related Tests

// The test class for OOPIF IME related unit tests in RenderWidgetHostViewAura.
// In each test, 3 views are created where one is in process with main frame and
// the other two are in distinct processes (this makes a total of 4 RWHVs).
class InputMethodAuraTestBase : public RenderWidgetHostViewAuraTest {
 public:
  InputMethodAuraTestBase() {}
  ~InputMethodAuraTestBase() override {}

  void SetUp() override {
    RenderWidgetHostViewAuraTest::SetUp();
    InitializeAura();

    view_for_first_process_ = CreateViewForProcess(tab_process());

    second_process_host_ = CreateNewProcessHost();
    view_for_second_process_ = CreateViewForProcess(second_process_host_);

    third_process_host_ = CreateNewProcessHost();
    view_for_third_process_ = CreateViewForProcess(third_process_host_);

    views_.insert(views_.begin(),
                  {tab_view(), view_for_first_process_,
                   view_for_second_process_, view_for_third_process_});
    processes_.insert(processes_.begin(),
                      {tab_process(), tab_process(), second_process_host_,
                       third_process_host_});
    active_view_sequence_.insert(active_view_sequence_.begin(),
                                 {0, 1, 2, 1, 1, 3, 0, 3, 1});
  }

  void TearDown() override {
    RenderWidgetHost* widget_for_first_process =
        view_for_first_process_->GetRenderWidgetHost();
    view_for_first_process_->Destroy();
    delete widget_for_first_process;

    RenderWidgetHost* widget_for_second_process =
        view_for_second_process_->GetRenderWidgetHost();
    view_for_second_process_->Destroy();
    delete widget_for_second_process;

    RenderWidgetHost* widget_for_third_process =
        view_for_third_process_->GetRenderWidgetHost();
    view_for_third_process_->Destroy();
    delete widget_for_third_process;

    RenderWidgetHostViewAuraTest::TearDown();
  }

 protected:
  ui::TextInputClient* text_input_client() const { return view_; }

  bool has_composition_text() const {
    return tab_view()->has_composition_text_;
  }

  MockRenderProcessHost* CreateNewProcessHost() {
    MockRenderProcessHost* process_host =
        new MockRenderProcessHost(browser_context());
    return process_host;
  }

  MockRenderWidgetHostImpl* CreateRenderWidgetHostForProcess(
      MockRenderProcessHost* process_host) {
    return MockRenderWidgetHostImpl::Create(render_widget_host_delegate(),
                                            process_host,
                                            process_host->GetNextRoutingID());
  }

  TestRenderWidgetHostView* CreateViewForProcess(
      MockRenderProcessHost* process_host) {
    RenderWidgetHostImpl* host = CreateRenderWidgetHostForProcess(process_host);
    TestRenderWidgetHostView* view = new TestRenderWidgetHostView(host);
    host->SetView(view);
    return view;
  }

  void SetHasCompositionTextToTrue() {
    ui::CompositionText composition_text;
    composition_text.text = base::ASCIIToUTF16("text");
    tab_view()->SetCompositionText(composition_text);
    EXPECT_TRUE(has_composition_text());
  }

  MockRenderProcessHost* tab_process() const { return process_host_; }

  RenderWidgetHostViewAura* tab_view() const { return view_; }

  std::vector<RenderWidgetHostViewBase*> views_;
  std::vector<MockRenderProcessHost*> processes_;
  // A sequence of indices in [0, 3] which determines the index of a RWHV in
  // |views_|. This sequence is used in the tests to sequentially make a RWHV
  // active for a subsequent IME result method call.
  std::vector<size_t> active_view_sequence_;

 private:
  // This will initialize |window_| in RenderWidgetHostViewAura. It is needed
  // for RenderWidgetHostViewAura::GetInputMethod() to work.
  void InitializeAura() {
    view_->InitAsChild(nullptr);
    view_->Show();
  }

  TestRenderWidgetHostView* view_for_first_process_;
  MockRenderProcessHost* second_process_host_;
  TestRenderWidgetHostView* view_for_second_process_;
  MockRenderProcessHost* third_process_host_;
  TestRenderWidgetHostView* view_for_third_process_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodAuraTestBase);
};

// A group of tests which verify that the IME method results are routed to the
// right RenderWidget when there are multiple RenderWidgetHostViews on tab. Each
// test will verify the correctness of routing for one of the IME result
// methods. The method is called on ui::TextInputClient (i.e., RWHV for the tab
// in aura) and then the test verifies that the IPC is routed to the
// RenderWidget corresponding to the active view (i.e., the RenderWidget
// with focused <input>).
class InputMethodResultAuraTest : public InputMethodAuraTestBase {
 public:
  InputMethodResultAuraTest() {}
  ~InputMethodResultAuraTest() override {}

 protected:
  const IPC::Message* RunAndReturnIPCSent(const base::Closure closure,
                                          MockRenderProcessHost* process,
                                          int32_t message_id) {
    process->sink().ClearMessages();
    closure.Run();
    return process->sink().GetFirstMessageMatching(message_id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodResultAuraTest);
};

// This test verifies ui::TextInputClient::SetCompositionText.
TEST_F(InputMethodResultAuraTest, SetCompositionText) {
  base::Closure ime_call =
      base::Bind(&ui::TextInputClient::SetCompositionText,
                 base::Unretained(text_input_client()), ui::CompositionText());
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    EXPECT_TRUE(!!RunAndReturnIPCSent(ime_call, processes_[index],
                                      InputMsg_ImeSetComposition::ID));
  }
}

// This test is for ui::TextInputClient::ConfirmCompositionText.
TEST_F(InputMethodResultAuraTest, ConfirmCompositionText) {
  base::Closure ime_call =
      base::Bind(&ui::TextInputClient::ConfirmCompositionText,
                 base::Unretained(text_input_client()));
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    SetHasCompositionTextToTrue();
    EXPECT_TRUE(!!RunAndReturnIPCSent(ime_call, processes_[index],
                                      InputMsg_ImeFinishComposingText::ID));
  }
}

// This test is for ui::TextInputClient::ClearCompositionText.
TEST_F(InputMethodResultAuraTest, ClearCompositionText) {
  base::Closure ime_call =
      base::Bind(&ui::TextInputClient::ClearCompositionText,
                 base::Unretained(text_input_client()));
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    SetHasCompositionTextToTrue();
    EXPECT_TRUE(!!RunAndReturnIPCSent(ime_call, processes_[index],
                                      InputMsg_ImeSetComposition::ID));
  }
}

// This test is for ui::TextInputClient::InsertText with empty text.
TEST_F(InputMethodResultAuraTest, FinishComposingText) {
  base::Closure ime_call =
      base::Bind(&ui::TextInputClient::InsertText,
                 base::Unretained(text_input_client()), base::string16());
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    SetHasCompositionTextToTrue();
    EXPECT_TRUE(!!RunAndReturnIPCSent(ime_call, processes_[index],
                                      InputMsg_ImeFinishComposingText::ID));
  }
}

// This test is for ui::TextInputClient::InsertText with non-empty text.
TEST_F(InputMethodResultAuraTest, CommitText) {
  base::Closure ime_call = base::Bind(&ui::TextInputClient::InsertText,
                                      base::Unretained(text_input_client()),
                                      base::UTF8ToUTF16("hello"));
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    EXPECT_TRUE(!!RunAndReturnIPCSent(ime_call, processes_[index],
                                      InputMsg_ImeCommitText::ID));
  }
}

// This test is for RenderWidgetHostViewAura::FinishImeCompositionSession which
// is in response to a mouse click during an ongoing composition.
TEST_F(InputMethodResultAuraTest, FinishImeCompositionSession) {
  base::Closure ime_finish_session_call =
      base::Bind(&RenderWidgetHostViewEventHandler::FinishImeCompositionSession,
                 base::Unretained(tab_view()->event_handler()));
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    SetHasCompositionTextToTrue();
    EXPECT_TRUE(!!RunAndReturnIPCSent(ime_finish_session_call,
                                      processes_[index],
                                      InputMsg_ImeFinishComposingText::ID));
  }
}

// This test is for ui::TextInputClient::ChangeTextDirectionAndLayoutAlignment.
TEST_F(InputMethodResultAuraTest, ChangeTextDirectionAndLayoutAlignment) {
  base::Closure ime_finish_session_call = base::Bind(
      base::IgnoreResult(
          &RenderWidgetHostViewAura::ChangeTextDirectionAndLayoutAlignment),
      base::Unretained(tab_view()), base::i18n::LEFT_TO_RIGHT);
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    EXPECT_TRUE(!!RunAndReturnIPCSent(ime_finish_session_call,
                                      processes_[index],
                                      ViewMsg_SetTextDirection::ID));
  }
}

// A class of tests which verify the correctness of some tracked IME related
// state at the browser side. Each test verifies the correctness tracking for
// one specific state. To do so, the views are activated in a predetermined
// sequence and each time, the IPC call for the corresponding state is simulated
// through calling the method on the view. Then the test verifies that the value
// returned by the view or ui::TextInputClient is the expected value from IPC.
class InputMethodStateAuraTest : public InputMethodAuraTestBase {
 public:
  InputMethodStateAuraTest() {}
  ~InputMethodStateAuraTest() override {}

 protected:
  gfx::SelectionBound GetSelectionBoundFromRect(const gfx::Rect& rect) {
    gfx::SelectionBound bound;
    bound.SetEdge(gfx::PointF(rect.origin()), gfx::PointF(rect.bottom_left()));
    return bound;
  }

  gfx::Rect TransformRectToViewsRootCoordSpace(const gfx::Rect rect,
                                               RenderWidgetHostView* view) {
    return gfx::Rect(view->TransformPointToRootCoordSpace(rect.origin()),
                     rect.size());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodStateAuraTest);
};

// This test is for caret bounds which are calculated based on the tracked value
// for selection bounds.
TEST_F(InputMethodStateAuraTest, GetCaretBounds) {
  ViewHostMsg_SelectionBounds_Params params;
  params.is_anchor_first = true;
  params.anchor_dir = blink::kWebTextDirectionLeftToRight;
  params.focus_dir = blink::kWebTextDirectionLeftToRight;
  params.anchor_rect = gfx::Rect(0, 0, 10, 10);
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    params.focus_rect = gfx::Rect(10 + index, 10 + index, 10, 10);
    views_[index]->SelectionBoundsChanged(params);

    // Calculate the bounds.
    gfx::SelectionBound anchor_bound = GetSelectionBoundFromRect(
        TransformRectToViewsRootCoordSpace(params.anchor_rect, views_[index]));
    gfx::SelectionBound focus_bound = GetSelectionBoundFromRect(
        TransformRectToViewsRootCoordSpace(params.focus_rect, views_[index]));
    anchor_bound.set_type(gfx::SelectionBound::LEFT);
    focus_bound.set_type(gfx::SelectionBound::RIGHT);
    gfx::Rect measured_rect =
        gfx::RectBetweenSelectionBounds(anchor_bound, focus_bound);

    EXPECT_EQ(measured_rect, text_input_client()->GetCaretBounds());
  }
}

// This test is for composition character bounds.
TEST_F(InputMethodStateAuraTest, GetCompositionCharacterBounds) {
  gfx::Rect bound;
  // Initially, there should be no bounds.
  EXPECT_FALSE(text_input_client()->GetCompositionCharacterBounds(0, &bound));
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    // Simulate an IPC to set character bounds for the view.
    views_[index]->ImeCompositionRangeChanged(gfx::Range(),
                                              {gfx::Rect(1, 2, 3, 4 + index)});

    // No bounds at index 1.
    EXPECT_FALSE(text_input_client()->GetCompositionCharacterBounds(1, &bound));

    // Valid bound at index 0.
    EXPECT_TRUE(text_input_client()->GetCompositionCharacterBounds(0, &bound));
    EXPECT_EQ(4 + (int)index, bound.height());
  }
}

// This test is for selected text.
TEST_F(InputMethodStateAuraTest, GetSelectedText) {
  base::string16 text = base::ASCIIToUTF16("some text of length 22");
  size_t offset = 0U;
  gfx::Range selection_range(20, 21);

  for (auto index : active_view_sequence_) {
    render_widget_host_delegate()->set_focused_widget(
        RenderWidgetHostImpl::From(views_[index]->GetRenderWidgetHost()));
    views_[index]->SelectionChanged(text, offset, selection_range);
    base::string16 expected_text = text.substr(
        selection_range.GetMin() - offset, selection_range.length());

    EXPECT_EQ(expected_text, views_[index]->GetSelectedText());

    // Changing offset to make sure that the next view has a different text
    // selection.
    offset++;
  }
}

// This test is for text range.
TEST_F(InputMethodStateAuraTest, GetTextRange) {
  base::string16 text = base::ASCIIToUTF16("some text of length 22");
  size_t offset = 0U;
  gfx::Range selection_range;

  for (auto index : active_view_sequence_) {
    render_widget_host_delegate()->set_focused_widget(
        RenderWidgetHostImpl::From(views_[index]->GetRenderWidgetHost()));
    gfx::Range expected_range(offset, offset + text.length());
    views_[index]->SelectionChanged(text, offset, selection_range);
    gfx::Range range_from_client;

    // For aura this always returns true.
    EXPECT_TRUE(text_input_client()->GetTextRange(&range_from_client));
    EXPECT_EQ(expected_range, range_from_client);

    // Changing offset to make sure that the next view has a different text
    // selection.
    offset++;
  }
}

// This test is for selection range.
TEST_F(InputMethodStateAuraTest, GetSelectionRange) {
  base::string16 text;
  gfx::Range expected_range(0U, 1U);

  for (auto index : active_view_sequence_) {
    render_widget_host_delegate()->set_focused_widget(
        RenderWidgetHostImpl::From(views_[index]->GetRenderWidgetHost()));
    views_[index]->SelectionChanged(text, 0U, expected_range);
    gfx::Range range_from_client;

    // This method always returns true.
    EXPECT_TRUE(text_input_client()->GetSelectionRange(&range_from_client));
    EXPECT_EQ(expected_range, range_from_client);

    // Changing range to make sure that the next view has a different text
    // selection.
    expected_range.set_end(expected_range.end() + 1U);
  }
}

#if defined(USE_X11)
// This test will verify that after selection, the selected text is written to
// the clipboard from the focused widget.
TEST_F(InputMethodStateAuraTest, SelectedTextCopiedToClipboard) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  EXPECT_TRUE(!!clipboard);
  std::vector<std::string> texts = {"text0", "text1", "text2", "text3"};
  for (auto index : active_view_sequence_) {
    clipboard->Clear(ui::CLIPBOARD_TYPE_SELECTION);

    // Focus the corresponding widget.
    render_widget_host_delegate()->set_focused_widget(
        RenderWidgetHostImpl::From(views_[index]->GetRenderWidgetHost()));

    // Change the selection of the currently focused widget. It suffices to just
    // call the method on the view.
    base::string16 expected_text = base::ASCIIToUTF16(texts[index]);
    views_[index]->SelectionChanged(expected_text, 0U, gfx::Range(0, 5));

    // Retrieve the selected text from clipboard and verify it is as expected.
    base::string16 result_text;
    clipboard->ReadText(ui::CLIPBOARD_TYPE_SELECTION, &result_text);
    EXPECT_EQ(expected_text, result_text);
  }
}
#endif

// This test verifies that when any view on the page cancels an ongoing
// composition, the RenderWidgetHostViewAura will receive the notification and
// the current composition is canceled.
TEST_F(InputMethodStateAuraTest, ImeCancelCompositionForAllViews) {
  for (auto* view : views_) {
    ActivateViewForTextInputManager(view, ui::TEXT_INPUT_TYPE_TEXT);
    // There is no composition in the beginning.
    EXPECT_FALSE(has_composition_text());
    SetHasCompositionTextToTrue();
    view->ImeCancelComposition();
    // The composition must have been canceled.
    EXPECT_FALSE(has_composition_text());
  }
}

}  // namespace content
