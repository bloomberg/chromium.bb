// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_impl.h"

#include <cmath>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "cc/animation/scrollbar_animation_controller_thinning.h"
#include "cc/base/math_util.h"
#include "cc/input/top_controls_manager.h"
#include "cc/layers/delegated_renderer_layer_impl.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/io_surface_layer_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/quad_sink.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/layers/tiled_layer_impl.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/gl_renderer.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/layer_tiling_data.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "media/base/media.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/rect_conversions.h"
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
      : proxy_(),
        always_impl_thread_(&proxy_),
        always_main_thread_blocked_(&proxy_),
        on_can_draw_state_changed_called_(false),
        did_notify_ready_to_activate_(false),
        did_request_commit_(false),
        did_request_redraw_(false),
        did_request_manage_tiles_(false),
        did_upload_visible_tile_(false),
        did_lose_output_surface_(false),
        reduce_memory_result_(true),
        current_limit_bytes_(0),
        current_priority_cutoff_value_(0) {
    media::InitializeMediaLibraryForTesting();
  }

  virtual void SetUp() OVERRIDE {
    LayerTreeSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();
    settings.impl_side_painting = true;
    settings.texture_id_allocation_chunk_size = 1;

    host_impl_ = LayerTreeHostImpl::Create(
        settings, this, &proxy_, &stats_instrumentation_, NULL);
    host_impl_->InitializeRenderer(CreateOutputSurface());
    host_impl_->SetViewportSize(gfx::Size(10, 10));
  }

  virtual void TearDown() OVERRIDE {}

  virtual void DidLoseOutputSurfaceOnImplThread() OVERRIDE {
    did_lose_output_surface_ = true;
  }
  virtual void OnSwapBuffersCompleteOnImplThread() OVERRIDE {}
  virtual void BeginImplFrame(const BeginFrameArgs& args) OVERRIDE {}
  virtual void OnCanDrawStateChanged(bool can_draw) OVERRIDE {
    on_can_draw_state_changed_called_ = true;
  }
  virtual void NotifyReadyToActivate() OVERRIDE {
    did_notify_ready_to_activate_ = true;
    host_impl_->ActivatePendingTree();
  }
  virtual void SetNeedsRedrawOnImplThread() OVERRIDE {
    did_request_redraw_ = true;
  }
  virtual void SetNeedsRedrawRectOnImplThread(gfx::Rect damage_rect) OVERRIDE {
    did_request_redraw_ = true;
  }
  virtual void SetNeedsManageTilesOnImplThread() OVERRIDE {
    did_request_manage_tiles_ = true;
  }
  virtual void DidInitializeVisibleTileOnImplThread() OVERRIDE {
    did_upload_visible_tile_ = true;
  }
  virtual void SetNeedsCommitOnImplThread() OVERRIDE {
    did_request_commit_ = true;
  }
  virtual void PostAnimationEventsToMainThreadOnImplThread(
      scoped_ptr<AnimationEventsVector> events,
      base::Time wall_clock_time) OVERRIDE {}
  virtual bool ReduceContentsTextureMemoryOnImplThread(
      size_t limit_bytes, int priority_cutoff) OVERRIDE {
    current_limit_bytes_ = limit_bytes;
    current_priority_cutoff_value_ = priority_cutoff;
    return reduce_memory_result_;
  }
  virtual void SendManagedMemoryStats() OVERRIDE {}
  virtual bool IsInsideDraw() OVERRIDE { return false; }
  virtual void RenewTreePriority() OVERRIDE {}
  virtual void RequestScrollbarAnimationOnImplThread(base::TimeDelta delay)
      OVERRIDE { requested_scrollbar_animation_delay_ = delay; }
  virtual void DidActivatePendingTree() OVERRIDE {}

  void set_reduce_memory_result(bool reduce_memory_result) {
    reduce_memory_result_ = reduce_memory_result;
  }

  void CreateLayerTreeHost(bool partial_swap,
                           scoped_ptr<OutputSurface> output_surface) {
    LayerTreeSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();
    settings.partial_swap_enabled = partial_swap;

    host_impl_ = LayerTreeHostImpl::Create(
        settings, this, &proxy_, &stats_instrumentation_, NULL);

    host_impl_->InitializeRenderer(output_surface.Pass());
    host_impl_->SetViewportSize(gfx::Size(10, 10));
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
    ASSERT_EQ(layer->ScrollDelta(), gfx::Vector2d());
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

    ASSERT_EQ(1, times_encountered);
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

  LayerImpl* CreateScrollAndContentsLayers(LayerTreeImpl* layer_tree_impl,
                                           gfx::Size content_size) {
    scoped_ptr<LayerImpl> root =
        LayerImpl::Create(layer_tree_impl, 1);
    root->SetBounds(content_size);
    root->SetContentBounds(content_size);
    root->SetPosition(gfx::PointF());
    root->SetAnchorPoint(gfx::PointF());

    scoped_ptr<LayerImpl> scroll =
        LayerImpl::Create(layer_tree_impl, 2);
    LayerImpl* scroll_layer = scroll.get();
    scroll->SetScrollable(true);
    scroll->SetScrollOffset(gfx::Vector2d());
    scroll->SetMaxScrollOffset(gfx::Vector2d(content_size.width(),
                                             content_size.height()));
    scroll->SetBounds(content_size);
    scroll->SetContentBounds(content_size);
    scroll->SetPosition(gfx::PointF());
    scroll->SetAnchorPoint(gfx::PointF());

    scoped_ptr<LayerImpl> contents =
        LayerImpl::Create(layer_tree_impl, 3);
    contents->SetDrawsContent(true);
    contents->SetBounds(content_size);
    contents->SetContentBounds(content_size);
    contents->SetPosition(gfx::PointF());
    contents->SetAnchorPoint(gfx::PointF());

    scroll->AddChild(contents.Pass());
    root->AddChild(scroll.Pass());

    layer_tree_impl->SetRootLayer(root.Pass());
    return scroll_layer;
  }

  LayerImpl* SetupScrollAndContentsLayers(gfx::Size content_size) {
    LayerImpl* scroll_layer = CreateScrollAndContentsLayers(
        host_impl_->active_tree(), content_size);
    host_impl_->active_tree()->DidBecomeActive();
    return scroll_layer;
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
    DrawFrame();
  }

  void DrawFrame() {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);
  }

  void pinch_zoom_pan_viewport_forces_commit_redraw(float device_scale_factor);
  void pinch_zoom_pan_viewport_test(float device_scale_factor);
  void pinch_zoom_pan_viewport_and_scroll_test(float device_scale_factor);
  void pinch_zoom_pan_viewport_and_scroll_boundary_test(
      float device_scale_factor);

  void CheckNotifyCalledIfCanDrawChanged(bool always_draw) {
    // Note: It is not possible to disable the renderer once it has been set,
    // so we do not need to test that disabling the renderer notifies us
    // that can_draw changed.
    EXPECT_FALSE(host_impl_->CanDraw());
    on_can_draw_state_changed_called_ = false;

    // Set up the root layer, which allows us to draw.
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
    host_impl_->SetViewportSize(gfx::Size());
    if (always_draw) {
      EXPECT_TRUE(host_impl_->CanDraw());
    } else {
      EXPECT_FALSE(host_impl_->CanDraw());
    }
    EXPECT_TRUE(on_can_draw_state_changed_called_);
    on_can_draw_state_changed_called_ = false;

    host_impl_->SetViewportSize(gfx::Size(100, 100));
    EXPECT_TRUE(host_impl_->CanDraw());
    EXPECT_TRUE(on_can_draw_state_changed_called_);
    on_can_draw_state_changed_called_ = false;

    // Toggle contents textures purged without causing any evictions,
    // and make sure that it does not change can_draw.
    set_reduce_memory_result(false);
    host_impl_->SetMemoryPolicy(ManagedMemoryPolicy(
        host_impl_->memory_allocation_limit_bytes() - 1));
    EXPECT_TRUE(host_impl_->CanDraw());
    EXPECT_FALSE(on_can_draw_state_changed_called_);
    on_can_draw_state_changed_called_ = false;

    // Toggle contents textures purged to make sure it toggles can_draw.
    set_reduce_memory_result(true);
    host_impl_->SetMemoryPolicy(ManagedMemoryPolicy(
        host_impl_->memory_allocation_limit_bytes() - 1));
    if (always_draw) {
      EXPECT_TRUE(host_impl_->CanDraw());
    } else {
      EXPECT_FALSE(host_impl_->CanDraw());
    }
    EXPECT_TRUE(on_can_draw_state_changed_called_);
    on_can_draw_state_changed_called_ = false;

    host_impl_->active_tree()->ResetContentsTexturesPurged();
    EXPECT_TRUE(host_impl_->CanDraw());
    EXPECT_TRUE(on_can_draw_state_changed_called_);
    on_can_draw_state_changed_called_ = false;
  }

  void SetupMouseMoveAtWithDeviceScale(float device_scale_factor);

 protected:
  virtual scoped_ptr<OutputSurface> CreateOutputSurface() {
    return CreateFakeOutputSurface();
  }

  void DrawOneFrame() {
    LayerTreeHostImpl::FrameData frame_data;
    host_impl_->PrepareToDraw(&frame_data, gfx::Rect());
    host_impl_->DidDrawAllLayers(frame_data);
  }

  FakeProxy proxy_;
  DebugScopedSetImplThread always_impl_thread_;
  DebugScopedSetMainThreadBlocked always_main_thread_blocked_;

  scoped_ptr<LayerTreeHostImpl> host_impl_;
  FakeRenderingStatsInstrumentation stats_instrumentation_;
  bool on_can_draw_state_changed_called_;
  bool did_notify_ready_to_activate_;
  bool did_request_commit_;
  bool did_request_redraw_;
  bool did_request_manage_tiles_;
  bool did_upload_visible_tile_;
  bool did_lose_output_surface_;
  bool reduce_memory_result_;
  base::TimeDelta requested_scrollbar_animation_delay_;
  size_t current_limit_bytes_;
  int current_priority_cutoff_value_;
};

TEST_F(LayerTreeHostImplTest, NotifyIfCanDrawChanged) {
  bool always_draw = false;
  CheckNotifyCalledIfCanDrawChanged(always_draw);
}

TEST_F(LayerTreeHostImplTest, CanDrawIncompleteFrames) {
  LayerTreeSettings settings;
  settings.impl_side_painting = true;
  host_impl_ = LayerTreeHostImpl::Create(
      settings, this, &proxy_, &stats_instrumentation_, NULL);

  scoped_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::CreateAlwaysDrawAndSwap3d());

  host_impl_->InitializeRenderer(
      output_surface.PassAs<OutputSurface>());
  host_impl_->SetViewportSize(gfx::Size(10, 10));

  bool always_draw = true;
  CheckNotifyCalledIfCanDrawChanged(always_draw);
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
    root->SetMaxScrollOffset(gfx::Vector2d(100, 100));
    root->SetScrollOffset(scroll_offset);
    root->SetScrollable(true);
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
  host_impl_->SetViewportSize(gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
  host_impl_->ScrollEnd();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollWithoutRootLayer) {
  // We should not crash when trying to scroll an empty layer tree.
  EXPECT_EQ(InputHandler::ScrollIgnored,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));
}

TEST_F(LayerTreeHostImplTest, ScrollWithoutRenderer) {
  LayerTreeSettings settings;
  host_impl_ = LayerTreeHostImpl::Create(
      settings, this, &proxy_, &stats_instrumentation_, NULL);
  scoped_ptr<TestWebGraphicsContext3D> context_owned =
      TestWebGraphicsContext3D::Create();
  context_owned->set_times_make_current_succeeds(0);

  scoped_ptr<FakeOutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.Pass()));

  // Initialization will fail here.
  host_impl_->InitializeRenderer(output_surface.PassAs<OutputSurface>());
  host_impl_->SetViewportSize(gfx::Size(10, 10));

  SetupScrollAndContentsLayers(gfx::Size(100, 100));

  // We should not crash when trying to scroll after the renderer initialization
  // fails.
  EXPECT_EQ(InputHandler::ScrollIgnored,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));
}

TEST_F(LayerTreeHostImplTest, ReplaceTreeWhileScrolling) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  // We should not crash if the tree is replaced while we are scrolling.
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));
  host_impl_->active_tree()->DetachLayerTree();

  scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));

  // We should still be scrolling, because the scrolled layer also exists in the
  // new tree.
  gfx::Vector2d scroll_delta(0, 10);
  host_impl_->ScrollBy(gfx::Point(), scroll_delta);
  host_impl_->ScrollEnd();
  scoped_ptr<ScrollAndScaleSet> scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info, scroll_layer->id(), scroll_delta);
}

TEST_F(LayerTreeHostImplTest, ClearRootRenderSurfaceAndScroll) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  // We should be able to scroll even if the root layer loses its render surface
  // after the most recent render.
  host_impl_->active_tree()->root_layer()->ClearRenderSurface();
  host_impl_->active_tree()->set_needs_update_draw_properties();

  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));
}

TEST_F(LayerTreeHostImplTest, WheelEventHandlers) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();
  LayerImpl* root = host_impl_->active_tree()->root_layer();

  root->SetHaveWheelEventHandlers(true);

  // With registered event handlers, wheel scrolls have to go to the main
  // thread.
  EXPECT_EQ(InputHandler::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));

  // But gesture scrolls can still be handled.
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Gesture));
}

TEST_F(LayerTreeHostImplTest, FlingOnlyWhenScrollingTouchscreen) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  // Ignore the fling since no layer is being scrolled
  EXPECT_EQ(InputHandler::ScrollIgnored,
            host_impl_->FlingScrollBegin());

  // Start scrolling a layer
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Gesture));

  // Now the fling should go ahead since we've started scrolling a layer
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->FlingScrollBegin());
}

TEST_F(LayerTreeHostImplTest, FlingOnlyWhenScrollingTouchpad) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  // Ignore the fling since no layer is being scrolled
  EXPECT_EQ(InputHandler::ScrollIgnored,
            host_impl_->FlingScrollBegin());

  // Start scrolling a layer
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));

  // Now the fling should go ahead since we've started scrolling a layer
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->FlingScrollBegin());
}

TEST_F(LayerTreeHostImplTest, NoFlingWhenScrollingOnMain) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();
  LayerImpl* root = host_impl_->active_tree()->root_layer();

  root->SetShouldScrollOnMainThread(true);

  // Start scrolling a layer
  EXPECT_EQ(InputHandler::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Gesture));

  // The fling should be ignored since there's no layer being scrolled impl-side
  EXPECT_EQ(InputHandler::ScrollIgnored,
            host_impl_->FlingScrollBegin());
}

TEST_F(LayerTreeHostImplTest, ShouldScrollOnMainThread) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();
  LayerImpl* root = host_impl_->active_tree()->root_layer();

  root->SetShouldScrollOnMainThread(true);

  EXPECT_EQ(InputHandler::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));
  EXPECT_EQ(InputHandler::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Gesture));
}

TEST_F(LayerTreeHostImplTest, NonFastScrollableRegionBasic) {
  SetupScrollAndContentsLayers(gfx::Size(200, 200));
  host_impl_->SetViewportSize(gfx::Size(100, 100));

  LayerImpl* root = host_impl_->active_tree()->root_layer();
  root->SetContentsScale(2.f, 2.f);
  root->SetNonFastScrollableRegion(gfx::Rect(0, 0, 50, 50));

  InitializeRendererAndDrawFrame();

  // All scroll types inside the non-fast scrollable region should fail.
  EXPECT_EQ(InputHandler::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(25, 25),
                                    InputHandler::Wheel));
  EXPECT_EQ(InputHandler::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(25, 25),
                                    InputHandler::Gesture));

  // All scroll types outside this region should succeed.
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(75, 75),
                                    InputHandler::Wheel));
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
  host_impl_->ScrollEnd();
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(75, 75),
                                    InputHandler::Gesture));
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
  host_impl_->ScrollEnd();
}

TEST_F(LayerTreeHostImplTest, NonFastScrollableRegionWithOffset) {
  SetupScrollAndContentsLayers(gfx::Size(200, 200));
  host_impl_->SetViewportSize(gfx::Size(100, 100));

  LayerImpl* root = host_impl_->active_tree()->root_layer();
  root->SetContentsScale(2.f, 2.f);
  root->SetNonFastScrollableRegion(gfx::Rect(0, 0, 50, 50));
  root->SetPosition(gfx::PointF(-25.f, 0.f));

  InitializeRendererAndDrawFrame();

  // This point would fall into the non-fast scrollable region except that we've
  // moved the layer down by 25 pixels.
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(40, 10),
                                    InputHandler::Wheel));
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 1));
  host_impl_->ScrollEnd();

  // This point is still inside the non-fast region.
  EXPECT_EQ(InputHandler::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(10, 10),
                                    InputHandler::Wheel));
}

TEST_F(LayerTreeHostImplTest, ScrollByReturnsCorrectValue) {
  SetupScrollAndContentsLayers(gfx::Size(200, 200));
  host_impl_->SetViewportSize(gfx::Size(100, 100));

  InitializeRendererAndDrawFrame();

  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Gesture));

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
  host_impl_->SetViewportSize(gfx::Size(100, 1000));

  InitializeRendererAndDrawFrame();

  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(),
                                    InputHandler::Wheel));

  // Trying to scroll without a vertical scrollbar will fail.
  EXPECT_FALSE(host_impl_->ScrollVerticallyByPage(
      gfx::Point(), SCROLL_FORWARD));
  EXPECT_FALSE(host_impl_->ScrollVerticallyByPage(
      gfx::Point(), SCROLL_BACKWARD));

  scoped_ptr<cc::PaintedScrollbarLayerImpl> vertical_scrollbar(
      cc::PaintedScrollbarLayerImpl::Create(
          host_impl_->active_tree(),
          20,
          VERTICAL));
  vertical_scrollbar->SetBounds(gfx::Size(15, 1000));
  host_impl_->RootScrollLayer()->SetVerticalScrollbarLayer(
      vertical_scrollbar.get());

  // Trying to scroll with a vertical scrollbar will succeed.
  EXPECT_TRUE(host_impl_->ScrollVerticallyByPage(
      gfx::Point(), SCROLL_FORWARD));
  EXPECT_FLOAT_EQ(875.f, host_impl_->RootScrollLayer()->ScrollDelta().y());
  EXPECT_TRUE(host_impl_->ScrollVerticallyByPage(
      gfx::Point(), SCROLL_BACKWARD));
}

TEST_F(LayerTreeHostImplTest, ScrollWithUserUnscrollableLayers) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(200, 200));
  host_impl_->SetViewportSize(gfx::Size(100, 100));

  gfx::Size overflow_size(400, 400);
  ASSERT_EQ(1u, scroll_layer->children().size());
  LayerImpl* overflow = scroll_layer->children()[0];
  overflow->SetBounds(overflow_size);
  overflow->SetContentBounds(overflow_size);
  overflow->SetScrollable(true);
  overflow->SetMaxScrollOffset(gfx::Vector2d(overflow_size.width(),
                                             overflow_size.height()));
  overflow->SetScrollOffset(gfx::Vector2d());
  overflow->SetPosition(gfx::PointF());
  overflow->SetAnchorPoint(gfx::PointF());

  InitializeRendererAndDrawFrame();
  gfx::Point scroll_position(10, 10);

  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(scroll_position, InputHandler::Wheel));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), scroll_layer->TotalScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), overflow->TotalScrollOffset());

  gfx::Vector2dF scroll_delta(10, 10);
  host_impl_->ScrollBy(scroll_position, scroll_delta);
  host_impl_->ScrollEnd();
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), scroll_layer->TotalScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 10), overflow->TotalScrollOffset());

  overflow->set_user_scrollable_horizontal(false);

  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(scroll_position, InputHandler::Wheel));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), scroll_layer->TotalScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 10), overflow->TotalScrollOffset());

  host_impl_->ScrollBy(scroll_position, scroll_delta);
  host_impl_->ScrollEnd();
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 0), scroll_layer->TotalScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), overflow->TotalScrollOffset());

  overflow->set_user_scrollable_vertical(false);

  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(scroll_position, InputHandler::Wheel));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 0), scroll_layer->TotalScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), overflow->TotalScrollOffset());

  host_impl_->ScrollBy(scroll_position, scroll_delta);
  host_impl_->ScrollEnd();
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 10), scroll_layer->TotalScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), overflow->TotalScrollOffset());
}

TEST_F(LayerTreeHostImplTest,
       ClearRootRenderSurfaceAndHitTestTouchHandlerRegion) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  // We should be able to hit test for touch event handlers even if the root
  // layer loses its render surface after the most recent render.
  host_impl_->active_tree()->root_layer()->ClearRenderSurface();
  host_impl_->active_tree()->set_needs_update_draw_properties();

  EXPECT_EQ(host_impl_->HaveTouchEventHandlersAt(gfx::Point()), false);
}

TEST_F(LayerTreeHostImplTest, ImplPinchZoom) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  EXPECT_EQ(scroll_layer, host_impl_->RootScrollLayer());

  float min_page_scale = 1.f, max_page_scale = 4.f;

  // The impl-based pinch zoom should adjust the max scroll position.
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    host_impl_->active_tree()->SetPageScaleDelta(1.f);
    scroll_layer->SetScrollDelta(gfx::Vector2d());

    float page_scale_delta = 2.f;
    host_impl_->ScrollBegin(gfx::Point(50, 50), InputHandler::Gesture);
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd();
    host_impl_->ScrollEnd();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_commit_);

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);

    EXPECT_EQ(gfx::Vector2d(75, 75).ToString(),
              scroll_layer->max_scroll_offset().ToString());
  }

  // Scrolling after a pinch gesture should always be in local space.  The
  // scroll deltas do not have the page scale factor applied.
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    host_impl_->active_tree()->SetPageScaleDelta(1.f);
    scroll_layer->SetScrollDelta(gfx::Vector2d());

    float page_scale_delta = 2.f;
    host_impl_->ScrollBegin(gfx::Point(), InputHandler::Gesture);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point());
    host_impl_->PinchGestureEnd();
    host_impl_->ScrollEnd();

    gfx::Vector2d scroll_delta(0, 10);
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(5, 5),
                                      InputHandler::Wheel));
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    host_impl_->ScrollEnd();

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    ExpectContains(*scroll_info.get(),
                   scroll_layer->id(),
                   scroll_delta);
  }
}

TEST_F(LayerTreeHostImplTest, PinchGesture) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
  InitializeRendererAndDrawFrame();

  LayerImpl* scroll_layer = host_impl_->RootScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 1.f;
  float max_page_scale = 4.f;

  // Basic pinch zoom in gesture
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    scroll_layer->SetScrollDelta(gfx::Vector2d());

    float page_scale_delta = 2.f;
    host_impl_->ScrollBegin(gfx::Point(50, 50), InputHandler::Gesture);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd();
    host_impl_->ScrollEnd();
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
    scroll_layer->SetScrollDelta(gfx::Vector2d());
    float page_scale_delta = 10.f;

    host_impl_->ScrollBegin(gfx::Point(50, 50), InputHandler::Gesture);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd();
    host_impl_->ScrollEnd();

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, max_page_scale);
  }

  // Zoom-out clamping
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    scroll_layer->SetScrollDelta(gfx::Vector2d());
    scroll_layer->SetScrollOffset(gfx::Vector2d(50, 50));

    float page_scale_delta = 0.1f;
    host_impl_->ScrollBegin(gfx::Point(), InputHandler::Gesture);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point());
    host_impl_->PinchGestureEnd();
    host_impl_->ScrollEnd();

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
    scroll_layer->SetScrollDelta(gfx::Vector2d());
    scroll_layer->SetScrollOffset(gfx::Vector2d(20, 20));

    float page_scale_delta = 1.f;
    host_impl_->ScrollBegin(gfx::Point(10, 10), InputHandler::Gesture);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(10, 10));
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(20, 20));
    host_impl_->PinchGestureEnd();
    host_impl_->ScrollEnd();

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
    scroll_layer->SetScrollDelta(gfx::Vector2d());
    scroll_layer->SetScrollOffset(gfx::Vector2d(20, 20));

    float page_scale_delta = 1.f;
    host_impl_->ScrollBegin(gfx::Point(10, 10), InputHandler::Gesture);
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

  // Two-finger panning should work when starting fully zoomed out.
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(0.5f,
                                                           0.5f,
                                                           4.f);
    scroll_layer->SetScrollDelta(gfx::Vector2d());
    scroll_layer->SetScrollOffset(gfx::Vector2d(0, 0));
    host_impl_->active_tree()->UpdateMaxScrollOffset();

    host_impl_->ScrollBegin(gfx::Point(0, 0), InputHandler::Gesture);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(2.f, gfx::Point(0, 0));
    host_impl_->PinchGestureUpdate(1.f, gfx::Point(0, 0));
    host_impl_->ScrollBy(gfx::Point(0, 0), gfx::Vector2d(10, 10));
    host_impl_->PinchGestureUpdate(1.f, gfx::Point(10, 10));
    host_impl_->PinchGestureEnd();
    host_impl_->ScrollEnd();

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, 2.f);
    ExpectContains(*scroll_info, scroll_layer->id(), gfx::Vector2d(20, 20));
  }
}

TEST_F(LayerTreeHostImplTest, PageScaleAnimation) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
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

  // Non-anchor zoom-in
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    scroll_layer->SetScrollOffset(gfx::Vector2d(50, 50));

    host_impl_->StartPageScaleAnimation(gfx::Vector2d(), false, 2.f, duration);
    did_request_redraw_ = false;
    host_impl_->Animate(start_time, base::Time());
    EXPECT_TRUE(did_request_redraw_);

    did_request_redraw_ = false;
    host_impl_->Animate(halfway_through_animation, base::Time());
    EXPECT_TRUE(did_request_redraw_);

    did_request_redraw_ = false;
    did_request_commit_ = false;
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
    scroll_layer->SetScrollOffset(gfx::Vector2d(50, 50));

    host_impl_->StartPageScaleAnimation(
        gfx::Vector2d(25, 25), true, min_page_scale, duration);
    did_request_redraw_ = false;
    host_impl_->Animate(start_time, base::Time());
    EXPECT_TRUE(did_request_redraw_);

    did_request_redraw_ = false;
    did_request_commit_ = false;
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
  host_impl_->SetViewportSize(gfx::Size(50, 50));
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

  // Anchor zoom with unchanged page scale should not change scroll or scale.
  {
    host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f,
                                                           min_page_scale,
                                                           max_page_scale);
    scroll_layer->SetScrollOffset(gfx::Vector2d(50, 50));

    host_impl_->StartPageScaleAnimation(gfx::Vector2d(), true, 1.f, duration);
    host_impl_->Animate(start_time, base::Time());
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

class LayerTreeHostImplOverridePhysicalTime : public LayerTreeHostImpl {
 public:
  LayerTreeHostImplOverridePhysicalTime(
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* client,
      Proxy* proxy,
      RenderingStatsInstrumentation* rendering_stats_instrumentation)
      : LayerTreeHostImpl(settings,
                          client,
                          proxy,
                          rendering_stats_instrumentation,
                          NULL) {}

  virtual base::TimeTicks CurrentPhysicalTimeTicks() const OVERRIDE {
    return fake_current_physical_time_;
  }

  void SetCurrentPhysicalTimeTicksForTest(base::TimeTicks fake_now) {
    fake_current_physical_time_ = fake_now;
  }

 private:
  base::TimeTicks fake_current_physical_time_;
};

TEST_F(LayerTreeHostImplTest, ScrollbarLinearFadeScheduling) {
  LayerTreeSettings settings;
  settings.scrollbar_animator = LayerTreeSettings::LinearFade;
  settings.scrollbar_linear_fade_delay_ms = 20;
  settings.scrollbar_linear_fade_length_ms = 20;

  gfx::Size viewport_size(10, 10);
  gfx::Size content_size(100, 100);

  LayerTreeHostImplOverridePhysicalTime* host_impl_override_time =
      new LayerTreeHostImplOverridePhysicalTime(
          settings, this, &proxy_, &stats_instrumentation_);
  host_impl_ = make_scoped_ptr<LayerTreeHostImpl>(host_impl_override_time);
  host_impl_->InitializeRenderer(CreateOutputSurface());
  host_impl_->SetViewportSize(viewport_size);

  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  root->SetBounds(viewport_size);

  scoped_ptr<LayerImpl> scroll =
      LayerImpl::Create(host_impl_->active_tree(), 2);
  scroll->SetScrollable(true);
  scroll->SetScrollOffset(gfx::Vector2d());
  scroll->SetMaxScrollOffset(gfx::Vector2d(content_size.width(),
                                           content_size.height()));
  scroll->SetBounds(content_size);
  scroll->SetContentBounds(content_size);

  scoped_ptr<LayerImpl> contents =
      LayerImpl::Create(host_impl_->active_tree(), 3);
  contents->SetDrawsContent(true);
  contents->SetBounds(content_size);
  contents->SetContentBounds(content_size);

  scoped_ptr<PaintedScrollbarLayerImpl> scrollbar =
      PaintedScrollbarLayerImpl::Create(host_impl_->active_tree(), 4, VERTICAL);
  scroll->SetVerticalScrollbarLayer(scrollbar.get());

  scroll->AddChild(contents.Pass());
  root->AddChild(scroll.Pass());
  root->AddChild(scrollbar.PassAs<LayerImpl>());

  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  InitializeRendererAndDrawFrame();

  base::TimeTicks fake_now = gfx::FrameTime::Now();
  host_impl_override_time->SetCurrentPhysicalTimeTicksForTest(fake_now);

  // If no scroll happened recently, StartScrollbarAnimation should have no
  // effect.
  host_impl_->StartScrollbarAnimation();
  EXPECT_EQ(base::TimeDelta(), requested_scrollbar_animation_delay_);
  EXPECT_FALSE(did_request_redraw_);

  // If no scroll happened during a scroll gesture, StartScrollbarAnimation
  // should have no effect.
  host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel);
  host_impl_->ScrollEnd();
  host_impl_->StartScrollbarAnimation();
  EXPECT_EQ(base::TimeDelta(), requested_scrollbar_animation_delay_);
  EXPECT_FALSE(did_request_redraw_);

  // After a scroll, a fade animation should be scheduled about 20ms from now.
  host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel);
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2dF(5, 0));
  host_impl_->ScrollEnd();
  did_request_redraw_ = false;
  host_impl_->StartScrollbarAnimation();
  EXPECT_LT(base::TimeDelta::FromMilliseconds(19),
            requested_scrollbar_animation_delay_);
  EXPECT_FALSE(did_request_redraw_);
  requested_scrollbar_animation_delay_ = base::TimeDelta();

  // After the fade begins, we should start getting redraws instead of a
  // scheduled animation.
  fake_now += base::TimeDelta::FromMilliseconds(25);
  host_impl_override_time->SetCurrentPhysicalTimeTicksForTest(fake_now);
  host_impl_->StartScrollbarAnimation();
  EXPECT_EQ(base::TimeDelta(), requested_scrollbar_animation_delay_);
  EXPECT_TRUE(did_request_redraw_);
  did_request_redraw_ = false;

  // If no scroll happened recently, StartScrollbarAnimation should have no
  // effect.
  fake_now += base::TimeDelta::FromMilliseconds(25);
  host_impl_override_time->SetCurrentPhysicalTimeTicksForTest(fake_now);
  host_impl_->StartScrollbarAnimation();
  EXPECT_EQ(base::TimeDelta(), requested_scrollbar_animation_delay_);
  EXPECT_FALSE(did_request_redraw_);

  // Setting the scroll offset outside a scroll should also cause the scrollbar
  // to appear and to schedule a fade.
  host_impl_->RootScrollLayer()->SetScrollOffset(gfx::Vector2d(5, 5));
  host_impl_->StartScrollbarAnimation();
  EXPECT_LT(base::TimeDelta::FromMilliseconds(19),
            requested_scrollbar_animation_delay_);
  EXPECT_FALSE(did_request_redraw_);
  requested_scrollbar_animation_delay_ = base::TimeDelta();

  // None of the above should have called CurrentFrameTimeTicks, so if we call
  // it now we should get the current time.
  fake_now += base::TimeDelta::FromMilliseconds(10);
  host_impl_override_time->SetCurrentPhysicalTimeTicksForTest(fake_now);
  EXPECT_EQ(fake_now, host_impl_->CurrentFrameTimeTicks());
}

void LayerTreeHostImplTest::SetupMouseMoveAtWithDeviceScale(
    float device_scale_factor) {
  LayerTreeSettings settings;
  settings.scrollbar_animator = LayerTreeSettings::Thinning;

  gfx::Size viewport_size(300, 200);
  gfx::Size device_viewport_size = gfx::ToFlooredSize(
      gfx::ScaleSize(viewport_size, device_scale_factor));
  gfx::Size content_size(1000, 1000);

  host_impl_ = LayerTreeHostImpl::Create(
      settings, this, &proxy_, &stats_instrumentation_, NULL);
  host_impl_->InitializeRenderer(CreateOutputSurface());
  host_impl_->SetDeviceScaleFactor(device_scale_factor);
  host_impl_->SetViewportSize(device_viewport_size);

  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  root->SetBounds(viewport_size);

  scoped_ptr<LayerImpl> scroll =
      LayerImpl::Create(host_impl_->active_tree(), 2);
  scroll->SetScrollable(true);
  scroll->SetScrollOffset(gfx::Vector2d());
  scroll->SetMaxScrollOffset(gfx::Vector2d(content_size.width(),
                                           content_size.height()));
  scroll->SetBounds(content_size);
  scroll->SetContentBounds(content_size);

  scoped_ptr<LayerImpl> contents =
      LayerImpl::Create(host_impl_->active_tree(), 3);
  contents->SetDrawsContent(true);
  contents->SetBounds(content_size);
  contents->SetContentBounds(content_size);

  // The scrollbar is on the right side.
  scoped_ptr<PaintedScrollbarLayerImpl> scrollbar =
      PaintedScrollbarLayerImpl::Create(host_impl_->active_tree(), 5, VERTICAL);
  scrollbar->SetDrawsContent(true);
  scrollbar->SetBounds(gfx::Size(15, viewport_size.height()));
  scrollbar->SetContentBounds(gfx::Size(15, viewport_size.height()));
  scrollbar->SetPosition(gfx::Point(285, 0));
  scroll->SetVerticalScrollbarLayer(scrollbar.get());

  scroll->AddChild(contents.Pass());
  root->AddChild(scroll.Pass());
  root->AddChild(scrollbar.PassAs<LayerImpl>());

  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  InitializeRendererAndDrawFrame();

  LayerImpl* root_scroll = host_impl_->active_tree()->RootScrollLayer();
  ASSERT_TRUE(root_scroll->scrollbar_animation_controller());
  ScrollbarAnimationControllerThinning* scrollbar_animation_controller =
      static_cast<ScrollbarAnimationControllerThinning*>(
          root_scroll->scrollbar_animation_controller());
  scrollbar_animation_controller->set_mouse_move_distance_for_test(100.f);

  host_impl_->MouseMoveAt(gfx::Point(1, 1));
  EXPECT_FALSE(scrollbar_animation_controller->mouse_is_near_scrollbar());

  host_impl_->MouseMoveAt(gfx::Point(200, 50));
  EXPECT_TRUE(scrollbar_animation_controller->mouse_is_near_scrollbar());

  host_impl_->MouseMoveAt(gfx::Point(184, 100));
  EXPECT_FALSE(scrollbar_animation_controller->mouse_is_near_scrollbar());

  scrollbar_animation_controller->set_mouse_move_distance_for_test(102.f);
  host_impl_->MouseMoveAt(gfx::Point(184, 100));
  EXPECT_TRUE(scrollbar_animation_controller->mouse_is_near_scrollbar());

  did_request_redraw_ = false;
  EXPECT_EQ(0, host_impl_->scroll_layer_id_when_mouse_over_scrollbar());
  host_impl_->MouseMoveAt(gfx::Point(290, 100));
  EXPECT_EQ(2, host_impl_->scroll_layer_id_when_mouse_over_scrollbar());
  host_impl_->MouseMoveAt(gfx::Point(290, 120));
  EXPECT_EQ(2, host_impl_->scroll_layer_id_when_mouse_over_scrollbar());
  host_impl_->MouseMoveAt(gfx::Point(150, 120));
  EXPECT_EQ(0, host_impl_->scroll_layer_id_when_mouse_over_scrollbar());
}

TEST_F(LayerTreeHostImplTest, MouseMoveAtWithDeviceScaleOf1) {
  SetupMouseMoveAtWithDeviceScale(1.f);
}

TEST_F(LayerTreeHostImplTest, MouseMoveAtWithDeviceScaleOf2) {
  SetupMouseMoveAtWithDeviceScale(2.f);
}

TEST_F(LayerTreeHostImplTest, CompositorFrameMetadata) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
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
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));
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
  host_impl_->ScrollBegin(gfx::Point(), InputHandler::Gesture);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2.f, gfx::Point());
  host_impl_->PinchGestureEnd();
  host_impl_->ScrollEnd();
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

  virtual bool WillDraw(DrawMode draw_mode, ResourceProvider* provider)
      OVERRIDE {
    will_draw_called_ = true;
    if (will_draw_returns_false_)
      return false;
    return TiledLayerImpl::WillDraw(draw_mode, provider);
  }

  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE {
    append_quads_called_ = true;
    TiledLayerImpl::AppendQuads(quad_sink, append_quads_data);
  }

  virtual void DidDraw(ResourceProvider* provider) OVERRIDE {
    did_draw_called_ = true;
    TiledLayerImpl::DidDraw(provider);
  }

  bool will_draw_called() const { return will_draw_called_; }
  bool append_quads_called() const { return append_quads_called_; }
  bool did_draw_called() const { return did_draw_called_; }

  void set_will_draw_returns_false() { will_draw_returns_false_ = true; }

  void ClearDidDrawCheck() {
    will_draw_called_ = false;
    append_quads_called_ = false;
    did_draw_called_ = false;
  }

 protected:
  DidDrawCheckLayer(LayerTreeImpl* tree_impl, int id)
      : TiledLayerImpl(tree_impl, id),
        will_draw_returns_false_(false),
        will_draw_called_(false),
        append_quads_called_(false),
        did_draw_called_(false) {
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
  bool will_draw_returns_false_;
  bool will_draw_called_;
  bool append_quads_called_;
  bool did_draw_called_;
};

TEST_F(LayerTreeHostImplTest, WillDrawReturningFalseDoesNotCall) {
  // The root layer is always drawn, so run this test on a child layer that
  // will be masked out by the root layer's bounds.
  host_impl_->active_tree()->SetRootLayer(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 1));
  DidDrawCheckLayer* root = static_cast<DidDrawCheckLayer*>(
      host_impl_->active_tree()->root_layer());

  root->AddChild(DidDrawCheckLayer::Create(host_impl_->active_tree(), 2));
  DidDrawCheckLayer* layer =
      static_cast<DidDrawCheckLayer*>(root->children()[0]);

  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect(10, 10)));
    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);

    EXPECT_TRUE(layer->will_draw_called());
    EXPECT_TRUE(layer->append_quads_called());
    EXPECT_TRUE(layer->did_draw_called());
  }

  {
    LayerTreeHostImpl::FrameData frame;

    layer->set_will_draw_returns_false();
    layer->ClearDidDrawCheck();

    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect(10, 10)));
    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);

    EXPECT_TRUE(layer->will_draw_called());
    EXPECT_FALSE(layer->append_quads_called());
    EXPECT_FALSE(layer->did_draw_called());
  }
}

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

  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_FALSE(layer->will_draw_called());
  EXPECT_FALSE(layer->did_draw_called());

  EXPECT_TRUE(layer->visible_content_rect().IsEmpty());

  // Ensure visible_content_rect for layer is not empty
  layer->SetPosition(gfx::PointF());

  EXPECT_FALSE(layer->will_draw_called());
  EXPECT_FALSE(layer->did_draw_called());

  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_TRUE(layer->will_draw_called());
  EXPECT_TRUE(layer->did_draw_called());

  EXPECT_FALSE(layer->visible_content_rect().IsEmpty());
}

TEST_F(LayerTreeHostImplTest, WillDrawNotCalledOnOccludedLayer) {
  gfx::Size big_size(1000, 1000);
  host_impl_->SetViewportSize(big_size);

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

  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
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
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
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
          resource_provider->CreateResource(gfx::Size(1, 1),
                                            GL_CLAMP_TO_EDGE,
                                            ResourceProvider::TextureUsageAny,
                                            RGBA_8888);
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

  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
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

  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
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

  EXPECT_FALSE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
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

  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}

TEST_F(LayerTreeHostImplTest, ScrollRootIgnored) {
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl_->active_tree(), 1);
  root->SetScrollable(false);
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  InitializeRendererAndDrawFrame();

  // Scroll event is ignored because layer is not scrollable.
  EXPECT_EQ(InputHandler::ScrollIgnored,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollNonScrollableRootWithTopControls) {
  LayerTreeSettings settings;
  settings.calculate_top_controls_position = true;
  settings.top_controls_height = 50;

  host_impl_ = LayerTreeHostImpl::Create(
      settings, this, &proxy_, &stats_instrumentation_, NULL);
  host_impl_->InitializeRenderer(CreateOutputSurface());
  host_impl_->SetViewportSize(gfx::Size(10, 10));

  gfx::Size layer_size(5, 5);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl_->active_tree(), 1);
  root->SetScrollable(true);
  root->SetMaxScrollOffset(gfx::Vector2d(layer_size.width(),
                                         layer_size.height()));
  root->SetBounds(layer_size);
  root->SetContentBounds(layer_size);
  root->SetPosition(gfx::PointF());
  root->SetAnchorPoint(gfx::PointF());
  root->SetDrawsContent(false);
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->FindRootScrollLayer();
  InitializeRendererAndDrawFrame();

  EXPECT_EQ(InputHandler::ScrollIgnored,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Gesture));

  host_impl_->top_controls_manager()->ScrollBegin();
  host_impl_->top_controls_manager()->ScrollBy(gfx::Vector2dF(0.f, 50.f));
  host_impl_->top_controls_manager()->ScrollEnd();
  EXPECT_EQ(host_impl_->top_controls_manager()->content_top_offset(), 0.f);

  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(),
                                    InputHandler::Gesture));
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
  host_impl_->SetViewportSize(surface_size);
  InitializeRendererAndDrawFrame();

  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandler::Wheel));
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
  host_impl_->SetViewportSize(surface_size);
  InitializeRendererAndDrawFrame();

  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandler::Wheel));
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
  host_impl_->SetViewportSize(surface_size);
  InitializeRendererAndDrawFrame();

  // Scroll event is ignored because the input coordinate is outside the layer
  // boundaries.
  EXPECT_EQ(InputHandler::ScrollIgnored,
            host_impl_->ScrollBegin(gfx::Point(15, 5),
                                    InputHandler::Wheel));
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollMissesBackfacingChild) {
  gfx::Size surface_size(10, 10);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl_->active_tree(), 1);
  scoped_ptr<LayerImpl> child = CreateScrollableLayer(2, surface_size);
  host_impl_->SetViewportSize(surface_size);

  gfx::Transform matrix;
  matrix.RotateAboutXAxis(180.0);
  child->SetTransform(matrix);
  child->SetDoubleSided(false);

  root->AddChild(child.Pass());
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  InitializeRendererAndDrawFrame();

  // Scroll event is ignored because the scrollable layer is not facing the
  // viewer and there is nothing scrollable behind it.
  EXPECT_EQ(InputHandler::ScrollIgnored,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandler::Wheel));
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
  host_impl_->SetViewportSize(surface_size);
  InitializeRendererAndDrawFrame();

  // Scrolling fails because the content layer is asking to be scrolled on the
  // main thread.
  EXPECT_EQ(InputHandler::ScrollOnMainThread,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandler::Wheel));
}

TEST_F(LayerTreeHostImplTest, ScrollRootAndChangePageScaleOnMainThread) {
  gfx::Size surface_size(10, 10);
  float page_scale = 2.f;
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl_->active_tree(), 1);
  scoped_ptr<LayerImpl> root_scrolling = CreateScrollableLayer(2, surface_size);
  root->AddChild(root_scrolling.Pass());
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  host_impl_->SetViewportSize(surface_size);
  InitializeRendererAndDrawFrame();

  LayerImpl* root_scroll = host_impl_->active_tree()->RootScrollLayer();

  gfx::Vector2d scroll_delta(0, 10);
  gfx::Vector2d expected_scroll_delta = scroll_delta;
  gfx::Vector2d expected_max_scroll = root_scroll->max_scroll_offset();
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandler::Wheel));
  host_impl_->ScrollBy(gfx::Point(), scroll_delta);
  host_impl_->ScrollEnd();

  // Set new page scale from main thread.
  host_impl_->active_tree()->SetPageScaleFactorAndLimits(page_scale,
                                                         page_scale,
                                                         page_scale);

  scoped_ptr<ScrollAndScaleSet> scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info.get(), root_scroll->id(), expected_scroll_delta);

  // The scroll range should also have been updated.
  EXPECT_EQ(expected_max_scroll, root_scroll->max_scroll_offset());

  // The page scale delta remains constant because the impl thread did not
  // scale.
  EXPECT_EQ(1.f, host_impl_->active_tree()->page_scale_delta());
}

TEST_F(LayerTreeHostImplTest, ScrollRootAndChangePageScaleOnImplThread) {
  gfx::Size surface_size(10, 10);
  float page_scale = 2.f;
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl_->active_tree(), 1);
  scoped_ptr<LayerImpl> root_scrolling = CreateScrollableLayer(2, surface_size);
  root->AddChild(root_scrolling.Pass());
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  host_impl_->SetViewportSize(surface_size);
  host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f, 1.f, page_scale);
  InitializeRendererAndDrawFrame();

  LayerImpl* root_scroll = host_impl_->active_tree()->RootScrollLayer();

  gfx::Vector2d scroll_delta(0, 10);
  gfx::Vector2d expected_scroll_delta = scroll_delta;
  gfx::Vector2d expected_max_scroll = root_scroll->max_scroll_offset();
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandler::Wheel));
  host_impl_->ScrollBy(gfx::Point(), scroll_delta);
  host_impl_->ScrollEnd();

  // Set new page scale on impl thread by pinching.
  host_impl_->ScrollBegin(gfx::Point(), InputHandler::Gesture);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(page_scale, gfx::Point());
  host_impl_->PinchGestureEnd();
  host_impl_->ScrollEnd();
  DrawOneFrame();

  // The scroll delta is not scaled because the main thread did not scale.
  scoped_ptr<ScrollAndScaleSet> scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info.get(), root_scroll->id(), expected_scroll_delta);

  // The scroll range should also have been updated.
  EXPECT_EQ(expected_max_scroll, root_scroll->max_scroll_offset());

  // The page scale delta should match the new scale on the impl side.
  EXPECT_EQ(page_scale, host_impl_->active_tree()->total_page_scale_factor());
}

TEST_F(LayerTreeHostImplTest, PageScaleDeltaAppliedToRootScrollLayerOnly) {
  gfx::Size surface_size(10, 10);
  float default_page_scale = 1.f;
  gfx::Transform default_page_scale_matrix;
  default_page_scale_matrix.Scale(default_page_scale, default_page_scale);

  float new_page_scale = 2.f;
  gfx::Transform new_page_scale_matrix;
  new_page_scale_matrix.Scale(new_page_scale, new_page_scale);

  // Create a normal scrollable root layer and another scrollable child layer.
  LayerImpl* scroll = SetupScrollAndContentsLayers(surface_size);
  LayerImpl* root = host_impl_->active_tree()->root_layer();
  LayerImpl* child = scroll->children()[0];

  scoped_ptr<LayerImpl> scrollable_child =
      CreateScrollableLayer(4, surface_size);
  child->AddChild(scrollable_child.Pass());
  LayerImpl* grand_child = child->children()[0];

  // Set new page scale on impl thread by pinching.
  host_impl_->ScrollBegin(gfx::Point(), InputHandler::Gesture);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(new_page_scale, gfx::Point());
  host_impl_->PinchGestureEnd();
  host_impl_->ScrollEnd();
  DrawOneFrame();

  EXPECT_EQ(1.f, root->contents_scale_x());
  EXPECT_EQ(1.f, root->contents_scale_y());
  EXPECT_EQ(1.f, scroll->contents_scale_x());
  EXPECT_EQ(1.f, scroll->contents_scale_y());
  EXPECT_EQ(1.f, child->contents_scale_x());
  EXPECT_EQ(1.f, child->contents_scale_y());
  EXPECT_EQ(1.f, grand_child->contents_scale_x());
  EXPECT_EQ(1.f, grand_child->contents_scale_y());

  // Make sure all the layers are drawn with the page scale delta applied, i.e.,
  // the page scale delta on the root layer is applied hierarchically.
  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_EQ(1.f, root->draw_transform().matrix().getDouble(0, 0));
  EXPECT_EQ(1.f, root->draw_transform().matrix().getDouble(1, 1));
  EXPECT_EQ(new_page_scale, scroll->draw_transform().matrix().getDouble(0, 0));
  EXPECT_EQ(new_page_scale, scroll->draw_transform().matrix().getDouble(1, 1));
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
  scoped_ptr<LayerImpl> root_scrolling =
      LayerImpl::Create(host_impl_->active_tree(), 2);
  root_scrolling->SetBounds(surface_size);
  root_scrolling->SetContentBounds(surface_size);
  root_scrolling->SetScrollable(true);
  root->AddChild(root_scrolling.Pass());
  int child_scroll_layer_id = 3;
  scoped_ptr<LayerImpl> child_scrolling =
      CreateScrollableLayer(child_scroll_layer_id, surface_size);
  LayerImpl* child = child_scrolling.get();
  root->AddChild(child_scrolling.Pass());
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  host_impl_->SetViewportSize(surface_size);
  InitializeRendererAndDrawFrame();

  gfx::Vector2d scroll_delta(0, 10);
  gfx::Vector2d expected_scroll_delta(scroll_delta);
  gfx::Vector2d expected_max_scroll(child->max_scroll_offset());
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandler::Wheel));
  host_impl_->ScrollBy(gfx::Point(), scroll_delta);
  host_impl_->ScrollEnd();

  float page_scale = 2.f;
  host_impl_->active_tree()->SetPageScaleFactorAndLimits(page_scale,
                                                         1.f,
                                                         page_scale);

  DrawOneFrame();

  scoped_ptr<ScrollAndScaleSet> scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(
      *scroll_info.get(), child_scroll_layer_id, expected_scroll_delta);

  // The scroll range should not have changed.
  EXPECT_EQ(child->max_scroll_offset(), expected_max_scroll);

  // The page scale delta remains constant because the impl thread did not
  // scale.
  EXPECT_EQ(1.f, host_impl_->active_tree()->page_scale_delta());
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
  host_impl_->SetViewportSize(surface_size);
  InitializeRendererAndDrawFrame();
  {
    gfx::Vector2d scroll_delta(-8, -7);
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(),
                                      InputHandler::Wheel));
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
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl_->active_tree(), 1);
  scoped_ptr<LayerImpl> root_scrolling = CreateScrollableLayer(2, surface_size);

  scoped_ptr<LayerImpl> grand_child = CreateScrollableLayer(4, surface_size);
  grand_child->SetScrollOffset(gfx::Vector2d(0, 2));

  scoped_ptr<LayerImpl> child = CreateScrollableLayer(3, surface_size);
  child->SetScrollOffset(gfx::Vector2d(0, 3));
  child->AddChild(grand_child.Pass());

  root_scrolling->AddChild(child.Pass());
  root->AddChild(root_scrolling.Pass());
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  host_impl_->SetViewportSize(surface_size);
  InitializeRendererAndDrawFrame();
  {
    gfx::Vector2d scroll_delta(0, -10);
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(),
                                      InputHandler::NonBubblingGesture));
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    host_impl_->ScrollEnd();

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();

    // The grand child should have scrolled up to its limit.
    LayerImpl* child =
        host_impl_->active_tree()->root_layer()->children()[0]->children()[0];
    LayerImpl* grand_child = child->children()[0];
    ExpectContains(*scroll_info.get(), grand_child->id(), gfx::Vector2d(0, -2));

    // The child should not have scrolled.
    ExpectNone(*scroll_info.get(), child->id());

    // The next time we scroll we should only scroll the parent.
    scroll_delta = gfx::Vector2d(0, -3);
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(5, 5),
                                      InputHandler::NonBubblingGesture));
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
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(5, 5),
                                      InputHandler::NonBubblingGesture));
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

    scroll_delta = gfx::Vector2d(0, -2);
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(1, 1),
                                      InputHandler::NonBubblingGesture));
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

  host_impl_->SetViewportSize(surface_size);
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  InitializeRendererAndDrawFrame();
  {
    gfx::Vector2d scroll_delta(0, 4);
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(5, 5),
                                      InputHandler::Wheel));
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
  host_impl_->SetViewportSize(surface_size);

  // Draw one frame and then immediately rebuild the layer tree to mimic a tree
  // synchronization.
  InitializeRendererAndDrawFrame();
  host_impl_->active_tree()->DetachLayerTree();
  host_impl_->active_tree()->SetRootLayer(
      CreateScrollableLayer(2, surface_size));
  host_impl_->active_tree()->DidBecomeActive();

  // Scrolling should still work even though we did not draw yet.
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(5, 5),
                                    InputHandler::Wheel));
}

TEST_F(LayerTreeHostImplTest, ScrollAxisAlignedRotatedLayer) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));

  // Rotate the root layer 90 degrees counter-clockwise about its center.
  gfx::Transform rotate_transform;
  rotate_transform.Rotate(-90.0);
  host_impl_->active_tree()->root_layer()->SetTransform(rotate_transform);

  gfx::Size surface_size(50, 50);
  host_impl_->SetViewportSize(surface_size);
  InitializeRendererAndDrawFrame();

  // Scroll to the right in screen coordinates with a gesture.
  gfx::Vector2d gesture_scroll_delta(10, 0);
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(),
                                    InputHandler::Gesture));
  host_impl_->ScrollBy(gfx::Point(), gesture_scroll_delta);
  host_impl_->ScrollEnd();

  // The layer should have scrolled down in its local coordinates.
  scoped_ptr<ScrollAndScaleSet> scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info.get(),
                 scroll_layer->id(),
                 gfx::Vector2d(0, gesture_scroll_delta.x()));

  // Reset and scroll down with the wheel.
  scroll_layer->SetScrollDelta(gfx::Vector2dF());
  gfx::Vector2d wheel_scroll_delta(0, 10);
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(),
                                    InputHandler::Wheel));
  host_impl_->ScrollBy(gfx::Point(), wheel_scroll_delta);
  host_impl_->ScrollEnd();

  // The layer should have scrolled down in its local coordinates.
  scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info.get(),
                 scroll_layer->id(),
                 wheel_scroll_delta);
}

TEST_F(LayerTreeHostImplTest, ScrollNonAxisAlignedRotatedLayer) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  int child_layer_id = 4;
  float child_layer_angle = -20.f;

  // Create a child layer that is rotated to a non-axis-aligned angle.
  scoped_ptr<LayerImpl> child = CreateScrollableLayer(
      child_layer_id,
      scroll_layer->content_bounds());
  gfx::Transform rotate_transform;
  rotate_transform.Translate(-50.0, -50.0);
  rotate_transform.Rotate(child_layer_angle);
  rotate_transform.Translate(50.0, 50.0);
  child->SetTransform(rotate_transform);

  // Only allow vertical scrolling.
  child->SetMaxScrollOffset(gfx::Vector2d(0, child->content_bounds().height()));
  scroll_layer->AddChild(child.Pass());

  gfx::Size surface_size(50, 50);
  host_impl_->SetViewportSize(surface_size);
  InitializeRendererAndDrawFrame();
  {
    // Scroll down in screen coordinates with a gesture.
    gfx::Vector2d gesture_scroll_delta(0, 10);
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(1, 1),
                                      InputHandler::Gesture));
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

    // The root scroll layer should not have scrolled, because the input delta
    // was close to the layer's axis of movement.
    EXPECT_EQ(scroll_info->scrolls.size(), 1u);
  }
  {
    // Now reset and scroll the same amount horizontally.
    scroll_layer->children()[1]->SetScrollDelta(
        gfx::Vector2dF());
    gfx::Vector2d gesture_scroll_delta(10, 0);
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(1, 1),
                                      InputHandler::Gesture));
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

    // The root scroll layer should have scrolled more, since the input scroll
    // delta was mostly orthogonal to the child layer's vertical scroll axis.
    gfx::Vector2d expected_root_scroll_delta(
        gesture_scroll_delta.x() *
            std::pow(std::cos(MathUtil::Deg2Rad(child_layer_angle)), 2),
        0);
    ExpectContains(*scroll_info.get(),
                   scroll_layer->id(),
                   expected_root_scroll_delta);
  }
}

TEST_F(LayerTreeHostImplTest, ScrollScaledLayer) {
  LayerImpl* scroll_layer =
      SetupScrollAndContentsLayers(gfx::Size(100, 100));

  // Scale the layer to twice its normal size.
  int scale = 2;
  gfx::Transform scale_transform;
  scale_transform.Scale(scale, scale);
  scroll_layer->SetTransform(scale_transform);

  gfx::Size surface_size(50, 50);
  host_impl_->SetViewportSize(surface_size);
  InitializeRendererAndDrawFrame();

  // Scroll down in screen coordinates with a gesture.
  gfx::Vector2d scroll_delta(0, 10);
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Gesture));
  host_impl_->ScrollBy(gfx::Point(), scroll_delta);
  host_impl_->ScrollEnd();

  // The layer should have scrolled down in its local coordinates, but half the
  // amount.
  scoped_ptr<ScrollAndScaleSet> scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info.get(),
                 scroll_layer->id(),
                 gfx::Vector2d(0, scroll_delta.y() / scale));

  // Reset and scroll down with the wheel.
  scroll_layer->SetScrollDelta(gfx::Vector2dF());
  gfx::Vector2d wheel_scroll_delta(0, 10);
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));
  host_impl_->ScrollBy(gfx::Point(), wheel_scroll_delta);
  host_impl_->ScrollEnd();

  // The scale should not have been applied to the scroll delta.
  scroll_info = host_impl_->ProcessScrollDeltas();
  ExpectContains(*scroll_info.get(),
                 scroll_layer->id(),
                 wheel_scroll_delta);
}

class TestScrollOffsetDelegate : public LayerScrollOffsetDelegate {
 public:
  TestScrollOffsetDelegate() : page_scale_factor_(0.f) {}

  virtual ~TestScrollOffsetDelegate() {}

  virtual void SetMaxScrollOffset(gfx::Vector2dF max_scroll_offset) OVERRIDE {
    max_scroll_offset_ = max_scroll_offset;
  }

  virtual void SetTotalScrollOffset(gfx::Vector2dF new_value) OVERRIDE {
    last_set_scroll_offset_ = new_value;
  }

  virtual gfx::Vector2dF GetTotalScrollOffset() OVERRIDE {
    return getter_return_value_;
  }

  virtual bool IsExternalFlingActive() const OVERRIDE { return false; }

  virtual void SetTotalPageScaleFactor(float page_scale_factor) OVERRIDE {
    page_scale_factor_ = page_scale_factor;
  }

  virtual void SetScrollableSize(gfx::SizeF scrollable_size) OVERRIDE {
    scrollable_size_ = scrollable_size;
  }

  gfx::Vector2dF last_set_scroll_offset() {
    return last_set_scroll_offset_;
  }

  void set_getter_return_value(gfx::Vector2dF value) {
    getter_return_value_ = value;
  }

  gfx::Vector2dF max_scroll_offset() const {
    return max_scroll_offset_;
  }

  gfx::SizeF scrollable_size() const {
    return scrollable_size_;
  }

  float page_scale_factor() const {
    return page_scale_factor_;
  }

 private:
  gfx::Vector2dF last_set_scroll_offset_;
  gfx::Vector2dF getter_return_value_;
  gfx::Vector2dF max_scroll_offset_;
  gfx::SizeF scrollable_size_;
  float page_scale_factor_;
};

TEST_F(LayerTreeHostImplTest, RootLayerScrollOffsetDelegation) {
  TestScrollOffsetDelegate scroll_delegate;
  host_impl_->SetViewportSize(gfx::Size(10, 20));
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));

  // Setting the delegate results in the current scroll offset being set.
  gfx::Vector2dF initial_scroll_delta(10.f, 10.f);
  scroll_layer->SetScrollOffset(gfx::Vector2d());
  scroll_layer->SetScrollDelta(initial_scroll_delta);
  host_impl_->SetRootLayerScrollOffsetDelegate(&scroll_delegate);
  EXPECT_EQ(initial_scroll_delta.ToString(),
            scroll_delegate.last_set_scroll_offset().ToString());

  // Setting the delegate results in the scrollable_size, max_scroll_offset and
  // page_scale being set.
  EXPECT_EQ(gfx::SizeF(100, 100), scroll_delegate.scrollable_size());
  EXPECT_EQ(gfx::Vector2dF(90, 80), scroll_delegate.max_scroll_offset());
  EXPECT_EQ(1.f, scroll_delegate.page_scale_factor());

  // Updating page scale immediately updates the delegate.
  host_impl_->active_tree()->SetPageScaleFactorAndLimits(2.f, 0.5f, 4.f);
  EXPECT_EQ(2.f, scroll_delegate.page_scale_factor());
  host_impl_->active_tree()->SetPageScaleDelta(1.5f);
  EXPECT_EQ(3.f, scroll_delegate.page_scale_factor());
  host_impl_->active_tree()->SetPageScaleDelta(1.f);
  host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f, 0.5f, 4.f);
  EXPECT_EQ(1.f, scroll_delegate.page_scale_factor());

  // Scrolling should be relative to the offset as returned by the delegate.
  gfx::Vector2dF scroll_delta(0.f, 10.f);
  gfx::Vector2dF current_offset(7.f, 8.f);

  scroll_delegate.set_getter_return_value(current_offset);
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Gesture));

  host_impl_->ScrollBy(gfx::Point(), scroll_delta);
  EXPECT_EQ(current_offset + scroll_delta,
            scroll_delegate.last_set_scroll_offset());

  current_offset = gfx::Vector2dF(42.f, 41.f);
  scroll_delegate.set_getter_return_value(current_offset);
  host_impl_->ScrollBy(gfx::Point(), scroll_delta);
  EXPECT_EQ(current_offset + scroll_delta,
            scroll_delegate.last_set_scroll_offset());
  host_impl_->ScrollEnd();

  // Forces a full tree synchronization and ensures that the scroll delegate
  // sees the correct size of the new tree.
  gfx::Size new_size(42, 24);
  host_impl_->CreatePendingTree();
  CreateScrollAndContentsLayers(host_impl_->pending_tree(), new_size);
  host_impl_->ActivatePendingTree();
  EXPECT_EQ(new_size, scroll_delegate.scrollable_size());

  // Un-setting the delegate should propagate the delegate's current offset to
  // the root scrollable layer.
  current_offset = gfx::Vector2dF(13.f, 12.f);
  scroll_delegate.set_getter_return_value(current_offset);
  host_impl_->SetRootLayerScrollOffsetDelegate(NULL);

  EXPECT_EQ(current_offset.ToString(),
            scroll_layer->TotalScrollOffset().ToString());
}

TEST_F(LayerTreeHostImplTest, OverscrollRoot) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
  host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f, 0.5f, 4.f);
  InitializeRendererAndDrawFrame();
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->current_fling_velocity());

  // In-bounds scrolling does not affect overscroll.
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->current_fling_velocity());

  // Overscroll events are reflected immediately.
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 50));
  EXPECT_EQ(gfx::Vector2dF(0, 10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->current_fling_velocity());

  // In-bounds scrolling resets accumulated overscroll for the scrolled axes.
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, -50));
  EXPECT_EQ(gfx::Vector2dF(0, 0), host_impl_->accumulated_root_overscroll());
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, -10));
  EXPECT_EQ(gfx::Vector2dF(0, -10), host_impl_->accumulated_root_overscroll());
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(10, 0));
  EXPECT_EQ(gfx::Vector2dF(0, -10), host_impl_->accumulated_root_overscroll());
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(-15, 0));
  EXPECT_EQ(gfx::Vector2dF(-5, -10), host_impl_->accumulated_root_overscroll());
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 60));
  EXPECT_EQ(gfx::Vector2dF(-5, 10), host_impl_->accumulated_root_overscroll());
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(10, -60));
  EXPECT_EQ(gfx::Vector2dF(0, -10), host_impl_->accumulated_root_overscroll());

  // Overscroll accumulates within the scope of ScrollBegin/ScrollEnd as long
  // as no scroll occurs.
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, -20));
  EXPECT_EQ(gfx::Vector2dF(0, -30), host_impl_->accumulated_root_overscroll());
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, -20));
  EXPECT_EQ(gfx::Vector2dF(0, -50), host_impl_->accumulated_root_overscroll());
  // Overscroll resets on valid scroll.
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
  EXPECT_EQ(gfx::Vector2dF(0, 0), host_impl_->accumulated_root_overscroll());
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, -20));
  EXPECT_EQ(gfx::Vector2dF(0, -10), host_impl_->accumulated_root_overscroll());
  host_impl_->ScrollEnd();

  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));
  // Fling velocity is reflected immediately.
  host_impl_->NotifyCurrentFlingVelocity(gfx::Vector2dF(10, 0));
  EXPECT_EQ(gfx::Vector2dF(10, 0), host_impl_->current_fling_velocity());
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, -20));
  EXPECT_EQ(gfx::Vector2dF(0, -20), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(gfx::Vector2dF(10, 0), host_impl_->current_fling_velocity());
}


TEST_F(LayerTreeHostImplTest, OverscrollChildWithoutBubbling) {
  // Scroll child layers beyond their maximum scroll range and make sure root
  // overscroll does not accumulate.
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
  host_impl_->SetViewportSize(surface_size);
  InitializeRendererAndDrawFrame();
  {
    gfx::Vector2d scroll_delta(0, -10);
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(),
                                      InputHandler::NonBubblingGesture));
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd();

    LayerImpl* child = host_impl_->active_tree()->root_layer()->children()[0];
    LayerImpl* grand_child = child->children()[0];

    // The next time we scroll we should only scroll the parent, but overscroll
    // should still not reach the root layer.
    scroll_delta = gfx::Vector2d(0, -30);
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(5, 5),
                                      InputHandler::NonBubblingGesture));
    EXPECT_EQ(host_impl_->CurrentlyScrollingLayer(), grand_child);
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    EXPECT_EQ(host_impl_->CurrentlyScrollingLayer(), child);
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd();

    // After scrolling the parent, another scroll on the opposite direction
    // should scroll the child, resetting the fling velocity.
    scroll_delta = gfx::Vector2d(0, 70);
    host_impl_->NotifyCurrentFlingVelocity(gfx::Vector2dF(10, 0));
    EXPECT_EQ(gfx::Vector2dF(10, 0), host_impl_->current_fling_velocity());
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(5, 5),
                                      InputHandler::NonBubblingGesture));
    EXPECT_EQ(host_impl_->CurrentlyScrollingLayer(), grand_child);
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    EXPECT_EQ(host_impl_->CurrentlyScrollingLayer(), grand_child);
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->current_fling_velocity());
    host_impl_->ScrollEnd();
  }
}

TEST_F(LayerTreeHostImplTest, OverscrollChildEventBubbling) {
  // When we try to scroll a non-scrollable child layer, the scroll delta
  // should be applied to one of its ancestors if possible. Overscroll should
  // be reflected only when it has bubbled up to the root scrolling layer.
  gfx::Size surface_size(10, 10);
  gfx::Size content_size(20, 20);
  scoped_ptr<LayerImpl> root = CreateScrollableLayer(1, content_size);
  scoped_ptr<LayerImpl> child = CreateScrollableLayer(2, content_size);

  child->SetScrollable(false);
  root->AddChild(child.Pass());

  host_impl_->SetViewportSize(surface_size);
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  InitializeRendererAndDrawFrame();
  {
    gfx::Vector2d scroll_delta(0, 8);
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(5, 5),
                                      InputHandler::Wheel));
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    EXPECT_EQ(gfx::Vector2dF(0, 6), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    EXPECT_EQ(gfx::Vector2dF(0, 14), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd();
  }
}

TEST_F(LayerTreeHostImplTest, OverscrollAlways) {
  LayerTreeSettings settings;
  settings.always_overscroll = true;
  host_impl_ = LayerTreeHostImpl::Create(
      settings, this, &proxy_, &stats_instrumentation_, NULL);

  SetupScrollAndContentsLayers(gfx::Size(50, 50));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
  host_impl_->active_tree()->SetPageScaleFactorAndLimits(1.f, 0.5f, 4.f);
  InitializeRendererAndDrawFrame();
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->current_fling_velocity());

  // Even though the layer can't scroll the overscroll still happens.
  EXPECT_EQ(InputHandler::ScrollStarted,
            host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));
  host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 10));
  EXPECT_EQ(gfx::Vector2dF(0, 10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->current_fling_velocity());
}

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
            GL_CLAMP_TO_EDGE,
            ResourceProvider::TextureUsageAny,
            RGBA_8888)) {
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
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with translucent content and painting, so drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetExpectation(true, false);
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with translucent opacity, drawn with blending.
  layer1->SetContentsOpaque(true);
  layer1->SetOpacity(0.5f);
  layer1->SetExpectation(true, false);
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with translucent opacity and painting, drawn with blending.
  layer1->SetContentsOpaque(true);
  layer1->SetOpacity(0.5f);
  layer1->SetExpectation(true, false);
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
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
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  layer2->SetContentsOpaque(true);
  layer2->SetOpacity(1.f);
  layer2->SetExpectation(false, false);
  layer2->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Parent layer with translucent content, drawn with blending.
  // Child layer with opaque content, drawn without blending.
  layer1->SetContentsOpaque(false);
  layer1->SetExpectation(true, false);
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  layer2->SetExpectation(false, false);
  layer2->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Parent layer with translucent content but opaque painting, drawn without
  // blending.
  // Child layer with opaque content, drawn without blending.
  layer1->SetContentsOpaque(true);
  layer1->SetExpectation(false, false);
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  layer2->SetExpectation(false, false);
  layer2->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
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
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  layer2->SetExpectation(false, false);
  layer2->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Draw again, but with child non-opaque, to make sure
  // layer1 not culled.
  layer1->SetContentsOpaque(true);
  layer1->SetOpacity(1.f);
  layer1->SetExpectation(false, false);
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  layer2->SetContentsOpaque(true);
  layer2->SetOpacity(0.5f);
  layer2->SetExpectation(true, false);
  layer2->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // A second way of making the child non-opaque.
  layer1->SetContentsOpaque(true);
  layer1->SetOpacity(1.f);
  layer1->SetExpectation(false, false);
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  layer2->SetContentsOpaque(false);
  layer2->SetOpacity(1.f);
  layer2->SetExpectation(true, false);
  layer2->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // And when the layer says its not opaque but is painted opaque, it is not
  // blended.
  layer1->SetContentsOpaque(true);
  layer1->SetOpacity(1.f);
  layer1->SetExpectation(false, false);
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  layer2->SetContentsOpaque(true);
  layer2->SetOpacity(1.f);
  layer2->SetExpectation(false, false);
  layer2->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with partially opaque contents, drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(true, false);
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with partially opaque contents partially culled, drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(5, 5, 5, 2));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(true, false);
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with partially opaque contents culled, drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(7, 5, 3, 5));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(true, false);
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with partially opaque contents and translucent contents culled, drawn
  // without blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(false, false);
  layer1->set_update_rect(gfx::RectF(layer1->content_bounds()));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);
}

class LayerTreeHostImplViewportCoveredTest : public LayerTreeHostImplTest {
 public:
  LayerTreeHostImplViewportCoveredTest() :
      gutter_quad_material_(DrawQuad::SOLID_COLOR),
      child_(NULL),
      did_activate_pending_tree_(false) {}

  void CreateLayerTreeHostImpl(bool always_draw) {
    LayerTreeSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();
    settings.impl_side_painting = true;
    host_impl_ = LayerTreeHostImpl::Create(
        settings, this, &proxy_, &stats_instrumentation_, NULL);

    scoped_ptr<FakeOutputSurface> output_surface;
    if (always_draw)
      output_surface = FakeOutputSurface::CreateAlwaysDrawAndSwap3d().Pass();
    else
      output_surface = FakeOutputSurface::Create3d().Pass();

    host_impl_->InitializeRenderer(output_surface.PassAs<OutputSurface>());
    viewport_size_ = gfx::Size(1000, 1000);
  }

  void SetupActiveTreeLayers() {
    host_impl_->active_tree()->set_background_color(SK_ColorGRAY);
    host_impl_->active_tree()->SetRootLayer(
        LayerImpl::Create(host_impl_->active_tree(), 1));
    host_impl_->active_tree()->root_layer()->AddChild(
        BlendStateCheckLayer::Create(host_impl_->active_tree(),
                                     2,
                                     host_impl_->resource_provider()));
    child_ = static_cast<BlendStateCheckLayer*>(
        host_impl_->active_tree()->root_layer()->children()[0]);
    child_->SetExpectation(false, false);
    child_->SetContentsOpaque(true);
  }

  // Expect no gutter rects.
  void TestLayerCoversFullViewport() {
    gfx::Rect layer_rect(viewport_size_);
    child_->SetPosition(layer_rect.origin());
    child_->SetBounds(layer_rect.size());
    child_->SetContentBounds(layer_rect.size());
    child_->SetQuadRect(gfx::Rect(layer_rect.size()));
    child_->SetQuadVisibleRect(gfx::Rect(layer_rect.size()));

    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
    ASSERT_EQ(1u, frame.render_passes.size());

    EXPECT_EQ(0u, CountGutterQuads(frame.render_passes[0]->quad_list));
    EXPECT_EQ(1u, frame.render_passes[0]->quad_list.size());
    ValidateTextureDrawQuads(frame.render_passes[0]->quad_list);

    VerifyQuadsExactlyCoverViewport(frame.render_passes[0]->quad_list);
    host_impl_->DidDrawAllLayers(frame);
  }

  // Expect fullscreen gutter rect.
  void TestEmptyLayer() {
    gfx::Rect layer_rect(0, 0, 0, 0);
    child_->SetPosition(layer_rect.origin());
    child_->SetBounds(layer_rect.size());
    child_->SetContentBounds(layer_rect.size());
    child_->SetQuadRect(gfx::Rect(layer_rect.size()));
    child_->SetQuadVisibleRect(gfx::Rect(layer_rect.size()));

    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
    ASSERT_EQ(1u, frame.render_passes.size());

    EXPECT_EQ(1u, CountGutterQuads(frame.render_passes[0]->quad_list));
    EXPECT_EQ(1u, frame.render_passes[0]->quad_list.size());
    ValidateTextureDrawQuads(frame.render_passes[0]->quad_list);

    VerifyQuadsExactlyCoverViewport(frame.render_passes[0]->quad_list);
    host_impl_->DidDrawAllLayers(frame);
  }

  // Expect four surrounding gutter rects.
  void TestLayerInMiddleOfViewport() {
    gfx::Rect layer_rect(500, 500, 200, 200);
    child_->SetPosition(layer_rect.origin());
    child_->SetBounds(layer_rect.size());
    child_->SetContentBounds(layer_rect.size());
    child_->SetQuadRect(gfx::Rect(layer_rect.size()));
    child_->SetQuadVisibleRect(gfx::Rect(layer_rect.size()));

    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
    ASSERT_EQ(1u, frame.render_passes.size());

    EXPECT_EQ(4u, CountGutterQuads(frame.render_passes[0]->quad_list));
    EXPECT_EQ(5u, frame.render_passes[0]->quad_list.size());
    ValidateTextureDrawQuads(frame.render_passes[0]->quad_list);

    VerifyQuadsExactlyCoverViewport(frame.render_passes[0]->quad_list);
    host_impl_->DidDrawAllLayers(frame);
  }

  // Expect no gutter rects.
  void TestLayerIsLargerThanViewport() {
    gfx::Rect layer_rect(viewport_size_.width() + 10,
                         viewport_size_.height() + 10);
    child_->SetPosition(layer_rect.origin());
    child_->SetBounds(layer_rect.size());
    child_->SetContentBounds(layer_rect.size());
    child_->SetQuadRect(gfx::Rect(layer_rect.size()));
    child_->SetQuadVisibleRect(gfx::Rect(layer_rect.size()));

    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
    ASSERT_EQ(1u, frame.render_passes.size());

    EXPECT_EQ(0u, CountGutterQuads(frame.render_passes[0]->quad_list));
    EXPECT_EQ(1u, frame.render_passes[0]->quad_list.size());
    ValidateTextureDrawQuads(frame.render_passes[0]->quad_list);

    host_impl_->DidDrawAllLayers(frame);
  }

  virtual void DidActivatePendingTree() OVERRIDE {
    did_activate_pending_tree_ = true;
  }

  void set_gutter_quad_material(DrawQuad::Material material) {
    gutter_quad_material_ = material;
  }
  void set_gutter_texture_size(gfx::Size gutter_texture_size) {
    gutter_texture_size_ = gutter_texture_size;
  }

 protected:
  size_t CountGutterQuads(const QuadList& quad_list) {
    size_t num_gutter_quads = 0;
    for (size_t i = 0; i < quad_list.size(); ++i) {
      num_gutter_quads += (quad_list[i]->material ==
                           gutter_quad_material_) ? 1 : 0;
    }
    return num_gutter_quads;
  }

  void VerifyQuadsExactlyCoverViewport(const QuadList& quad_list) {
    LayerTestCommon::VerifyQuadsExactlyCoverRect(
        quad_list, gfx::Rect(DipSizeToPixelSize(viewport_size_)));
  }

  // Make sure that the texture coordinates match their expectations.
  void ValidateTextureDrawQuads(const QuadList& quad_list) {
    for (size_t i = 0; i < quad_list.size(); ++i) {
      if (quad_list[i]->material != DrawQuad::TEXTURE_CONTENT)
        continue;
      const TextureDrawQuad* quad = TextureDrawQuad::MaterialCast(quad_list[i]);
      gfx::SizeF gutter_texture_size_pixels = gfx::ScaleSize(
          gutter_texture_size_, host_impl_->device_scale_factor());
      EXPECT_EQ(quad->uv_top_left.x(),
                quad->rect.x() / gutter_texture_size_pixels.width());
      EXPECT_EQ(quad->uv_top_left.y(),
                quad->rect.y() / gutter_texture_size_pixels.height());
      EXPECT_EQ(quad->uv_bottom_right.x(),
                quad->rect.right() / gutter_texture_size_pixels.width());
      EXPECT_EQ(quad->uv_bottom_right.y(),
                quad->rect.bottom() / gutter_texture_size_pixels.height());
    }
  }

  gfx::Size DipSizeToPixelSize(gfx::Size size) {
    return gfx::ToRoundedSize(
        gfx::ScaleSize(size, host_impl_->device_scale_factor()));
  }

  DrawQuad::Material gutter_quad_material_;
  gfx::Size gutter_texture_size_;
  gfx::Size viewport_size_;
  BlendStateCheckLayer* child_;
  bool did_activate_pending_tree_;
};

TEST_F(LayerTreeHostImplViewportCoveredTest, ViewportCovered) {
  bool always_draw = false;
  CreateLayerTreeHostImpl(always_draw);

  host_impl_->SetViewportSize(DipSizeToPixelSize(viewport_size_));
  SetupActiveTreeLayers();
  TestLayerCoversFullViewport();
  TestEmptyLayer();
  TestLayerInMiddleOfViewport();
  TestLayerIsLargerThanViewport();
}

TEST_F(LayerTreeHostImplViewportCoveredTest, ViewportCoveredScaled) {
  bool always_draw = false;
  CreateLayerTreeHostImpl(always_draw);

  host_impl_->SetDeviceScaleFactor(2.f);
  host_impl_->SetViewportSize(DipSizeToPixelSize(viewport_size_));
  SetupActiveTreeLayers();
  TestLayerCoversFullViewport();
  TestEmptyLayer();
  TestLayerInMiddleOfViewport();
  TestLayerIsLargerThanViewport();
}

TEST_F(LayerTreeHostImplViewportCoveredTest, ViewportCoveredOverhangBitmap) {
  bool always_draw = false;
  CreateLayerTreeHostImpl(always_draw);

  host_impl_->SetViewportSize(DipSizeToPixelSize(viewport_size_));
  SetupActiveTreeLayers();

  SkBitmap skbitmap;
  skbitmap.setConfig(SkBitmap::kARGB_8888_Config, 2, 2);
  skbitmap.allocPixels();
  skbitmap.setImmutable();

  // Specify an overhang bitmap to use.
  UIResourceBitmap ui_resource_bitmap(skbitmap);
  ui_resource_bitmap.SetWrapMode(UIResourceBitmap::REPEAT);
  UIResourceId ui_resource_id = 12345;
  host_impl_->CreateUIResource(ui_resource_id, ui_resource_bitmap);
  host_impl_->SetOverhangUIResource(ui_resource_id, gfx::Size(32, 32));
  set_gutter_quad_material(DrawQuad::TEXTURE_CONTENT);
  set_gutter_texture_size(gfx::Size(32, 32));

  TestLayerCoversFullViewport();
  TestEmptyLayer();
  TestLayerInMiddleOfViewport();
  TestLayerIsLargerThanViewport();

  // Change the resource size.
  host_impl_->SetOverhangUIResource(ui_resource_id, gfx::Size(128, 16));
  set_gutter_texture_size(gfx::Size(128, 16));

  TestLayerCoversFullViewport();
  TestEmptyLayer();
  TestLayerInMiddleOfViewport();
  TestLayerIsLargerThanViewport();

  // Change the device scale factor
  host_impl_->SetDeviceScaleFactor(2.f);
  host_impl_->SetViewportSize(DipSizeToPixelSize(viewport_size_));

  TestLayerCoversFullViewport();
  TestEmptyLayer();
  TestLayerInMiddleOfViewport();
  TestLayerIsLargerThanViewport();
}

TEST_F(LayerTreeHostImplViewportCoveredTest, ActiveTreeGrowViewportInvalid) {
  bool always_draw = true;
  CreateLayerTreeHostImpl(always_draw);

  // Pending tree to force active_tree size invalid. Not used otherwise.
  host_impl_->CreatePendingTree();
  host_impl_->SetViewportSize(DipSizeToPixelSize(viewport_size_));
  EXPECT_TRUE(host_impl_->active_tree()->ViewportSizeInvalid());

  SetupActiveTreeLayers();
  TestEmptyLayer();
  TestLayerInMiddleOfViewport();
  TestLayerIsLargerThanViewport();
}

TEST_F(LayerTreeHostImplViewportCoveredTest, ActiveTreeShrinkViewportInvalid) {
  bool always_draw = true;
  CreateLayerTreeHostImpl(always_draw);

  // Set larger viewport and activate it to active tree.
  host_impl_->CreatePendingTree();
  gfx::Size larger_viewport(viewport_size_.width() + 100,
                            viewport_size_.height() + 100);
  host_impl_->SetViewportSize(DipSizeToPixelSize(larger_viewport));
  EXPECT_TRUE(host_impl_->active_tree()->ViewportSizeInvalid());
  host_impl_->ActivatePendingTree();
  EXPECT_TRUE(did_activate_pending_tree_);
  EXPECT_FALSE(host_impl_->active_tree()->ViewportSizeInvalid());

  // Shrink pending tree viewport without activating.
  host_impl_->CreatePendingTree();
  host_impl_->SetViewportSize(DipSizeToPixelSize(viewport_size_));
  EXPECT_TRUE(host_impl_->active_tree()->ViewportSizeInvalid());

  SetupActiveTreeLayers();
  TestEmptyLayer();
  TestLayerInMiddleOfViewport();
  TestLayerIsLargerThanViewport();
}

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
  scoped_refptr<TestContextProvider> provider(TestContextProvider::Create());
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(provider));
  host_impl_->InitializeRenderer(output_surface.Pass());

  scoped_ptr<LayerImpl> root =
      FakeDrawableLayerImpl::Create(host_impl_->active_tree(), 1);
  root->SetAnchorPoint(gfx::PointF());
  root->SetBounds(gfx::Size(10, 10));
  root->SetContentBounds(gfx::Size(10, 10));
  root->SetDrawsContent(true);
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  EXPECT_FALSE(provider->TestContext3d()->reshape_called());
  provider->TestContext3d()->clear_reshape_called();

  LayerTreeHostImpl::FrameData frame;
  host_impl_->SetViewportSize(gfx::Size(10, 10));
  host_impl_->SetDeviceScaleFactor(1.f);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(provider->TestContext3d()->reshape_called());
  EXPECT_EQ(provider->TestContext3d()->width(), 10);
  EXPECT_EQ(provider->TestContext3d()->height(), 10);
  EXPECT_EQ(provider->TestContext3d()->scale_factor(), 1.f);
  host_impl_->DidDrawAllLayers(frame);
  provider->TestContext3d()->clear_reshape_called();

  host_impl_->SetViewportSize(gfx::Size(20, 30));
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(provider->TestContext3d()->reshape_called());
  EXPECT_EQ(provider->TestContext3d()->width(), 20);
  EXPECT_EQ(provider->TestContext3d()->height(), 30);
  EXPECT_EQ(provider->TestContext3d()->scale_factor(), 1.f);
  host_impl_->DidDrawAllLayers(frame);
  provider->TestContext3d()->clear_reshape_called();

  host_impl_->SetDeviceScaleFactor(2.f);
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  EXPECT_TRUE(provider->TestContext3d()->reshape_called());
  EXPECT_EQ(provider->TestContext3d()->width(), 20);
  EXPECT_EQ(provider->TestContext3d()->height(), 30);
  EXPECT_EQ(provider->TestContext3d()->scale_factor(), 2.f);
  host_impl_->DidDrawAllLayers(frame);
  provider->TestContext3d()->clear_reshape_called();
}

// Make sure damage tracking propagates all the way to the graphics context,
// where it should request to swap only the sub-buffer that is damaged.
TEST_F(LayerTreeHostImplTest, PartialSwapReceivesDamageRect) {
  scoped_refptr<TestContextProvider> provider(
      TestContextProvider::Create());
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(provider));

  provider->BindToCurrentThread();
  TestWebGraphicsContext3D* context = provider->TestContext3d();
  context->set_have_post_sub_buffer(true);

  // This test creates its own LayerTreeHostImpl, so
  // that we can force partial swap enabled.
  LayerTreeSettings settings;
  settings.partial_swap_enabled = true;
  scoped_ptr<LayerTreeHostImpl> layer_tree_host_impl =
      LayerTreeHostImpl::Create(
          settings, this, &proxy_, &stats_instrumentation_, NULL);
  layer_tree_host_impl->InitializeRenderer(output_surface.Pass());
  layer_tree_host_impl->SetViewportSize(gfx::Size(500, 500));

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
  EXPECT_TRUE(layer_tree_host_impl->PrepareToDraw(&frame, gfx::Rect()));
  layer_tree_host_impl->DrawLayers(&frame, gfx::FrameTime::Now());
  layer_tree_host_impl->DidDrawAllLayers(frame);
  layer_tree_host_impl->SwapBuffers(frame);
  gfx::Rect actual_swap_rect = context->update_rect();
  gfx::Rect expected_swap_rect = gfx::Rect(0, 0, 500, 500);
  EXPECT_EQ(expected_swap_rect.x(), actual_swap_rect.x());
  EXPECT_EQ(expected_swap_rect.y(), actual_swap_rect.y());
  EXPECT_EQ(expected_swap_rect.width(), actual_swap_rect.width());
  EXPECT_EQ(expected_swap_rect.height(), actual_swap_rect.height());
  EXPECT_EQ(context->last_update_type(),
            TestWebGraphicsContext3D::PrepareTexture);
  // Second frame, only the damaged area should get swapped. Damage should be
  // the union of old and new child rects.
  // expected damage rect: gfx::Rect(26, 28);
  // expected swap rect: vertically flipped, with origin at bottom left corner.
  layer_tree_host_impl->active_tree()->root_layer()->children()[0]->SetPosition(
      gfx::PointF());
  EXPECT_TRUE(layer_tree_host_impl->PrepareToDraw(&frame, gfx::Rect()));
  layer_tree_host_impl->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
  layer_tree_host_impl->SwapBuffers(frame);
  actual_swap_rect = context->update_rect();
  expected_swap_rect = gfx::Rect(0, 500-28, 26, 28);
  EXPECT_EQ(expected_swap_rect.x(), actual_swap_rect.x());
  EXPECT_EQ(expected_swap_rect.y(), actual_swap_rect.y());
  EXPECT_EQ(expected_swap_rect.width(), actual_swap_rect.width());
  EXPECT_EQ(expected_swap_rect.height(), actual_swap_rect.height());
  EXPECT_EQ(context->last_update_type(),
            TestWebGraphicsContext3D::PostSubBuffer);

  // Make sure that partial swap is constrained to the viewport dimensions
  // expected damage rect: gfx::Rect(500, 500);
  // expected swap rect: flipped damage rect, but also clamped to viewport
  layer_tree_host_impl->SetViewportSize(gfx::Size(10, 10));
  // This will damage everything.
  layer_tree_host_impl->active_tree()->root_layer()->SetBackgroundColor(
      SK_ColorBLACK);
  EXPECT_TRUE(layer_tree_host_impl->PrepareToDraw(&frame, gfx::Rect()));
  layer_tree_host_impl->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
  layer_tree_host_impl->SwapBuffers(frame);
  actual_swap_rect = context->update_rect();
  expected_swap_rect = gfx::Rect(10, 10);
  EXPECT_EQ(expected_swap_rect.x(), actual_swap_rect.x());
  EXPECT_EQ(expected_swap_rect.y(), actual_swap_rect.y());
  EXPECT_EQ(expected_swap_rect.width(), actual_swap_rect.width());
  EXPECT_EQ(expected_swap_rect.height(), actual_swap_rect.height());
  EXPECT_EQ(context->last_update_type(),
            TestWebGraphicsContext3D::PrepareTexture);
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
  root->SetForceRenderSurface(true);
  root->AddChild(child.Pass());

  host_impl_->active_tree()->SetRootLayer(root.Pass());

  LayerTreeHostImpl::FrameData frame;

  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
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
    my_quad->SetNew(shared_quad_state, quad_rect, gray, false);
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
    context_->set_have_post_sub_buffer(true);

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
  scoped_ptr<MockContext> mock_context_owned(new MockContext);
  MockContext* mock_context = mock_context_owned.get();

  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      mock_context_owned.PassAs<TestWebGraphicsContext3D>()));
  MockContextHarness harness(mock_context);

  // Run test case
  CreateLayerTreeHost(false, output_surface.Pass());
  SetupRootLayerImpl(FakeLayerWithQuads::Create(host_impl_->active_tree(), 1));

  // Without partial swap, and no clipping, no scissor is set.
  harness.MustDrawSolidQuad();
  harness.MustSetNoScissor();
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
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
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);
  }
  Mock::VerifyAndClearExpectations(&mock_context);
}

TEST_F(LayerTreeHostImplTest, PartialSwap) {
  scoped_ptr<MockContext> context_owned(new MockContext);
  MockContext* mock_context = context_owned.get();
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  MockContextHarness harness(mock_context);

  CreateLayerTreeHost(true, output_surface.Pass());
  SetupRootLayerImpl(FakeLayerWithQuads::Create(host_impl_->active_tree(), 1));

  // The first frame is not a partially-swapped one.
  harness.MustSetScissor(0, 0, 10, 10);
  harness.MustDrawSolidQuad();
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
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
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);
  }
  Mock::VerifyAndClearExpectations(&mock_context);
}

static scoped_ptr<LayerTreeHostImpl> SetupLayersForOpacity(
    bool partial_swap,
    LayerTreeHostImplClient* client,
    Proxy* proxy,
    RenderingStatsInstrumentation* stats_instrumentation) {
  scoped_refptr<TestContextProvider> provider(TestContextProvider::Create());
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(provider));
  provider->BindToCurrentThread();
  provider->TestContext3d()->set_have_post_sub_buffer(true);

  LayerTreeSettings settings;
  settings.partial_swap_enabled = partial_swap;
  scoped_ptr<LayerTreeHostImpl> my_host_impl = LayerTreeHostImpl::Create(
      settings, client, proxy, stats_instrumentation, NULL);
  my_host_impl->InitializeRenderer(output_surface.Pass());
  my_host_impl->SetViewportSize(gfx::Size(100, 100));

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
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame, gfx::Rect()));

    // Verify all quads have been computed
    ASSERT_EQ(2U, frame.render_passes.size());
    ASSERT_EQ(1U, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(1U, frame.render_passes[1]->quad_list.size());
    EXPECT_EQ(DrawQuad::SOLID_COLOR,
              frame.render_passes[0]->quad_list[0]->material);
    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[1]->quad_list[0]->material);

    my_host_impl->DrawLayers(&frame, gfx::FrameTime::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, ContributingLayerEmptyScissorNoPartialSwap) {
  scoped_ptr<LayerTreeHostImpl> my_host_impl =
      SetupLayersForOpacity(false, this, &proxy_, &stats_instrumentation_);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(my_host_impl->PrepareToDraw(&frame, gfx::Rect()));

    // Verify all quads have been computed
    ASSERT_EQ(2U, frame.render_passes.size());
    ASSERT_EQ(1U, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(1U, frame.render_passes[1]->quad_list.size());
    EXPECT_EQ(DrawQuad::SOLID_COLOR,
              frame.render_passes[0]->quad_list[0]->material);
    EXPECT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[1]->quad_list[0]->material);

    my_host_impl->DrawLayers(&frame, gfx::FrameTime::Now());
    my_host_impl->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, LayersFreeTextures) {
  scoped_ptr<TestWebGraphicsContext3D> context =
      TestWebGraphicsContext3D::Create();
  TestWebGraphicsContext3D* context3d = context.get();
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(context.Pass()));
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
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
  host_impl_->SwapBuffers(frame);

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
  scoped_ptr<MockDrawQuadsToFillScreenContext> mock_context_owned(
      new MockDrawQuadsToFillScreenContext);
  MockDrawQuadsToFillScreenContext* mock_context = mock_context_owned.get();

  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      mock_context_owned.PassAs<TestWebGraphicsContext3D>()));

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
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
  Mock::VerifyAndClearExpectations(&mock_context);

  // Verify no quads are drawn when transparent background is set.
  host_impl_->active_tree()->set_has_transparent_background(true);
  host_impl_->SetFullRootLayerDamage();
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
  Mock::VerifyAndClearExpectations(&mock_context);
}

TEST_F(LayerTreeHostImplTest, ReleaseContentsTextureShouldTriggerCommit) {
  set_reduce_memory_result(false);

  // If changing the memory limit wouldn't result in changing what was
  // committed, then no commit should be requested.
  set_reduce_memory_result(false);
  host_impl_->set_max_memory_needed_bytes(
      host_impl_->memory_allocation_limit_bytes() - 1);
  host_impl_->SetMemoryPolicy(ManagedMemoryPolicy(
      host_impl_->memory_allocation_limit_bytes() - 1));
  EXPECT_FALSE(did_request_commit_);
  did_request_commit_ = false;

  // If changing the memory limit would result in changing what was
  // committed, then a commit should be requested, even though nothing was
  // evicted.
  set_reduce_memory_result(false);
  host_impl_->set_max_memory_needed_bytes(
      host_impl_->memory_allocation_limit_bytes());
  host_impl_->SetMemoryPolicy(ManagedMemoryPolicy(
      host_impl_->memory_allocation_limit_bytes() - 1));
  EXPECT_TRUE(did_request_commit_);
  did_request_commit_ = false;

  // Especially if changing the memory limit caused evictions, we need
  // to re-commit.
  set_reduce_memory_result(true);
  host_impl_->set_max_memory_needed_bytes(1);
  host_impl_->SetMemoryPolicy(ManagedMemoryPolicy(
      host_impl_->memory_allocation_limit_bytes() - 1));
  EXPECT_TRUE(did_request_commit_);
  did_request_commit_ = false;

  // But if we set it to the same value that it was before, we shouldn't
  // re-commit.
  host_impl_->SetMemoryPolicy(ManagedMemoryPolicy(
      host_impl_->memory_allocation_limit_bytes()));
  EXPECT_FALSE(did_request_commit_);
}

class LayerTreeHostImplTestWithDelegatingRenderer
    : public LayerTreeHostImplTest {
 protected:
  virtual scoped_ptr<OutputSurface> CreateOutputSurface() OVERRIDE {
    return FakeOutputSurface::CreateDelegating3d().PassAs<OutputSurface>();
  }

  void DrawFrameAndTestDamage(const gfx::RectF& expected_damage) {
    bool expect_to_draw = !expected_damage.IsEmpty();

    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

    if (!expect_to_draw) {
      // With no damage, we don't draw, and no quads are created.
      ASSERT_EQ(0u, frame.render_passes.size());
    } else {
      ASSERT_EQ(1u, frame.render_passes.size());

      // Verify the damage rect for the root render pass.
      const RenderPass* root_render_pass = frame.render_passes.back();
      EXPECT_RECT_EQ(expected_damage, root_render_pass->damage_rect);

      // Verify the root and child layers' quads are generated and not being
      // culled.
      ASSERT_EQ(2u, root_render_pass->quad_list.size());

      LayerImpl* child = host_impl_->active_tree()->root_layer()->children()[0];
      gfx::RectF expected_child_visible_rect(child->content_bounds());
      EXPECT_RECT_EQ(expected_child_visible_rect,
                     root_render_pass->quad_list[0]->visible_rect);

      LayerImpl* root = host_impl_->active_tree()->root_layer();
      gfx::RectF expected_root_visible_rect(root->content_bounds());
      EXPECT_RECT_EQ(expected_root_visible_rect,
                     root_render_pass->quad_list[1]->visible_rect);
    }

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);
    EXPECT_EQ(expect_to_draw, host_impl_->SwapBuffers(frame));
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

  // Child layer is in the bottom right corner.
  scoped_ptr<SolidColorLayerImpl> child =
      SolidColorLayerImpl::Create(host_impl_->active_tree(), 2);
  child->SetAnchorPoint(gfx::PointF(0.f, 0.f));
  child->SetPosition(gfx::PointF(9.f, 9.f));
  child->SetBounds(gfx::Size(1, 1));
  child->SetContentBounds(gfx::Size(1, 1));
  child->SetDrawsContent(true);
  root->AddChild(child.PassAs<LayerImpl>());

  host_impl_->active_tree()->SetRootLayer(root.PassAs<LayerImpl>());

  // Draw a frame. In the first frame, the entire viewport should be damaged.
  gfx::Rect full_frame_damage(host_impl_->DrawViewportSize());
  DrawFrameAndTestDamage(full_frame_damage);

  // The second frame has damage that doesn't touch the child layer. Its quads
  // should still be generated.
  gfx::Rect small_damage = gfx::Rect(0, 0, 1, 1);
  host_impl_->active_tree()->root_layer()->set_update_rect(small_damage);
  DrawFrameAndTestDamage(small_damage);

  // The third frame should have no damage, so no quads should be generated.
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
  LayerTreeSettings settings;
  settings.layer_transforms_should_scale_layer_contents = true;
  host_impl_ = LayerTreeHostImpl::Create(
      settings, this, &proxy_, &stats_instrumentation_, NULL);
  host_impl_->InitializeRenderer(CreateOutputSurface());
  host_impl_->SetViewportSize(gfx::Size(10, 10));

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
  host_impl_->SetViewportSize(root_size);
  host_impl_->SetDeviceScaleFactor(device_scale_factor);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

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

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);
  }


  // Applying a DSF should change the render surface size, but won't affect
  // which part of the mask is used.
  device_scale_factor = 2.f;
  gfx::Size device_viewport =
      gfx::ToFlooredSize(gfx::ScaleSize(root_size, device_scale_factor));
  host_impl_->SetViewportSize(device_viewport);
  host_impl_->SetDeviceScaleFactor(device_scale_factor);
  host_impl_->active_tree()->set_needs_update_draw_properties();
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

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

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
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
  host_impl_->active_tree()->set_needs_update_draw_properties();
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

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

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
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
  host_impl_->SetViewportSize(root_size);
  host_impl_->SetDeviceScaleFactor(device_scale_factor);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

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

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);
  }

  // Applying a DSF should change the render surface size, but won't affect
  // which part of the mask is used.
  device_scale_factor = 2.f;
  gfx::Size device_viewport =
      gfx::ToFlooredSize(gfx::ScaleSize(root_size, device_scale_factor));
  host_impl_->SetViewportSize(device_viewport);
  host_impl_->SetDeviceScaleFactor(device_scale_factor);
  host_impl_->active_tree()->set_needs_update_draw_properties();
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

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

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
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
  host_impl_->active_tree()->set_needs_update_draw_properties();
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

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

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);
  }

  // Applying a different contents scale to the mask layer means it will have
  // a larger texture, but it should use the same tex coords to cover the
  // layer it masks.
  mask_layer->SetContentBounds(mask_size);
  mask_layer->SetContentsScale(1.f, 1.f);
  host_impl_->active_tree()->set_needs_update_draw_properties();
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

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

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, ReflectionMaskLayerWithDifferentBounds) {
  // The replica's mask layer has bounds 100x100 but the replica is of a
  // layer with bounds 50x50.

  scoped_ptr<LayerImpl> scoped_root =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  LayerImpl* root = scoped_root.get();
  host_impl_->active_tree()->SetRootLayer(scoped_root.Pass());

  scoped_ptr<LayerImpl> scoped_content_layer =
      LayerImpl::Create(host_impl_->active_tree(), 3);
  LayerImpl* content_layer = scoped_content_layer.get();
  root->AddChild(scoped_content_layer.Pass());

  scoped_ptr<LayerImpl> scoped_replica_layer =
      LayerImpl::Create(host_impl_->active_tree(), 2);
  LayerImpl* replica_layer = scoped_replica_layer.get();
  content_layer->SetReplicaLayer(scoped_replica_layer.Pass());

  scoped_ptr<FakeMaskLayerImpl> scoped_mask_layer =
      FakeMaskLayerImpl::Create(host_impl_->active_tree(), 4);
  FakeMaskLayerImpl* mask_layer = scoped_mask_layer.get();
  replica_layer->SetMaskLayer(scoped_mask_layer.PassAs<LayerImpl>());

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
  host_impl_->SetViewportSize(root_size);
  host_impl_->SetDeviceScaleFactor(device_scale_factor);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

    ASSERT_EQ(1u, frame.render_passes.size());
    ASSERT_EQ(2u, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[1]->material);
    const RenderPassDrawQuad* replica_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[1]);
    EXPECT_TRUE(replica_quad->is_replica);
    EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(),
              replica_quad->rect.ToString());
    EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(),
              replica_quad->mask_uv_rect.ToString());

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);
  }

  // Applying a DSF should change the render surface size, but won't affect
  // which part of the mask is used.
  device_scale_factor = 2.f;
  gfx::Size device_viewport =
      gfx::ToFlooredSize(gfx::ScaleSize(root_size, device_scale_factor));
  host_impl_->SetViewportSize(device_viewport);
  host_impl_->SetDeviceScaleFactor(device_scale_factor);
  host_impl_->active_tree()->set_needs_update_draw_properties();
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

    ASSERT_EQ(1u, frame.render_passes.size());
    ASSERT_EQ(2u, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[1]->material);
    const RenderPassDrawQuad* replica_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[1]);
    EXPECT_TRUE(replica_quad->is_replica);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
              replica_quad->rect.ToString());
    EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(),
              replica_quad->mask_uv_rect.ToString());

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
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
  host_impl_->active_tree()->set_needs_update_draw_properties();
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

    ASSERT_EQ(1u, frame.render_passes.size());
    ASSERT_EQ(2u, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[1]->material);
    const RenderPassDrawQuad* replica_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[1]);
    EXPECT_TRUE(replica_quad->is_replica);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
              replica_quad->rect.ToString());
    EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(),
              replica_quad->mask_uv_rect.ToString());

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);
  }

  // Applying a different contents scale to the mask layer means it will have
  // a larger texture, but it should use the same tex coords to cover the
  // layer it masks.
  mask_layer->SetContentBounds(mask_size);
  mask_layer->SetContentsScale(1.f, 1.f);
  host_impl_->active_tree()->set_needs_update_draw_properties();
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

    ASSERT_EQ(1u, frame.render_passes.size());
    ASSERT_EQ(2u, frame.render_passes[0]->quad_list.size());
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[1]->material);
    const RenderPassDrawQuad* replica_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[1]);
    EXPECT_TRUE(replica_quad->is_replica);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
              replica_quad->rect.ToString());
    EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(),
              replica_quad->mask_uv_rect.ToString());

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, ReflectionMaskLayerForSurfaceWithUnclippedChild) {
  // The replica is of a layer with bounds 50x50, but it has a child that causes
  // the surface bounds to be larger.

  scoped_ptr<LayerImpl> scoped_root =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  LayerImpl* root = scoped_root.get();
  host_impl_->active_tree()->SetRootLayer(scoped_root.Pass());

  scoped_ptr<LayerImpl> scoped_content_layer =
      LayerImpl::Create(host_impl_->active_tree(), 2);
  LayerImpl* content_layer = scoped_content_layer.get();
  root->AddChild(scoped_content_layer.Pass());

  scoped_ptr<LayerImpl> scoped_content_child_layer =
      LayerImpl::Create(host_impl_->active_tree(), 3);
  LayerImpl* content_child_layer = scoped_content_child_layer.get();
  content_layer->AddChild(scoped_content_child_layer.Pass());

  scoped_ptr<LayerImpl> scoped_replica_layer =
      LayerImpl::Create(host_impl_->active_tree(), 4);
  LayerImpl* replica_layer = scoped_replica_layer.get();
  content_layer->SetReplicaLayer(scoped_replica_layer.Pass());

  scoped_ptr<FakeMaskLayerImpl> scoped_mask_layer =
      FakeMaskLayerImpl::Create(host_impl_->active_tree(), 5);
  FakeMaskLayerImpl* mask_layer = scoped_mask_layer.get();
  replica_layer->SetMaskLayer(scoped_mask_layer.PassAs<LayerImpl>());

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

  gfx::Size child_size(50, 50);
  content_child_layer->SetBounds(child_size);
  content_child_layer->SetContentBounds(child_size);
  content_child_layer->SetPosition(gfx::Point(50, 0));
  content_child_layer->SetAnchorPoint(gfx::PointF());
  content_child_layer->SetDrawsContent(true);

  gfx::Size mask_size(50, 50);
  mask_layer->SetBounds(mask_size);
  mask_layer->SetContentBounds(mask_size);
  mask_layer->SetPosition(gfx::PointF());
  mask_layer->SetAnchorPoint(gfx::PointF());
  mask_layer->SetDrawsContent(true);

  float device_scale_factor = 1.f;
  host_impl_->SetViewportSize(root_size);
  host_impl_->SetDeviceScaleFactor(device_scale_factor);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

    ASSERT_EQ(1u, frame.render_passes.size());
    ASSERT_EQ(2u, frame.render_passes[0]->quad_list.size());

    // The surface is 100x50.
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* render_pass_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_FALSE(render_pass_quad->is_replica);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              render_pass_quad->rect.ToString());

    // The mask covers the owning layer only.
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[1]->material);
    const RenderPassDrawQuad* replica_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[1]);
    EXPECT_TRUE(replica_quad->is_replica);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              replica_quad->rect.ToString());
    EXPECT_EQ(gfx::RectF(0.f, 0.f, 2.f, 1.f).ToString(),
              replica_quad->mask_uv_rect.ToString());

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);
  }

  // Move the child to (-50, 0) instead. Now the mask should be moved to still
  // cover the layer being replicated.
  content_child_layer->SetPosition(gfx::Point(-50, 0));
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

    ASSERT_EQ(1u, frame.render_passes.size());
    ASSERT_EQ(2u, frame.render_passes[0]->quad_list.size());

    // The surface is 100x50 with its origin at (-50, 0).
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* render_pass_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_FALSE(render_pass_quad->is_replica);
    EXPECT_EQ(gfx::Rect(-50, 0, 100, 50).ToString(),
              render_pass_quad->rect.ToString());

    // The mask covers the owning layer only.
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[1]->material);
    const RenderPassDrawQuad* replica_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[1]);
    EXPECT_TRUE(replica_quad->is_replica);
    EXPECT_EQ(gfx::Rect(-50, 0, 100, 50).ToString(),
              replica_quad->rect.ToString());
    EXPECT_EQ(gfx::RectF(-1.f, 0.f, 2.f, 1.f).ToString(),
              replica_quad->mask_uv_rect.ToString());

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, MaskLayerForSurfaceWithClippedLayer) {
  // The masked layer has bounds 50x50, but it has a child that causes
  // the surface bounds to be larger. It also has a parent that clips the
  // masked layer and its surface.

  scoped_ptr<LayerImpl> scoped_root =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  LayerImpl* root = scoped_root.get();
  host_impl_->active_tree()->SetRootLayer(scoped_root.Pass());

  scoped_ptr<LayerImpl> scoped_clipping_layer =
      LayerImpl::Create(host_impl_->active_tree(), 2);
  LayerImpl* clipping_layer = scoped_clipping_layer.get();
  root->AddChild(scoped_clipping_layer.Pass());

  scoped_ptr<LayerImpl> scoped_content_layer =
      LayerImpl::Create(host_impl_->active_tree(), 3);
  LayerImpl* content_layer = scoped_content_layer.get();
  clipping_layer->AddChild(scoped_content_layer.Pass());

  scoped_ptr<LayerImpl> scoped_content_child_layer =
      LayerImpl::Create(host_impl_->active_tree(), 4);
  LayerImpl* content_child_layer = scoped_content_child_layer.get();
  content_layer->AddChild(scoped_content_child_layer.Pass());

  scoped_ptr<FakeMaskLayerImpl> scoped_mask_layer =
      FakeMaskLayerImpl::Create(host_impl_->active_tree(), 6);
  FakeMaskLayerImpl* mask_layer = scoped_mask_layer.get();
  content_layer->SetMaskLayer(scoped_mask_layer.PassAs<LayerImpl>());

  gfx::Size root_size(100, 100);
  root->SetBounds(root_size);
  root->SetContentBounds(root_size);
  root->SetPosition(gfx::PointF());
  root->SetAnchorPoint(gfx::PointF());

  gfx::Rect clipping_rect(20, 10, 10, 20);
  clipping_layer->SetBounds(clipping_rect.size());
  clipping_layer->SetContentBounds(clipping_rect.size());
  clipping_layer->SetPosition(clipping_rect.origin());
  clipping_layer->SetAnchorPoint(gfx::PointF());
  clipping_layer->SetMasksToBounds(true);

  gfx::Size layer_size(50, 50);
  content_layer->SetBounds(layer_size);
  content_layer->SetContentBounds(layer_size);
  content_layer->SetPosition(gfx::Point() - clipping_rect.OffsetFromOrigin());
  content_layer->SetAnchorPoint(gfx::PointF());
  content_layer->SetDrawsContent(true);

  gfx::Size child_size(50, 50);
  content_child_layer->SetBounds(child_size);
  content_child_layer->SetContentBounds(child_size);
  content_child_layer->SetPosition(gfx::Point(50, 0));
  content_child_layer->SetAnchorPoint(gfx::PointF());
  content_child_layer->SetDrawsContent(true);

  gfx::Size mask_size(100, 100);
  mask_layer->SetBounds(mask_size);
  mask_layer->SetContentBounds(mask_size);
  mask_layer->SetPosition(gfx::PointF());
  mask_layer->SetAnchorPoint(gfx::PointF());
  mask_layer->SetDrawsContent(true);

  float device_scale_factor = 1.f;
  host_impl_->SetViewportSize(root_size);
  host_impl_->SetDeviceScaleFactor(device_scale_factor);
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

    ASSERT_EQ(1u, frame.render_passes.size());
    ASSERT_EQ(1u, frame.render_passes[0]->quad_list.size());

    // The surface is clipped to 10x20.
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              frame.render_passes[0]->quad_list[0]->material);
    const RenderPassDrawQuad* render_pass_quad =
        RenderPassDrawQuad::MaterialCast(frame.render_passes[0]->quad_list[0]);
    EXPECT_FALSE(render_pass_quad->is_replica);
    EXPECT_EQ(gfx::Rect(20, 10, 10, 20).ToString(),
              render_pass_quad->rect.ToString());

    // The masked layer is 50x50, but the surface size is 10x20. So the texture
    // coords in the mask are scaled by 10/50 and 20/50.
    // The surface is clipped to (20,10) so the mask texture coords are offset
    // by 20/50 and 10/50
    EXPECT_EQ(gfx::ScaleRect(gfx::RectF(20.f, 10.f, 10.f, 20.f),
                             1.f / 50.f).ToString(),
              render_pass_quad->mask_uv_rect.ToString());

    host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
    host_impl_->DidDrawAllLayers(frame);
  }
}

class GLRendererWithSetupQuadForAntialiasing : public GLRenderer {
 public:
  using GLRenderer::SetupQuadForAntialiasing;
};

TEST_F(LayerTreeHostImplTest, FarAwayQuadsDontNeedAA) {
  // Due to precision issues (especially on Android), sometimes far
  // away quads can end up thinking they need AA.
  float device_scale_factor = 4.f / 3.f;
  host_impl_->SetDeviceScaleFactor(device_scale_factor);
  gfx::Size root_size(2000, 1000);
  gfx::Size device_viewport_size =
      gfx::ToCeiledSize(gfx::ScaleSize(root_size, device_scale_factor));
  host_impl_->SetViewportSize(device_viewport_size);

  host_impl_->CreatePendingTree();
  host_impl_->pending_tree()
      ->SetPageScaleFactorAndLimits(1.f, 1.f / 16.f, 16.f);

  scoped_ptr<LayerImpl> scoped_root =
      LayerImpl::Create(host_impl_->pending_tree(), 1);
  LayerImpl* root = scoped_root.get();

  host_impl_->pending_tree()->SetRootLayer(scoped_root.Pass());

  scoped_ptr<LayerImpl> scoped_scrolling_layer =
      LayerImpl::Create(host_impl_->pending_tree(), 2);
  LayerImpl* scrolling_layer = scoped_scrolling_layer.get();
  root->AddChild(scoped_scrolling_layer.Pass());

  gfx::Size content_layer_bounds(100000, 100);
  gfx::Size pile_tile_size(3000, 3000);
  scoped_refptr<FakePicturePileImpl> pile(FakePicturePileImpl::CreateFilledPile(
      pile_tile_size, content_layer_bounds));

  scoped_ptr<FakePictureLayerImpl> scoped_content_layer =
      FakePictureLayerImpl::CreateWithPile(host_impl_->pending_tree(), 3, pile);
  LayerImpl* content_layer = scoped_content_layer.get();
  scrolling_layer->AddChild(scoped_content_layer.PassAs<LayerImpl>());
  content_layer->SetBounds(content_layer_bounds);
  content_layer->SetDrawsContent(true);

  root->SetBounds(root_size);

  gfx::Vector2d scroll_offset(100000, 0);
  scrolling_layer->SetScrollable(true);
  scrolling_layer->SetMaxScrollOffset(scroll_offset);
  scrolling_layer->SetScrollOffset(scroll_offset);

  host_impl_->ActivatePendingTree();

  host_impl_->active_tree()->UpdateDrawProperties();
  ASSERT_EQ(1u, host_impl_->active_tree()->RenderSurfaceLayerList().size());

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));

  ASSERT_EQ(1u, frame.render_passes.size());
  ASSERT_LE(1u, frame.render_passes[0]->quad_list.size());
  const DrawQuad* quad = frame.render_passes[0]->quad_list[0];

  float edge[24];
  gfx::QuadF device_layer_quad;
  bool antialiased =
      GLRendererWithSetupQuadForAntialiasing::SetupQuadForAntialiasing(
          quad->quadTransform(), quad, &device_layer_quad, edge);
  EXPECT_FALSE(antialiased);

  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);
}


class CompositorFrameMetadataTest : public LayerTreeHostImplTest {
 public:
  CompositorFrameMetadataTest()
      : swap_buffers_complete_(0) {}

  virtual void OnSwapBuffersCompleteOnImplThread() OVERRIDE {
    swap_buffers_complete_++;
  }

  int swap_buffers_complete_;
};

TEST_F(CompositorFrameMetadataTest, CompositorFrameAckCountsAsSwapComplete) {
  SetupRootLayerImpl(FakeLayerWithQuads::Create(host_impl_->active_tree(), 1));
  {
    LayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
    host_impl_->DrawLayers(&frame, base::TimeTicks());
    host_impl_->DidDrawAllLayers(frame);
  }
  CompositorFrameAck ack;
  host_impl_->ReclaimResources(&ack);
  host_impl_->OnSwapBuffersComplete();
  EXPECT_EQ(swap_buffers_complete_, 1);
}

class CountingSoftwareDevice : public SoftwareOutputDevice {
 public:
  CountingSoftwareDevice() : frames_began_(0), frames_ended_(0) {}

  virtual SkCanvas* BeginPaint(gfx::Rect damage_rect) OVERRIDE {
    ++frames_began_;
    return SoftwareOutputDevice::BeginPaint(damage_rect);
  }
  virtual void EndPaint(SoftwareFrameData* frame_data) OVERRIDE {
    ++frames_ended_;
    SoftwareOutputDevice::EndPaint(frame_data);
  }

  int frames_began_, frames_ended_;
};

TEST_F(LayerTreeHostImplTest, ForcedDrawToSoftwareDeviceBasicRender) {
  // No main thread evictions in resourceless software mode.
  set_reduce_memory_result(false);
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->SetViewportSize(gfx::Size(50, 50));
  CountingSoftwareDevice* software_device = new CountingSoftwareDevice();
  FakeOutputSurface* output_surface = FakeOutputSurface::CreateDeferredGL(
      scoped_ptr<SoftwareOutputDevice>(software_device)).release();
  EXPECT_TRUE(host_impl_->InitializeRenderer(
      scoped_ptr<OutputSurface>(output_surface)));

  output_surface->set_forced_draw_to_software_device(true);
  EXPECT_TRUE(output_surface->ForcedDrawToSoftwareDevice());

  EXPECT_EQ(0, software_device->frames_began_);
  EXPECT_EQ(0, software_device->frames_ended_);

  DrawFrame();

  EXPECT_EQ(1, software_device->frames_began_);
  EXPECT_EQ(1, software_device->frames_ended_);

  // Call other API methods that are likely to hit NULL pointer in this mode.
  EXPECT_TRUE(host_impl_->AsValue());
  EXPECT_TRUE(host_impl_->ActivationStateAsValue());
}

TEST_F(LayerTreeHostImplTest,
       ForcedDrawToSoftwareDeviceSkipsUnsupportedLayers) {
  set_reduce_memory_result(false);
  FakeOutputSurface* output_surface = FakeOutputSurface::CreateDeferredGL(
      scoped_ptr<SoftwareOutputDevice>(new CountingSoftwareDevice())).release();
  host_impl_->InitializeRenderer(
      scoped_ptr<OutputSurface>(output_surface));

  output_surface->set_forced_draw_to_software_device(true);
  EXPECT_TRUE(output_surface->ForcedDrawToSoftwareDevice());

  // SolidColorLayerImpl will be drawn.
  scoped_ptr<SolidColorLayerImpl> root_layer =
      SolidColorLayerImpl::Create(host_impl_->active_tree(), 1);

  // VideoLayerImpl will not be drawn.
  FakeVideoFrameProvider provider;
  scoped_ptr<VideoLayerImpl> video_layer =
      VideoLayerImpl::Create(host_impl_->active_tree(), 2, &provider);
  video_layer->SetBounds(gfx::Size(10, 10));
  video_layer->SetContentBounds(gfx::Size(10, 10));
  video_layer->SetDrawsContent(true);
  root_layer->AddChild(video_layer.PassAs<LayerImpl>());
  SetupRootLayerImpl(root_layer.PassAs<LayerImpl>());

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_EQ(1u, frame.will_draw_layers.size());
  EXPECT_EQ(host_impl_->active_tree()->root_layer(), frame.will_draw_layers[0]);
}

class LayerTreeHostImplTestDeferredInitialize : public LayerTreeHostImplTest {
 protected:
  virtual void SetUp() OVERRIDE {
    LayerTreeHostImplTest::SetUp();

    set_reduce_memory_result(false);

    scoped_ptr<FakeOutputSurface> output_surface(
        FakeOutputSurface::CreateDeferredGL(
            scoped_ptr<SoftwareOutputDevice>(new CountingSoftwareDevice())));
    output_surface_ = output_surface.get();

    EXPECT_TRUE(host_impl_->InitializeRenderer(
        output_surface.PassAs<OutputSurface>()));

    scoped_ptr<SolidColorLayerImpl> root_layer =
        SolidColorLayerImpl::Create(host_impl_->active_tree(), 1);
    SetupRootLayerImpl(root_layer.PassAs<LayerImpl>());

    onscreen_context_provider_ = TestContextProvider::Create();
    offscreen_context_provider_ = TestContextProvider::Create();
  }

  FakeOutputSurface* output_surface_;
  scoped_refptr<TestContextProvider> onscreen_context_provider_;
  scoped_refptr<TestContextProvider> offscreen_context_provider_;
};


TEST_F(LayerTreeHostImplTestDeferredInitialize, Success) {
  // Software draw.
  DrawFrame();

  EXPECT_FALSE(host_impl_->output_surface()->context_provider());
  EXPECT_FALSE(host_impl_->offscreen_context_provider());

  // DeferredInitialize and hardware draw.
  EXPECT_TRUE(output_surface_->InitializeAndSetContext3d(
      onscreen_context_provider_, offscreen_context_provider_));
  EXPECT_EQ(onscreen_context_provider_,
            host_impl_->output_surface()->context_provider());
  EXPECT_EQ(offscreen_context_provider_,
            host_impl_->offscreen_context_provider());

  // Defer intialized GL draw.
  DrawFrame();

  // Revert back to software.
  output_surface_->ReleaseGL();
  EXPECT_FALSE(host_impl_->output_surface()->context_provider());
  EXPECT_FALSE(host_impl_->offscreen_context_provider());

  // Software draw again.
  DrawFrame();
}

TEST_F(LayerTreeHostImplTestDeferredInitialize, Fails_OnscreenContext_0) {
  // Software draw.
  DrawFrame();

  // Fail initialization of the onscreen context before the OutputSurface binds
  // it to the thread.
  onscreen_context_provider_->UnboundTestContext3d()
      ->set_times_make_current_succeeds(0);

  EXPECT_FALSE(host_impl_->output_surface()->context_provider());
  EXPECT_FALSE(host_impl_->offscreen_context_provider());
  EXPECT_FALSE(did_lose_output_surface_);

  // DeferredInitialize fails.
  EXPECT_FALSE(output_surface_->InitializeAndSetContext3d(
      onscreen_context_provider_, offscreen_context_provider_));
  EXPECT_FALSE(host_impl_->output_surface()->context_provider());
  EXPECT_FALSE(host_impl_->offscreen_context_provider());

  // Software draw again.
  DrawFrame();
}

TEST_F(LayerTreeHostImplTestDeferredInitialize, Fails_OnscreenContext_1) {
  // Software draw.
  DrawFrame();

  // Fail initialization of the onscreen context after the OutputSurface binds
  // it to the thread.
  onscreen_context_provider_->UnboundTestContext3d()
      ->set_times_make_current_succeeds(2);

  EXPECT_FALSE(host_impl_->output_surface()->context_provider());
  EXPECT_FALSE(host_impl_->offscreen_context_provider());
  EXPECT_FALSE(did_lose_output_surface_);

  // DeferredInitialize fails.
  EXPECT_FALSE(output_surface_->InitializeAndSetContext3d(
      onscreen_context_provider_, offscreen_context_provider_));
  EXPECT_FALSE(host_impl_->output_surface()->context_provider());
  EXPECT_FALSE(host_impl_->offscreen_context_provider());

  // We lose the output surface.
  EXPECT_TRUE(did_lose_output_surface_);
}

TEST_F(LayerTreeHostImplTestDeferredInitialize, Fails_OnscreenContext_2) {
  // Software draw.
  DrawFrame();

  // Fail initialization of the onscreen context after the OutputSurface binds
  // it to the thread and during renderer initialization.
  onscreen_context_provider_->UnboundTestContext3d()
      ->set_times_make_current_succeeds(1);

  EXPECT_FALSE(host_impl_->output_surface()->context_provider());
  EXPECT_FALSE(host_impl_->offscreen_context_provider());
  EXPECT_FALSE(did_lose_output_surface_);

  // DeferredInitialize fails.
  EXPECT_FALSE(output_surface_->InitializeAndSetContext3d(
      onscreen_context_provider_, offscreen_context_provider_));
  EXPECT_FALSE(host_impl_->output_surface()->context_provider());
  EXPECT_FALSE(host_impl_->offscreen_context_provider());

  // We lose the output surface.
  EXPECT_TRUE(did_lose_output_surface_);
}

TEST_F(LayerTreeHostImplTestDeferredInitialize, Fails_OffscreenContext) {
  // Software draw.
  DrawFrame();

  // Fail initialization of the offscreen context.
  offscreen_context_provider_->UnboundTestContext3d()
      ->set_times_make_current_succeeds(0);

  EXPECT_FALSE(host_impl_->output_surface()->context_provider());
  EXPECT_FALSE(host_impl_->offscreen_context_provider());
  EXPECT_FALSE(did_lose_output_surface_);

  // DeferredInitialize fails.
  EXPECT_FALSE(output_surface_->InitializeAndSetContext3d(
      onscreen_context_provider_, offscreen_context_provider_));
  EXPECT_FALSE(host_impl_->output_surface()->context_provider());
  EXPECT_FALSE(host_impl_->offscreen_context_provider());

  // We lose the output surface.
  EXPECT_TRUE(did_lose_output_surface_);
}

// Checks that we have a non-0 default allocation if we pass a context that
// doesn't support memory management extensions.
TEST_F(LayerTreeHostImplTest, DefaultMemoryAllocation) {
  LayerTreeSettings settings;
  host_impl_ = LayerTreeHostImpl::Create(
      settings, this, &proxy_, &stats_instrumentation_, NULL);

  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(TestWebGraphicsContext3D::Create()));
  host_impl_->InitializeRenderer(output_surface.Pass());
  EXPECT_LT(0ul, host_impl_->memory_allocation_limit_bytes());
}

TEST_F(LayerTreeHostImplTest, MemoryPolicy) {
  ManagedMemoryPolicy policy1(
      456, gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING, 1000);
  int everything_cutoff_value = ManagedMemoryPolicy::PriorityCutoffToValue(
      gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING);
  int nothing_cutoff_value = ManagedMemoryPolicy::PriorityCutoffToValue(
      gpu::MemoryAllocation::CUTOFF_ALLOW_NOTHING);

  host_impl_->SetVisible(true);
  host_impl_->SetMemoryPolicy(policy1);
  EXPECT_EQ(policy1.bytes_limit_when_visible, current_limit_bytes_);
  EXPECT_EQ(everything_cutoff_value, current_priority_cutoff_value_);

  host_impl_->SetVisible(false);
  EXPECT_EQ(0u, current_limit_bytes_);
  EXPECT_EQ(nothing_cutoff_value, current_priority_cutoff_value_);

  host_impl_->SetVisible(true);
  EXPECT_EQ(policy1.bytes_limit_when_visible, current_limit_bytes_);
  EXPECT_EQ(everything_cutoff_value, current_priority_cutoff_value_);
}

class LayerTreeHostImplTestManageTiles : public LayerTreeHostImplTest {
 public:
  virtual void SetUp() OVERRIDE {
    LayerTreeSettings settings;
    settings.impl_side_painting = true;

    fake_host_impl_ = new FakeLayerTreeHostImpl(settings, &proxy_);
    host_impl_.reset(fake_host_impl_);
    host_impl_->InitializeRenderer(CreateOutputSurface());
    host_impl_->SetViewportSize(gfx::Size(10, 10));
  }

  FakeLayerTreeHostImpl* fake_host_impl_;
};

TEST_F(LayerTreeHostImplTestManageTiles, ManageTilesWhenInvisible) {
  fake_host_impl_->DidModifyTilePriorities();
  EXPECT_TRUE(fake_host_impl_->manage_tiles_needed());
  fake_host_impl_->SetVisible(false);
  EXPECT_FALSE(fake_host_impl_->manage_tiles_needed());
}

TEST_F(LayerTreeHostImplTest, UIResourceManagement) {
  scoped_ptr<TestWebGraphicsContext3D> context =
      TestWebGraphicsContext3D::Create();
  TestWebGraphicsContext3D* context3d = context.get();
  scoped_ptr<OutputSurface> output_surface = CreateFakeOutputSurface();
  host_impl_->InitializeRenderer(output_surface.Pass());

  EXPECT_EQ(0u, context3d->NumTextures());

  SkBitmap skbitmap;
  skbitmap.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
  skbitmap.allocPixels();
  skbitmap.setImmutable();

  UIResourceId ui_resource_id = 1;
  UIResourceBitmap bitmap(skbitmap);
  host_impl_->CreateUIResource(ui_resource_id, bitmap);
  EXPECT_EQ(1u, context3d->NumTextures());
  ResourceProvider::ResourceId id1 =
      host_impl_->ResourceIdForUIResource(ui_resource_id);
  EXPECT_NE(0u, id1);

  // Multiple requests with the same id is allowed.  The previous texture is
  // deleted.
  host_impl_->CreateUIResource(ui_resource_id, bitmap);
  EXPECT_EQ(1u, context3d->NumTextures());
  ResourceProvider::ResourceId id2 =
      host_impl_->ResourceIdForUIResource(ui_resource_id);
  EXPECT_NE(0u, id2);
  EXPECT_NE(id1, id2);

  // Deleting invalid UIResourceId is allowed and does not change state.
  host_impl_->DeleteUIResource(-1);
  EXPECT_EQ(1u, context3d->NumTextures());

  // Should return zero for invalid UIResourceId.  Number of textures should
  // not change.
  EXPECT_EQ(0u, host_impl_->ResourceIdForUIResource(-1));
  EXPECT_EQ(1u, context3d->NumTextures());

  host_impl_->DeleteUIResource(ui_resource_id);
  EXPECT_EQ(0u, host_impl_->ResourceIdForUIResource(ui_resource_id));
  EXPECT_EQ(0u, context3d->NumTextures());

  // Should not change state for multiple deletion on one UIResourceId
  host_impl_->DeleteUIResource(ui_resource_id);
  EXPECT_EQ(0u, context3d->NumTextures());
}

void ShutdownReleasesContext_Callback(scoped_ptr<CopyOutputResult> result) {
}

TEST_F(LayerTreeHostImplTest, ShutdownReleasesContext) {
  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create();

  host_impl_ = LayerTreeHostImpl::Create(
      LayerTreeSettings(), this, &proxy_, &stats_instrumentation_, NULL);
  host_impl_->InitializeRenderer(FakeOutputSurface::Create3d(context_provider)
                                     .PassAs<OutputSurface>());
  host_impl_->SetViewportSize(gfx::Size(10, 10));

  SetupRootLayerImpl(LayerImpl::Create(host_impl_->active_tree(), 1));

  ScopedPtrVector<CopyOutputRequest> requests;
  requests.push_back(CopyOutputRequest::CreateRequest(
      base::Bind(&ShutdownReleasesContext_Callback)));

  host_impl_->active_tree()->root_layer()->PassCopyRequests(&requests);

  LayerTreeHostImpl::FrameData frame;
  EXPECT_TRUE(host_impl_->PrepareToDraw(&frame, gfx::Rect()));
  host_impl_->DrawLayers(&frame, gfx::FrameTime::Now());
  host_impl_->DidDrawAllLayers(frame);

  // The CopyOutputResult's callback has a ref on the ContextProvider and a
  // texture in a texture mailbox.
  EXPECT_FALSE(context_provider->HasOneRef());
  EXPECT_EQ(1u, context_provider->TestContext3d()->NumTextures());

  host_impl_.reset();

  // The CopyOutputResult's callback was cancelled, the CopyOutputResult
  // released, and the texture deleted.
  EXPECT_TRUE(context_provider->HasOneRef());
  EXPECT_EQ(0u, context_provider->TestContext3d()->NumTextures());
}

TEST_F(LayerTreeHostImplTest, TouchFlingShouldNotBubble) {
  // When flinging via touch, only the child should scroll (we should not
  // bubble).
  gfx::Size surface_size(10, 10);
  gfx::Size content_size(20, 20);
  scoped_ptr<LayerImpl> root = CreateScrollableLayer(1, content_size);
  scoped_ptr<LayerImpl> child = CreateScrollableLayer(2, content_size);

  root->AddChild(child.Pass());

  host_impl_->SetViewportSize(surface_size);
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  InitializeRendererAndDrawFrame();
  {
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(),
                                      InputHandler::Gesture));

    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->FlingScrollBegin());

    gfx::Vector2d scroll_delta(0, 100);
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);

    host_impl_->ScrollEnd();

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();

    // Only the child should have scrolled.
    ASSERT_EQ(1u, scroll_info->scrolls.size());
    ExpectNone(*scroll_info.get(),
               host_impl_->active_tree()->root_layer()->id());
  }
}

TEST_F(LayerTreeHostImplTest, TouchFlingShouldBubbleIfPrecedingScrollBubbled) {
  // When flinging via touch, bubble scrolls if the touch scroll
  // immediately preceding the fling bubbled.
  gfx::Size surface_size(10, 10);
  gfx::Size root_content_size(10, 20);
  gfx::Size child_content_size(40, 40);
  scoped_ptr<LayerImpl> root = CreateScrollableLayer(1, root_content_size);
  scoped_ptr<LayerImpl> child = CreateScrollableLayer(2, child_content_size);

  root->AddChild(child.Pass());

  host_impl_->SetViewportSize(surface_size);
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  InitializeRendererAndDrawFrame();
  {
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(),
                                      InputHandler::Gesture));

    // Touch scroll before starting the fling. The second scroll should bubble.
    EXPECT_TRUE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 100)));
    EXPECT_TRUE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 5)));

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();

    // The root should have (partially) scrolled.
    EXPECT_EQ(2u, scroll_info->scrolls.size());
    ExpectContains(*scroll_info.get(),
                   host_impl_->active_tree()->root_layer()->id(),
                   gfx::Vector2d(0, 5));

    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->FlingScrollBegin());

    EXPECT_TRUE(host_impl_->ScrollBy(gfx::Point(), gfx::Vector2d(0, 5)));
    host_impl_->ScrollEnd();

    // The root should have (fully) scrolled from the fling.
    scroll_info = host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(2u, scroll_info->scrolls.size());
    ExpectContains(*scroll_info.get(),
                   host_impl_->active_tree()->root_layer()->id(),
                   gfx::Vector2d(0, 10));
  }
}

TEST_F(LayerTreeHostImplTest, WheelFlingShouldBubble) {
  // When flinging via wheel, the root should eventually scroll (we should
  // bubble).
  gfx::Size surface_size(10, 10);
  gfx::Size content_size(20, 20);
  scoped_ptr<LayerImpl> root = CreateScrollableLayer(1, content_size);
  scoped_ptr<LayerImpl> child = CreateScrollableLayer(2, content_size);

  root->AddChild(child.Pass());

  host_impl_->SetViewportSize(surface_size);
  host_impl_->active_tree()->SetRootLayer(root.Pass());
  host_impl_->active_tree()->DidBecomeActive();
  InitializeRendererAndDrawFrame();
  {
    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->ScrollBegin(gfx::Point(), InputHandler::Wheel));

    EXPECT_EQ(InputHandler::ScrollStarted,
              host_impl_->FlingScrollBegin());

    gfx::Vector2d scroll_delta(0, 100);
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);
    host_impl_->ScrollBy(gfx::Point(), scroll_delta);

    host_impl_->ScrollEnd();

    scoped_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();

    // The root should have scrolled.
    ASSERT_EQ(2u, scroll_info->scrolls.size());
    ExpectContains(*scroll_info.get(),
                   host_impl_->active_tree()->root_layer()->id(),
                   gfx::Vector2d(0, 10));
  }
}

}  // namespace
}  // namespace cc
