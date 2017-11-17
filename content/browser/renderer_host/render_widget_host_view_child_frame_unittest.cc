// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"

#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/surface_sequence.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "content/browser/compositor/test/no_transport_image_transport_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/frame_connector_delegate.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/dummy_render_widget_host_delegate.h"
#include "content/test/fake_renderer_compositor_frame_sink.h"
#include "content/test/mock_widget_impl.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"

namespace content {
namespace {

const viz::LocalSurfaceId kArbitraryLocalSurfaceId(
    1,
    base::UnguessableToken::Deserialize(2, 3));

}  // namespace

class MockFrameConnectorDelegate : public FrameConnectorDelegate {
 public:
  MockFrameConnectorDelegate(bool use_zoom_for_device_scale_factor)
      : FrameConnectorDelegate(use_zoom_for_device_scale_factor) {}
  ~MockFrameConnectorDelegate() override {}

  void SetChildFrameSurface(const viz::SurfaceInfo& surface_info,
                            const viz::SurfaceSequence& sequence) override {
    last_surface_info_ = surface_info;
  }

  void SetViewportIntersection(const gfx::Rect& intersection) {
    viewport_intersection_rect_ = intersection;
  }

  RenderWidgetHostViewBase* GetParentRenderWidgetHostView() override {
    return nullptr;
  }

  void BubbleScrollEvent(const blink::WebGestureEvent& event) override {
    if (event.GetType() == blink::WebInputEvent::kGestureScrollUpdate)
      seen_bubbled_gsu_ = true;
  }

  viz::SurfaceInfo last_surface_info_;
  bool seen_bubbled_gsu_ = false;
};

class RenderWidgetHostViewChildFrameTest : public testing::Test {
 public:
  RenderWidgetHostViewChildFrameTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    SetUpEnvironment(false /* use_zoom_for_device_scale_factor */);
  }

  void SetUpEnvironment(bool use_zoom_for_device_scale_factor) {
    browser_context_.reset(new TestBrowserContext);

// ImageTransportFactory doesn't exist on Android.
#if !defined(OS_ANDROID)
    ImageTransportFactory::SetFactory(
        std::make_unique<NoTransportImageTransportFactory>());
#endif

    MockRenderProcessHost* process_host =
        new MockRenderProcessHost(browser_context_.get());
    int32_t routing_id = process_host->GetNextRoutingID();
    mojom::WidgetPtr widget;
    widget_impl_ = std::make_unique<MockWidgetImpl>(mojo::MakeRequest(&widget));
    widget_host_ = new RenderWidgetHostImpl(
        &delegate_, process_host, routing_id, std::move(widget), false);
    view_ = RenderWidgetHostViewChildFrame::Create(widget_host_);

    test_frame_connector_ =
        new MockFrameConnectorDelegate(use_zoom_for_device_scale_factor);
    view_->SetFrameConnectorDelegate(test_frame_connector_);

    viz::mojom::CompositorFrameSinkPtr sink;
    viz::mojom::CompositorFrameSinkRequest sink_request =
        mojo::MakeRequest(&sink);
    viz::mojom::CompositorFrameSinkClientRequest client_request =
        mojo::MakeRequest(&renderer_compositor_frame_sink_ptr_);
    renderer_compositor_frame_sink_ =
        std::make_unique<FakeRendererCompositorFrameSink>(
            std::move(sink), std::move(client_request));
    view_->DidCreateNewRendererCompositorFrameSink(
        renderer_compositor_frame_sink_ptr_.get());
  }

  void TearDown() override {
    if (view_)
      view_->Destroy();
    delete widget_host_;
    delete test_frame_connector_;

    browser_context_.reset();

    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                    browser_context_.release());
    base::RunLoop().RunUntilIdle();
#if !defined(OS_ANDROID)
    ImageTransportFactory::Terminate();
#endif
  }

  viz::SurfaceId GetSurfaceId() const {
    return viz::SurfaceId(view_->frame_sink_id_,
                          view_->last_received_local_surface_id_);
  }

  viz::LocalSurfaceId GetLocalSurfaceId() const {
    return view_->last_received_local_surface_id_;
  }

  void ClearCompositorSurfaceIfNecessary() {
    view_->ClearCompositorSurfaceIfNecessary();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<BrowserContext> browser_context_;
  DummyRenderWidgetHostDelegate delegate_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  std::unique_ptr<MockWidgetImpl> widget_impl_;
  RenderWidgetHostImpl* widget_host_;
  RenderWidgetHostViewChildFrame* view_;
  MockFrameConnectorDelegate* test_frame_connector_;
  std::unique_ptr<FakeRendererCompositorFrameSink>
      renderer_compositor_frame_sink_;

 private:
  viz::mojom::CompositorFrameSinkClientPtr renderer_compositor_frame_sink_ptr_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewChildFrameTest);
};

viz::CompositorFrame CreateDelegatedFrame(float scale_factor,
                                          gfx::Size size,
                                          const gfx::Rect& damage) {
  viz::CompositorFrame frame;
  frame.metadata.device_scale_factor = scale_factor;
  frame.metadata.begin_frame_ack = viz::BeginFrameAck(0, 1, true);

  std::unique_ptr<viz::RenderPass> pass = viz::RenderPass::Create();
  pass->SetNew(1, gfx::Rect(size), damage, gfx::Transform());
  frame.render_pass_list.push_back(std::move(pass));
  return frame;
}

TEST_F(RenderWidgetHostViewChildFrameTest, VisibilityTest) {
  // Calling show and hide also needs to be propagated to child frame by the
  // |frame_connector_| which itself requires a |frame_proxy_in_parent_renderer|
  // (set to nullptr for MockFrameConnectorDelegate). To avoid crashing the test
  // |frame_connector_| is to set to nullptr.
  view_->SetFrameConnectorDelegate(nullptr);

  view_->Show();
  ASSERT_TRUE(view_->IsShowing());

  view_->Hide();
  ASSERT_FALSE(view_->IsShowing());
}

// Verify that SubmitCompositorFrame behavior is correct when a delegated
// frame is received from a renderer process.
TEST_F(RenderWidgetHostViewChildFrameTest, SwapCompositorFrame) {
  gfx::Size view_size(100, 100);
  gfx::Rect view_rect(view_size);
  float scale_factor = 1.f;
  viz::LocalSurfaceId local_surface_id(1, base::UnguessableToken::Create());

  view_->SetSize(view_size);
  view_->Show();

  view_->SubmitCompositorFrame(
      local_surface_id,
      CreateDelegatedFrame(scale_factor, view_size, view_rect), nullptr);

  viz::SurfaceId id = GetSurfaceId();
  if (id.is_valid()) {
#if !defined(OS_ANDROID)
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    viz::SurfaceManager* manager = factory->GetContextFactoryPrivate()
                                       ->GetFrameSinkManager()
                                       ->surface_manager();
    viz::Surface* surface = manager->GetSurfaceForId(id);
    EXPECT_TRUE(surface);
#endif

    // Surface ID should have been passed to FrameConnectorDelegate to
    // be sent to the embedding renderer.
    EXPECT_EQ(viz::SurfaceInfo(id, scale_factor, view_size),
              test_frame_connector_->last_surface_info_);
  }
}

// Check that the same local surface id can be used after frame eviction.
TEST_F(RenderWidgetHostViewChildFrameTest, FrameEviction) {
  gfx::Size view_size(100, 100);
  gfx::Rect view_rect(view_size);
  float scale_factor = 1.f;

  view_->SetSize(view_size);
  view_->Show();

  // Submit a frame.
  view_->SubmitCompositorFrame(
      kArbitraryLocalSurfaceId,
      CreateDelegatedFrame(scale_factor, view_size, view_rect), nullptr);

  EXPECT_EQ(kArbitraryLocalSurfaceId, GetLocalSurfaceId());
  EXPECT_TRUE(view_->has_frame());

  // Evict the frame. has_frame() should return false.
  ClearCompositorSurfaceIfNecessary();
  EXPECT_EQ(kArbitraryLocalSurfaceId, GetLocalSurfaceId());
  EXPECT_FALSE(view_->has_frame());

  // Submit another frame with the same local surface id. The same id should be
  // usable.
  view_->SubmitCompositorFrame(
      kArbitraryLocalSurfaceId,
      CreateDelegatedFrame(scale_factor, view_size, view_rect), nullptr);
  EXPECT_EQ(kArbitraryLocalSurfaceId, GetLocalSurfaceId());
  EXPECT_TRUE(view_->has_frame());
}

// Tests that the viewport intersection rect is dispatched to the RenderWidget
// whenever screen rects are updated.
TEST_F(RenderWidgetHostViewChildFrameTest, ViewportIntersectionUpdated) {
  gfx::Rect intersection_rect(5, 5, 100, 80);
  test_frame_connector_->SetViewportIntersection(intersection_rect);

  MockRenderProcessHost* process =
      static_cast<MockRenderProcessHost*>(widget_host_->GetProcess());
  process->Init();

  widget_host_->Init();

  const IPC::Message* intersection_update =
      process->sink().GetUniqueMessageMatching(
          ViewMsg_SetViewportIntersection::ID);
  ASSERT_TRUE(intersection_update);
  std::tuple<gfx::Rect> sent_rect;
  ViewMsg_SetViewportIntersection::Read(intersection_update, &sent_rect);
  EXPECT_EQ(intersection_rect, std::get<0>(sent_rect));
}

// Tests specific to non-scroll-latching behaviour.
// TODO(mcnee): Remove once scroll-latching lands. crbug.com/526463
class RenderWidgetHostViewChildFrameScrollLatchingDisabledTest
    : public RenderWidgetHostViewChildFrameTest {
 public:
  RenderWidgetHostViewChildFrameScrollLatchingDisabledTest() {}

  void SetUp() override {
    feature_list_.InitWithFeatures({},
                                   {features::kTouchpadAndWheelScrollLatching,
                                    features::kAsyncWheelEvents});

    RenderWidgetHostViewChildFrameTest::SetUp();
    DCHECK(!view_->wheel_scroll_latching_enabled());
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(
      RenderWidgetHostViewChildFrameScrollLatchingDisabledTest);
};

class RenderWidgetHostViewChildFrameZoomForDSFTest
    : public RenderWidgetHostViewChildFrameTest {
 public:
  RenderWidgetHostViewChildFrameZoomForDSFTest() {}

  void SetUp() override {
    SetUpEnvironment(true /* use_zoom_for_device_scale_factor */);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewChildFrameZoomForDSFTest);
};

// Test that when a child scrolls and then stops consuming once it hits the
// extent, we don't bubble the subsequent unconsumed GestureScrollUpdates
// in the same gesture.
TEST_F(RenderWidgetHostViewChildFrameScrollLatchingDisabledTest,
       DoNotBubbleIfChildHasAlreadyScrolled) {
  blink::WebGestureEvent gesture_scroll(
      blink::WebGestureEvent::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::kTimeStampForTesting);
  view_->GestureEventAck(gesture_scroll, INPUT_EVENT_ACK_STATE_IGNORED);

  gesture_scroll.SetType(blink::WebGestureEvent::kGestureScrollUpdate);
  view_->GestureEventAck(gesture_scroll, INPUT_EVENT_ACK_STATE_CONSUMED);
  ASSERT_FALSE(test_frame_connector_->seen_bubbled_gsu_);

  view_->GestureEventAck(gesture_scroll,
                         INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_FALSE(test_frame_connector_->seen_bubbled_gsu_);
}

// Tests that moving the child around does not affect the physical backing size.
TEST_F(RenderWidgetHostViewChildFrameZoomForDSFTest, PhysicalBackingSize) {
  ScreenInfo screen_info;
  screen_info.device_scale_factor = 2.0f;
  test_frame_connector_->SetScreenInfoForTesting(screen_info);

  gfx::Size frame_size_in_pixels(1276, 410);
  gfx::Rect frame_rect_in_pixels(frame_size_in_pixels);
  test_frame_connector_->SetRect(frame_rect_in_pixels);
  EXPECT_EQ(frame_size_in_pixels, view_->GetPhysicalBackingSize());

  frame_rect_in_pixels.set_origin(gfx::Point(230, 263));
  test_frame_connector_->SetRect(frame_rect_in_pixels);
  EXPECT_EQ(frame_size_in_pixels, view_->GetPhysicalBackingSize());
}

}  // namespace content
