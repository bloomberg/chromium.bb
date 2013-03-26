// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_impl.h"

#include <cmath>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/hash_tables.h"
#include "cc/base/math_util.h"
#include "cc/input/top_controls_manager.h"
#include "cc/layers/delegated_renderer_layer_impl.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/io_surface_layer_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/quad_sink.h"
#include "cc/layers/scrollbar_geometry_fixed_thumb.h"
#include "cc/layers/scrollbar_layer_impl.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/layers/tiled_layer_impl.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/gl_renderer.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/layer_tiling_data.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/fake_web_scrollbar_theme_geometry.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "media/base/media.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

using ::testing::Mock;
using ::testing::Return;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::_;
using media::VideoFrame;

namespace cc {
namespace {

class LayerTreeHostImplTest : public testing::Test,
                              public LayerTreeHostImplClient {
 public:
  LayerTreeHostImplTest()
      : proxy_(scoped_ptr<Thread>(NULL)),
        always_impl_thread_(&proxy_),
        always_main_thread_blocked_(&proxy_),
        on_can_draw_state_changed_called_(false),
        has_pending_tree_(false),
        did_request_commit_(false),
        did_request_redraw_(false),
        did_upload_visible_tile_(false),
        reduce_memory_result_(true) {
    media::InitializeMediaLibraryForTesting();
  }

  virtual void SetUp() OVERRIDE {
    LayerTreeSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();

    host_impl_ = LayerTreeHostImpl::Create(settings,
                                           this,
                                           &proxy_,
                                           &stats_instrumentation_);
    host_impl_->InitializeRenderer(CreateOutputSurface());
    host_impl_->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
  }

  virtual void TearDown() OVERRIDE {}

  virtual void DidLoseOutputSurfaceOnImplThread() OVERRIDE {}
  virtual void OnSwapBuffersCompleteOnImplThread() OVERRIDE {}
  virtual void OnVSyncParametersChanged(base::TimeTicks timebase,
                                        base::TimeDelta interval) OVERRIDE {}
  virtual void OnCanDrawStateChanged(bool can_draw) OVERRIDE {
    on_can_draw_state_changed_called_ = true;
  }
  virtual void OnHasPendingTreeStateChanged(bool has_pending_tree) OVERRIDE {
    has_pending_tree_ = has_pending_tree;
  }
  virtual void SetNeedsRedrawOnImplThread() OVERRIDE {
    did_request_redraw_ = true;
  }
  virtual void DidInitializeVisibleTileOnImplThread() OVERRIDE {
    did_upload_visible_tile_ = true;
  }
  virtual void SetNeedsCommitOnImplThread() OVERRIDE {
    did_request_commit_ = true;
  }
  virtual void SetNeedsManageTilesOnImplThread() OVERRIDE {}
  virtual void PostAnimationEventsToMainThreadOnImplThread(
      scoped_ptr<AnimationEventsVector> events,
      base::Time wall_clock_time) OVERRIDE {}
  virtual bool ReduceContentsTextureMemoryOnImplThread(
      size_t limit_bytes, int priority_cutoff) OVERRIDE {
    return reduce_memory_result_;
  }
  virtual void ReduceWastedContentsTextureMemoryOnImplThread() OVERRIDE {}
  virtual void SendManagedMemoryStats() OVERRIDE {}
  virtual bool IsInsideDraw() OVERRIDE { return false; }
  virtual void RenewTreePriority() OVERRIDE {}
  virtual void RequestScrollbarAnimationOnImplThread(base::TimeDelta delay)
      OVERRIDE {}

  void set_reduce_memory_result(bool reduce_memory_result) {
    reduce_memory_result_ = reduce_memory_result;
  }

  void CreateLayerTreeHost(bool partial_swap,
                           scoped_ptr<OutputSurface> output_surface) {
    LayerTreeSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();
    settings.partial_swap_enabled = partial_swap;

    host_impl_ = LayerTreeHostImpl::Create(settings,
                                           this,
                                           &proxy_,
                                           &stats_instrumentation_);

    host_impl_->InitializeRenderer(output_surface.Pass());
    host_impl_->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
  }

  void SetupRootLayerImpl(scoped_ptr<LayerImpl> root) {
    root->SetAnchorPoint(gfx::PointF());
    root->SetPosition(gfx::PointF());
    root->SetBounds(gfx::Size(10, 10));
    root->SetContentBounds(gfx::Size(10, 10));
    root->SetDrawsContent(true);
    root->draw_properties().visible_content_rect = gfx::Rect(0, 0, 10, 10);
    host_impl_->active_tree()->SetRootLayer(root.Pass());
  }

  static void ExpectClearedScrollDeltasRecursive(LayerImpl* layer) {
    ASSERT_EQ(layer->scroll_delta(), gfx::Vector2d());
    for (size_t i = 0; i < layer->children().size(); ++i)
      ExpectClearedScrollDeltasRecursive(layer->children()[i]);
  }

  static void ExpectContains(const ScrollAndScaleSet& scroll_info,
                             int id,
                             gfx::Vector2d scroll_delta) {
    int times_encountered = 0;

    for (size_t i = 0; i < scroll_info.scrolls.size(); ++i) {
      if (scroll_info.scrolls[i].layer_id != id)
        continue;
      EXPECT_VECTOR_EQ(scroll_delta, scroll_info.scrolls[i].scroll_delta);
      times_encountered++;
    }

    ASSERT_EQ(times_encountered, 1);
  }

  static void ExpectNone(const ScrollAndScaleSet& scroll_info, int id) {
    int times_encountered = 0;

    for (size_t i = 0; i < scroll_info.scrolls.size(); ++i) {
      if (scroll_info.scrolls[i].layer_id != id)
        continue;
      times_encountered++;
    }

    ASSERT_EQ(0, times_encountered);
  }

  void SetupScrollAndContentsLayers(gfx::Size content_size) {
    scoped_ptr<LayerImpl> root =
        LayerImpl::Create(host_impl_->active_tree(), 1);
    root->SetScrollable(true);
    root->SetScrollOffset(gfx::Vector2d());
    root->SetMaxScrollOffset(gfx::Vector2d(content_size.width(),
                                           content_size.height()));
    root->SetBounds(content_size);
    root->SetContentBounds(content_size);
    root->SetPosition(gfx::PointF());
    root->SetAnchorPoint(gfx::PointF());

    scoped_ptr<LayerImpl> contents =
        LayerImpl::Create(host_impl_->active_tree(), 2);
    contents->SetDrawsContent(true);
    contents->SetBounds(content_size);
    contents->SetContentBounds(content_size);
    contents->SetPosition(gfx::PointF());
    contents->SetAnchorPoint(gfx::PointF());
    root->AddChild(contents.Pass());
    host_impl_->active_tree()->SetRootLayer(root.Pass());
    host_impl_->active_tree()->DidBecomeActive();
  }

  scoped_ptr<LayerImpl> CreateScrollableLayer(int id, gfx::Size size) {
    scoped_ptr<LayerImpl> layer =
        LayerImpl::Create(host_impl_->active_tree(), id);
    layer->SetScrollable(true);
    layer->SetDrawsContent(true);
    layer->SetBounds(size);
    layer->SetContentBounds(size);
    layer->SetMaxScrollOffset(gfx::Vector2d(size.width() * 2,
                                            size.height() * 2));
    return layer.Pass();
  }

  void InitializeRendererAndDrawFrame() {
    host_impl_->InitializeRenderer(CreateOutputSurface());
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
    host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
    host_impl_->DidDrawAllLayers(frame);
  }

  void pinch_zoom_pan_viewport_forces_commit_redraw(float device_scale_factor);
  void pinch_zoom_pan_viewport_test(float device_scale_factor);
  void pinch_zoom_pan_viewport_and_scroll_test(float device_scale_factor);
  void pinch_zoom_pan_viewport_and_scroll_boundary_test(
      float device_scale_factor);

 protected:
  virtual scoped_ptr<OutputSurface> CreateOutputSurface() {
    return CreateFakeOutputSurface();
  }

  void DrawOneFrame() {
    LayerTreeHostImpl::FrameData frame_data;
    host_impl_->PrepareToDraw(&frame_data);
    host_impl_->DidDrawAllLayers(frame_data);
  }

  FakeProxy proxy_;
  DebugScopedSetImplThread always_impl_thread_;
  DebugScopedSetMainThreadBlocked always_main_thread_blocked_;

  scoped_ptr<LayerTreeHostImpl> host_impl_;
  FakeRenderingStatsInstrumentation stats_instrumentation_;
  bool on_can_draw_state_changed_called_;
  bool has_pending_tree_;
  bool did_request_commit_;
  bool did_request_redraw_;
  bool did_upload_visible_tile_;
  bool reduce_memory_result_;
};

class TestWebGraphicsContext3DMakeCurrentFails
    : public TestWebGraphicsContext3D {
 public:
  virtual bool makeContextCurrent() OVERRIDE { return false; }
};

TEST_F(LayerTreeHostImplTest, NotifyIfCanDrawChanged) {
  // Note: It is not possible to disable the renderer once it has been set,
  // so we do not need to test that disabling the renderer notifies us
  // that can_draw changed.
  EXPECT_FALSE(host_impl_->CanDraw());
  on_can_draw_state_changed_called_ = false;

  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  EXPECT_TRUE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  // Toggle the root layer to make sure it toggles can_draw
  host_impl_->active_tree()->SetRootLayer(scoped_ptr<LayerImpl>());
  EXPECT_FALSE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  EXPECT_TRUE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  // Toggle the device viewport size to make sure it toggles can_draw.
  host_impl_->SetViewportSize(gfx::Size(100, 100), gfx::Size());
  EXPECT_FALSE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  host_impl_->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));
  EXPECT_TRUE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  // Toggle contents textures purged without causing any evictions,
  // and make sure that it does not change can_draw.
  set_reduce_memory_result(false);
  host_impl_->SetManagedMemoryPolicy(ManagedMemoryPolicy(
      host_impl_->memory_allocation_limit_bytes() - 1));
  EXPECT_TRUE(host_impl_->CanDraw());
  EXPECT_FALSE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  // Toggle contents textures purged to make sure it toggles can_draw.
  set_reduce_memory_result(true);
  host_impl_->SetManagedMemoryPolicy(ManagedMemoryPolicy(
      host_impl_->memory_allocation_limit_bytes() - 1));
  EXPECT_FALSE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  host_impl_->active_tree()->ResetContentsTexturesPurged();
  EXPECT_TRUE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;
}

TEST_F(LayerTreeHostImplTest, ScrollDeltaNoLayers) {
  ASSERT_FALSE(host_impl_->active_tree()->root_layer());

  scoped_ptr<ScrollAndScaleSet> scroll_info = host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 0u);
}

TEST_F(LayerTreeHostImplTest, ScrollDeltaTreeButNoChanges) {
  {
    scoped_ptr<LayerImpl> root =
        LayerImpl::Create(host_impl_->active_tree(), 1);
    root->AddChild(LayerImpl::Create(host_impl_->active_tree(), 2));
    root->AddChild(LayerImpl::Create(host_impl_->active_tree(), 3));
    root->children()[1]->AddChild(
        LayerImpl::Create(host_impl_->active_tree(), 4));
    root->children()[1]->AddChild(
        LayerImpl::Create(host_impl_->active_tree(), 5));
    root->children()[1]->children()[0]->AddChild(
        LayerImpl::Create(host_impl_->active_tree(), 6));
    host_impl_->active_tree()->SetRootLayer(root.Pass());
  }
  LayerImpl* root = host_impl_->active_tree()->root_layer();

  ExpectClearedScrollDeltasRecursive(root);

  scoped_ptr<ScrollAndScaleSet> scroll_info;

  scroll_info = host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 0u);
  ExpectClearedScrollDeltasRecursive(root);

  scroll_info = host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 0u);
  ExpectClearedScrollDeltasRecursive(root);
}

TEST_F(LayerTreeHostImplTest, ScrollDeltaRepeatedScrolls) {
  gfx::Vector2d scroll_offset(20, 30);
  gfx::Vector2d scroll_delta(11, -15);
  {
    scoped_ptr<LayerImpl> root =
        LayerImpl::Create(host_impl_->active_tree(), 1);
    root->SetScrollOffset(scroll_offset);
    root->SetScrollable(true);
    root->SetMaxScrollOffset(gfx::Vector2d(100, 100));
    root->ScrollBy(scroll_delta);
    host_impl_->active_tree()->SetRootLayer(root.Pass());
  }
  LayerImpl* root = host_impl_->active_tree()->root_layer();

  scoped_ptr<ScrollAndScaleSet> scroll_info;

  scroll_info = host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 1u);
  EXPECT_VECTOR_EQ(root->sent_scroll_delta(), scroll_delta);
  ExpectContains(*scroll_info, root->id(), scroll_delta);

  gfx::Vector2d scroll_delta2(-5, 27);
  root->ScrollBy(scroll_delta2);
  scroll_info = host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 1u);
  EXPECT_VECTOR_EQ(root->sent_scroll_delta(), scroll_delta + scroll_delta2);
  ExpectContains(*scroll_info, root->id(), scroll_delta + scroll_delta2);

  root->ScrollBy(gfx::Vector2d());
  scroll_info = host_impl_->ProcessScrollDeltas();
  EXPECT_EQ(root->sent_scroll_delta(), scroll_delta + scroll_delta2);
}

TEST_F(LayerTreeHostImplTest, ScrollRootCallsCommitAndRedraw) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Wheel));
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
  host_impl_->ScrollEnd();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollWithoutRootLayer) {
  // We should not crash when trying to scroll an empty layer tree.
  EXPECT_EQ(InputHandlerClient::ScrollIgnored,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Wheel));
}

TEST_F(LayerTreeHostImplTest, ScrollWithoutRenderer) {
  LayerTreeSettings settings;
  host_impl_ = LayerTreeHostImpl::Create(settings,
                                         this,
                                         &proxy_,
                                         &stats_instrumentation_);

  // Initialization will fail here.
  host_impl_->InitializeRenderer(FakeOutputSurface::Create3d(
      scoped_ptr<WebKit::WebGraphicsContext3D>(
          new TestWebGraphicsContext3DMakeCurrentFails))
      .PassAs<OutputSurface>());
  host_impl_->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));

  SetupScrollAndContentsLayers(gfx::Size(100, 100));

  // We should not crash when trying to scroll after the renderer initialization
  // fails.
  EXPECT_EQ(InputHandlerClient::ScrollIgnored,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Wheel));
}

TEST_F(LayerTreeHostImplTest, ReplaceTreeWhileScrolling) {
  int scroll_layer_id = 1;

  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  // We should not crash if the tree is replaced while we are scrolling.
  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Wheel));
  host_impl_->active_tree()->DetachLayerTree();

  SetupScrollAndContentsLayers(gfx::Size(100, 100));

  // We should still be scrolling, because the scrolled layer also exists in the
  // new tree.
  gfx::Vector2d scroll_delta(0, 10);
  host_impl_->ScrollBy(gfx::Point(), scroll_delta);
  host_impl_->ScrollEnd();
  scoped_ptr<ScrollAndScaleSet> scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info, scroll_layer_id, scroll_delta);
}

TEST_F(LayerTreeHostImplTest, ClearRootRenderSurfaceAndScroll) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  // We should be able to scroll even if the root layer loses its render surface
  // after the most recent render.
  host_impl_->active_tree()->root_layer()->ClearRenderSurface();
  host_impl_->active_tree()->set_needs_update_draw_properties();

  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Wheel));
}

TEST_F(LayerTreeHostImplTest, WheelEventHandlers) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();
  LayerImpl* root = host_impl_->active_tree()->root_layer();

  root->SetHaveWheelEventHandlers(true);

  // With registered event handlers, wheel scrolls have to go to the main
  // thread.
  EXPECT_EQ(InputHandlerClient::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Wheel));

  // But gesture scrolls can still be handled.
  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Gesture));
}

TEST_F(LayerTreeHostImplTest, ShouldScrollOnMainThread) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();
  LayerImpl* root = host_impl_->active_tree()->root_layer();

  root->SetShouldScrollOnMainThread(true);

  EXPECT_EQ(InputHandlerClient::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Wheel));
  EXPECT_EQ(InputHandlerClient::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Gesture));
}

TEST_F(LayerTreeHostImplTest, NonFastScrollableRegionBasic) {
  SetupScrollAndContentsLayers(gfx::Size(200, 200));
  host_impl_->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

  LayerImpl* root = host_impl_->active_tree()->root_layer();
  root->SetContentsScale(2.f, 2.f);
  root->SetNonFastScrollableRegion(gfx::Rect(0, 0, 50, 50));

  InitializeRendererAndDrawFrame();

  // All scroll types inside the non-fast scrollable region should fail.
  EXPECT_EQ(InputHandlerClient::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(25, 25),
                                    InputHandlerClient::Wheel));
  EXPECT_EQ(InputHandlerClient::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(25, 25),
                                    InputHandlerClient::Gesture));

  // All scroll types outside this region should succeed.
  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(75, 75),
                                    InputHandlerClient::Wheel));
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
  host_impl_->ScrollEnd();
  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(75, 75),
                                    InputHandlerClient::Gesture));
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
  host_impl_->ScrollEnd();
}

TEST_F(LayerTreeHostImplTest, NonFastScrollableRegionWithOffset) {
  SetupScrollAndContentsLayers(gfx::Size(200, 200));
  host_impl_->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

  LayerImpl* root = host_impl_->active_tree()->root_layer();
  root->SetContentsScale(2.f, 2.f);
  root->SetNonFastScrollableRegion(gfx::Rect(0, 0, 50, 50));
  root->SetPosition(gfx::PointF(-25.f, 0.f));

  InitializeRendererAndDrawFrame();

  // This point would fall into the non-fast scrollable region except that we've
  // moved the layer down by 25 pixels.
  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(40, 10),
                                    InputHandlerClient::Wheel));
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 1));
  host_impl_->ScrollEnd();

  // This point is still inside the non-fast region.
  EXPECT_EQ(InputHandlerClient::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(10, 10),
                                    InputHandlerClient::Wheel));
}

TEST_F(LayerTreeHostImplTest, ScrollByReturnsCorrectValue) {
  SetupScrollAndContentsLayers(gfx::Size(200, 200));
  host_impl_->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

  InitializeRendererAndDrawFrame();

  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Gesture));

  // Trying to scroll to the left/top will not succeed.
  EXPECT_FALSE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(-10, 0)));
  EXPECT_FALSE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, -10)));
  EXPECT_FALSE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(-10, -10)));

  // Scrolling to the right/bottom will succeed.
  EXPECT_TRUE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(10, 0)));
  EXPECT_TRUE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10)));
  EXPECT_TRUE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(10, 10)));

  // Scrolling to left/top will now succeed.
  EXPECT_TRUE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(-10, 0)));
  EXPECT_TRUE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, -10)));
  EXPECT_TRUE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(-10, -10)));

  // Scrolling diagonally against an edge will succeed.
  EXPECT_TRUE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(10, -10)));
  EXPECT_TRUE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(-10, 0)));
  EXPECT_TRUE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(-10, 10)));

  // Trying to scroll more than the available space will also succeed.
  EXPECT_TRUE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(5000, 5000)));
}

TEST_F(LayerTreeHostImplTest, ScrollVerticallyByPageReturnsCorrectValue) {
  SetupScrollAndContentsLayers(gfx::Size(200, 2000));
  host_impl_->SetViewportSize(gfx::Size(100, 1000), gfx::Size(100, 1000));

  InitializeRendererAndDrawFrame();

  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(),
                                    InputHandlerClient::Wheel));

  // Trying to scroll without a vertical scrollbar will fail.
  EXPECT_FALSE(host_impl_->ScrollVerticallyByPage(
      gfx::Point(), WebKit::WebScrollbar::ScrollForward));
  EXPECT_FALSE(host_impl_->ScrollVerticallyByPage(
      gfx::Point(), WebKit::WebScrollbar::ScrollBackward));

  scoped_ptr<cc::ScrollbarLayerImpl> vertical_scrollbar(
      cc::ScrollbarLayerImpl::Create(
          host_impl_->active_tree(),
          20,
          scoped_ptr<ScrollbarGeometryFixedThumb>()));
  vertical_scrollbar->SetBounds(gfx::Size(15, 1000));
  host_impl_->RootScrollLayer()->SetVerticalScrollbarLayer(
      vertical_scrollbar.get());

  // Trying to scroll with a vertical scrollbar will succeed.
  EXPECT_TRUE(host_impl_->ScrollVerticallyByPage(
      gfx::Point(), WebKit::WebScrollbar::ScrollForward));
  EXPECT_FLOAT_EQ(875.f, host_impl_->RootScrollLayer()->scroll_delta().y());
  EXPECT_TRUE(host_impl_->ScrollVerticallyByPage(
      gfx::Point(), WebKit::WebScrollbar::ScrollBackward));
}

TEST_F(LayerTreeHostImplTest,
       ClearRootRenderSurfaceAndHitTestTouchHandlerRegion) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  // We should be able to hit test for touch event handlers even if the root
  // layer loses its render surface after the most recent render.
  host_impl_->active_tree()->root_layer()->ClearRenderSurface();
  host_impl_->active_tree()->set_needs_update_draw_properties();

  EXPECT_EQ(host_impl_->HaveTouchEventHandlersAt(gfx::Point()), false);
}

TEST_F(LayerTreeHostImplTest, ImplPinchZoom) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  LayerImpl* scroll_layer = host_impl_->RootScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 1.f, max_page_scale = 4.f;
  gfx::Transform identity_scale_transform;

  // The impl-based pinch zoom should adjust the max scroll position.
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    host_impl_->active_tree()->SetPageScaleDelta(1.f);
    scroll_layer->SetImplTransform(identity_scale_transform);
    scroll_layer->SetScrollDelta(gfx::Vector2d());

    float page_scale_delta = 2.f;
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_commit_);

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);

    EXPECT_EQ(gfx::Vector2d(75, 75),
              host_impl_->active_tree()->root_layer()->max_scroll_offset());
  }

  // Scrolling after a pinch gesture should always be in local space.  The
  // scroll deltas do not have the page scale factor applied.
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    host_impl_->active_tree()->SetPageScaleDelta(1.f);
    scroll_layer->SetImplTransform(identity_scale_transform);
    scroll_layer->SetScrollDelta(gfx::Vector2d());

    float page_scale_delta = 2.f;
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point());
    host_impl_->PinchGestureEnd();

    gfx::Vector2d scroll_delta(0, 10);
    EXPECT_EQ(InputHandlerClient::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(5, 5),
                                      InputHandlerClient::Wheel));
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    host_impl_->ScrollEnd();

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    ExpectContains(*scroll_info.get(),
                   host_impl_->active_tree()->root_layer()->id(),
                   scroll_delta);
  }
}

TEST_F(LayerTreeHostImplTest, PinchGesture) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  LayerImpl* scroll_layer = host_impl_->RootScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 1.f;
  float max_page_scale = 4.f;
  gfx::Transform identity_scale_transform;

  // Basic pinch zoom in gesture
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    scroll_layer->SetImplTransform(identity_scale_transform);
    scroll_layer->SetScrollDelta(gfx::Vector2d());

    float page_scale_delta = 2.f;
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_commit_);

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);
  }

  // Zoom-in clamping
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    scroll_layer->SetImplTransform(identity_scale_transform);
    scroll_layer->SetScrollDelta(gfx::Vector2d());
    float page_scale_delta = 10.f;

    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd();

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, max_page_scale);
  }

  // Zoom-out clamping
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    scroll_layer->SetImplTransform(identity_scale_transform);
    scroll_layer->SetScrollDelta(gfx::Vector2d());
    scroll_layer->SetScrollOffset(gfx::Vector2d(50, 50));

    float page_scale_delta = 0.1f;
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point());
    host_impl_->PinchGestureEnd();

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, min_page_scale);

    EXPECT_TRUE(scroll_info->scrolls.empty());
  }

  // Two-finger panning should not happen based on pinch events only
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    scroll_layer->SetImplTransform(identity_scale_transform);
    scroll_layer->SetScrollDelta(gfx::Vector2d());
    scroll_layer->SetScrollOffset(gfx::Vector2d(20, 20));

    float page_scale_delta = 1.f;
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(10, 10));
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(20, 20));
    host_impl_->PinchGestureEnd();

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);
    EXPECT_TRUE(scroll_info->scrolls.empty());
  }

  // Two-finger panning should work with interleaved scroll events
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    scroll_layer->SetImplTransform(identity_scale_transform);
    scroll_layer->SetScrollDelta(gfx::Vector2d());
    scroll_layer->SetScrollOffset(gfx::Vector2d(20, 20));

    float page_scale_delta = 1.f;
    host_impl_->ScrollBegin(gfx::Point(10, 10), InputHandlerClient::Wheel);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(10, 10));
    host_impl_->ScrollBy(gfx::Point(10, 10), gfx::Vector2d(-10, -10));
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(20, 20));
    host_impl_->PinchGestureEnd();
    host_impl_->ScrollEnd();

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);
    ExpectContains(*scroll_info, scroll_layer->id(), gfx::Vector2d(-10, -10));
  }
}

TEST_F(LayerTreeHostImplTest, PageScaleAnimation) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  LayerImpl* scroll_layer = host_impl_->RootScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 0.5f;
  float max_page_scale = 4.f;
  base::TimeTicks start_time = base::TimeTicks() +
                               base::TimeDelta::FromSeconds(1);
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;
  gfx::Transform identity_scale_transform;

  // Non-anchor zoom-in
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    scroll_layer->SetImplTransform(identity_scale_transform);
    scroll_layer->SetScrollOffset(gfx::Vector2d(50, 50));

    host_impl_->StartPageScaleAnimation(gfx::Vector2d(),
                                        false,
                                        2.f,
                                        start_time,
                                        duration);
    host_impl_->Animate(halfway_through_animation, base::Time());
    EXPECT_TRUE(did_request_redraw_);
    host_impl_->Animate(end_time, base::Time());
    EXPECT_TRUE(did_request_commit_);

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, 2);
    ExpectContains(*scroll_info, scroll_layer->id(), gfx::Vector2d(-50, -50));
  }

  // Anchor zoom-out
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    scroll_layer->SetImplTransform(identity_scale_transform);
    scroll_layer->SetScrollOffset(gfx::Vector2d(50, 50));

    host_impl_->StartPageScaleAnimation(gfx::Vector2d(25, 25),
                                        true,
                                        min_page_scale,
                                        start_time, duration);
    host_impl_->Animate(end_time, base::Time());
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_commit_);

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, min_page_scale);
    // Pushed to (0,0) via clamping against contents layer size.
    ExpectContains(*scroll_info, scroll_layer->id(), gfx::Vector2d(-50, -50));
  }
}

TEST_F(LayerTreeHostImplTest, PageScaleAnimationNoOp) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  LayerImpl* scroll_layer = host_impl_->RootScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 0.5f;
  float max_page_scale = 4.f;
  base::TimeTicks start_time = base::TimeTicks() +
                               base::TimeDelta::FromSeconds(1);
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;
  gfx::Transform identity_scale_transform;

  // Anchor zoom with unchanged page scale should not change scroll or scale.
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    scroll_layer->SetImplTransform(identity_scale_transform);
    scroll_layer->SetScrollOffset(gfx::Vector2d(50, 50));

    host_impl_->StartPageScaleAnimation(gfx::Vector2d(),
                                        true,
                                        1.f,
                                        start_time,
                                        duration);
    host_impl_->Animate(halfway_through_animation, base::Time());
    EXPECT_TRUE(did_request_redraw_);
    host_impl_->Animate(end_time, base::Time());
    EXPECT_TRUE(did_request_commit_);

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, 1);
    ExpectNone(*scroll_info, scroll_layer->id());
  }
}

TEST_F(LayerTreeHostImplTest, CompositorFrameMetadata) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50), gfx::Size(50, 50));
  host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f, 0.5f, 4.f);
  InitializeRendererAndDrawFrame();
  {
    CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(), metadata.root_scroll_offset);
    EXPECT_EQ(1.f, metadata.page_scale_factor);
    EXPECT_EQ(gfx::SizeF(50.f, 50.f), metadata.viewport_size);
    EXPECT_EQ(gfx::SizeF(100.f, 100.f), metadata.root_layer_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
    EXPECT_EQ(4.f, metadata.max_page_scale_factor);
  }

  // Scrolling should update metadata immediately.
  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Wheel));
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
  {
    CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(0.f, 10.f), metadata.root_scroll_offset);
  }
  host_impl_->ScrollEnd();
  {
    CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(0.f, 10.f), metadata.root_scroll_offset);
  }

  // Page scale should update metadata correctly (shrinking only the viewport).
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2.f, gfx::Point());
  host_impl_->PinchGestureEnd();
  {
    CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(0.f, 10.f), metadata.root_scroll_offset);
    EXPECT_EQ(2.f, metadata.page_scale_factor);
    EXPECT_EQ(gfx::SizeF(25.f, 25.f), metadata.viewport_size);
    EXPECT_EQ(gfx::SizeF(100.f, 100.f), metadata.root_layer_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
    EXPECT_EQ(4.f, metadata.max_page_scale_factor);
  }

  // Likewise if set from the main thread.
  host_impl_->ProcessScrollDeltas();
  host_impl_->active_tree()->SetPageScaleFactorAndLimits(4.f, 0.5f, 4.f);
  host_impl_->active_tree()->SetPageScaleDelta(1.f);
  {
    CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(0.f, 10.f), metadata.root_scroll_offset);
    EXPECT_EQ(4.f, metadata.page_scale_factor);
    EXPECT_EQ(gfx::SizeF(12.5f, 12.5f), metadata.viewport_size);
    EXPECT_EQ(gfx::SizeF(100.f, 100.f), metadata.root_layer_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
    EXPECT_EQ(4.f, metadata.max_page_scale_factor);
  }
}

class DidDrawCheckLayer : public TiledLayerImpl {
 public:
  static scoped_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return scoped_ptr<LayerImpl>(new DidDrawCheckLayer(tree_impl, id));
  }

  virtual void DidDraw(ResourceProvider* provider) OVERRIDE {
    did_draw_called_ = true;
  }

  virtual void WillDraw(ResourceProvider* provider) OVERRIDE {
    will_draw_called_ = true;
  }

  bool did_draw_called() const { return did_draw_called_; }
  bool will_draw_called() const { return will_draw_called_; }

  void ClearDidDrawCheck() {
    did_draw_called_ = false;
    will_draw_called_ = false;
  }

 protected:
  DidDrawCheckLayer(LayerTreeImpl* tree_impl, int id)
      : TiledLayerImpl(tree_impl, id),
        did_draw_called_(false),
        will_draw_called_(false) {
    SetAnchorPoint(gfx::PointF());
    SetBounds(gfx::Size(10, 10));
    SetContentBounds(gfx::Size(10, 10));
    SetDrawsContent(true);
    set_skips_draw(false);
    draw_properties().visible_content_rect = gfx::Rect(0, 0, 10, 10);

    scoped_ptr<LayerTilingData> tiler =
        LayerTilingData::Create(gfx::Size(100, 100),
                                LayerTilingData::HAS_BORDER_TEXELS);
    tiler->SetBounds(content_bounds());
    SetTilingData(*tiler.get());
  }

 private:
  bool did_draw_called_;
  bool will_draw_called_;
};

TEST_F(LayerTreeHostImplTest, DidDrawNotCalledOnHiddenLayer) {
  // The root layer is always drawn, so run this test on a child layer that
  // will be masked out by the root layer's bounds.
  host_impl_->active_tree()->SetRootLayer(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 1));
  DidDrawCheckLayer* root = static_cast<DidDrawCheckLayer*>(
      host_impl_->active_tree()->root_layer());
  root->SetMasksToBounds(true);

  root->AddChild(DidDrawCheckLayer::Create(host_impl_->active_tree(), 2));
  DidDrawCheckLayer* layer =
      static_cast<DidDrawCheckLayer*>(root->children()[0]);
  // Ensure visible_content_rect for layer is empty.
  layer->SetPosition(gfx::PointF(100.f, 100.f));
  layer->SetBounds(gfx::Size(10, 10));
  layer->SetContentBounds(gfx::Size(10, 10));

  LayerTreeHostImpl::FrameData frame;

  EXPECT_FALSE(layer->will_draw_called());
  EXPECT_FALSE(layer->did_draw_called());

  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_FALSE(layer->will_draw_called());
  EXPECT_FALSE(layer->did_draw_called());

  EXPECT_TRUE(layer->visible_content_rect().IsEmpty());

  // Ensure visible_content_rect for layer is not empty
  layer->SetPosition(gfx::PointF());

  EXPECT_FALSE(layer->will_draw_called());
  EXPECT_FALSE(layer->did_draw_called());

  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_TRUE(layer->will_draw_called());
  EXPECT_TRUE(layer->did_draw_called());

  EXPECT_FALSE(layer->visible_content_rect().IsEmpty());
}

TEST_F(LayerTreeHostImplTest, WillDrawNotCalledOnOccludedLayer) {
  gfx::Size big_size(1000, 1000);
  host_impl_->SetViewportSize(big_size, big_size);

  host_impl_->active_tree()->SetRootLayer(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 1));
  DidDrawCheckLayer* root =
      static_cast<DidDrawCheckLayer*>(host_impl_->active_tree()->root_layer());

  root->AddChild(DidDrawCheckLayer::Create(host_impl_->active_tree(), 2));
  DidDrawCheckLayer* occluded_layer =
      static_cast<DidDrawCheckLayer*>(root->children()[0]);

  root->AddChild(DidDrawCheckLayer::Create(host_impl_->active_tree(), 3));
  DidDrawCheckLayer* top_layer =
      static_cast<DidDrawCheckLayer*>(root->children()[1]);
  // This layer covers the occluded_layer above. Make this layer large so it can
  // occlude.
  top_layer->SetBounds(big_size);
  top_layer->SetContentBounds(big_size);
  top_layer->SetContentsOpaque(true);

  LayerTreeHostImpl::FrameData frame;

  EXPECT_FALSE(occluded_layer->will_draw_called());
  EXPECT_FALSE(occluded_layer->did_draw_called());
  EXPECT_FALSE(top_layer->will_draw_called());
  EXPECT_FALSE(top_layer->did_draw_called());

  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_FALSE(occluded_layer->will_draw_called());
  EXPECT_FALSE(occluded_layer->did_draw_called());
  EXPECT_TRUE(top_layer->will_draw_called());
  EXPECT_TRUE(top_layer->did_draw_called());
}

TEST_F(LayerTreeHostImplTest, DidDrawCalledOnAllLayers) {
  host_impl_->active_tree()->SetRootLayer(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 1));
  DidDrawCheckLayer* root =
      static_cast<DidDrawCheckLayer*>(host_impl_->active_tree()->root_layer());

  root->AddChild(DidDrawCheckLayer::Create(host_impl_->active_tree(), 2));
  DidDrawCheckLayer* layer1 =
      static_cast<DidDrawCheckLayer*>(root->children()[0]);

  layer1->AddChild(DidDrawCheckLayer::Create(host_impl_->active_tree(), 3));
  DidDrawCheckLayer* layer2 =
      static_cast<DidDrawCheckLayer*>(layer1->children()[0]);

  layer1->SetOpacity(0.3f);
  layer1->SetPreserves3d(false);

  EXPECT_FALSE(root->did_draw_called());
  EXPECT_FALSE(layer1->did_draw_called());
  EXPECT_FALSE(layer2->did_draw_called());

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_TRUE(root->did_draw_called());
  EXPECT_TRUE(layer1->did_draw_called());
  EXPECT_TRUE(layer2->did_draw_called());

  EXPECT_NE(root->render_surface(), layer1->render_surface());
  EXPECT_TRUE(!!layer1->render_surface());
}

class MissingTextureAnimatingLayer : public DidDrawCheckLayer {
 public:
  static scoped_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl,
                                      int id,
                                      bool tile_missing,
                                      bool skips_draw,
                                      bool animating,
                                      ResourceProvider* resource_provider) {
    return scoped_ptr<LayerImpl>(new MissingTextureAnimatingLayer(
        tree_impl,
        id,
        tile_missing,
        skips_draw,
        animating,
        resource_provider));
  }

 private:
  MissingTextureAnimatingLayer(LayerTreeImpl* tree_impl,
                               int id,
                               bool tile_missing,
                               bool skips_draw,
                               bool animating,
                               ResourceProvider* resource_provider)
      : DidDrawCheckLayer(tree_impl, id) {
    scoped_ptr<LayerTilingData> tiling_data =
        LayerTilingData::Create(gfx::Size(10, 10),
                                LayerTilingData::NO_BORDER_TEXELS);
    tiling_data->SetBounds(bounds());
    SetTilingData(*tiling_data.get());
    set_skips_draw(skips_draw);
    if (!tile_missing) {
      ResourceProvider::ResourceId resource =
          resource_provider->CreateResource(gfx::Size(),
                                            GL_RGBA,
                                            ResourceProvider::TextureUsageAny);
      resource_provider->AllocateForTesting(resource);
      PushTileProperties(0, 0, resource, gfx::Rect(), false);
    }
    if (animating)
      AddAnimatedTransformToLayer(this, 10.0, 3, 0);
  }
};

TEST_F(LayerTreeHostImplTest, PrepareToDrawFailsWhenAnimationUsesCheckerboard) {
  // When the texture is not missing, we draw as usual.
  host_impl_->active_tree()->SetRootLayer(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 1));
  DidDrawCheckLayer* root =
      static_cast<DidDrawCheckLayer*>(host_impl_->active_tree()->root_layer());
  root->AddChild(
      MissingTextureAnimatingLayer::Create(host_impl_->active_tree(),
                                           2,
                                           false,
                                           false,
                                           true,
                                           host_impl_->resource_provider()));

  LayerTreeHostImpl::FrameData frame;

  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);

  // When a texture is missing and we're not animating, we draw as usual with
  // checkerboarding.
  host_impl_->active_tree()->SetRootLayer(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 3));
  root =
      static_cast<DidDrawCheckLayer*>(host_impl_->active_tree()->root_layer());
  root->AddChild(
      MissingTextureAnimatingLayer::Create(host_impl_->active_tree(),
                                           4,
                                           true,
                                           false,
                                           false,
                                           host_impl_->resource_provider()));

  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);

  // When a texture is missing and we're animating, we don't want to draw
  // anything.
  host_impl_->active_tree()->SetRootLayer(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 5));
  root =
      static_cast<DidDrawCheckLayer*>(host_impl_->active_tree()->root_layer());
  root->AddChild(
      MissingTextureAnimatingLayer::Create(host_impl_->active_tree(),
                                           6,
                                           true,
                                           false,
                                           true,
                                           host_impl_->resource_provider()));

  EXPECT_FALSE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);

  // When the layer skips draw and we're animating, we still draw the frame.
  host_impl_->active_tree()->SetRootLayer(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 7));
  root =
      static_cast<DidDrawCheckLayer*>(host_impl_->active_tree()->root_layer());
  root->AddChild(
      MissingTextureAnimatingLayer::Create(host_impl_->active_tree(),
                                           8,
                                           false,
                                           true,
                                           true,
                                           host_impl_->resource_provider()));

  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(LayerTreeHostImplTest, ScrollRootIgnored) {
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl_->active_tree(), 1);
  root->SetScrollable(false);
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  InitializeRendererAndDrawFrame();

  // Scroll event is ignored because layer is not scrollable.
  EXPECT_EQ(InputHandlerClient::ScrollIgnored,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Wheel));
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollNonScrollableRootWithTopControls) {
  LayerTreeSettings settings;
  settings.calculate_top_controls_position = true;
  settings.top_controls_height = 50;

  host_impl_ = LayerTreeHostImpl::Create(settings,
                                         this,
                                         &proxy_,
                                         &stats_instrumentation_);
  host_impl_->InitializeRenderer(CreateOutputSurface());
  host_impl_->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));

  gfx::Size layer_size(5, 5);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl_->active_tree(), 1);
  root->SetScrollable(true);
  root->SetMaxScrollOffset(gfx::Vector2d(layer_size.width(),
                                         layer_size.height()));
  root->SetBounds(layer_size);
  root->SetContentBounds(layer_size);
  root->SetPosition(gfx::PointF());
  root->SetAnchorPoint(gfx::PointF());
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->FindRootScrollLayer();
  InitializeRendererAndDrawFrame();

  EXPECT_EQ(InputHandlerClient::ScrollIgnored,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Gesture));

  host_impl_->top_controls_manager()->ScrollBegin();
  host_impl_->top_controls_manager()->ScrollBy(gfx::Vector2dF(0.f, 50.f));
  host_impl_->top_controls_manager()->ScrollEnd();
  EXPECT_EQ(host_impl_->top_controls_manager()->content_top_offset(), 0.f);

  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(),
                                    InputHandlerClient::Gesture));
}

TEST_F(LayerTreeHostImplTest, ScrollNonCompositedRoot) {
  // Test the configuration where a non-composited root layer is embedded in a
  // scrollable outer layer.
  gfx::Size surface_size(10, 10);

  scoped_ptr<LayerImpl> content_layer =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  content_layer->SetDrawsContent(true);
  content_layer->SetPosition(gfx::PointF());
  content_layer->SetAnchorPoint(gfx::PointF());
  content_layer->SetBounds(surface_size);
  content_layer->SetContentBounds(gfx::Size(surface_size.width() * 2,
                                            surface_size.height() * 2));
  content_layer->SetContentsScale(2.f, 2.f);

  scoped_ptr<LayerImpl> scroll_layer =
      LayerImpl::Create(host_impl_->active_tree(), 2);
  scroll_layer->SetScrollable(true);
  scroll_layer->SetMaxScrollOffset(gfx::Vector2d(surface_size.width(),
                                                 surface_size.height()));
  scroll_layer->SetBounds(surface_size);
  scroll_layer->SetContentBounds(surface_size);
  scroll_layer->SetPosition(gfx::PointF());
  scroll_layer->SetAnchorPoint(gfx::PointF());
  scroll_layer->AddChild(content_layer.Pass());

  host_impl_->active_tree()->SetRootLayer(scroll_layer.Pass());
  host_impl_->SetViewportSize(surface_size, surface_size);
  InitializeRendererAndDrawFrame();

  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandlerClient::Wheel));
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
  host_impl_->ScrollEnd();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollChildCallsCommitAndRedraw) {
  gfx::Size surface_size(10, 10);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl_->active_tree(), 1);
  root->SetBounds(surface_size);
  root->SetContentBounds(surface_size);
  root->AddChild(CreateScrollableLayer(2, surface_size));
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->SetViewportSize(surface_size, surface_size);
  InitializeRendererAndDrawFrame();

  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandlerClient::Wheel));
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
  host_impl_->ScrollEnd();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollMissesChild) {
  gfx::Size surface_size(10, 10);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl_->active_tree(), 1);
  root->AddChild(CreateScrollableLayer(2, surface_size));
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->SetViewportSize(surface_size, surface_size);
  InitializeRendererAndDrawFrame();

  // Scroll event is ignored because the input coordinate is outside the layer
  // boundaries.
  EXPECT_EQ(InputHandlerClient::ScrollIgnored,
            host_impl_->ScrollBegin(gfx::Point(15, 5),
                                    InputHandlerClient::Wheel));
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollMissesBackfacingChild) {
  gfx::Size surface_size(10, 10);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl_->active_tree(), 1);
  scoped_ptr<LayerImpl> child = CreateScrollableLayer(2, surface_size);
  host_impl_->SetViewportSize(surface_size, surface_size);

  gfx::Transform matrix;
  matrix.RotateAboutXAxis(180.0);
  child->SetTransform(matrix);
  child->SetDoubleSided(false);

  root->AddChild(child.Pass());
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  InitializeRendererAndDrawFrame();

  // Scroll event is ignored because the scrollable layer is not facing the
  // viewer and there is nothing scrollable behind it.
  EXPECT_EQ(InputHandlerClient::ScrollIgnored,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandlerClient::Wheel));
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollBlockedByContentLayer) {
  gfx::Size surface_size(10, 10);
  scoped_ptr<LayerImpl> content_layer = CreateScrollableLayer(1, surface_size);
  content_layer->SetShouldScrollOnMainThread(true);
  content_layer->SetScrollable(false);

  scoped_ptr<LayerImpl> scroll_layer = CreateScrollableLayer(2, surface_size);
  scroll_layer->AddChild(content_layer.Pass());

  host_impl_->active_tree()->SetRootLayer(scroll_layer.Pass());
  host_impl_->SetViewportSize(surface_size, surface_size);
  InitializeRendererAndDrawFrame();

  // Scrolling fails because the content layer is asking to be scrolled on the
  // main thread.
  EXPECT_EQ(InputHandlerClient::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandlerClient::Wheel));
}

TEST_F(LayerTreeHostImplTest, ScrollRootAndChangePageScaleOnMainThread) {
  gfx::Size surface_size(10, 10);
  float page_scale = 2.f;
  scoped_ptr<LayerImpl> root = CreateScrollableLayer(1, surface_size);
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  host_impl_->SetViewportSize(surface_size, surface_size);
  InitializeRendererAndDrawFrame();

  gfx::Vector2d scroll_delta(0, 10);
  gfx::Vector2d expected_scroll_delta = scroll_delta;
  gfx::Vector2d expected_max_scroll =
      host_impl_->active_tree()->root_layer()->max_scroll_offset();
  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandlerClient::Wheel));
  host_impl_->ScrollBy(gfx::Point(), scroll_delta);
  host_impl_->ScrollEnd();

  // Set new page scale from main thread.
  host_impl_->active_tree()->SetPageScaleFactorAndLimits(page_scale,
                                                         page_scale,
                                                         page_scale);

  scoped_ptr<ScrollAndScaleSet> scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info.get(),
                 host_impl_->active_tree()->root_layer()->id(),
                 expected_scroll_delta);

  // The scroll range should also have been updated.
  EXPECT_EQ(expected_max_scroll,
            host_impl_->active_tree()->root_layer()->max_scroll_offset());

  // The page scale delta remains constant because the impl thread did not
  // scale.
  // TODO: If possible, use gfx::Transform() or Skia equality functions. At
  //       the moment we avoid that because skia does exact bit-wise equality
  //       checking that does not consider -0 == +0.
  //       http://code.google.com/p/chromium/issues/detail?id=162747
  EXPECT_EQ(1.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(0, 0));
  EXPECT_EQ(0.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(0, 1));
  EXPECT_EQ(0.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(0, 2));
  EXPECT_EQ(0.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(0, 3));
  EXPECT_EQ(0.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(1, 0));
  EXPECT_EQ(1.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(1, 1));
  EXPECT_EQ(0.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(1, 2));
  EXPECT_EQ(0.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(1, 3));
  EXPECT_EQ(0.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(2, 0));
  EXPECT_EQ(0.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(2, 1));
  EXPECT_EQ(1.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(2, 2));
  EXPECT_EQ(0.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(2, 3));
  EXPECT_EQ(0.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(3, 0));
  EXPECT_EQ(0.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(3, 1));
  EXPECT_EQ(0.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(3, 2));
  EXPECT_EQ(1.0,
            host_impl_->active_tree()->root_layer()->
                impl_transform().matrix().getDouble(3, 3));
}

TEST_F(LayerTreeHostImplTest, ScrollRootAndChangePageScaleOnImplThread) {
  gfx::Size surface_size(10, 10);
  float page_scale = 2.f;
  scoped_ptr<LayerImpl> root = CreateScrollableLayer(1, surface_size);
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  host_impl_->SetViewportSize(surface_size, surface_size);
  host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f, 1.f, page_scale);
  InitializeRendererAndDrawFrame();

  gfx::Vector2d scroll_delta(0, 10);
  gfx::Vector2d expected_scroll_delta = scroll_delta;
  gfx::Vector2d expected_max_scroll =
      host_impl_->active_tree()->root_layer()->max_scroll_offset();
  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandlerClient::Wheel));
  host_impl_->ScrollBy(gfx::Point(), scroll_delta);
  host_impl_->ScrollEnd();

  // Set new page scale on impl thread by pinching.
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(page_scale, gfx::Point());
  host_impl_->PinchGestureEnd();
  DrawOneFrame();

  // The scroll delta is not scaled because the main thread did not scale.
  scoped_ptr<ScrollAndScaleSet> scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info.get(),
                 host_impl_->active_tree()->root_layer()->id(),
                 expected_scroll_delta);

  // The scroll range should also have been updated.
  EXPECT_EQ(expected_max_scroll,
            host_impl_->active_tree()->root_layer()->max_scroll_offset());

  // The page scale delta should match the new scale on the impl side.
  gfx::Transform expected_scale;
  expected_scale.Scale(page_scale, page_scale);
  EXPECT_EQ(expected_scale,
            host_impl_->active_tree()->root_layer()->impl_transform());
}

TEST_F(LayerTreeHostImplTest, PageScaleDeltaAppliedToRootScrollLayerOnly) {
  gfx::Size surface_size(10, 10);
  float default_page_scale = 1.f;
  gfx::Transform default_page_scale_matrix;

  float new_page_scale = 2.f;
  gfx::Transform new_page_scale_matrix;
  new_page_scale_matrix.Scale(new_page_scale, new_page_scale);

  // Create a normal scrollable root layer and another scrollable child layer.
  SetupScrollAndContentsLayers(surface_size);
  LayerImpl* root = host_impl_->active_tree()->root_layer();
  LayerImpl* child = root->children()[0];

  scoped_ptr<LayerImpl> scrollable_child =
      CreateScrollableLayer(3, surface_size);
  child->AddChild(scrollable_child.Pass());
  LayerImpl* grand_child = child->children()[0];

  // Set new page scale on impl thread by pinching.
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(new_page_scale, gfx::Point());
  host_impl_->PinchGestureEnd();
  DrawOneFrame();

  // The page scale delta should only be applied to the scrollable root layer.
  EXPECT_EQ(root->impl_transform(), new_page_scale_matrix);
  EXPECT_EQ(child->impl_transform(), default_page_scale_matrix);
  EXPECT_EQ(grand_child->impl_transform(), default_page_scale_matrix);

  // Make sure all the layers are drawn with the page scale delta applied, i.e.,
  // the page scale delta on the root layer is applied hierarchically.
  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_EQ(new_page_scale, root->draw_transform().matrix().getDouble(0, 0));
  EXPECT_EQ(new_page_scale, root->draw_transform().matrix().getDouble(1, 1));
  EXPECT_EQ(new_page_scale, child->draw_transform().matrix().getDouble(0, 0));
  EXPECT_EQ(new_page_scale, child->draw_transform().matrix().getDouble(1, 1));
  EXPECT_EQ(new_page_scale,
            grand_child->draw_transform().matrix().getDouble(0, 0));
  EXPECT_EQ(new_page_scale,
            grand_child->draw_transform().matrix().getDouble(1, 1));
}

TEST_F(LayerTreeHostImplTest, ScrollChildAndChangePageScaleOnMainThread) {
  gfx::Size surface_size(10, 10);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl_->active_tree(), 1);
  root->SetBounds(surface_size);
  root->SetContentBounds(surface_size);
  // Also mark the root scrollable so it becomes the root scroll layer.
  root->SetScrollable(true);
  int scroll_layer_id = 2;
  root->AddChild(CreateScrollableLayer(scroll_layer_id, surface_size));
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  host_impl_->SetViewportSize(surface_size, surface_size);
  InitializeRendererAndDrawFrame();

  LayerImpl* child = host_impl_->active_tree()->root_layer()->children()[0];

  gfx::Vector2d scroll_delta(0, 10);
  gfx::Vector2d expected_scroll_delta(scroll_delta);
  gfx::Vector2d expected_max_scroll(child->max_scroll_offset());
  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandlerClient::Wheel));
  host_impl_->ScrollBy(gfx::Point(), scroll_delta);
  host_impl_->ScrollEnd();

  float page_scale = 2.f;
  host_impl_->active_tree()->SetPageScaleFactorAndLimits(page_scale,
                                                         1.f,
                                                         page_scale);

  DrawOneFrame();

  scoped_ptr<ScrollAndScaleSet> scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info.get(), scroll_layer_id, expected_scroll_delta);

  // The scroll range should not have changed.
  EXPECT_EQ(child->max_scroll_offset(), expected_max_scroll);

  // The page scale delta remains constant because the impl thread did not
  // scale.
  gfx::Transform identity_transform;
  EXPECT_EQ(child->impl_transform(), gfx::Transform());
}

TEST_F(LayerTreeHostImplTest, ScrollChildBeyondLimit) {
  // Scroll a child layer beyond its maximum scroll range and make sure the
  // parent layer is scrolled on the axis on which the child was unable to
  // scroll.
  gfx::Size surface_size(10, 10);
  scoped_ptr<LayerImpl> root = CreateScrollableLayer(1, surface_size);

  scoped_ptr<LayerImpl> grand_child = CreateScrollableLayer(3, surface_size);
  grand_child->SetScrollOffset(gfx::Vector2d(0, 5));

  scoped_ptr<LayerImpl> child = CreateScrollableLayer(2, surface_size);
  child->SetScrollOffset(gfx::Vector2d(3, 0));
  child->AddChild(grand_child.Pass());

  root->AddChild(child.Pass());
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  host_impl_->SetViewportSize(surface_size, surface_size);
  InitializeRendererAndDrawFrame();
  {
    gfx::Vector2d scroll_delta(-8, -7);
    EXPECT_EQ(InputHandlerClient::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(5, 5),
                                      InputHandlerClient::Wheel));
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    host_impl_->ScrollEnd();

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();

    // The grand child should have scrolled up to its limit.
    LayerImpl* child = host_impl_->active_tree()->root_layer()->children()[0];
    LayerImpl* grand_child = child->children()[0];
    ExpectContains(*scroll_info.get(), grand_child->id(), gfx::Vector2d(0, -5));

    // The child should have only scrolled on the other axis.
    ExpectContains(*scroll_info.get(), child->id(), gfx::Vector2d(-3, 0));
  }
}

TEST_F(LayerTreeHostImplTest, ScrollWithoutBubbling) {
  // Scroll a child layer beyond its maximum scroll range and make sure the
  // the scroll doesn't bubble up to the parent layer.
  gfx::Size surface_size(10, 10);
  scoped_ptr<LayerImpl> root = CreateScrollableLayer(1, surface_size);

  scoped_ptr<LayerImpl> grand_child = CreateScrollableLayer(3, surface_size);
  grand_child->SetScrollOffset(gfx::Vector2d(0, 2));

  scoped_ptr<LayerImpl> child = CreateScrollableLayer(2, surface_size);
  child->SetScrollOffset(gfx::Vector2d(0, 3));
  child->AddChild(grand_child.Pass());

  root->AddChild(child.Pass());
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  host_impl_->SetViewportSize(surface_size, surface_size);
  InitializeRendererAndDrawFrame();
  {
    gfx::Vector2d scroll_delta(0, -10);
    EXPECT_EQ(InputHandlerClient::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(5, 5),
                                      InputHandlerClient::NonBubblingGesture));
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    host_impl_->ScrollEnd();

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();

    // The grand child should have scrolled up to its limit.
    LayerImpl* child = host_impl_->active_tree()->root_layer()->children()[0];
    LayerImpl* grand_child = child->children()[0];
    ExpectContains(*scroll_info.get(), grand_child->id(), gfx::Vector2d(0, -2));

    // The child should not have scrolled.
    ExpectNone(*scroll_info.get(), child->id());

    // The next time we scroll we should only scroll the parent.
    scroll_delta = gfx::Vector2d(0, -3);
    EXPECT_EQ(InputHandlerClient::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(5, 5),
                                      InputHandlerClient::NonBubblingGesture));
    EXPECT_EQ(host_impl_->CurrentlyScrollingLayer(), grand_child);
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    EXPECT_EQ(host_impl_->CurrentlyScrollingLayer(), child);
    host_impl_->ScrollEnd();

    scroll_info = host_impl_->ProcessScrollDeltas();

    // The child should have scrolled up to its limit.
    ExpectContains(*scroll_info.get(), child->id(), gfx::Vector2d(0, -3));

    // The grand child should not have scrolled.
    ExpectContains(*scroll_info.get(), grand_child->id(), gfx::Vector2d(0, -2));

    // After scrolling the parent, another scroll on the opposite direction
    // should still scroll the child.
    scroll_delta = gfx::Vector2d(0, 7);
    EXPECT_EQ(InputHandlerClient::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(5, 5),
                                      InputHandlerClient::NonBubblingGesture));
    EXPECT_EQ(host_impl_->CurrentlyScrollingLayer(), grand_child);
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    EXPECT_EQ(host_impl_->CurrentlyScrollingLayer(), grand_child);
    host_impl_->ScrollEnd();

    scroll_info = host_impl_->ProcessScrollDeltas();

    // The grand child should have scrolled.
    ExpectContains(*scroll_info.get(), grand_child->id(), gfx::Vector2d(0, 5));

    // The child should not have scrolled.
    ExpectContains(*scroll_info.get(), child->id(), gfx::Vector2d(0, -3));


    // Scrolling should be adjusted from viewport space.
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(2.f, 2.f, 2.f);
    host_impl_->active_tree()->SetPageScaleDelta(1.f);
    gfx::Transform scale_transform;
    scale_transform.Scale(2.f, 2.f);
    host_impl_->active_tree()->root_layer()->SetImplTransform(scale_transform);

    scroll_delta = gfx::Vector2d(0, -2);
    EXPECT_EQ(InputHandlerClient::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(1, 1),
                                      InputHandlerClient::NonBubblingGesture));
    EXPECT_EQ(grand_child, host_impl_->CurrentlyScrollingLayer());
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    host_impl_->ScrollEnd();

    scroll_info = host_impl_->ProcessScrollDeltas();

    // Should have scrolled by half the amount in layer space (5 - 2/2)
    ExpectContains(*scroll_info.get(), grand_child->id(), gfx::Vector2d(0, 4));
  }
}

TEST_F(LayerTreeHostImplTest, ScrollEventBubbling) {
  // When we try to scroll a non-scrollable child layer, the scroll delta
  // should be applied to one of its ancestors if possible.
  gfx::Size surface_size(10, 10);
  gfx::Size content_size(20, 20);
  scoped_ptr<LayerImpl> root = CreateScrollableLayer(1, content_size);
  scoped_ptr<LayerImpl> child = CreateScrollableLayer(2, content_size);

  child->SetScrollable(false);
  root->AddChild(child.Pass());

  host_impl_->SetViewportSize(surface_size, surface_size);
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  InitializeRendererAndDrawFrame();
  {
    gfx::Vector2d scroll_delta(0, 4);
    EXPECT_EQ(InputHandlerClient::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(5, 5),
                                      InputHandlerClient::Wheel));
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    host_impl_->ScrollEnd();

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();

    // Only the root should have scrolled.
    ASSERT_EQ(scroll_info->scrolls.size(), 1u);
    ExpectContains(*scroll_info.get(),
                   host_impl_->active_tree()->root_layer()->id(),
                   scroll_delta);
  }
}

TEST_F(LayerTreeHostImplTest, ScrollBeforeRedraw) {
  gfx::Size surface_size(10, 10);
  host_impl_->active_tree()->SetRootLayer(
      CreateScrollableLayer(1, surface_size));
  host_impl_->active_tree()->DidBecomeActive();
  host_impl_->SetViewportSize(surface_size, surface_size);

  // Draw one frame and then immediately rebuild the layer tree to mimic a tree
  // synchronization.
  InitializeRendererAndDrawFrame();
  host_impl_->active_tree()->DetachLayerTree();
  host_impl_->active_tree()->SetRootLayer(
      CreateScrollableLayer(2, surface_size));
  host_impl_->active_tree()->DidBecomeActive();

  // Scrolling should still work even though we did not draw yet.
  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandlerClient::Wheel));
}

TEST_F(LayerTreeHostImplTest, ScrollAxisAlignedRotatedLayer) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));

  // Rotate the root layer 90 degrees counter-clockwise about its center.
  gfx::Transform rotate_transform;
  rotate_transform.Rotate(-90.0);
  host_impl_->active_tree()->root_layer()->SetTransform(rotate_transform);

  gfx::Size surface_size(50, 50);
  host_impl_->SetViewportSize(surface_size, surface_size);
  InitializeRendererAndDrawFrame();

  // Scroll to the right in screen coordinates with a gesture.
  gfx::Vector2d gesture_scroll_delta(10, 0);
  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(),
                                    InputHandlerClient::Gesture));
  host_impl_->ScrollBy(gfx::Point(), gesture_scroll_delta);
  host_impl_->ScrollEnd();

  // The layer should have scrolled down in its local coordinates.
  scoped_ptr<ScrollAndScaleSet> scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info.get(),
                 host_impl_->active_tree()->root_layer()->id(),
                 gfx::Vector2d(0, gesture_scroll_delta.x()));

  // Reset and scroll down with the wheel.
  host_impl_->active_tree()->root_layer()->SetScrollDelta(gfx::Vector2dF());
  gfx::Vector2d wheel_scroll_delta(0, 10);
  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(),
                                    InputHandlerClient::Wheel));
  host_impl_->ScrollBy(gfx::Point(), wheel_scroll_delta);
  host_impl_->ScrollEnd();

  // The layer should have scrolled down in its local coordinates.
  scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info.get(),
                 host_impl_->active_tree()->root_layer()->id(),
                 wheel_scroll_delta);
}

TEST_F(LayerTreeHostImplTest, ScrollNonAxisAlignedRotatedLayer) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  int child_layer_id = 3;
  float child_layer_angle = -20.f;

  // Create a child layer that is rotated to a non-axis-aligned angle.
  scoped_ptr<LayerImpl> child = CreateScrollableLayer(
      child_layer_id,
      host_impl_->active_tree()->root_layer()->content_bounds());
  gfx::Transform rotate_transform;
  rotate_transform.Translate(-50.0, -50.0);
  rotate_transform.Rotate(child_layer_angle);
  rotate_transform.Translate(50.0, 50.0);
  child->SetTransform(rotate_transform);

  // Only allow vertical scrolling.
  child->SetMaxScrollOffset(gfx::Vector2d(0, child->content_bounds().height()));
  host_impl_->active_tree()->root_layer()->AddChild(child.Pass());

  gfx::Size surface_size(50, 50);
  host_impl_->SetViewportSize(surface_size, surface_size);
  InitializeRendererAndDrawFrame();
  {
    // Scroll down in screen coordinates with a gesture.
    gfx::Vector2d gesture_scroll_delta(0, 10);
    EXPECT_EQ(InputHandlerClient::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(),
                                      InputHandlerClient::Gesture));
    host_impl_->ScrollBy(gfx::Point(), gesture_scroll_delta);
    host_impl_->ScrollEnd();

    // The child layer should have scrolled down in its local coordinates an
    // amount proportional to the angle between it and the input scroll delta.
    gfx::Vector2d expected_scroll_delta(
        0,
        gesture_scroll_delta.y() *
            std::cos(MathUtil::Deg2Rad(child_layer_angle)));
    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    ExpectContains(*scroll_info.get(), child_layer_id, expected_scroll_delta);

    // The root layer should not have scrolled, because the input delta was
    // close to the layer's axis of movement.
    EXPECT_EQ(scroll_info->scrolls.size(), 1u);
  }
  {
    // Now reset and scroll the same amount horizontally.
    host_impl_->active_tree()->root_layer()->children()[1]->SetScrollDelta(
        gfx::Vector2dF());
    gfx::Vector2d gesture_scroll_delta(10, 0);
    EXPECT_EQ(InputHandlerClient::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(),
                                      InputHandlerClient::Gesture));
    host_impl_->ScrollBy(gfx::Point(), gesture_scroll_delta);
    host_impl_->ScrollEnd();

    // The child layer should have scrolled down in its local coordinates an
    // amount proportional to the angle between it and the input scroll delta.
    gfx::Vector2d expected_scroll_delta(
        0,
        -gesture_scroll_delta.x() *
            std::sin(MathUtil::Deg2Rad(child_layer_angle)));
    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    ExpectContains(*scroll_info.get(), child_layer_id, expected_scroll_delta);

    // The root layer should have scrolled more, since the input scroll delta
    // was mostly orthogonal to the child layer's vertical scroll axis.
    gfx::Vector2d expected_root_scroll_delta(
        gesture_scroll_delta.x() *
            std::pow(std::cos(MathUtil::Deg2Rad(child_layer_angle)), 2),
        0);
    ExpectContains(*scroll_info.get(),
                   host_impl_->active_tree()->root_layer()->id(),
                   expected_root_scroll_delta);
  }
}

TEST_F(LayerTreeHostImplTest, ScrollScaledLayer) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));

  // Scale the layer to twice its normal size.
  int scale = 2;
  gfx::Transform scale_transform;
  scale_transform.Scale(scale, scale);
  host_impl_->active_tree()->root_layer()->SetTransform(scale_transform);

  gfx::Size surface_size(50, 50);
  host_impl_->SetViewportSize(surface_size, surface_size);
  InitializeRendererAndDrawFrame();

  // Scroll down in screen coordinates with a gesture.
  gfx::Vector2d scroll_delta(0, 10);
  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Gesture));
  host_impl_->ScrollBy(gfx::Point(), scroll_delta);
  host_impl_->ScrollEnd();

  // The layer should have scrolled down in its local coordinates, but half the
  // amount.
  scoped_ptr<ScrollAndScaleSet> scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info.get(),
                 host_impl_->active_tree()->root_layer()->id(),
                 gfx::Vector2d(0, scroll_delta.y() / scale));

  // Reset and scroll down with the wheel.
  host_impl_->active_tree()->root_layer()->SetScrollDelta(gfx::Vector2dF());
  gfx::Vector2d wheel_scroll_delta(0, 10);
  EXPECT_EQ(InputHandlerClient::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandlerClient::Wheel));
  host_impl_->ScrollBy(gfx::Point(), wheel_scroll_delta);
  host_impl_->ScrollEnd();

  // The scale should not have been applied to the scroll delta.
  scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info.get(),
                 host_impl_->active_tree()->root_layer()->id(),
                 wheel_scroll_delta);
}

class BlendStateTrackerContext: public TestWebGraphicsContext3D {
 public:
  BlendStateTrackerContext() : blend_(false) {}

  virtual void enable(WebKit::WGC3Denum cap) OVERRIDE {
    if (cap == GL_BLEND)
      blend_ = true;
  }

  virtual void disable(WebKit::WGC3Denum cap) OVERRIDE {
    if (cap == GL_BLEND)
      blend_ = false;
  }

  bool blend() const { return blend_; }

 private:
  bool blend_;
};

class BlendStateCheckLayer : public LayerImpl {
 public:
  static scoped_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl,
                                      int id,
                                      ResourceProvider* resource_provider) {
    return scoped_ptr<LayerImpl>(new BlendStateCheckLayer(tree_impl,
                                                          id,
                                                          resource_provider));
  }

  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE {
    quads_appended_ = true;

    gfx::Rect opaque_rect;
    if (contents_opaque())
      opaque_rect = quad_rect_;
    else
      opaque_rect = opaque_content_rect_;

    SharedQuadState* shared_quad_state =
        quad_sink->UseSharedQuadState(CreateSharedQuadState());
    scoped_ptr<TileDrawQuad> test_blending_draw_quad = TileDrawQuad::Create();
    test_blending_draw_quad->SetNew(shared_quad_state,
                                    quad_rect_,
                                    opaque_rect,
                                    resource_id_,
                                    gfx::RectF(0.f, 0.f, 1.f, 1.f),
                                    gfx::Size(1, 1),
                                    false);
    test_blending_draw_quad->visible_rect = quad_visible_rect_;
    EXPECT_EQ(blend_, test_blending_draw_quad->ShouldDrawWithBlending());
    EXPECT_EQ(has_render_surface_, !!render_surface());
    quad_sink->Append(test_blending_draw_quad.PassAs<DrawQuad>(),
                      append_quads_data);
  }

  void SetExpectation(bool blend, bool has_render_surface) {
    blend_ = blend;
    has_render_surface_ = has_render_surface;
    quads_appended_ = false;
  }

  bool quads_appended() const { return quads_appended_; }

  void SetQuadRect(gfx::Rect rect) { quad_rect_ = rect; }
  void SetQuadVisibleRect(gfx::Rect rect) { quad_visible_rect_ = rect; }
  void SetOpaqueContentRect(gfx::Rect rect) { opaque_content_rect_ = rect; }

 private:
  BlendStateCheckLayer(LayerTreeImpl* tree_impl,
                       int id,
                       ResourceProvider* resource_provider)
      : LayerImpl(tree_impl, id),
        blend_(false),
        has_render_surface_(false),
        quads_appended_(false),
        quad_rect_(5, 5, 5, 5),
        quad_visible_rect_(5, 5, 5, 5),
        resource_id_(resource_provider->CreateResource(
            gfx::Size(1, 1),
            GL_RGBA,
            ResourceProvider::TextureUsageAny)) {
    resource_provider->AllocateForTesting(resource_id_);
    SetAnchorPoint(gfx::PointF());
    SetBounds(gfx::Size(10, 10));
    SetContentBounds(gfx::Size(10, 10));
    SetDrawsContent(true);
  }

  bool blend_;
  bool has_render_surface_;
  bool quads_appended_;
  gfx::Rect quad_rect_;
  gfx::Rect opaque_content_rect_;
  gfx::Rect quad_visible_rect_;
  ResourceProvider::ResourceId resource_id_;
};

TEST_F(LayerTreeHostImplTest, BlendingOffWhenDrawingOpaqueLayers) {
  {
    scoped_ptr<LayerImpl> root =
        LayerImpl::Create(host_impl_->active_tree(), 1);
    root->SetAnchorPoint(gfx::PointF());
    root->SetBounds(gfx::Size(10, 10));
    root->SetContentBounds(root->bounds());
    root->SetDrawsContent(false);
    host_impl_->active_tree()->SetRootLayer(root.Pass());
  }
  LayerImpl* root = host_impl_->active_tree()->root_layer();

  root->AddChild(
      BlendStateCheckLayer::Create(host_impl_->active_tree(),
                                   2,
                                   host_impl_->resource_provider()));
  BlendStateCheckLayer* layer1 =
      static_cast<BlendStateCheckLayer*>(root->children()[0]);
  layer1->SetPosition(gfx::PointF(2.f, 2.f));

  LayerTreeHostImpl::FrameData frame;

  // Opaque layer, drawn without blending.
  layer1->SetContentsOpaque(true);
  layer1->SetExpectation(false, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with translucent content and painting, so drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetExpectation(true, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with translucent opacity, drawn with blending.
  layer1->SetContentsOpaque(true);
  layer1->SetOpacity(0.5f);
  layer1->SetExpectation(true, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with translucent opacity and painting, drawn with blending.
  layer1->SetContentsOpaque(true);
  layer1->SetOpacity(0.5f);
  layer1->SetExpectation(true, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  layer1->AddChild(
      BlendStateCheckLayer::Create(host_impl_->active_tree(),
                                   3,
                                   host_impl_->resource_provider()));
  BlendStateCheckLayer* layer2 =
      static_cast<BlendStateCheckLayer*>(layer1->children()[0]);
  layer2->SetPosition(gfx::PointF(4.f, 4.f));

  // 2 opaque layers, drawn without blending.
  layer1->SetContentsOpaque(true);
  layer1->SetOpacity(1.f);
  layer1->SetExpectation(false, false);
  layer2->SetContentsOpaque(true);
  layer2->SetOpacity(1.f);
  layer2->SetExpectation(false, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Parent layer with translucent content, drawn with blending.
  // Child layer with opaque content, drawn without blending.
  layer1->SetContentsOpaque(false);
  layer1->SetExpectation(true, false);
  layer2->SetExpectation(false, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Parent layer with translucent content but opaque painting, drawn without
  // blending.
  // Child layer with opaque content, drawn without blending.
  layer1->SetContentsOpaque(true);
  layer1->SetExpectation(false, false);
  layer2->SetExpectation(false, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Parent layer with translucent opacity and opaque content. Since it has a
  // drawing child, it's drawn to a render surface which carries the opacity,
  // so it's itself drawn without blending.
  // Child layer with opaque content, drawn without blending (parent surface
  // carries the inherited opacity).
  layer1->SetContentsOpaque(true);
  layer1->SetOpacity(0.5f);
  layer1->SetExpectation(false, true);
  layer2->SetExpectation(false, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Draw again, but with child non-opaque, to make sure
  // layer1 not culled.
  layer1->SetContentsOpaque(true);
  layer1->SetOpacity(1.f);
  layer1->SetExpectation(false, false);
  layer2->SetContentsOpaque(true);
  layer2->SetOpacity(0.5f);
  layer2->SetExpectation(true, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // A second way of making the child non-opaque.
  layer1->SetContentsOpaque(true);
  layer1->SetOpacity(1.f);
  layer1->SetExpectation(false, false);
  layer2->SetContentsOpaque(false);
  layer2->SetOpacity(1.f);
  layer2->SetExpectation(true, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // And when the layer says its not opaque but is painted opaque, it is not
  // blended.
  layer1->SetContentsOpaque(true);
  layer1->SetOpacity(1.f);
  layer1->SetExpectation(false, false);
  layer2->SetContentsOpaque(true);
  layer2->SetOpacity(1.f);
  layer2->SetExpectation(false, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with partially opaque contents, drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(true, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with partially opaque contents partially culled, drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(5, 5, 5, 2));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(true, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with partially opaque contents culled, drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(7, 5, 3, 5));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(true, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with partially opaque contents and translucent contents culled, drawn
  // without blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(false, false);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(LayerTreeHostImplTest, ViewportCovered) {
  host_impl_->InitializeRenderer(CreateOutputSurface());
  host_impl_->active_tree()->set_background_color(SK_ColorGRAY);

  gfx::Size viewport_size(1000, 1000);
  host_impl_->SetViewportSize(viewport_size, viewport_size);

  host_impl_->active_tree()->SetRootLayer(
      LayerImpl::Create(host_impl_->active_tree(), 1));
  host_impl_->active_tree()->root_layer()->AddChild(
      BlendStateCheckLayer::Create(host_impl_->active_tree(),
                                   2,
                                   host_impl_->resource_provider()));
  BlendStateCheckLayer* child = static_cast<BlendStateCheckLayer*>(
      host_impl_->active_tree()->root_layer()->children()[0]);
  child->SetExpectation(false, false);
  child->SetContentsOpaque(true);

  // No gutter rects
  {
    gfx::Rect layer_rect(0, 0, 1000, 1000);
    child->SetPosition(layer_rect.origin());
    child->SetBounds(layer_rect.size());
    child->SetContentBounds(layer_rect.size());
    child->SetQuadRect(gfx::Rect(layer_rect.size()));
    child->SetQuadVisibleRect(gfx::Rect(layer_rect.size()));

    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
    ASSERT_EQ(1u, frame.render_passes.size());

    size_t num_gutter_quads = 0;
    for (size_t i = 0; i < frame.render_passes[0]->quad_list.size(); ++i)
      num_gutter_quads += (frame.render_passes[0]->quad_list[i]->material ==
                           DrawQuad::SOLID_COLOR) ? 1 : 0;
    EXPECT_EQ(0u, num_gutter_quads);
    EXPECT_EQ(1u, frame.render_passes[0]->quad_list.size());

    LayerTestCommon::VerifyQuadsExactlyCoverRect(
        frame.render_passes[0]->quad_list, gfx::Rect(viewport_size));
    host_impl_->DidDrawAllLayers(frame);
  }

  // Empty visible content area (fullscreen gutter rect)
  {
    gfx::Rect layer_rect(0, 0, 0, 0);
    child->SetPosition(layer_rect.origin());
    child->SetBounds(layer_rect.size());
    child->SetContentBounds(layer_rect.size());
    child->SetQuadRect(gfx::Rect(layer_rect.size()));
    child->SetQuadVisibleRect(gfx::Rect(layer_rect.size()));

    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
    ASSERT_EQ(1u, frame.render_passes.size());

    size_t num_gutter_quads = 0;
    for (size_t i = 0; i < frame.render_passes[0]->quad_list.size(); ++i)
      num_gutter_quads += (frame.render_passes[0]->quad_list[i]->material ==
                           DrawQuad::SOLID_COLOR) ? 1 : 0;
    EXPECT_EQ(1u, num_gutter_quads);
    EXPECT_EQ(1u, frame.render_passes[0]->quad_list.size());

    LayerTestCommon::VerifyQuadsExactlyCoverRect(
        frame.render_passes[0]->quad_list, gfx::Rect(viewport_size));
    host_impl_->DidDrawAllLayers(frame);
  }

  // Content area in middle of clip rect (four surrounding gutter rects)
  {
    gfx::Rect layer_rect(500, 500, 200, 200);
    child->SetPosition(layer_rect.origin());
    child->SetBounds(layer_rect.size());
    child->SetContentBounds(layer_rect.size());
    child->SetQuadRect(gfx::Rect(layer_rect.size()));
    child->SetQuadVisibleRect(gfx::Rect(layer_rect.size()));

    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
    ASSERT_EQ(1u, frame.render_passes.size());

    size_t num_gutter_quads = 0;
    for (size_t i = 0; i < frame.render_passes[0]->quad_list.size(); ++i)
      num_gutter_quads += (frame.render_passes[0]->quad_list[i]->material ==
                           DrawQuad::SOLID_COLOR) ? 1 : 0;
    EXPECT_EQ(4u, num_gutter_quads);
    EXPECT_EQ(5u, frame.render_passes[0]->quad_list.size());

    LayerTestCommon::VerifyQuadsExactlyCoverRect(
        frame.render_passes[0]->quad_list, gfx::Rect(viewport_size));
    host_impl_->DidDrawAllLayers(frame);
  }
}


class ReshapeTrackerContext: public TestWebGraphicsContext3D {
 public:
  ReshapeTrackerContext() : reshape_called_(false) {}

  virtual void reshape(int width, int height) OVERRIDE {
    reshape_called_ = true;
  }

  bool reshape_called() const { return reshape_called_; }

 private:
  bool reshape_called_;
};

class FakeDrawableLayerImpl: public LayerImpl {
 public:
  static scoped_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return scoped_ptr<LayerImpl>(new FakeDrawableLayerImpl(tree_impl, id));
  }
 protected:
  FakeDrawableLayerImpl(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id) {}
};

// Only reshape when we know we are going to draw. Otherwise, the reshape
// can leave the window at the wrong size if we never draw and the proper
// viewport size is never set.
TEST_F(LayerTreeHostImplTest, ReshapeNotCalledUntilDraw) {
  scoped_ptr<OutputSurface> output_surface = FakeOutputSurface::Create3d(
      scoped_ptr<WebKit::WebGraphicsContext3D>(new ReshapeTrackerContext))
      .PassAs<OutputSurface>();
  ReshapeTrackerContext* reshape_tracker =
      static_cast<ReshapeTrackerContext*>(output_surface->context3d());
  host_impl_->InitializeRenderer(output_surface.Pass());

  scoped_ptr<LayerImpl> root =
      FakeDrawableLayerImpl::Create(host_impl_->active_tree(), 1);
  root->SetAnchorPoint(gfx::PointF());
  root->SetBounds(gfx::Size(10, 10));
  root->SetDrawsContent(true);
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  EXPECT_FALSE(reshape_tracker->reshape_called());

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  EXPECT_TRUE(reshape_tracker->reshape_called());
  host_impl_->DidDrawAllLayers(frame);
}

class PartialSwapTrackerContext : public TestWebGraphicsContext3D {
 public:
  virtual void postSubBufferCHROMIUM(int x, int y, int width, int height)
      OVERRIDE {
    partial_swap_rect_ = gfx::Rect(x, y, width, height);
  }

  virtual WebKit::WebString getString(WebKit::WGC3Denum name) OVERRIDE {
    if (name == GL_EXTENSIONS) {
      return WebKit::WebString(
          "GL_CHROMIUM_post_sub_buffer GL_CHROMIUM_set_visibility");
    }

    return WebKit::WebString();
  }

  gfx::Rect partial_swap_rect() const { return partial_swap_rect_; }

 private:
  gfx::Rect partial_swap_rect_;
};

// Make sure damage tracking propagates all the way to the graphics context,
// where it should request to swap only the sub-buffer that is damaged.
TEST_F(LayerTreeHostImplTest, PartialSwapReceivesDamageRect) {
  scoped_ptr<OutputSurface> output_surface =
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new PartialSwapTrackerContext)).PassAs<OutputSurface>();
  PartialSwapTrackerContext* partial_swap_tracker =
      static_cast<PartialSwapTrackerContext*>(output_surface->context3d());

  // This test creates its own LayerTreeHostImpl, so
  // that we can force partial swap enabled.
  LayerTreeSettings settings;
  settings.partial_swap_enabled = true;
  scoped_ptr<LayerTreeHostImpl> layer_tree_host_impl =
      LayerTreeHostImpl::Create(settings,
                                this,
                                &proxy_,
                                &stats_instrumentation_);
  layer_tree_host_impl->InitializeRenderer(output_surface.Pass());
  layer_tree_host_impl->SetViewportSize(gfx::Size(500, 500),
                                        gfx::Size(500, 500));

  scoped_ptr<LayerImpl> root =
      FakeDrawableLayerImpl::Create(layer_tree_host_impl->active_tree(), 1);
  scoped_ptr<LayerImpl> child =
      FakeDrawableLayerImpl::Create(layer_tree_host_impl->active_tree(), 2);
  child->SetPosition(gfx::PointF(12.f, 13.f));
  child->SetAnchorPoint(gfx::PointF());
  child->SetBounds(gfx::Size(14, 15));
  child->SetContentBounds(gfx::Size(14, 15));
  child->SetDrawsContent(true);
  root->SetAnchorPoint(gfx::PointF());
  root->SetBounds(gfx::Size(500, 500));
  root->SetContentBounds(gfx::Size(500, 500));
  root->SetDrawsContent(true);
  root->AddChild(child.Pass());
  layer_tree_host_impl->active_tree()->SetRootLayer(root.Pass());

  LayerTreeHostImpl::FrameData frame;

  // First frame, the entire screen should get swapped.
  EXPECT_TRUE(layer_tree_host_impl->PrepareToDraw(&frame));
  layer_tree_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
  layer_tree_host_impl->DidDrawAllLayers(frame);
  layer_tree_host_impl->SwapBuffers();
  gfx::Rect actual_swap_rect = partial_swap_tracker->partial_swap_rect();
  gfx::Rect expected_swap_rect = gfx::Rect(0, 0, 500, 500);
  EXPECT_EQ(expected_swap_rect.x(), actual_swap_rect.x());
  EXPECT_EQ(expected_swap_rect.y(), actual_swap_rect.y());
  EXPECT_EQ(expected_swap_rect.width(), actual_swap_rect.width());
  EXPECT_EQ(expected_swap_rect.height(), actual_swap_rect.height());

  // Second frame, only the damaged area should get swapped. Damage should be
  // the union of old and new child rects.
  // expected damage rect: gfx::Rect(26, 28);
  // expected swap rect: vertically flipped, with origin at bottom left corner.
  layer_tree_host_impl->active_tree()->root_layer()->children()[0]->SetPosition(
      gfx::PointF());
  EXPECT_TRUE(layer_tree_host_impl->PrepareToDraw(&frame));
  layer_tree_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
  layer_tree_host_impl->SwapBuffers();
  actual_swap_rect = partial_swap_tracker->partial_swap_rect();
  expected_swap_rect = gfx::Rect(0, 500-28, 26, 28);
  EXPECT_EQ(expected_swap_rect.x(), actual_swap_rect.x());
  EXPECT_EQ(expected_swap_rect.y(), actual_swap_rect.y());
  EXPECT_EQ(expected_swap_rect.width(), actual_swap_rect.width());
  EXPECT_EQ(expected_swap_rect.height(), actual_swap_rect.height());

  // Make sure that partial swap is constrained to the viewport dimensions
  // expected damage rect: gfx::Rect(500, 500);
  // expected swap rect: flipped damage rect, but also clamped to viewport
  layer_tree_host_impl->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
  // This will damage everything.
  layer_tree_host_impl->active_tree()->root_layer()->SetOpacity(0.7f);
  EXPECT_TRUE(layer_tree_host_impl->PrepareToDraw(&frame));
  layer_tree_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
  layer_tree_host_impl->SwapBuffers();
  actual_swap_rect = partial_swap_tracker->partial_swap_rect();
  expected_swap_rect = gfx::Rect(10, 10);
  EXPECT_EQ(expected_swap_rect.x(), actual_swap_rect.x());
  EXPECT_EQ(expected_swap_rect.y(), actual_swap_rect.y());
  EXPECT_EQ(expected_swap_rect.width(), actual_swap_rect.width());
  EXPECT_EQ(expected_swap_rect.height(), actual_swap_rect.height());
}

TEST_F(LayerTreeHostImplTest, RootLayerDoesntCreateExtraSurface) {
  scoped_ptr<LayerImpl> root =
      FakeDrawableLayerImpl::Create(host_impl_->active_tree(), 1);
  scoped_ptr<LayerImpl> child =
      FakeDrawableLayerImpl::Create(host_impl_->active_tree(), 2);
  child->SetAnchorPoint(gfx::PointF());
  child->SetBounds(gfx::Size(10, 10));
  child->SetContentBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);
  root->SetAnchorPoint(gfx::PointF());
  root->SetBounds(gfx::Size(10, 10));
  root->SetContentBounds(gfx::Size(10, 10));
  root->SetDrawsContent(true);
  root->SetOpacity(0.7f);
  root->AddChild(child.Pass());

  host_impl_->active_tree()->SetRootLayer(root.Pass());

  LayerTreeHostImpl::FrameData frame;

  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  EXPECT_EQ(1u, frame.render_surface_layer_list->size());
  EXPECT_EQ(1u, frame.render_passes.size());
  host_impl_->DidDrawAllLayers(frame);
}

class FakeLayerWithQuads : public LayerImpl {
 public:
  static scoped_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return scoped_ptr<LayerImpl>(new FakeLayerWithQuads(tree_impl, id));
  }

  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE {
    SharedQuadState* shared_quad_state =
        quad_sink->UseSharedQuadState(CreateSharedQuadState());

    SkColor gray = SkColorSetRGB(100, 100, 100);
    gfx::Rect quad_rect(content_bounds());
    scoped_ptr<SolidColorDrawQuad> my_quad = SolidColorDrawQuad::Create();
    my_quad->SetNew(shared_quad_state, quad_rect, gray);
    quad_sink->Append(my_quad.PassAs<DrawQuad>(), append_quads_data);
  }

 private:
  FakeLayerWithQuads(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id) {}
};

class MockContext : public TestWebGraphicsContext3D {
 public:
  MOCK_METHOD1(useProgram, void(WebKit::WebGLId program));
  MOCK_METHOD5(uniform4f, void(WebKit::WGC3Dint location,
                               WebKit::WGC3Dfloat x,
                               WebKit::WGC3Dfloat y,
                               WebKit::WGC3Dfloat z,
                               WebKit::WGC3Dfloat w));
  MOCK_METHOD4(uniformMatrix4fv, void(WebKit::WGC3Dint location,
                                      WebKit::WGC3Dsizei count,
                                      WebKit::WGC3Dboolean transpose,
                                      const WebKit::WGC3Dfloat* value));
  MOCK_METHOD4(drawElements, void(WebKit::WGC3Denum mode,
                                  WebKit::WGC3Dsizei count,
                                  WebKit::WGC3Denum type,
                                  WebKit::WGC3Dintptr offset));
  MOCK_METHOD1(getString, WebKit::WebString(WebKit::WGC3Denum name));
  MOCK_METHOD0(getRequestableExtensionsCHROMIUM, WebKit::WebString());
  MOCK_METHOD1(enable, void(WebKit::WGC3Denum cap));
  MOCK_METHOD1(disable, void(WebKit::WGC3Denum cap));
  MOCK_METHOD4(scissor, void(WebKit::WGC3Dint x,
                             WebKit::WGC3Dint y,
                             WebKit::WGC3Dsizei width,
                             WebKit::WGC3Dsizei height));
};

class MockContextHarness {
 private:
  MockContext* context_;

 public:
  explicit MockContextHarness(MockContext* context)
      : context_(context) {
    // Catch "uninteresting" calls
    EXPECT_CALL(*context_, useProgram(_))
        .Times(0);

    EXPECT_CALL(*context_, drawElements(_, _, _, _))
        .Times(0);

    // These are not asserted
    EXPECT_CALL(*context_, uniformMatrix4fv(_, _, _, _))
        .WillRepeatedly(Return());

    EXPECT_CALL(*context_, uniform4f(_, _, _, _, _))
        .WillRepeatedly(Return());

    // Any other strings are empty
    EXPECT_CALL(*context_, getString(_))
        .WillRepeatedly(Return(WebKit::WebString()));

    // Support for partial swap, if needed
    EXPECT_CALL(*context_, getString(GL_EXTENSIONS))
        .WillRepeatedly(Return(
            WebKit::WebString("GL_CHROMIUM_post_sub_buffer")));

    EXPECT_CALL(*context_, getRequestableExtensionsCHROMIUM())
        .WillRepeatedly(Return(
            WebKit::WebString("GL_CHROMIUM_post_sub_buffer")));

    // Any un-sanctioned calls to enable() are OK
    EXPECT_CALL(*context_, enable(_))
        .WillRepeatedly(Return());

    // Any un-sanctioned calls to disable() are OK
    EXPECT_CALL(*context_, disable(_))
        .WillRepeatedly(Return());
  }

  void MustDrawSolidQuad() {
    EXPECT_CALL(*context_, drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0))
        .WillOnce(Return())
        .RetiresOnSaturation();

    EXPECT_CALL(*context_, useProgram(_))
        .WillOnce(Return())
        .RetiresOnSaturation();
  }

  void MustSetScissor(int x, int y, int width, int height) {
    EXPECT_CALL(*context_, enable(GL_SCISSOR_TEST))
        .WillRepeatedly(Return());

    EXPECT_CALL(*context_, scissor(x, y, width, height))
        .Times(AtLeast(1))
        .WillRepeatedly(Return());
  }

  void MustSetNoScissor() {
    EXPECT_CALL(*context_, disable(GL_SCISSOR_TEST))
        .WillRepeatedly(Return());

    EXPECT_CALL(*context_, enable(GL_SCISSOR_TEST))
        .Times(0);

    EXPECT_CALL(*context_, scissor(_, _, _, _))
        .Times(0);
  }
};

TEST_F(LayerTreeHostImplTest, NoPartialSwap) {
  scoped_ptr<OutputSurface> output_surface =
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new MockContext)).PassAs<OutputSurface>();
  MockContext* mock_context =
      static_cast<MockContext*>(output_surface->context3d());
  MockContextHarness harness(mock_context);

  // Run test case
  CreateLayerTreeHost(false, output_surface.Pass());
  SetupRootLayerImpl(FakeLayerWithQuads::Create(host_impl_->active_tree(), 1));

  // Without partial swap, and no clipping, no scissor is set.
  harness.MustDrawSolidQuad();
  harness.MustSetNoScissor();
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
    host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
    host_impl_->DidDrawAllLayers(frame);
  }
  Mock::VerifyAndClearExpectations(&mock_context);

  // Without partial swap, but a layer does clip its subtree, one scissor is
  // set.
  host_impl_->active_tree()->root_layer()->SetMasksToBounds(true);
  harness.MustDrawSolidQuad();
  harness.MustSetScissor(0, 0, 10, 10);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
    host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
    host_impl_->DidDrawAllLayers(frame);
  }
  Mock::VerifyAndClearExpectations(&mock_context);
}

TEST_F(LayerTreeHostImplTest, PartialSwap) {
  scoped_ptr<OutputSurface> output_surface =
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new MockContext)).PassAs<OutputSurface>();
  MockContext* mock_context =
      static_cast<MockContext*>(output_surface->context3d());
  MockContextHarness harness(mock_context);

  CreateLayerTreeHost(true, output_surface.Pass());
  SetupRootLayerImpl(FakeLayerWithQuads::Create(host_impl_->active_tree(), 1));

  // The first frame is not a partially-swapped one.
  harness.MustSetScissor(0, 0, 10, 10);
  harness.MustDrawSolidQuad();
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
    host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
    host_impl_->DidDrawAllLayers(frame);
  }
  Mock::VerifyAndClearExpectations(&mock_context);

  // Damage a portion of the frame.
  host_impl_->active_tree()->root_layer()->set_update_rect(
      gfx::Rect(0, 0, 2, 3));

  // The second frame will be partially-swapped (the y coordinates are flipped).
  harness.MustSetScissor(0, 7, 2, 3);
  harness.MustDrawSolidQuad();
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
    host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
    host_impl_->DidDrawAllLayers(frame);
  }
  Mock::VerifyAndClearExpectations(&mock_context);
}

class PartialSwapContext : public TestWebGraphicsContext3D {
 public:
  virtual WebKit::WebString getString(WebKit::WGC3Denum name) OVERRIDE {
    if (name == GL_EXTENSIONS)
      return WebKit::WebString("GL_CHROMIUM_post_sub_buffer");
    return WebKit::WebString();
  }

  virtual WebKit::WebString getRequestableExtensionsCHROMIUM() OVERRIDE {
    return WebKit::WebString("GL_CHROMIUM_post_sub_buffer");
  }

  // Unlimited texture size.
  virtual void getIntegerv(WebKit::WGC3Denum pname, WebKit::WGC3Dint* value)
      OVERRIDE {
    if (pname == GL_MAX_TEXTURE_SIZE)
      *value = 8192;
  }
};

static scoped_ptr<LayerTreeHostImpl> SetupLayersForOpacity(
    bool partial_swap,
    LayerTreeHostImplClient* client,
    Proxy* proxy,
    RenderingStatsInstrumentation* stats_instrumentation) {
  scoped_ptr<OutputSurface> output_surface =
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new PartialSwapContext)).PassAs<OutputSurface>();

  LayerTreeSettings settings;
  settings.partial_swap_enabled = partial_swap;
  scoped_ptr<LayerTreeHostImpl> my_host_impl =
      LayerTreeHostImpl::Create(settings, client, proxy, stats_instrumentation);
  my_host_impl->InitializeRenderer(output_surface.Pass());
  my_host_impl->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

  /*
    Layers are created as follows:

    +--------------------+
    |                  1 |
    |  +-----------+     |
    |  |         2 |     |
    |  | +-------------------+
    |  | |   3               |
    |  | +-------------------+
    |  |           |     |
    |  +-----------+     |
    |                    |
    |                    |
    +--------------------+

    Layers 1, 2 have render surfaces
  */
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(my_host_impl->active_tree(), 1);
  scoped_ptr<LayerImpl> child =
      LayerImpl::Create(my_host_impl->active_tree(), 2);
  scoped_ptr<LayerImpl> grand_child =
      FakeLayerWithQuads::Create(my_host_impl->active_tree(), 3);

  gfx::Rect root_rect(0, 0, 100, 100);
  gfx::Rect child_rect(10, 10, 50, 50);
  gfx::Rect grand_child_rect(5, 5, 150, 150);

  root->CreateRenderSurface();
  root->SetAnchorPoint(gfx::PointF());
  root->SetPosition(root_rect.origin());
  root->SetBounds(root_rect.size());
  root->SetContentBounds(root->bounds());
  root->draw_properties().visible_content_rect = root_rect;
  root->SetDrawsContent(false);
  root->render_surface()->SetContentRect(gfx::Rect(root_rect.size()));

  child->SetAnchorPoint(gfx::PointF());
  child->SetPosition(gfx::PointF(child_rect.x(), child_rect.y()));
  child->SetOpacity(0.5f);
  child->SetBounds(gfx::Size(child_rect.width(), child_rect.height()));
  child->SetContentBounds(child->bounds());
  child->draw_properties().visible_content_rect = child_rect;
  child->SetDrawsContent(false);
  child->SetForceRenderSurface(true);

  grand_child->SetAnchorPoint(gfx::PointF());
  grand_child->SetPosition(grand_child_rect.origin());
  grand_child->SetBounds(grand_child_rect.size());
  grand_child->SetContentBounds(grand_child->bounds());
  grand_child->draw_properties().visible_content_rect = grand_child_rect;
  grand_child->SetDrawsContent(true);

  child->AddChild(grand_child.Pass());
  root->AddChild(child.Pass());

  my_host_impl->active_tree()->SetRootLayer(root.Pass());
  return my_host_impl.Pass();
}

TEST_F(LayerTreeHostImplTest, ContributingLayerEmptyScissorPartialSwap) {
  scoped_ptr<LayerTreeHostImpl> my_host_impl =
      SetupLayersForOpacity(true, this, &proxy_, &stats_instrumentation_);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Verify all quads have been computed
    ASSERT_EQ(2U, frame.render_passes.size());
    ASSERT_EQ(1U, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(1U, frame.render_passes[1]->quad_list.size());
    EXPECT_EQ(DrawQuad::SOLID_COLOR,
              frame.render_passes[0]->quad_list[0]->material);
    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[1]->quad_list[0]->material);

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, ContributingLayerEmptyScissorNoPartialSwap) {
  scoped_ptr<LayerTreeHostImpl> my_host_impl =
      SetupLayersForOpacity(false, this, &proxy_, &stats_instrumentation_);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Verify all quads have been computed
    ASSERT_EQ(2U, frame.render_passes.size());
    ASSERT_EQ(1U, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(1U, frame.render_passes[1]->quad_list.size());
    EXPECT_EQ(DrawQuad::SOLID_COLOR,
              frame.render_passes[0]->quad_list[0]->material);
    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[1]->quad_list[0]->material);

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }
}

// Fake WebKit::WebGraphicsContext3D that tracks the number of textures in use.
class TrackingWebGraphicsContext3D : public TestWebGraphicsContext3D {
 public:
  TrackingWebGraphicsContext3D()
      : TestWebGraphicsContext3D(),
        num_textures_(0) {}

  virtual WebKit::WebGLId createTexture() OVERRIDE {
    WebKit::WebGLId id = TestWebGraphicsContext3D::createTexture();

    textures_[id] = true;
    ++num_textures_;
    return id;
  }

  virtual void deleteTexture(WebKit::WebGLId id) OVERRIDE {
    if (textures_.find(id) == textures_.end())
      return;

    textures_[id] = false;
    --num_textures_;
  }

  virtual WebKit::WebString getString(WebKit::WGC3Denum name) OVERRIDE {
    if (name == GL_EXTENSIONS) {
      return WebKit::WebString(
          "GL_CHROMIUM_iosurface GL_ARB_texture_rectangle");
    }

    return WebKit::WebString();
  }

  unsigned num_textures() const { return num_textures_; }

 private:
  base::hash_map<WebKit::WebGLId, bool> textures_;
  unsigned num_textures_;
};

static unsigned CreateResourceId(ResourceProvider* resource_provider) {
  return resource_provider->CreateResource(
      gfx::Size(20, 12),
      resource_provider->best_texture_format(),
      ResourceProvider::TextureUsageAny);
}

static unsigned CreateTextureId(ResourceProvider* resource_provider) {
  return ResourceProvider::ScopedReadLockGL(
      resource_provider, CreateResourceId(resource_provider)).texture_id();
}

TEST_F(LayerTreeHostImplTest, LayersFreeTextures) {
  scoped_ptr<TestWebGraphicsContext3D> context =
      TestWebGraphicsContext3D::Create();
  TestWebGraphicsContext3D* context3d = context.get();
  scoped_ptr<OutputSurface> output_surface = FakeOutputSurface::Create3d(
      context.PassAs<WebKit::WebGraphicsContext3D>()).PassAs<OutputSurface>();
  host_impl_->InitializeRenderer(output_surface.Pass());

  scoped_ptr<LayerImpl> root_layer =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  root_layer->SetBounds(gfx::Size(10, 10));
  root_layer->SetAnchorPoint(gfx::PointF());

  scoped_refptr<VideoFrame> softwareFrame =
      media::VideoFrame::CreateColorFrame(
          gfx::Size(4, 4), 0x80, 0x80, 0x80, base::TimeDelta());
  FakeVideoFrameProvider provider;
  provider.set_frame(softwareFrame);
  scoped_ptr<VideoLayerImpl> video_layer =
      VideoLayerImpl::Create(host_impl_->active_tree(), 4, &provider);
  video_layer->SetBounds(gfx::Size(10, 10));
  video_layer->SetAnchorPoint(gfx::PointF());
  video_layer->SetContentBounds(gfx::Size(10, 10));
  video_layer->SetDrawsContent(true);
  root_layer->AddChild(video_layer.PassAs<LayerImpl>());

  scoped_ptr<IOSurfaceLayerImpl> io_surface_layer =
      IOSurfaceLayerImpl::Create(host_impl_->active_tree(), 5);
  io_surface_layer->SetBounds(gfx::Size(10, 10));
  io_surface_layer->SetAnchorPoint(gfx::PointF());
  io_surface_layer->SetContentBounds(gfx::Size(10, 10));
  io_surface_layer->SetDrawsContent(true);
  io_surface_layer->SetIOSurfaceProperties(1, gfx::Size(10, 10));
  root_layer->AddChild(io_surface_layer.PassAs<LayerImpl>());

  host_impl_->active_tree()->SetRootLayer(root_layer.Pass());

  EXPECT_EQ(0u, context3d->NumTextures());

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
  host_impl_->SwapBuffers();

  EXPECT_GT(context3d->NumTextures(), 0u);

  // Kill the layer tree.
  host_impl_->active_tree()->SetRootLayer(
      LayerImpl::Create(host_impl_->active_tree(), 100));
  // There should be no textures left in use after.
  EXPECT_EQ(0u, context3d->NumTextures());
}

class MockDrawQuadsToFillScreenContext : public TestWebGraphicsContext3D {
 public:
  MOCK_METHOD1(useProgram, void(WebKit::WebGLId program));
  MOCK_METHOD4(drawElements, void(WebKit::WGC3Denum mode,
                                  WebKit::WGC3Dsizei count,
                                  WebKit::WGC3Denum type,
                                  WebKit::WGC3Dintptr offset));
};

TEST_F(LayerTreeHostImplTest, HasTransparentBackground) {
  scoped_ptr<OutputSurface> output_surface =
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new MockDrawQuadsToFillScreenContext)).PassAs<OutputSurface>();
  MockDrawQuadsToFillScreenContext* mock_context =
      static_cast<MockDrawQuadsToFillScreenContext*>(
          output_surface->context3d());

  // Run test case
  CreateLayerTreeHost(false, output_surface.Pass());
  SetupRootLayerImpl(LayerImpl::Create(host_impl_->active_tree(), 1));
  host_impl_->active_tree()->set_background_color(SK_ColorWHITE);

  // Verify one quad is drawn when transparent background set is not set.
  host_impl_->active_tree()->set_has_transparent_background(false);
  EXPECT_CALL(*mock_context, useProgram(_))
      .Times(1);
  EXPECT_CALL(*mock_context, drawElements(_, _, _, _))
      .Times(1);
  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
  Mock::VerifyAndClearExpectations(&mock_context);

  // Verify no quads are drawn when transparent background is set.
  host_impl_->active_tree()->set_has_transparent_background(true);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
  host_impl_->DidDrawAllLayers(frame);
  Mock::VerifyAndClearExpectations(&mock_context);
}

static void AddDrawingLayerTo(LayerImpl* parent,
                              int id,
                              gfx::Rect layer_rect,
                              LayerImpl** result) {
  scoped_ptr<LayerImpl> layer =
      FakeLayerWithQuads::Create(parent->layer_tree_impl(), id);
  LayerImpl* layer_ptr = layer.get();
  layer_ptr->SetAnchorPoint(gfx::PointF());
  layer_ptr->SetPosition(gfx::PointF(layer_rect.origin()));
  layer_ptr->SetBounds(layer_rect.size());
  layer_ptr->SetContentBounds(layer_rect.size());
  layer_ptr->SetDrawsContent(true);  // only children draw content
  layer_ptr->SetContentsOpaque(true);
  parent->AddChild(layer.Pass());
  if (result)
    *result = layer_ptr;
}

static void SetupLayersForTextureCaching(
    LayerTreeHostImpl* layer_tree_host_impl,
    LayerImpl*& root_ptr,
    LayerImpl*& intermediate_layer_ptr,
    LayerImpl*& surface_layer_ptr,
    LayerImpl*& child_ptr,
    gfx::Size root_size) {
  scoped_ptr<OutputSurface> output_surface =
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new PartialSwapContext)).PassAs<OutputSurface>();

  layer_tree_host_impl->InitializeRenderer(output_surface.Pass());
  layer_tree_host_impl->SetViewportSize(root_size, root_size);

  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(layer_tree_host_impl->active_tree(), 1);
  root_ptr = root.get();

  root->SetAnchorPoint(gfx::PointF());
  root->SetPosition(gfx::PointF());
  root->SetBounds(root_size);
  root->SetContentBounds(root_size);
  root->SetDrawsContent(true);
  layer_tree_host_impl->active_tree()->SetRootLayer(root.Pass());

  AddDrawingLayerTo(root_ptr,
                    2,
                    gfx::Rect(10, 10, root_size.width(), root_size.height()),
                    &intermediate_layer_ptr);
  // Only children draw content.
  intermediate_layer_ptr->SetDrawsContent(false);

  // Surface layer is the layer that changes its opacity
  // It will contain other layers that draw content.
  AddDrawingLayerTo(intermediate_layer_ptr,
                    3,
                    gfx::Rect(10, 10, root_size.width(), root_size.height()),
                    &surface_layer_ptr);
  // Only children draw content.
  surface_layer_ptr->SetDrawsContent(false);
  surface_layer_ptr->SetOpacity(0.5f);
  surface_layer_ptr->SetForceRenderSurface(true);

  // Child of the surface layer will produce some quads
  AddDrawingLayerTo(surface_layer_ptr,
                    4,
                    gfx::Rect(5,
                              5,
                              root_size.width() - 25,
                              root_size.height() - 25),
                    &child_ptr);
}

class GLRendererWithReleaseTextures : public GLRenderer {
 public:
  using GLRenderer::ReleaseRenderPassTextures;
};

TEST_F(LayerTreeHostImplTest, TextureCachingWithOcclusion) {
  LayerTreeSettings settings;
  settings.minimum_occlusion_tracking_size = gfx::Size();
  settings.cache_render_pass_contents = true;
  scoped_ptr<LayerTreeHostImpl> my_host_impl =
      LayerTreeHostImpl::Create(settings,
                                this,
                                &proxy_,
                                &stats_instrumentation_);

  // Layers are structure as follows:
  //
  //  R +-- S1 +- L10 (owning)
  //    |      +- L11
  //    |      +- L12
  //    |
  //    +-- S2 +- L20 (owning)
  //           +- L21
  //
  // Occlusion:
  // L12 occludes L11 (internal)
  // L20 occludes L10 (external)
  // L21 occludes L20 (internal)

  LayerImpl* root_ptr;
  LayerImpl* layer_s1_ptr;
  LayerImpl* layer_s2_ptr;

  scoped_ptr<OutputSurface> output_surface =
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new PartialSwapContext)).PassAs<OutputSurface>();

  gfx::Size root_size(1000, 1000);

  my_host_impl->InitializeRenderer(output_surface.Pass());
  my_host_impl->SetViewportSize(root_size, root_size);

  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(my_host_impl->active_tree(), 1);
  root_ptr = root.get();

  root->SetAnchorPoint(gfx::PointF());
  root->SetPosition(gfx::PointF());
  root->SetBounds(root_size);
  root->SetContentBounds(root_size);
  root->SetDrawsContent(true);
  root->SetMasksToBounds(true);
  my_host_impl->active_tree()->SetRootLayer(root.Pass());

  AddDrawingLayerTo(root_ptr, 2, gfx::Rect(300, 300, 300, 300), &layer_s1_ptr);
  layer_s1_ptr->SetForceRenderSurface(true);

  AddDrawingLayerTo(layer_s1_ptr, 3, gfx::Rect(10, 10, 10, 10), 0);  // L11
  AddDrawingLayerTo(layer_s1_ptr, 4, gfx::Rect(0, 0, 30, 30), 0);  // L12

  AddDrawingLayerTo(root_ptr, 5, gfx::Rect(550, 250, 300, 400), &layer_s2_ptr);
  layer_s2_ptr->SetForceRenderSurface(true);

  AddDrawingLayerTo(layer_s2_ptr, 6, gfx::Rect(20, 20, 5, 5), 0);  // L21

  // Initial draw - must receive all quads
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive 3 render passes.
    // For Root, there are 2 quads; for S1, there are 2 quads (1 is occluded);
    // for S2, there is 2 quads.
    ASSERT_EQ(3U, frame.render_passes.size());

    EXPECT_EQ(2U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(2U, frame.render_passes[1]->quad_list.size());
    EXPECT_EQ(2U, frame.render_passes[2]->quad_list.size());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // "Unocclude" surface S1 and repeat draw.
  // Must remove S2's render pass since it's cached;
  // Must keep S1 quads because texture contained external occlusion.
  gfx::Transform transform = layer_s2_ptr->transform();
  transform.Translate(150.0, 150.0);
  layer_s2_ptr->SetTransform(transform);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive 2 render passes.
    // For Root, there are 2 quads
    // For S1, the number of quads depends on what got unoccluded, so not
    // asserted beyond being positive.
    // For S2, there is no render pass
    ASSERT_EQ(2U, frame.render_passes.size());

    EXPECT_GT(frame.render_passes[0]->quad_list.size(), 0U);
    EXPECT_EQ(2U, frame.render_passes[1]->quad_list.size());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // "Re-occlude" surface S1 and repeat draw.
  // Must remove S1's render pass since it is now available in full.
  // S2 has no change so must also be removed.
  transform = layer_s2_ptr->transform();
  transform.Translate(-15.0, -15.0);
  layer_s2_ptr->SetTransform(transform);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive 1 render pass - for the root.
    ASSERT_EQ(1U, frame.render_passes.size());

    EXPECT_EQ(2U, frame.render_passes[0]->quad_list.size());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, TextureCachingWithOcclusionEarlyOut) {
  LayerTreeSettings settings;
  settings.minimum_occlusion_tracking_size = gfx::Size();
  settings.cache_render_pass_contents = true;
  scoped_ptr<LayerTreeHostImpl> my_host_impl =
      LayerTreeHostImpl::Create(settings,
                                this,
                                &proxy_,
                                &stats_instrumentation_);

  // Layers are structure as follows:
  //
  //  R +-- S1 +- L10 (owning, non drawing)
  //    |      +- L11 (corner, unoccluded)
  //    |      +- L12 (corner, unoccluded)
  //    |      +- L13 (corner, unoccluded)
  //    |      +- L14 (corner, entirely occluded)
  //    |
  //    +-- S2 +- L20 (owning, drawing)
  //

  LayerImpl* root_ptr;
  LayerImpl* layer_s1_ptr;
  LayerImpl* layer_s2_ptr;

  scoped_ptr<OutputSurface> output_surface =
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new PartialSwapContext)).PassAs<OutputSurface>();

  gfx::Size root_size(1000, 1000);

  my_host_impl->InitializeRenderer(output_surface.Pass());
  my_host_impl->SetViewportSize(root_size, root_size);

  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(my_host_impl->active_tree(), 1);
  root_ptr = root.get();

  root->SetAnchorPoint(gfx::PointF());
  root->SetPosition(gfx::PointF());
  root->SetBounds(root_size);
  root->SetContentBounds(root_size);
  root->SetDrawsContent(true);
  root->SetMasksToBounds(true);
  my_host_impl->active_tree()->SetRootLayer(root.Pass());

  AddDrawingLayerTo(root_ptr, 2, gfx::Rect(0, 0, 800, 800), &layer_s1_ptr);
  layer_s1_ptr->SetForceRenderSurface(true);
  layer_s1_ptr->SetDrawsContent(false);

  AddDrawingLayerTo(layer_s1_ptr, 3, gfx::Rect(0, 0, 300, 300), 0);  // L11
  AddDrawingLayerTo(layer_s1_ptr, 4, gfx::Rect(0, 500, 300, 300), 0);  // L12
  AddDrawingLayerTo(layer_s1_ptr, 5, gfx::Rect(500, 0, 300, 300), 0);  // L13
  AddDrawingLayerTo(layer_s1_ptr, 6, gfx::Rect(500, 500, 300, 300), 0);  // L14
  AddDrawingLayerTo(layer_s1_ptr, 9, gfx::Rect(500, 500, 300, 300), 0);  // L14

  AddDrawingLayerTo(root_ptr, 7, gfx::Rect(450, 450, 450, 450), &layer_s2_ptr);
  layer_s2_ptr->SetForceRenderSurface(true);

  // Initial draw - must receive all quads
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive 3 render passes.
    // For Root, there are 2 quads; for S1, there are 3 quads; for S2, there is
    // 1 quad.
    ASSERT_EQ(3U, frame.render_passes.size());

    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());

    // L14 is culled, so only 3 quads.
    EXPECT_EQ(3U, frame.render_passes[1]->quad_list.size());
    EXPECT_EQ(2U, frame.render_passes[2]->quad_list.size());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // "Unocclude" surface S1 and repeat draw.
  // Must remove S2's render pass since it's cached;
  // Must keep S1 quads because texture contained external occlusion.
  gfx::Transform transform = layer_s2_ptr->transform();
  transform.Translate(100.0, 100.0);
  layer_s2_ptr->SetTransform(transform);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive 2 render passes.
    // For Root, there are 2 quads
    // For S1, the number of quads depends on what got unoccluded, so not
    // asserted beyond being positive.
    // For S2, there is no render pass
    ASSERT_EQ(2U, frame.render_passes.size());

    EXPECT_GT(frame.render_passes[0]->quad_list.size(), 0U);
    EXPECT_EQ(2U, frame.render_passes[1]->quad_list.size());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // "Re-occlude" surface S1 and repeat draw.
  // Must remove S1's render pass since it is now available in full.
  // S2 has no change so must also be removed.
  transform = layer_s2_ptr->transform();
  transform.Translate(-15.0, -15.0);
  layer_s2_ptr->SetTransform(transform);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive 1 render pass - for the root.
    ASSERT_EQ(1U, frame.render_passes.size());

    EXPECT_EQ(2U, frame.render_passes[0]->quad_list.size());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, TextureCachingWithOcclusionExternalOverInternal) {
  LayerTreeSettings settings;
  settings.minimum_occlusion_tracking_size = gfx::Size();
  settings.cache_render_pass_contents = true;
  scoped_ptr<LayerTreeHostImpl> my_host_impl =
      LayerTreeHostImpl::Create(settings,
                                this,
                                &proxy_,
                                &stats_instrumentation_);

  // Layers are structured as follows:
  //
  //  R +-- S1 +- L10 (owning, drawing)
  //    |      +- L11 (corner, occluded by L12)
  //    |      +- L12 (opposite corner)
  //    |
  //    +-- S2 +- L20 (owning, drawing)
  //

  LayerImpl* root_ptr;
  LayerImpl* layer_s1_ptr;
  LayerImpl* layer_s2_ptr;

  scoped_ptr<OutputSurface> output_surface =
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new PartialSwapContext)).PassAs<OutputSurface>();

  gfx::Size root_size(1000, 1000);

  my_host_impl->InitializeRenderer(output_surface.Pass());
  my_host_impl->SetViewportSize(root_size, root_size);

  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(my_host_impl->active_tree(), 1);
  root_ptr = root.get();

  root->SetAnchorPoint(gfx::PointF());
  root->SetPosition(gfx::PointF());
  root->SetBounds(root_size);
  root->SetContentBounds(root_size);
  root->SetDrawsContent(true);
  root->SetMasksToBounds(true);
  my_host_impl->active_tree()->SetRootLayer(root.Pass());

  AddDrawingLayerTo(root_ptr, 2, gfx::Rect(0, 0, 400, 400), &layer_s1_ptr);
  layer_s1_ptr->SetForceRenderSurface(true);

  AddDrawingLayerTo(layer_s1_ptr, 3, gfx::Rect(0, 0, 300, 300), 0);  // L11
  AddDrawingLayerTo(layer_s1_ptr, 4, gfx::Rect(100, 0, 300, 300), 0);  // L12

  AddDrawingLayerTo(root_ptr, 7, gfx::Rect(200, 0, 300, 300), &layer_s2_ptr);
  layer_s2_ptr->SetForceRenderSurface(true);

  // Initial draw - must receive all quads
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive 3 render passes.
    // For Root, there are 2 quads; for S1, there are 3 quads; for S2, there is
    // 1 quad.
    ASSERT_EQ(3U, frame.render_passes.size());

    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(3U, frame.render_passes[1]->quad_list.size());
    EXPECT_EQ(2U, frame.render_passes[2]->quad_list.size());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // "Unocclude" surface S1 and repeat draw.
  // Must remove S2's render pass since it's cached;
  // Must keep S1 quads because texture contained external occlusion.
  gfx::Transform transform = layer_s2_ptr->transform();
  transform.Translate(300.0, 0.0);
  layer_s2_ptr->SetTransform(transform);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive 2 render passes.
    // For Root, there are 2 quads
    // For S1, the number of quads depends on what got unoccluded, so not
    // asserted beyond being positive.
    // For S2, there is no render pass
    ASSERT_EQ(2U, frame.render_passes.size());

    EXPECT_GT(frame.render_passes[0]->quad_list.size(), 0U);
    EXPECT_EQ(2U, frame.render_passes[1]->quad_list.size());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, TextureCachingWithOcclusionExternalNotAligned) {
  LayerTreeSettings settings;
  settings.cache_render_pass_contents = true;
  scoped_ptr<LayerTreeHostImpl> my_host_impl =
      LayerTreeHostImpl::Create(settings,
                                this,
                                &proxy_,
                                &stats_instrumentation_);

  // Layers are structured as follows:
  //
  //  R +-- S1 +- L10 (rotated, drawing)
  //           +- L11 (occupies half surface)

  LayerImpl* root_ptr;
  LayerImpl* layer_s1_ptr;

  scoped_ptr<OutputSurface> output_surface =
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new PartialSwapContext)).PassAs<OutputSurface>();

  gfx::Size root_size(1000, 1000);

  my_host_impl->InitializeRenderer(output_surface.Pass());
  my_host_impl->SetViewportSize(root_size, root_size);

  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(my_host_impl->active_tree(), 1);
  root_ptr = root.get();

  root->SetAnchorPoint(gfx::PointF());
  root->SetPosition(gfx::PointF());
  root->SetBounds(root_size);
  root->SetContentBounds(root_size);
  root->SetDrawsContent(true);
  root->SetMasksToBounds(true);
  my_host_impl->active_tree()->SetRootLayer(root.Pass());

  AddDrawingLayerTo(root_ptr, 2, gfx::Rect(0, 0, 400, 400), &layer_s1_ptr);
  layer_s1_ptr->SetForceRenderSurface(true);
  gfx::Transform transform = layer_s1_ptr->transform();
  transform.Translate(200.0, 200.0);
  transform.Rotate(45.0);
  transform.Translate(-200.0, -200.0);
  layer_s1_ptr->SetTransform(transform);

  AddDrawingLayerTo(layer_s1_ptr, 3, gfx::Rect(200, 0, 200, 400), 0);  // L11

  // Initial draw - must receive all quads
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive 2 render passes.
    ASSERT_EQ(2U, frame.render_passes.size());

    EXPECT_EQ(2U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(1U, frame.render_passes[1]->quad_list.size());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // Change opacity and draw. Verify we used cached texture.
  layer_s1_ptr->SetOpacity(0.2f);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // One render pass must be gone due to cached texture.
    ASSERT_EQ(1U, frame.render_passes.size());

    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, TextureCachingWithOcclusionPartialSwap) {
  LayerTreeSettings settings;
  settings.minimum_occlusion_tracking_size = gfx::Size();
  settings.partial_swap_enabled = true;
  settings.cache_render_pass_contents = true;
  scoped_ptr<LayerTreeHostImpl> my_host_impl =
      LayerTreeHostImpl::Create(settings,
                                this,
                                &proxy_,
                                &stats_instrumentation_);

  // Layers are structure as follows:
  //
  //  R +-- S1 +- L10 (owning)
  //    |      +- L11
  //    |      +- L12
  //    |
  //    +-- S2 +- L20 (owning)
  //           +- L21
  //
  // Occlusion:
  // L12 occludes L11 (internal)
  // L20 occludes L10 (external)
  // L21 occludes L20 (internal)

  LayerImpl* root_ptr;
  LayerImpl* layer_s1_ptr;
  LayerImpl* layer_s2_ptr;

  scoped_ptr<OutputSurface> output_surface =
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new PartialSwapContext)).PassAs<OutputSurface>();

  gfx::Size root_size(1000, 1000);

  my_host_impl->InitializeRenderer(output_surface.Pass());
  my_host_impl->SetViewportSize(root_size, root_size);

  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(my_host_impl->active_tree(), 1);
  root_ptr = root.get();

  root->SetAnchorPoint(gfx::PointF());
  root->SetPosition(gfx::PointF());
  root->SetBounds(root_size);
  root->SetContentBounds(root_size);
  root->SetDrawsContent(true);
  root->SetMasksToBounds(true);
  my_host_impl->active_tree()->SetRootLayer(root.Pass());

  AddDrawingLayerTo(root_ptr, 2, gfx::Rect(300, 300, 300, 300), &layer_s1_ptr);
  layer_s1_ptr->SetForceRenderSurface(true);

  AddDrawingLayerTo(layer_s1_ptr, 3, gfx::Rect(10, 10, 10, 10), 0);  // L11
  AddDrawingLayerTo(layer_s1_ptr, 4, gfx::Rect(0, 0, 30, 30), 0);  // L12

  AddDrawingLayerTo(root_ptr, 5, gfx::Rect(550, 250, 300, 400), &layer_s2_ptr);
  layer_s2_ptr->SetForceRenderSurface(true);

  AddDrawingLayerTo(layer_s2_ptr, 6, gfx::Rect(20, 20, 5, 5), 0);  // L21

  // Initial draw - must receive all quads
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive 3 render passes.
    // For Root, there are 2 quads; for S1, there are 2 quads (one is occluded);
    // for S2, there is 2 quads.
    ASSERT_EQ(3U, frame.render_passes.size());

    EXPECT_EQ(2U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(2U, frame.render_passes[1]->quad_list.size());
    EXPECT_EQ(2U, frame.render_passes[2]->quad_list.size());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // "Unocclude" surface S1 and repeat draw.
  // Must remove S2's render pass since it's cached;
  // Must keep S1 quads because texture contained external occlusion.
  gfx::Transform transform = layer_s2_ptr->transform();
  transform.Translate(150.0, 150.0);
  layer_s2_ptr->SetTransform(transform);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive 2 render passes.
    // For Root, there are 2 quads.
    // For S1, there are 2 quads.
    // For S2, there is no render pass
    ASSERT_EQ(2U, frame.render_passes.size());

    EXPECT_EQ(2U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(2U, frame.render_passes[1]->quad_list.size());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // "Re-occlude" surface S1 and repeat draw.
  // Must remove S1's render pass since it is now available in full.
  // S2 has no change so must also be removed.
  transform = layer_s2_ptr->transform();
  transform.Translate(-15.0, -15.0);
  layer_s2_ptr->SetTransform(transform);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Root render pass only.
    ASSERT_EQ(1U, frame.render_passes.size());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, TextureCachingWithScissor) {
  LayerTreeSettings settings;
  settings.minimum_occlusion_tracking_size = gfx::Size();
  settings.cache_render_pass_contents = true;
  scoped_ptr<LayerTreeHostImpl> my_host_impl =
      LayerTreeHostImpl::Create(settings,
                                this,
                                &proxy_,
                                &stats_instrumentation_);

  /*
    Layers are created as follows:

    +--------------------+
    |                  1 |
    |  +-----------+     |
    |  |         2 |     |
    |  | +-------------------+
    |  | |   3               |
    |  | +-------------------+
    |  |           |     |
    |  +-----------+     |
    |                    |
    |                    |
    +--------------------+

    Layers 1, 2 have render surfaces
  */
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(my_host_impl->active_tree(), 1);
  scoped_ptr<TiledLayerImpl> child =
      TiledLayerImpl::Create(my_host_impl->active_tree(), 2);
  scoped_ptr<LayerImpl> grand_child =
      LayerImpl::Create(my_host_impl->active_tree(), 3);

  gfx::Rect root_rect(0, 0, 100, 100);
  gfx::Rect child_rect(10, 10, 50, 50);
  gfx::Rect grand_child_rect(5, 5, 150, 150);

  scoped_ptr<OutputSurface> output_surface =
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new PartialSwapContext)).PassAs<OutputSurface>();
  my_host_impl->InitializeRenderer(output_surface.Pass());

  root->SetAnchorPoint(gfx::PointF());
  root->SetPosition(gfx::PointF(root_rect.x(), root_rect.y()));
  root->SetBounds(gfx::Size(root_rect.width(), root_rect.height()));
  root->SetContentBounds(root->bounds());
  root->SetDrawsContent(true);
  root->SetMasksToBounds(true);

  child->SetAnchorPoint(gfx::PointF());
  child->SetPosition(gfx::PointF(child_rect.x(), child_rect.y()));
  child->SetOpacity(0.5f);
  child->SetBounds(gfx::Size(child_rect.width(), child_rect.height()));
  child->SetContentBounds(child->bounds());
  child->SetDrawsContent(true);
  child->set_skips_draw(false);

  // child layer has 10x10 tiles.
  scoped_ptr<LayerTilingData> tiler =
      LayerTilingData::Create(gfx::Size(10, 10),
                              LayerTilingData::HAS_BORDER_TEXELS);
  tiler->SetBounds(child->content_bounds());
  child->SetTilingData(*tiler.get());

  grand_child->SetAnchorPoint(gfx::PointF());
  grand_child->SetPosition(grand_child_rect.origin());
  grand_child->SetBounds(grand_child_rect.size());
  grand_child->SetContentBounds(grand_child->bounds());
  grand_child->SetDrawsContent(true);

  TiledLayerImpl* child_ptr = child.get();
  RenderPass::Id child_pass_id(child_ptr->id(), 0);

  child->AddChild(grand_child.Pass());
  root->AddChild(child.PassAs<LayerImpl>());
  my_host_impl->active_tree()->SetRootLayer(root.Pass());
  my_host_impl->SetViewportSize(root_rect.size(), root_rect.size());

  EXPECT_FALSE(my_host_impl->renderer()->HaveCachedResourcesForRenderPassId(
      child_pass_id));
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));
    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // We should have cached textures for surface 2.
  EXPECT_TRUE(my_host_impl->renderer()->HaveCachedResourcesForRenderPassId(
      child_pass_id));
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));
    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // We should still have cached textures for surface 2 after drawing with no
  // damage.
  EXPECT_TRUE(my_host_impl->renderer()->HaveCachedResourcesForRenderPassId(
      child_pass_id));

  // Damage a single tile of surface 2.
  child_ptr->set_update_rect(gfx::Rect(10, 10, 10, 10));
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));
    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // We should have a cached texture for surface 2 again even though it was
  // damaged.
  EXPECT_TRUE(my_host_impl->renderer()->HaveCachedResourcesForRenderPassId(
      child_pass_id));
}

TEST_F(LayerTreeHostImplTest, SurfaceTextureCaching) {
  LayerTreeSettings settings;
  settings.minimum_occlusion_tracking_size = gfx::Size();
  settings.partial_swap_enabled = true;
  settings.cache_render_pass_contents = true;
  scoped_ptr<LayerTreeHostImpl> my_host_impl =
      LayerTreeHostImpl::Create(settings,
                                this,
                                &proxy_,
                                &stats_instrumentation_);

  LayerImpl* root_ptr;
  LayerImpl* intermediate_layer_ptr;
  LayerImpl* surface_layer_ptr;
  LayerImpl* child_ptr;

  SetupLayersForTextureCaching(my_host_impl.get(),
                               root_ptr,
                               intermediate_layer_ptr,
                               surface_layer_ptr,
                               child_ptr,
                               gfx::Size(100, 100));
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive two render passes, each with one quad
    ASSERT_EQ(2U, frame.render_passes.size());
    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(1U, frame.render_passes[1]->quad_list.size());

    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[1]->quad_list[0]->material);
    const RenderPassDrawQuad* quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[1]->quad_list[0]);
    RenderPass* target_pass = frame.render_passes_by_id[quad->render_pass_id];
    ASSERT_TRUE(target_pass);
    EXPECT_FALSE(target_pass->damage_rect.IsEmpty());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // Draw without any change
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive one render pass, as the other one should be culled
    ASSERT_EQ(1U, frame.render_passes.size());

    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_TRUE(frame.render_passes_by_id.find(quad->render_pass_id) ==
                frame.render_passes_by_id.end());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // Change opacity and draw
  surface_layer_ptr->SetOpacity(0.6f);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive one render pass, as the other one should be culled
    ASSERT_EQ(1U, frame.render_passes.size());

    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_TRUE(frame.render_passes_by_id.find(quad->render_pass_id) ==
                frame.render_passes_by_id.end());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // Change less benign property and draw - should have contents changed flag
  surface_layer_ptr->SetStackingOrderChanged(true);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive two render passes, each with one quad
    ASSERT_EQ(2U, frame.render_passes.size());

    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(DrawQuad::SOLID_COLOR,
              frame.render_passes[0]->quad_list[0]->material);

    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[1]->quad_list[0]->material);
    const RenderPassDrawQuad* quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[1]->quad_list[0]);
    RenderPass* target_pass = frame.render_passes_by_id[quad->render_pass_id];
    ASSERT_TRUE(target_pass);
    EXPECT_FALSE(target_pass->damage_rect.IsEmpty());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // Change opacity again, and evict the cached surface texture.
  surface_layer_ptr->SetOpacity(0.5f);
  static_cast<GLRendererWithReleaseTextures*>(
      my_host_impl->renderer())->ReleaseRenderPassTextures();

  // Change opacity and draw
  surface_layer_ptr->SetOpacity(0.6f);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive two render passes
    ASSERT_EQ(2U, frame.render_passes.size());

    // Even though not enough properties changed, the entire thing must be
    // redrawn as we don't have cached textures
    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(1U, frame.render_passes[1]->quad_list.size());

    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[1]->quad_list[0]->material);
    const RenderPassDrawQuad* quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[1]->quad_list[0]);
    RenderPass* target_pass = frame.render_passes_by_id[quad->render_pass_id];
    ASSERT_TRUE(target_pass);
    EXPECT_TRUE(target_pass->damage_rect.IsEmpty());

    // Was our surface evicted?
    EXPECT_FALSE(my_host_impl->renderer()->HaveCachedResourcesForRenderPassId(
        target_pass->id));

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // Draw without any change, to make sure the state is clear
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive one render pass, as the other one should be culled
    ASSERT_EQ(1U, frame.render_passes.size());

    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_TRUE(frame.render_passes_by_id.find(quad->render_pass_id) ==
                frame.render_passes_by_id.end());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // Change location of the intermediate layer
  gfx::Transform transform = intermediate_layer_ptr->transform();
  transform.matrix().setDouble(0, 3, 1.0001);
  intermediate_layer_ptr->SetTransform(transform);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive one render pass, as the other one should be culled.
    ASSERT_EQ(1U, frame.render_passes.size());
    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());

    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_TRUE(frame.render_passes_by_id.find(quad->render_pass_id) ==
                frame.render_passes_by_id.end());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, SurfaceTextureCachingNoPartialSwap) {
  LayerTreeSettings settings;
  settings.minimum_occlusion_tracking_size = gfx::Size();
  settings.cache_render_pass_contents = true;
  scoped_ptr<LayerTreeHostImpl> my_host_impl =
      LayerTreeHostImpl::Create(settings,
                                this,
                                &proxy_,
                                &stats_instrumentation_);

  LayerImpl* root_ptr;
  LayerImpl* intermediate_layer_ptr;
  LayerImpl* surface_layer_ptr;
  LayerImpl* child_ptr;

  SetupLayersForTextureCaching(my_host_impl.get(),
                               root_ptr,
                               intermediate_layer_ptr,
                               surface_layer_ptr,
                               child_ptr,
                               gfx::Size(100, 100));
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive two render passes, each with one quad
    ASSERT_EQ(2U, frame.render_passes.size());
    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(1U, frame.render_passes[1]->quad_list.size());

    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[1]->quad_list[0]->material);
    const RenderPassDrawQuad* quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[1]->quad_list[0]);
    RenderPass* target_pass = frame.render_passes_by_id[quad->render_pass_id];
    EXPECT_FALSE(target_pass->damage_rect.IsEmpty());

    EXPECT_FALSE(frame.render_passes[0]->damage_rect.IsEmpty());
    EXPECT_FALSE(frame.render_passes[1]->damage_rect.IsEmpty());

    EXPECT_FALSE(
        frame.render_passes[0]->has_occlusion_from_outside_target_surface);
    EXPECT_FALSE(
        frame.render_passes[1]->has_occlusion_from_outside_target_surface);

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // Draw without any change
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Even though there was no change, we set the damage to entire viewport.
    // One of the passes should be culled as a result, since contents didn't
    // change and we have cached texture.
    ASSERT_EQ(1U, frame.render_passes.size());
    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());

    EXPECT_TRUE(frame.render_passes[0]->damage_rect.IsEmpty());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // Change opacity and draw
  surface_layer_ptr->SetOpacity(0.6f);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive one render pass, as the other one should be culled
    ASSERT_EQ(1U, frame.render_passes.size());

    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_TRUE(frame.render_passes_by_id.find(quad->render_pass_id) ==
                frame.render_passes_by_id.end());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // Change less benign property and draw - should have contents changed flag
  surface_layer_ptr->SetStackingOrderChanged(true);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive two render passes, each with one quad
    ASSERT_EQ(2U, frame.render_passes.size());

    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(DrawQuad::SOLID_COLOR,
              frame.render_passes[0]->quad_list[0]->material);

    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[1]->quad_list[0]->material);
    const RenderPassDrawQuad* quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[1]->quad_list[0]);
    RenderPass* target_pass = frame.render_passes_by_id[quad->render_pass_id];
    ASSERT_TRUE(target_pass);
    EXPECT_FALSE(target_pass->damage_rect.IsEmpty());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // Change opacity again, and evict the cached surface texture.
  surface_layer_ptr->SetOpacity(0.5f);
  static_cast<GLRendererWithReleaseTextures*>(
      my_host_impl->renderer())->ReleaseRenderPassTextures();

  // Change opacity and draw
  surface_layer_ptr->SetOpacity(0.6f);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive two render passes
    ASSERT_EQ(2U, frame.render_passes.size());

    // Even though not enough properties changed, the entire thing must be
    // redrawn as we don't have cached textures
    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());
    EXPECT_EQ(1U, frame.render_passes[1]->quad_list.size());

    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[1]->quad_list[0]->material);
    const RenderPassDrawQuad* quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[1]->quad_list[0]);
    RenderPass* target_pass = frame.render_passes_by_id[quad->render_pass_id];
    ASSERT_TRUE(target_pass);
    EXPECT_TRUE(target_pass->damage_rect.IsEmpty());

    // Was our surface evicted?
    EXPECT_FALSE(my_host_impl->renderer()->HaveCachedResourcesForRenderPassId(
        target_pass->id));

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // Draw without any change, to make sure the state is clear
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Even though there was no change, we set the damage to entire viewport.
    // One of the passes should be culled as a result, since contents didn't
    // change and we have cached texture.
    ASSERT_EQ(1U, frame.render_passes.size());
    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }

  // Change location of the intermediate layer
  gfx::Transform transform = intermediate_layer_ptr->transform();
  transform.matrix().setDouble(0, 3, 1.0001);
  intermediate_layer_ptr->SetTransform(transform);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame));

    // Must receive one render pass, as the other one should be culled.
    ASSERT_EQ(1U, frame.render_passes.size());
    EXPECT_EQ(1U, frame.render_passes[0]->quad_list.size());

    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_TRUE(frame.render_passes_by_id.find(quad->render_pass_id) ==
                frame.render_passes_by_id.end());

    my_host_impl->DrawLayers(&frame, base::TimeTicks::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, ReleaseContentsTextureShouldTriggerCommit) {
  set_reduce_memory_result(false);

  // Even if changing the memory limit didn't result in anything being
  // evicted, we need to re-commit because the new value may result in us
  // drawing something different than before.
  set_reduce_memory_result(false);
  host_impl_->SetManagedMemoryPolicy(ManagedMemoryPolicy(
      host_impl_->memory_allocation_limit_bytes() - 1));
  EXPECT_TRUE(did_request_commit_);
  did_request_commit_ = false;

  // Especially if changing the memory limit caused evictions, we need
  // to re-commit.
  set_reduce_memory_result(true);
  host_impl_->SetManagedMemoryPolicy(ManagedMemoryPolicy(
      host_impl_->memory_allocation_limit_bytes() - 1));
  EXPECT_TRUE(did_request_commit_);
  did_request_commit_ = false;

  // But if we set it to the same value that it was before, we shouldn't
  // re-commit.
  host_impl_->SetManagedMemoryPolicy(ManagedMemoryPolicy(
      host_impl_->memory_allocation_limit_bytes()));
  EXPECT_FALSE(did_request_commit_);
}

struct RenderPassRemovalTestData : public LayerTreeHostImpl::FrameData {
  ScopedPtrHashMap<RenderPass::Id, TestRenderPass> render_pass_cache;
  scoped_ptr<SharedQuadState> shared_quad_state;
};

class TestRenderer : public GLRenderer, public RendererClient {
 public:
  static scoped_ptr<TestRenderer> Create(ResourceProvider* resource_provider,
                                         OutputSurface* output_surface,
                                         Proxy* proxy) {
    scoped_ptr<TestRenderer> renderer(new TestRenderer(resource_provider,
                                                       output_surface,
                                                       proxy));
    if (!renderer->Initialize())
      return scoped_ptr<TestRenderer>();

    return renderer.Pass();
  }

  void ClearCachedTextures() { textures_.clear(); }
  void SetHaveCachedResourcesForRenderPassId(RenderPass::Id id) {
    textures_.insert(id);
  }

  virtual bool HaveCachedResourcesForRenderPassId(RenderPass::Id id) const
      OVERRIDE {
    return textures_.count(id);
  }

  // RendererClient implementation.
  virtual gfx::Size DeviceViewportSize() const OVERRIDE {
    return viewport_size_;
  }
  virtual const LayerTreeSettings& Settings() const OVERRIDE {
    return settings_;
  }
  virtual void DidLoseOutputSurface() OVERRIDE {}
  virtual void OnSwapBuffersComplete() OVERRIDE {}
  virtual void SetFullRootLayerDamage() OVERRIDE {}
  virtual void SetManagedMemoryPolicy(const ManagedMemoryPolicy& policy)
      OVERRIDE {}
  virtual void EnforceManagedMemoryPolicy(const ManagedMemoryPolicy& policy)
      OVERRIDE {}
  virtual bool HasImplThread() const OVERRIDE { return false; }
  virtual bool ShouldClearRootRenderPass() const OVERRIDE { return true; }
  virtual CompositorFrameMetadata MakeCompositorFrameMetadata() const
      OVERRIDE { return CompositorFrameMetadata(); }

 protected:
  TestRenderer(ResourceProvider* resource_provider,
               OutputSurface* output_surface,
               Proxy* proxy)
      : GLRenderer(this, output_surface, resource_provider) {}

 private:
  LayerTreeSettings settings_;
  gfx::Size viewport_size_;
  base::hash_set<RenderPass::Id> textures_;
};

static void ConfigureRenderPassTestData(const char* test_script,
                                        RenderPassRemovalTestData& test_data,
                                        TestRenderer* renderer) {
  renderer->ClearCachedTextures();

  // One shared state for all quads - we don't need the correct details
  test_data.shared_quad_state = SharedQuadState::Create();
  test_data.shared_quad_state->SetAll(gfx::Transform(),
                                      gfx::Size(),
                                      gfx::Rect(),
                                      gfx::Rect(),
                                      false,
                                      1.f);

  const char* current_char = test_script;

  // Pre-create root pass
  RenderPass::Id root_render_pass_id =
      RenderPass::Id(test_script[0], test_script[1]);
  scoped_ptr<TestRenderPass> pass = TestRenderPass::Create();
  pass->SetNew(root_render_pass_id, gfx::Rect(), gfx::Rect(), gfx::Transform());
  test_data.render_pass_cache.add(root_render_pass_id, pass.Pass());
  while (*current_char) {
    int layer_id = *current_char;
    current_char++;
    ASSERT_TRUE(current_char);
    int index = *current_char;
    current_char++;

    RenderPass::Id render_pass_id = RenderPass::Id(layer_id, index);

    bool is_replica = false;
    if (!test_data.render_pass_cache.contains(render_pass_id))
      is_replica = true;

    scoped_ptr<TestRenderPass> render_pass =
        test_data.render_pass_cache.take(render_pass_id);

    // Cycle through quad data and create all quads.
    while (*current_char && *current_char != '\n') {
      if (*current_char == 's') {
        // Solid color draw quad.
        scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
        quad->SetNew(test_data.shared_quad_state.get(),
                     gfx::Rect(0, 0, 10, 10),
                     SK_ColorWHITE);

        render_pass->AppendQuad(quad.PassAs<DrawQuad>());
        current_char++;
      } else if ((*current_char >= 'A') && (*current_char <= 'Z')) {
        // RenderPass draw quad.
        int layer_id = *current_char;
        current_char++;
        ASSERT_TRUE(current_char);
        int index = *current_char;
        current_char++;
        RenderPass::Id new_render_pass_id = RenderPass::Id(layer_id, index);
        ASSERT_NE(root_render_pass_id, new_render_pass_id);
        bool has_texture = false;
        bool contents_changed = true;

        if (*current_char == '[') {
          current_char++;
          while (*current_char && *current_char != ']') {
            switch (*current_char) {
              case 'c':
                contents_changed = false;
                break;
              case 't':
                has_texture = true;
                break;
            }
            current_char++;
          }
          if (*current_char == ']')
            current_char++;
        }

        if (test_data.render_pass_cache.find(new_render_pass_id) ==
            test_data.render_pass_cache.end()) {
          if (has_texture)
            renderer->SetHaveCachedResourcesForRenderPassId(new_render_pass_id);

          scoped_ptr<TestRenderPass> pass = TestRenderPass::Create();
          pass->SetNew(new_render_pass_id,
                       gfx::Rect(),
                       gfx::Rect(),
                       gfx::Transform());
          test_data.render_pass_cache.add(new_render_pass_id, pass.Pass());
        }

        gfx::Rect quad_rect = gfx::Rect(0, 0, 1, 1);
        gfx::Rect contents_changed_rect =
            contents_changed ? quad_rect : gfx::Rect();
        scoped_ptr<RenderPassDrawQuad> quad = RenderPassDrawQuad::Create();
        quad->SetNew(test_data.shared_quad_state.get(),
                     quad_rect,
                     new_render_pass_id,
                     is_replica,
                     1,
                     contents_changed_rect,
                     gfx::RectF(0.f, 0.f, 1.f, 1.f),
                     WebKit::WebFilterOperations(),
                     skia::RefPtr<SkImageFilter>(),
                     WebKit::WebFilterOperations());
        render_pass->AppendQuad(quad.PassAs<DrawQuad>());
      }
    }
    test_data.render_passes_by_id[render_pass_id] = render_pass.get();
    test_data.render_passes.insert(test_data.render_passes.begin(),
                                   render_pass.PassAs<RenderPass>());
    if (*current_char)
      current_char++;
  }
}

void DumpRenderPassTestData(const RenderPassRemovalTestData& test_data,
                            char* buffer) {
  char* pos = buffer;
  for (RenderPassList::const_reverse_iterator it =
           test_data.render_passes.rbegin();
       it != test_data.render_passes.rend();
       ++it) {
    const RenderPass* current_pass = *it;
    *pos = current_pass->id.layer_id;
    pos++;
    *pos = current_pass->id.index;
    pos++;

    QuadList::const_iterator quad_list_iterator =
        current_pass->quad_list.begin();
    while (quad_list_iterator != current_pass->quad_list.end()) {
      DrawQuad* current_quad = *quad_list_iterator;
      switch (current_quad->material) {
        case DrawQuad::SOLID_COLOR:
          *pos = 's';
          pos++;
          break;
        case DrawQuad::RENDER_PASS:
          *pos = RenderPassDrawQuad::MaterialCast(current_quad)->
                     render_pass_id.layer_id;
          pos++;
          *pos = RenderPassDrawQuad::MaterialCast(current_quad)->
                     render_pass_id.index;
          pos++;
          break;
        default:
          *pos = 'x';
          pos++;
          break;
      }

      quad_list_iterator++;
    }
    *pos = '\n';
    pos++;
  }
  *pos = '\0';
}

// Each RenderPassList is represented by a string which describes the
// configuration.
// The syntax of the string is as follows:
//
//                                                   RsssssX[c]ssYsssZ[t]ssW[ct]
// Identifies the render pass------------------------^ ^^^ ^ ^   ^     ^     ^
// These are solid color quads--------------------------+  | |   |     |     |
// Identifies RenderPassDrawQuad's RenderPass--------------+ |   |     |     |
// This quad's contents didn't change------------------------+   |     |     |
// This quad's contents changed and it has no texture------------+     |     |
// This quad has texture but its contents changed----------------------+     |
// This quad's contents didn't change and it has texture - will be removed---+
//
// Expected results have exactly the same syntax, except they do not use square
// brackets, since we only check the structure, not attributes.
//
// Test case configuration consists of initialization script and expected
// results, all in the same format.
struct TestCase {
  const char* name;
  const char* init_script;
  const char* expected_result;
};

TestCase remove_render_passes_cases[] = {
  {
    "Single root pass",
    "R0ssss\n",
    "R0ssss\n"
  }, {
    "Single pass - no quads",
    "R0\n",
    "R0\n"
  }, {
    "Two passes, no removal",
    "R0ssssA0sss\n"
    "A0ssss\n",
    "R0ssssA0sss\n"
    "A0ssss\n"
  }, {
    "Two passes, remove last",
    "R0ssssA0[ct]sss\n"
    "A0ssss\n",
    "R0ssssA0sss\n"
  }, {
    "Have texture but contents changed - leave pass",
    "R0ssssA0[t]sss\n"
    "A0ssss\n",
    "R0ssssA0sss\n"
    "A0ssss\n"
  }, {
    "Contents didn't change but no texture - leave pass",
    "R0ssssA0[c]sss\n"
    "A0ssss\n",
    "R0ssssA0sss\n"
    "A0ssss\n"
  }, {
    "Replica: two quads reference the same pass; remove",
    "R0ssssA0[ct]A0[ct]sss\n"
    "A0ssss\n",
    "R0ssssA0A0sss\n"
  }, {
    "Replica: two quads reference the same pass; leave",
    "R0ssssA0[c]A0[c]sss\n"
    "A0ssss\n",
    "R0ssssA0A0sss\n"
    "A0ssss\n",
  }, {
    "Many passes, remove all",
    "R0ssssA0[ct]sss\n"
    "A0sssB0[ct]C0[ct]s\n"
    "B0sssD0[ct]ssE0[ct]F0[ct]\n"
    "E0ssssss\n"
    "C0G0[ct]\n"
    "D0sssssss\n"
    "F0sssssss\n"
    "G0sss\n",

    "R0ssssA0sss\n"
  }, {
    "Deep recursion, remove all",

    "R0sssssA0[ct]ssss\n"
    "A0ssssB0sss\n"
    "B0C0\n"
    "C0D0\n"
    "D0E0\n"
    "E0F0\n"
    "F0G0\n"
    "G0H0\n"
    "H0sssI0sss\n"
    "I0J0\n"
    "J0ssss\n",

    "R0sssssA0ssss\n"
  }, {
    "Wide recursion, remove all",
    "R0A0[ct]B0[ct]C0[ct]D0[ct]E0[ct]F0[ct]G0[ct]H0[ct]I0[ct]J0[ct]\n"
    "A0s\n"
    "B0s\n"
    "C0ssss\n"
    "D0ssss\n"
    "E0s\n"
    "F0\n"
    "G0s\n"
    "H0s\n"
    "I0s\n"
    "J0ssss\n",

    "R0A0B0C0D0E0F0G0H0I0J0\n"
  }, {
    "Remove passes regardless of cache state",
    "R0ssssA0[ct]sss\n"
    "A0sssB0C0s\n"
    "B0sssD0[c]ssE0[t]F0\n"
    "E0ssssss\n"
    "C0G0\n"
    "D0sssssss\n"
    "F0sssssss\n"
    "G0sss\n",

    "R0ssssA0sss\n"
  }, {
    "Leave some passes, remove others",

    "R0ssssA0[c]sss\n"
    "A0sssB0[t]C0[ct]s\n"
    "B0sssD0[c]ss\n"
    "C0G0\n"
    "D0sssssss\n"
    "G0sss\n",

    "R0ssssA0sss\n"
    "A0sssB0C0s\n"
    "B0sssD0ss\n"
    "D0sssssss\n"
  }, {
    0, 0, 0
  }
};

static void VerifyRenderPassTestData(
    const TestCase& test_case,
    const RenderPassRemovalTestData& test_data) {
  char actual_result[1024];
  DumpRenderPassTestData(test_data, actual_result);
  EXPECT_STREQ(test_case.expected_result, actual_result) << "In test case: " <<
      test_case.name;
}

TEST_F(LayerTreeHostImplTest, TestRemoveRenderPasses) {
  scoped_ptr<OutputSurface> output_surface(CreateOutputSurface());
  ASSERT_TRUE(output_surface->context3d());
  scoped_ptr<ResourceProvider> resource_provider =
      ResourceProvider::Create(output_surface.get());

  scoped_ptr<TestRenderer> renderer =
      TestRenderer::Create(resource_provider.get(),
                           output_surface.get(),
                           &proxy_);

  int test_case_index = 0;
  while (remove_render_passes_cases[test_case_index].name) {
    RenderPassRemovalTestData test_data;
    ConfigureRenderPassTestData(
        remove_render_passes_cases[test_case_index].init_script,
        test_data,
        renderer.get());
    LayerTreeHostImpl::RemoveRenderPasses(
        LayerTreeHostImpl::CullRenderPassesWithCachedTextures(renderer.get()),
        &test_data);
    VerifyRenderPassTestData(remove_render_passes_cases[test_case_index],
                             test_data);
    test_case_index++;
  }
}

class LayerTreeHostImplTestWithDelegatingRenderer
    : public LayerTreeHostImplTest {
 protected:
  virtual scoped_ptr<OutputSurface> CreateOutputSurface() OVERRIDE {
    return FakeOutputSurface::CreateDelegating3d().PassAs<OutputSurface>();
  }

  void DrawFrameAndTestDamage(const gfx::RectF& expected_damage) {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));
    ASSERT_EQ(1u, frame.render_passes.size());

    // Verify the damage rect for the root render pass.
    const RenderPass* root_render_pass = frame.render_passes.back();
    EXPECT_RECT_EQ(expected_damage, root_render_pass->damage_rect);

    // Verify the root layer's quad is generated and not being culled.
    ASSERT_EQ(1u, root_render_pass->quad_list.size());
    gfx::Rect expected_visible_rect(
        host_impl_->active_tree()->root_layer()->content_bounds());
    EXPECT_RECT_EQ(expected_visible_rect,
                   root_render_pass->quad_list[0]->visible_rect);

    host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
    host_impl_->DidDrawAllLayers(frame);
  }
};

TEST_F(LayerTreeHostImplTestWithDelegatingRenderer, FrameIncludesDamageRect) {
  scoped_ptr<SolidColorLayerImpl> root =
      SolidColorLayerImpl::Create(host_impl_->active_tree(), 1);
  root->SetAnchorPoint(gfx::PointF());
  root->SetPosition(gfx::PointF());
  root->SetBounds(gfx::Size(10, 10));
  root->SetContentBounds(gfx::Size(10, 10));
  root->SetDrawsContent(true);
  host_impl_->active_tree()->SetRootLayer(root.PassAs<LayerImpl>());

  // Draw a frame. In the first frame, the entire viewport should be damaged.
  gfx::Rect full_frame_damage = gfx::Rect(host_impl_->device_viewport_size());
  DrawFrameAndTestDamage(full_frame_damage);

  // The second frame should have no damage, but the quads should still be
  // generated.
  gfx::Rect no_damage;
  DrawFrameAndTestDamage(no_damage);
}

class FakeMaskLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<FakeMaskLayerImpl> Create(LayerTreeImpl* tree_impl,
                                              int id) {
    return make_scoped_ptr(new FakeMaskLayerImpl(tree_impl, id));
  }

  virtual ResourceProvider::ResourceId ContentsResourceId() const OVERRIDE {
    return 0;
  }

 private:
  FakeMaskLayerImpl(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id) {}
};

TEST_F(LayerTreeHostImplTest, MaskLayerWithScaling) {
  // Root
  //  |
  //  +-- Scaling Layer (adds a 2x scale)
  //       |
  //       +-- Content Layer
  //             +--Mask
  scoped_ptr<LayerImpl> scoped_root =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  LayerImpl* root = scoped_root.get();
  host_impl_->active_tree()->SetRootLayer(scoped_root.Pass());

  scoped_ptr<LayerImpl> scoped_scaling_layer =
      LayerImpl::Create(host_impl_->active_tree(), 2);
  LayerImpl* scaling_layer = scoped_scaling_layer.get();
  root->AddChild(scoped_scaling_layer.Pass());

  scoped_ptr<LayerImpl> scoped_content_layer =
      LayerImpl::Create(host_impl_->active_tree(), 3);
  LayerImpl* content_layer = scoped_content_layer.get();
  scaling_layer->AddChild(scoped_content_layer.Pass());

  scoped_ptr<FakeMaskLayerImpl> scoped_mask_layer =
      FakeMaskLayerImpl::Create(host_impl_->active_tree(), 4);
  FakeMaskLayerImpl* mask_layer = scoped_mask_layer.get();
  content_layer->SetMaskLayer(scoped_mask_layer.PassAs<LayerImpl>());

  gfx::Size root_size(100, 100);
  root->SetBounds(root_size);
  root->SetContentBounds(root_size);
  root->SetPosition(gfx::PointF());
  root->SetAnchorPoint(gfx::PointF());

  gfx::Size scaling_layer_size(50, 50);
  scaling_layer->SetBounds(scaling_layer_size);
  scaling_layer->SetContentBounds(scaling_layer_size);
  scaling_layer->SetPosition(gfx::PointF());
  scaling_layer->SetAnchorPoint(gfx::PointF());
  gfx::Transform scale;
  scale.Scale(2.f, 2.f);
  scaling_layer->SetTransform(scale);

  content_layer->SetBounds(scaling_layer_size);
  content_layer->SetContentBounds(scaling_layer_size);
  content_layer->SetPosition(gfx::PointF());
  content_layer->SetAnchorPoint(gfx::PointF());
  content_layer->SetDrawsContent(true);

  mask_layer->SetBounds(scaling_layer_size);
  mask_layer->SetContentBounds(scaling_layer_size);
  mask_layer->SetPosition(gfx::PointF());
  mask_layer->SetAnchorPoint(gfx::PointF());
  mask_layer->SetDrawsContent(true);


  // Check that the tree scaling is correctly taken into account for the mask,
  // that should fully map onto the quad.
  float device_scale_factor = 1.f;
  host_impl_->SetViewportSize(root_size, root_size);
  host_impl_->SetDeviceScaleFactor(device_scale_factor); {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

    ASSERT_EQ(1u, frame.render_passes.size());
    ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* render_pass_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
              render_pass_quad->rect.ToString());
    EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(),
              render_pass_quad->mask_uv_rect.ToString());

    host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
    host_impl_->DidDrawAllLayers(frame);
  }


  // Applying a DSF should change the render surface size, but won't affect
  // which part of the mask is used.
  device_scale_factor = 2.f;
  gfx::Size device_viewport =
      gfx::ToFlooredSize(gfx::ScaleSize(root_size, device_scale_factor));
  host_impl_->SetViewportSize(root_size, device_viewport);
  host_impl_->SetDeviceScaleFactor(device_scale_factor);
  host_impl_->active_tree()->set_needs_update_draw_properties(); {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

    ASSERT_EQ(1u, frame.render_passes.size());
    ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* render_pass_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_EQ(gfx::Rect(0, 0, 200, 200).ToString(),
              render_pass_quad->rect.ToString());
    EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(),
              render_pass_quad->mask_uv_rect.ToString());

    host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
    host_impl_->DidDrawAllLayers(frame);
  }


  // Applying an equivalent content scale on the content layer and the mask
  // should still result in the same part of the mask being used.
  gfx::Size content_bounds =
      gfx::ToRoundedSize(gfx::ScaleSize(scaling_layer_size,
                                        device_scale_factor));
  content_layer->SetContentBounds(content_bounds);
  content_layer->SetContentsScale(device_scale_factor, device_scale_factor);
  mask_layer->SetContentBounds(content_bounds);
  mask_layer->SetContentsScale(device_scale_factor, device_scale_factor);
  host_impl_->active_tree()->set_needs_update_draw_properties(); {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

    ASSERT_EQ(1u, frame.render_passes.size());
    ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* render_pass_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_EQ(gfx::Rect(0, 0, 200, 200).ToString(),
              render_pass_quad->rect.ToString());
    EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(),
              render_pass_quad->mask_uv_rect.ToString());

    host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
    host_impl_->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, MaskLayerWithDifferentBounds) {
  // The mask layer has bounds 100x100 but is attached to a layer with bounds
  // 50x50.

  scoped_ptr<LayerImpl> scoped_root =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  LayerImpl* root = scoped_root.get();
  host_impl_->active_tree()->SetRootLayer(scoped_root.Pass());

  scoped_ptr<LayerImpl> scoped_content_layer =
      LayerImpl::Create(host_impl_->active_tree(), 3);
  LayerImpl* content_layer = scoped_content_layer.get();
  root->AddChild(scoped_content_layer.Pass());

  scoped_ptr<FakeMaskLayerImpl> scoped_mask_layer =
      FakeMaskLayerImpl::Create(host_impl_->active_tree(), 4);
  FakeMaskLayerImpl* mask_layer = scoped_mask_layer.get();
  content_layer->SetMaskLayer(scoped_mask_layer.PassAs<LayerImpl>());

  gfx::Size root_size(100, 100);
  root->SetBounds(root_size);
  root->SetContentBounds(root_size);
  root->SetPosition(gfx::PointF());
  root->SetAnchorPoint(gfx::PointF());

  gfx::Size layer_size(50, 50);
  content_layer->SetBounds(layer_size);
  content_layer->SetContentBounds(layer_size);
  content_layer->SetPosition(gfx::PointF());
  content_layer->SetAnchorPoint(gfx::PointF());
  content_layer->SetDrawsContent(true);

  gfx::Size mask_size(100, 100);
  mask_layer->SetBounds(mask_size);
  mask_layer->SetContentBounds(mask_size);
  mask_layer->SetPosition(gfx::PointF());
  mask_layer->SetAnchorPoint(gfx::PointF());
  mask_layer->SetDrawsContent(true);


  // Check that the mask fills the surface.
  float device_scale_factor = 1.f;
  host_impl_->SetViewportSize(root_size, root_size);
  host_impl_->SetDeviceScaleFactor(device_scale_factor); {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

    ASSERT_EQ(1u, frame.render_passes.size());
    ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* render_pass_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(),
              render_pass_quad->rect.ToString());
    EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(),
              render_pass_quad->mask_uv_rect.ToString());

    host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
    host_impl_->DidDrawAllLayers(frame);
  }


  // Applying a DSF should change the render surface size, but won't affect
  // which part of the mask is used.
  device_scale_factor = 2.f;
  gfx::Size device_viewport =
      gfx::ToFlooredSize(gfx::ScaleSize(root_size, device_scale_factor));
  host_impl_->SetViewportSize(root_size, device_viewport);
  host_impl_->SetDeviceScaleFactor(device_scale_factor);
  host_impl_->active_tree()->set_needs_update_draw_properties(); {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

    ASSERT_EQ(1u, frame.render_passes.size());
    ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* render_pass_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
              render_pass_quad->rect.ToString());
    EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(),
              render_pass_quad->mask_uv_rect.ToString());

    host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
    host_impl_->DidDrawAllLayers(frame);
  }


  // Applying an equivalent content scale on the content layer and the mask
  // should still result in the same part of the mask being used.
  gfx::Size layer_size_large =
      gfx::ToRoundedSize(gfx::ScaleSize(layer_size, device_scale_factor));
  content_layer->SetContentBounds(layer_size_large);
  content_layer->SetContentsScale(device_scale_factor, device_scale_factor);
  gfx::Size mask_size_large =
      gfx::ToRoundedSize(gfx::ScaleSize(mask_size, device_scale_factor));
  mask_layer->SetContentBounds(mask_size_large);
  mask_layer->SetContentsScale(device_scale_factor, device_scale_factor);
  host_impl_->active_tree()->set_needs_update_draw_properties(); {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

    ASSERT_EQ(1u, frame.render_passes.size());
    ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* render_pass_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
              render_pass_quad->rect.ToString());
    EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(),
              render_pass_quad->mask_uv_rect.ToString());

    host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
    host_impl_->DidDrawAllLayers(frame);
  }

  // Applying a different contents scale to the mask layer will still result
  // in the mask covering the owning layer.
  mask_layer->SetContentBounds(mask_size);
  mask_layer->SetContentsScale(device_scale_factor, device_scale_factor);
  host_impl_->active_tree()->set_needs_update_draw_properties(); {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame));

    ASSERT_EQ(1u, frame.render_passes.size());
    ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* render_pass_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
              render_pass_quad->rect.ToString());
    EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(),
              render_pass_quad->mask_uv_rect.ToString());

    host_impl_->DrawLayers(&frame, base::TimeTicks::Now());
    host_impl_->DidDrawAllLayers(frame);
  }
}

}  // namespace
}  // namespace cc
