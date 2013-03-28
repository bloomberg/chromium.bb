// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <algorithm>

#include "base/synchronization/lock.h"
#include "cc/animation/timing_function.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/scrollbar_layer.h"
#include "cc/output/output_surface.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/scheduler/frame_rate_controller.h"
#include "cc/test/fake_content_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_scrollbar_layer.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/occlusion_tracker_test_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/thread_proxy.h"
#include "skia/ext/refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

namespace cc {
namespace {

class LayerTreeHostTest : public LayerTreeTest {
};

// Test interleaving of redraws and commits
class LayerTreeHostTestCommitingWithContinuousRedraw
    : public LayerTreeHostTest {
 public:
  LayerTreeHostTestCommitingWithContinuousRedraw()
      : num_complete_commits_(0), num_draws_(0) {}

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_complete_commits_++;
    if (num_complete_commits_ == 2)
      EndTest();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (num_draws_ == 1)
      PostSetNeedsCommitToMainThread();
    num_draws_++;
    PostSetNeedsRedrawToMainThread();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_complete_commits_;
  int num_draws_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestCommitingWithContinuousRedraw);

// Two setNeedsCommits in a row should lead to at least 1 commit and at least 1
// draw with frame 0.
class LayerTreeHostTestSetNeedsCommit1 : public LayerTreeHostTest {
 public:
  LayerTreeHostTestSetNeedsCommit1() : num_commits_(0), num_draws_(0) {}

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_draws_++;
    if (!impl->active_tree()->source_frame_number())
      EndTest();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_commits_++;
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_GE(1, num_commits_);
    EXPECT_GE(1, num_draws_);
  }

 private:
  int num_commits_;
  int num_draws_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestSetNeedsCommit1);

// A setNeedsCommit should lead to 1 commit. Issuing a second commit after that
// first committed frame draws should lead to another commit.
class LayerTreeHostTestSetNeedsCommit2 : public LayerTreeHostTest {
 public:
  LayerTreeHostTestSetNeedsCommit2() : num_commits_(0), num_draws_(0) {}

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (impl->active_tree()->source_frame_number() == 0)
      PostSetNeedsCommitToMainThread();
    else if (impl->active_tree()->source_frame_number() == 1)
      EndTest();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_commits_++;
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(2, num_commits_);
    EXPECT_GE(2, num_draws_);
  }

 private:
  int num_commits_;
  int num_draws_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestSetNeedsCommit2);

// 1 setNeedsRedraw after the first commit has completed should lead to 1
// additional draw.
class LayerTreeHostTestSetNeedsRedraw : public LayerTreeHostTest {
 public:
  LayerTreeHostTestSetNeedsRedraw() : num_commits_(0), num_draws_(0) {}

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    EXPECT_EQ(0, impl->active_tree()->source_frame_number());
    if (!num_draws_) {
      // Redraw again to verify that the second redraw doesn't commit.
      PostSetNeedsRedrawToMainThread();
    } else {
      EndTest();
    }
    num_draws_++;
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    EXPECT_EQ(0, num_draws_);
    num_commits_++;
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_GE(2, num_draws_);
    EXPECT_EQ(1, num_commits_);
  }

 private:
  int num_commits_;
  int num_draws_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestSetNeedsRedraw);

class LayerTreeHostTestNoExtraCommitFromInvalidate : public LayerTreeHostTest {
 public:
  LayerTreeHostTestNoExtraCommitFromInvalidate()
      : root_layer_(ContentLayer::Create(&client_)) {}

  virtual void BeginTest() OVERRIDE {
    root_layer_->SetAutomaticallyComputeRasterScale(false);
    root_layer_->SetIsDrawable(true);
    root_layer_->SetBounds(gfx::Size(1, 1));
    layer_tree_host()->SetRootLayer(root_layer_);
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    switch (layer_tree_host()->commit_number()) {
      case 1:
        // Changing the content bounds will cause a single commit!
        root_layer_->SetRasterScale(4.f);
        break;
      default:
        // No extra commits.
        EXPECT_EQ(2, layer_tree_host()->commit_number());
        EndTest();
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<ContentLayer> root_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestNoExtraCommitFromInvalidate);

class LayerTreeHostTestCompositeAndReadback : public LayerTreeHostTest {
 public:
  LayerTreeHostTestCompositeAndReadback() : num_commits_(0) {}

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommit() OVERRIDE {
    num_commits_++;
    if (num_commits_ == 1) {
      char pixels[4];
      layer_tree_host()->CompositeAndReadback(&pixels, gfx::Rect(0, 0, 1, 1));
    } else if (num_commits_ == 2) {
      // This is inside the readback. We should get another commit after it.
    } else if (num_commits_ == 3) {
      EndTest();
    } else {
      NOTREACHED();
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_commits_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestCompositeAndReadback);

class LayerTreeHostTestCompositeAndReadbackBeforePreviousCommitDraws
    : public LayerTreeHostTest {
 public:
  LayerTreeHostTestCompositeAndReadbackBeforePreviousCommitDraws()
      : num_commits_(0) {}

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommit() OVERRIDE {
    num_commits_++;
    if (num_commits_ == 1) {
      layer_tree_host()->SetNeedsCommit();
    } else if (num_commits_ == 2) {
      char pixels[4];
      layer_tree_host()->CompositeAndReadback(&pixels, gfx::Rect(0, 0, 1, 1));
    } else if (num_commits_ == 3) {
      // This is inside the readback. We should get another commit after it.
    } else if (num_commits_ == 4) {
      EndTest();
    } else {
      NOTREACHED();
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_commits_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostTestCompositeAndReadbackBeforePreviousCommitDraws);

// If the layerTreeHost says it can't draw, Then we should not try to draw.
class LayerTreeHostTestCanDrawBlocksDrawing : public LayerTreeHostTest {
 public:
  LayerTreeHostTestCanDrawBlocksDrawing() : num_commits_(0), done_(false) {}

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (done_)
      return;
    // Only the initial draw should bring us here.
    EXPECT_TRUE(impl->CanDraw());
    EXPECT_EQ(0, impl->active_tree()->source_frame_number());
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (done_)
      return;
    if (num_commits_ >= 1) {
      // After the first commit, we should not be able to draw.
      EXPECT_FALSE(impl->CanDraw());
    }
  }

  virtual void DidCommit() OVERRIDE {
    num_commits_++;
    if (num_commits_ == 1) {
      // Make the viewport empty so the host says it can't draw.
      layer_tree_host()->SetViewportSize(gfx::Size(0, 0), gfx::Size(0, 0));
    } else if (num_commits_ == 2) {
      char pixels[4];
      layer_tree_host()->CompositeAndReadback(&pixels, gfx::Rect(0, 0, 1, 1));
    } else if (num_commits_ == 3) {
      // Let it draw so we go idle and end the test.
      layer_tree_host()->SetViewportSize(gfx::Size(1, 1), gfx::Size(1, 1));
      done_ = true;
      EndTest();
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_commits_;
  bool done_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestCanDrawBlocksDrawing);

// beginLayerWrite should prevent draws from executing until a commit occurs
class LayerTreeHostTestWriteLayersRedraw : public LayerTreeHostTest {
 public:
  LayerTreeHostTestWriteLayersRedraw() : num_commits_(0), num_draws_(0) {}

  virtual void BeginTest() OVERRIDE {
    PostAcquireLayerTextures();
    PostSetNeedsRedrawToMainThread();  // should be inhibited without blocking
    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_draws_++;
    EXPECT_EQ(num_draws_, num_commits_);
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_commits_++;
    EndTest();
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(1, num_commits_); }

 private:
  int num_commits_;
  int num_draws_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestWriteLayersRedraw);

// Verify that when resuming visibility, Requesting layer write permission
// will not deadlock the main thread even though there are not yet any
// scheduled redraws. This behavior is critical for reliably surviving tab
// switching. There are no failure conditions to this test, it just passes
// by not timing out.
class LayerTreeHostTestWriteLayersAfterVisible : public LayerTreeHostTest {
 public:
  LayerTreeHostTestWriteLayersAfterVisible() : num_commits_(0) {}

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_commits_++;
    if (num_commits_ == 2)
      EndTest();
    else if (num_commits_ < 2) {
      PostSetVisibleToMainThread(false);
      PostSetVisibleToMainThread(true);
      PostAcquireLayerTextures();
      PostSetNeedsCommitToMainThread();
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_commits_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestWriteLayersAfterVisible);

// A compositeAndReadback while invisible should force a normal commit without
// assertion.
class LayerTreeHostTestCompositeAndReadbackWhileInvisible
    : public LayerTreeHostTest {
 public:
  LayerTreeHostTestCompositeAndReadbackWhileInvisible() : num_commits_(0) {}

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    num_commits_++;
    if (num_commits_ == 1) {
      layer_tree_host()->SetVisible(false);
      layer_tree_host()->SetNeedsCommit();
      layer_tree_host()->SetNeedsCommit();
      char pixels[4];
      layer_tree_host()->CompositeAndReadback(&pixels, gfx::Rect(0, 0, 1, 1));
    } else {
      EndTest();
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_commits_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestCompositeAndReadbackWhileInvisible);

class LayerTreeHostTestAbortFrameWhenInvisible : public LayerTreeHostTest {
 public:
  LayerTreeHostTestAbortFrameWhenInvisible() {}

  virtual void BeginTest() OVERRIDE {
    // Request a commit (from the main thread), Which will trigger the commit
    // flow from the impl side.
    layer_tree_host()->SetNeedsCommit();
    // Then mark ourselves as not visible before processing any more messages
    // on the main thread.
    layer_tree_host()->SetVisible(false);
    // If we make it without kicking a frame, we pass!
    EndTestAfterDelay(1);
  }

  virtual void Layout() OVERRIDE {
    ASSERT_FALSE(true);
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
};

MULTI_THREAD_TEST_F(LayerTreeHostTestAbortFrameWhenInvisible);

// This test verifies that properties on the layer tree host are commited
// to the impl side.
class LayerTreeHostTestCommit : public LayerTreeHostTest {
 public:
  LayerTreeHostTestCommit() {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(20, 20), gfx::Size(20, 20));
    layer_tree_host()->set_background_color(SK_ColorGRAY);
    layer_tree_host()->SetPageScaleFactorAndLimits(5.f, 5.f, 5.f);

    PostSetNeedsCommitToMainThread();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    EXPECT_EQ(gfx::Size(20, 20), impl->layout_viewport_size());
    EXPECT_EQ(SK_ColorGRAY, impl->active_tree()->background_color());
    EXPECT_EQ(5.f, impl->active_tree()->page_scale_factor());

    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}
};

MULTI_THREAD_TEST_F(LayerTreeHostTestCommit);

// Verifies that StartPageScaleAnimation events propagate correctly
// from LayerTreeHost to LayerTreeHostImpl in the MT compositor.
class LayerTreeHostTestStartPageScaleAnimation : public LayerTreeHostTest {
public:
 LayerTreeHostTestStartPageScaleAnimation() {}

 virtual void BeginTest() OVERRIDE {
   layer_tree_host()->root_layer()->SetScrollable(true);
   layer_tree_host()->root_layer()->SetScrollOffset(gfx::Vector2d());
   layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.5f, 2.f);
   layer_tree_host()->StartPageScaleAnimation(
       gfx::Vector2d(), false, 1.25f, base::TimeDelta());
   PostSetNeedsCommitToMainThread();
   PostSetNeedsRedrawToMainThread();
 }

 virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta, float scale)
     OVERRIDE {
   gfx::Vector2d offset = layer_tree_host()->root_layer()->scroll_offset();
   layer_tree_host()->root_layer()->SetScrollOffset(offset + scroll_delta);
   layer_tree_host()->SetPageScaleFactorAndLimits(scale, 0.5f, 2.f);
 }

 virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
   impl->ProcessScrollDeltas();
   // We get one commit before the first draw, and the animation doesn't happen
   // until the second draw.
   if (impl->active_tree()->source_frame_number() == 1) {
     EXPECT_EQ(1.25f, impl->active_tree()->page_scale_factor());
     EndTest();
   } else {
     PostSetNeedsRedrawToMainThread();
   }
 }

 virtual void AfterTest() OVERRIDE {}
};

MULTI_THREAD_TEST_F(LayerTreeHostTestStartPageScaleAnimation);

class LayerTreeHostTestSetVisible : public LayerTreeHostTest {
 public:
  LayerTreeHostTestSetVisible() : num_draws_(0) {}

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
    PostSetVisibleToMainThread(false);
    // This is suppressed while we're invisible.
    PostSetNeedsRedrawToMainThread();
    // Triggers the redraw.
    PostSetVisibleToMainThread(true);
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    EXPECT_TRUE(impl->visible());
    ++num_draws_;
    EndTest();
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(1, num_draws_); }

 private:
  int num_draws_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestSetVisible);

class TestOpacityChangeLayerDelegate : public ContentLayerClient {
 public:
  TestOpacityChangeLayerDelegate() : test_layer_(0) {}

  void SetTestLayer(Layer* test_layer) { test_layer_ = test_layer; }

  virtual void PaintContents(SkCanvas*, gfx::Rect, gfx::RectF*) OVERRIDE {
    // Set layer opacity to 0.
    if (test_layer_)
      test_layer_->SetOpacity(0.f);
  }
  virtual void DidChangeLayerCanUseLCDText() OVERRIDE {}

 private:
  Layer* test_layer_;
};

class ContentLayerWithUpdateTracking : public ContentLayer {
 public:
  static scoped_refptr<ContentLayerWithUpdateTracking> Create(
      ContentLayerClient* client) {
    return make_scoped_refptr(new ContentLayerWithUpdateTracking(client));
  }

  int PaintContentsCount() { return paint_contents_count_; }
  void ResetPaintContentsCount() { paint_contents_count_ = 0; }

  virtual void Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker* occlusion) OVERRIDE {
    ContentLayer::Update(queue, occlusion);
    paint_contents_count_++;
  }

 private:
  explicit ContentLayerWithUpdateTracking(ContentLayerClient* client)
      : ContentLayer(client), paint_contents_count_(0) {
    SetAnchorPoint(gfx::PointF(0.f, 0.f));
    SetBounds(gfx::Size(10, 10));
    SetIsDrawable(true);
  }
  virtual ~ContentLayerWithUpdateTracking() {}

  int paint_contents_count_;
};

// Layer opacity change during paint should not prevent compositor resources
// from being updated during commit.
class LayerTreeHostTestOpacityChange : public LayerTreeHostTest {
 public:
  LayerTreeHostTestOpacityChange()
      : test_opacity_change_delegate_(),
        update_check_layer_(ContentLayerWithUpdateTracking::Create(
            &test_opacity_change_delegate_)) {
    test_opacity_change_delegate_.SetTestLayer(update_check_layer_.get());
  }

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
    layer_tree_host()->root_layer()->AddChild(update_check_layer_);

    PostSetNeedsCommitToMainThread();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {
    // Update() should have been called once.
    EXPECT_EQ(1, update_check_layer_->PaintContentsCount());

    // clear update_check_layer_ so LayerTreeHost dies.
    update_check_layer_ = NULL;
  }

 private:
  TestOpacityChangeLayerDelegate test_opacity_change_delegate_;
  scoped_refptr<ContentLayerWithUpdateTracking> update_check_layer_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestOpacityChange);

class NoScaleContentLayer : public ContentLayer {
 public:
  static scoped_refptr<NoScaleContentLayer> Create(ContentLayerClient* client) {
    return make_scoped_refptr(new NoScaleContentLayer(client));
  }

  virtual void CalculateContentsScale(float ideal_contents_scale,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* contentBounds) OVERRIDE {
    // Skip over the ContentLayer's method to the base Layer class.
    Layer::CalculateContentsScale(ideal_contents_scale,
                                  animating_transform_to_screen,
                                  contents_scale_x,
                                  contents_scale_y,
                                  contentBounds);
  }

 private:
  explicit NoScaleContentLayer(ContentLayerClient* client)
      : ContentLayer(client) {}
  virtual ~NoScaleContentLayer() {}
};

class LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers
    : public LayerTreeHostTest {
 public:
  LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers()
      : root_layer_(NoScaleContentLayer::Create(&client_)),
        child_layer_(ContentLayer::Create(&client_)) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(40, 40), gfx::Size(60, 60));
    layer_tree_host()->SetDeviceScaleFactor(1.5);
    EXPECT_EQ(gfx::Size(40, 40), layer_tree_host()->layout_viewport_size());
    EXPECT_EQ(gfx::Size(60, 60), layer_tree_host()->device_viewport_size());

    root_layer_->AddChild(child_layer_);

    root_layer_->SetIsDrawable(true);
    root_layer_->SetBounds(gfx::Size(30, 30));
    root_layer_->SetAnchorPoint(gfx::PointF(0.f, 0.f));

    child_layer_->SetIsDrawable(true);
    child_layer_->SetPosition(gfx::Point(2, 2));
    child_layer_->SetBounds(gfx::Size(10, 10));
    child_layer_->SetAnchorPoint(gfx::PointF(0.f, 0.f));

    layer_tree_host()->SetRootLayer(root_layer_);

    ASSERT_TRUE(layer_tree_host()->InitializeRendererIfNeeded());
    ResourceUpdateQueue queue;
    layer_tree_host()->UpdateLayers(&queue, std::numeric_limits<size_t>::max());
    PostSetNeedsCommitToMainThread();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // Should only do one commit.
    EXPECT_EQ(0, impl->active_tree()->source_frame_number());
    // Device scale factor should come over to impl.
    EXPECT_NEAR(impl->device_scale_factor(), 1.5f, 0.00001f);

    // Both layers are on impl.
    ASSERT_EQ(1u, impl->active_tree()->root_layer()->children().size());

    // Device viewport is scaled.
    EXPECT_EQ(gfx::Size(40, 40), impl->layout_viewport_size());
    EXPECT_EQ(gfx::Size(60, 60), impl->device_viewport_size());

    LayerImpl* root = impl->active_tree()->root_layer();
    LayerImpl* child = impl->active_tree()->root_layer()->children()[0];

    // Positions remain in layout pixels.
    EXPECT_EQ(gfx::Point(0, 0), root->position());
    EXPECT_EQ(gfx::Point(2, 2), child->position());

    // Compute all the layer transforms for the frame.
    LayerTreeHostImpl::FrameData frame_data;
    impl->PrepareToDraw(&frame_data);
    impl->DidDrawAllLayers(frame_data);

    const LayerTreeHostImpl::LayerList& render_surface_layer_list =
        *frame_data.render_surface_layer_list;

    // Both layers should be drawing into the root render surface.
    ASSERT_EQ(1u, render_surface_layer_list.size());
    ASSERT_EQ(root->render_surface(),
              render_surface_layer_list[0]->render_surface());
    ASSERT_EQ(2u, root->render_surface()->layer_list().size());

    // The root render surface is the size of the viewport.
    EXPECT_RECT_EQ(gfx::Rect(0, 0, 60, 60),
                   root->render_surface()->content_rect());

    // The content bounds of the child should be scaled.
    gfx::Size child_bounds_scaled =
        gfx::ToCeiledSize(gfx::ScaleSize(child->bounds(), 1.5));
    EXPECT_EQ(child_bounds_scaled, child->content_bounds());

    gfx::Transform scale_transform;
    scale_transform.Scale(impl->device_scale_factor(),
                          impl->device_scale_factor());

    // The root layer is scaled by 2x.
    gfx::Transform root_screen_space_transform = scale_transform;
    gfx::Transform root_draw_transform = scale_transform;

    EXPECT_EQ(root_draw_transform, root->draw_transform());
    EXPECT_EQ(root_screen_space_transform, root->screen_space_transform());

    // The child is at position 2,2, which is transformed to 3,3 after the scale
    gfx::Transform child_screen_space_transform;
    child_screen_space_transform.Translate(3.f, 3.f);
    gfx::Transform child_draw_transform = child_screen_space_transform;

    EXPECT_TRANSFORMATION_MATRIX_EQ(child_draw_transform,
                                    child->draw_transform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(child_screen_space_transform,
                                    child->screen_space_transform());

    EndTest();
  }

  virtual void AfterTest() OVERRIDE {
    root_layer_ = NULL;
    child_layer_ = NULL;
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<NoScaleContentLayer> root_layer_;
  scoped_refptr<ContentLayer> child_layer_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers);

// Verify atomicity of commits and reuse of textures.
class LayerTreeHostTestAtomicCommit : public LayerTreeHostTest {
 public:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    // Make sure partial texture updates are turned off.
    settings->max_partial_texture_updates = 0;
    // Linear fade animator prevents scrollbars from drawing immediately.
    settings->use_linear_fade_scrollbar_animator = false;
  }

  virtual void SetupTree() OVERRIDE {
    layer_ = FakeContentLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(10, 20));

    bool paint_scrollbar = true;
    bool has_thumb = false;
    scrollbar_ =
        FakeScrollbarLayer::Create(paint_scrollbar, has_thumb, layer_->id());
    scrollbar_->SetPosition(gfx::Point(0, 10));
    scrollbar_->SetBounds(gfx::Size(10, 10));

    layer_->AddChild(scrollbar_);

    layer_tree_host()->SetRootLayer(layer_);
    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ASSERT_EQ(0u, layer_tree_host()->settings().max_partial_texture_updates);

    TestWebGraphicsContext3D* context = static_cast<TestWebGraphicsContext3D*>(
        impl->output_surface()->context3d());

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        // Number of textures should be one for each layer
        ASSERT_EQ(2, context->NumTextures());
        // Number of textures used for commit should be one for each layer.
        EXPECT_EQ(2, context->NumUsedTextures());
        // Verify that used texture is correct.
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(0)));
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(1)));

        context->ResetUsedTextures();
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        // Number of textures should be doubled as the first textures
        // are used by impl thread and cannot by used for update.
        ASSERT_EQ(4, context->NumTextures());
        // Number of textures used for commit should still be
        // one for each layer.
        EXPECT_EQ(2, context->NumUsedTextures());
        // First textures should not have been used.
        EXPECT_FALSE(context->UsedTexture(context->TextureAt(0)));
        EXPECT_FALSE(context->UsedTexture(context->TextureAt(1)));
        // New textures should have been used.
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(2)));
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));

        context->ResetUsedTextures();
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    TestWebGraphicsContext3D* context = static_cast<TestWebGraphicsContext3D*>(
        impl->output_surface()->context3d());

    // Number of textures used for draw should always be one for each layer.
    EXPECT_EQ(2, context->NumUsedTextures());
    context->ResetUsedTextures();
  }

  virtual void Layout() OVERRIDE {
    layer_->SetNeedsDisplay();
    scrollbar_->SetNeedsDisplay();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> layer_;
  scoped_refptr<FakeScrollbarLayer> scrollbar_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestAtomicCommit);

static void SetLayerPropertiesForTesting(Layer* layer,
                                         Layer* parent,
                                         const gfx::Transform& transform,
                                         gfx::PointF anchor,
                                         gfx::PointF position,
                                         gfx::Size bounds,
                                         bool opaque) {
  layer->RemoveAllChildren();
  if (parent)
    parent->AddChild(layer);
  layer->SetTransform(transform);
  layer->SetAnchorPoint(anchor);
  layer->SetPosition(position);
  layer->SetBounds(bounds);
  layer->SetContentsOpaque(opaque);
}

class LayerTreeHostTestAtomicCommitWithPartialUpdate
    : public LayerTreeHostTest {
 public:
  LayerTreeHostTestAtomicCommitWithPartialUpdate() : num_commits_(0) {}

  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    // Allow one partial texture update.
    settings->max_partial_texture_updates = 1;
    // Linear fade animator prevents scrollbars from drawing immediately.
    settings->use_linear_fade_scrollbar_animator = false;
  }

  virtual void SetupTree() OVERRIDE {
    parent_ = FakeContentLayer::Create(&client_);
    parent_->SetBounds(gfx::Size(10, 20));

    child_ = FakeContentLayer::Create(&client_);
    child_->SetPosition(gfx::Point(0, 10));
    child_->SetBounds(gfx::Size(3, 10));

    bool paint_scrollbar = true;
    bool has_thumb = false;
    scrollbar_with_paints_ =
        FakeScrollbarLayer::Create(paint_scrollbar, has_thumb, parent_->id());
    scrollbar_with_paints_->SetPosition(gfx::Point(3, 10));
    scrollbar_with_paints_->SetBounds(gfx::Size(3, 10));

    paint_scrollbar = false;
    scrollbar_without_paints_ =
        FakeScrollbarLayer::Create(paint_scrollbar, has_thumb, parent_->id());
    scrollbar_without_paints_->SetPosition(gfx::Point(6, 10));
    scrollbar_without_paints_->SetBounds(gfx::Size(3, 10));

    parent_->AddChild(child_);
    parent_->AddChild(scrollbar_with_paints_);
    parent_->AddChild(scrollbar_without_paints_);

    layer_tree_host()->SetRootLayer(parent_);
    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ASSERT_EQ(1u, layer_tree_host()->settings().max_partial_texture_updates);

    TestWebGraphicsContext3D* context = static_cast<TestWebGraphicsContext3D*>(
        impl->output_surface()->context3d());

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        // Number of textures should be one for each layer.
        ASSERT_EQ(4, context->NumTextures());
        // Number of textures used for commit should be one for each layer.
        EXPECT_EQ(4, context->NumUsedTextures());
        // Verify that used textures are correct.
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(0)));
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(1)));
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(2)));
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));

        context->ResetUsedTextures();
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        // Number of textures should be two for each content layer and one
        // for each scrollbar, since they always do a partial update.
        ASSERT_EQ(6, context->NumTextures());
        // Number of textures used for commit should be one for each content
        // layer, and one for the scrollbar layer that paints.
        EXPECT_EQ(3, context->NumUsedTextures());

        // First content textures should not have been used.
        EXPECT_FALSE(context->UsedTexture(context->TextureAt(0)));
        EXPECT_FALSE(context->UsedTexture(context->TextureAt(1)));
        // The non-painting scrollbar's texture wasn't updated.
        EXPECT_FALSE(context->UsedTexture(context->TextureAt(2)));
        // The painting scrollbar's partial update texture was used.
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));
        // New textures should have been used.
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(4)));
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(5)));

        context->ResetUsedTextures();
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        // Number of textures should be two for each content layer and one
        // for each scrollbar, since they always do a partial update.
        ASSERT_EQ(6, context->NumTextures());
        // Number of textures used for commit should be one for each content
        // layer, and one for the scrollbar layer that paints.
        EXPECT_EQ(3, context->NumUsedTextures());

        // The non-painting scrollbar's texture wasn't updated.
        EXPECT_FALSE(context->UsedTexture(context->TextureAt(2)));
        // The painting scrollbar does a partial update.
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));
        // One content layer does a partial update also.
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(4)));
        EXPECT_FALSE(context->UsedTexture(context->TextureAt(5)));

        context->ResetUsedTextures();
        PostSetNeedsCommitToMainThread();
        break;
      case 3:
        // No textures should be used for commit.
        EXPECT_EQ(0, context->NumUsedTextures());

        context->ResetUsedTextures();
        PostSetNeedsCommitToMainThread();
        break;
      case 4:
        // Number of textures used for commit should be two. One for the
        // content layer, and one for the painting scrollbar. The
        // non-painting scrollbar doesn't update its texture.
        EXPECT_EQ(2, context->NumUsedTextures());

        context->ResetUsedTextures();
        PostSetNeedsCommitToMainThread();
        break;
      case 5:
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    TestWebGraphicsContext3D* context = static_cast<TestWebGraphicsContext3D*>(
        impl->output_surface()->context3d());

    // Number of textures used for drawing should one per layer except for
    // frame 3 where the viewport only contains one layer.
    if (impl->active_tree()->source_frame_number() == 3)
      EXPECT_EQ(1, context->NumUsedTextures());
    else
      EXPECT_EQ(4, context->NumUsedTextures());

    context->ResetUsedTextures();
  }

  virtual void Layout() OVERRIDE {
    switch (num_commits_++) {
      case 0:
      case 1:
        parent_->SetNeedsDisplay();
        child_->SetNeedsDisplay();
        scrollbar_with_paints_->SetNeedsDisplay();
        scrollbar_without_paints_->SetNeedsDisplay();
        break;
      case 2:
        // Damage part of layers.
        parent_->SetNeedsDisplayRect(gfx::RectF(0.f, 0.f, 5.f, 5.f));
        child_->SetNeedsDisplayRect(gfx::RectF(0.f, 0.f, 5.f, 5.f));
        scrollbar_with_paints_->SetNeedsDisplayRect(
            gfx::RectF(0.f, 0.f, 5.f, 5.f));
        scrollbar_without_paints_->SetNeedsDisplayRect(
            gfx::RectF(0.f, 0.f, 5.f, 5.f));
        break;
      case 3:
        child_->SetNeedsDisplay();
        scrollbar_with_paints_->SetNeedsDisplay();
        scrollbar_without_paints_->SetNeedsDisplay();
        layer_tree_host()->SetViewportSize(gfx::Size(10, 10),
                                           gfx::Size(10, 10));
        break;
      case 4:
        layer_tree_host()->SetViewportSize(gfx::Size(10, 20),
                                           gfx::Size(10, 20));
        break;
      case 5:
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> parent_;
  scoped_refptr<FakeContentLayer> child_;
  scoped_refptr<FakeScrollbarLayer> scrollbar_with_paints_;
  scoped_refptr<FakeScrollbarLayer> scrollbar_without_paints_;
  int num_commits_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestAtomicCommitWithPartialUpdate);

class LayerTreeHostTestFinishAllRendering : public LayerTreeHostTest {
 public:
  LayerTreeHostTestFinishAllRendering() : once_(false), draw_count_(0) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetNeedsRedraw();
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    if (once_)
      return;
    once_ = true;
    layer_tree_host()->SetNeedsRedraw();
    layer_tree_host()->AcquireLayerTextures();
    {
      base::AutoLock lock(lock_);
      draw_count_ = 0;
    }
    layer_tree_host()->FinishAllRendering();
    {
      base::AutoLock lock(lock_);
      EXPECT_EQ(0, draw_count_);
    }
    EndTest();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    base::AutoLock lock(lock_);
    ++draw_count_;
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  bool once_;
  base::Lock lock_;
  int draw_count_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestFinishAllRendering);

class LayerTreeHostTestCompositeAndReadbackCleanup : public LayerTreeHostTest {
 public:
  LayerTreeHostTestCompositeAndReadbackCleanup() {}

  virtual void BeginTest() OVERRIDE {
    Layer* root_layer = layer_tree_host()->root_layer();

    char pixels[4];
    layer_tree_host()->CompositeAndReadback(static_cast<void*>(&pixels),
                                            gfx::Rect(0, 0, 1, 1));
    EXPECT_FALSE(root_layer->render_surface());

    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestCompositeAndReadbackCleanup);

class LayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit
    : public LayerTreeHostTest {
 public:
  LayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit()
      : root_layer_(ContentLayerWithUpdateTracking::Create(&fake_delegate_)),
        surface_layer1_(
            ContentLayerWithUpdateTracking::Create(&fake_delegate_)),
        replica_layer1_(
            ContentLayerWithUpdateTracking::Create(&fake_delegate_)),
        surface_layer2_(
            ContentLayerWithUpdateTracking::Create(&fake_delegate_)),
        replica_layer2_(
            ContentLayerWithUpdateTracking::Create(&fake_delegate_)) {}

  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->cache_render_pass_contents = true;
  }

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(100, 100),
                                       gfx::Size(100, 100));

    root_layer_->SetBounds(gfx::Size(100, 100));
    surface_layer1_->SetBounds(gfx::Size(100, 100));
    surface_layer1_->SetForceRenderSurface(true);
    surface_layer1_->SetOpacity(0.5f);
    surface_layer2_->SetBounds(gfx::Size(100, 100));
    surface_layer2_->SetForceRenderSurface(true);
    surface_layer2_->SetOpacity(0.5f);

    surface_layer1_->SetReplicaLayer(replica_layer1_.get());
    surface_layer2_->SetReplicaLayer(replica_layer2_.get());

    root_layer_->AddChild(surface_layer1_);
    surface_layer1_->AddChild(surface_layer2_);
    layer_tree_host()->SetRootLayer(root_layer_);

    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    Renderer* renderer = host_impl->renderer();
    RenderPass::Id surface1_render_pass_id = host_impl->active_tree()
        ->root_layer()->children()[0]->render_surface()->RenderPassId();
    RenderPass::Id surface2_render_pass_id =
        host_impl->active_tree()->root_layer()->children()[0]->children()[0]
        ->render_surface()->RenderPassId();

    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_TRUE(renderer->HaveCachedResourcesForRenderPassId(
            surface1_render_pass_id));
        EXPECT_TRUE(renderer->HaveCachedResourcesForRenderPassId(
            surface2_render_pass_id));

        // Reduce the memory limit to only fit the root layer and one render
        // surface. This prevents any contents drawing into surfaces
        // from being allocated.
        host_impl->SetManagedMemoryPolicy(
            ManagedMemoryPolicy(100 * 100 * 4 * 2));
        break;
      case 1:
        EXPECT_FALSE(renderer->HaveCachedResourcesForRenderPassId(
            surface1_render_pass_id));
        EXPECT_FALSE(renderer->HaveCachedResourcesForRenderPassId(
            surface2_render_pass_id));

        EndTest();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(2, root_layer_->PaintContentsCount());
    EXPECT_EQ(2, surface_layer1_->PaintContentsCount());
    EXPECT_EQ(2, surface_layer2_->PaintContentsCount());

    // Clear layer references so LayerTreeHost dies.
    root_layer_ = NULL;
    surface_layer1_ = NULL;
    replica_layer1_ = NULL;
    surface_layer2_ = NULL;
    replica_layer2_ = NULL;
  }

 private:
  FakeContentLayerClient fake_delegate_;
  scoped_refptr<ContentLayerWithUpdateTracking> root_layer_;
  scoped_refptr<ContentLayerWithUpdateTracking> surface_layer1_;
  scoped_refptr<ContentLayerWithUpdateTracking> replica_layer1_;
  scoped_refptr<ContentLayerWithUpdateTracking> surface_layer2_;
  scoped_refptr<ContentLayerWithUpdateTracking> replica_layer2_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit);

class EvictionTestLayer : public Layer {
 public:
  static scoped_refptr<EvictionTestLayer> Create() {
    return make_scoped_refptr(new EvictionTestLayer());
  }

  virtual void Update(ResourceUpdateQueue*,
                      const OcclusionTracker*) OVERRIDE;
  virtual bool DrawsContent() const OVERRIDE { return true; }

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* impl) OVERRIDE;
  virtual void SetTexturePriorities(const PriorityCalculator&) OVERRIDE;

  bool HaveBackingTexture() const {
    return texture_.get() ? texture_->have_backing_texture() : false;
  }

 private:
  EvictionTestLayer() : Layer() {}
  virtual ~EvictionTestLayer() {}

  void CreateTextureIfNeeded() {
    if (texture_.get())
      return;
    texture_ = PrioritizedResource::Create(
        layer_tree_host()->contents_texture_manager());
    texture_->SetDimensions(gfx::Size(10, 10), GL_RGBA);
    bitmap_.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
  }

  scoped_ptr<PrioritizedResource> texture_;
  SkBitmap bitmap_;
};

class EvictionTestLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<EvictionTestLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                  int id) {
    return make_scoped_ptr(new EvictionTestLayerImpl(tree_impl, id));
  }
  virtual ~EvictionTestLayerImpl() {}

  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE {
    ASSERT_TRUE(has_texture_);
    ASSERT_NE(0u, layer_tree_impl()->resource_provider()->num_resources());
  }

  void SetHasTexture(bool has_texture) { has_texture_ = has_texture; }

 private:
  EvictionTestLayerImpl(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id), has_texture_(false) {}

  bool has_texture_;
};

void EvictionTestLayer::SetTexturePriorities(const PriorityCalculator&) {
  CreateTextureIfNeeded();
  if (!texture_.get())
    return;
  texture_->set_request_priority(PriorityCalculator::UIPriority(true));
}

void EvictionTestLayer::Update(ResourceUpdateQueue* queue,
                               const OcclusionTracker*) {
  CreateTextureIfNeeded();
  if (!texture_.get())
    return;

  gfx::Rect full_rect(0, 0, 10, 10);
  ResourceUpdate upload = ResourceUpdate::Create(
      texture_.get(), &bitmap_, full_rect, full_rect, gfx::Vector2d());
  queue->AppendFullUpload(upload);
}

scoped_ptr<LayerImpl> EvictionTestLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return EvictionTestLayerImpl::Create(tree_impl, layer_id_)
      .PassAs<LayerImpl>();
}

void EvictionTestLayer::PushPropertiesTo(LayerImpl* layer_impl) {
  Layer::PushPropertiesTo(layer_impl);

  EvictionTestLayerImpl* test_layer_impl =
      static_cast<EvictionTestLayerImpl*>(layer_impl);
  test_layer_impl->SetHasTexture(texture_->have_backing_texture());
}

class LayerTreeHostTestEvictTextures : public LayerTreeHostTest {
 public:
  LayerTreeHostTestEvictTextures()
      : layer_(EvictionTestLayer::Create()),
        impl_for_evict_textures_(0),
        num_commits_(0) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetRootLayer(layer_);
    layer_tree_host()->SetViewportSize(gfx::Size(10, 20), gfx::Size(10, 20));

    gfx::Transform identity_matrix;
    SetLayerPropertiesForTesting(layer_.get(),
                                 0,
                                 identity_matrix,
                                 gfx::PointF(0.f, 0.f),
                                 gfx::PointF(0.f, 0.f),
                                 gfx::Size(10, 20),
                                 true);

    PostSetNeedsCommitToMainThread();
  }

  void PostEvictTextures() {
    DCHECK(ImplThread());
    ImplThread()->PostTask(
        base::Bind(&LayerTreeHostTestEvictTextures::EvictTexturesOnImplThread,
                   base::Unretained(this)));
  }

  void EvictTexturesOnImplThread() {
    DCHECK(impl_for_evict_textures_);
    impl_for_evict_textures_->EnforceManagedMemoryPolicy(
        ManagedMemoryPolicy(0));
  }

  // Commit 1: Just commit and draw normally, then post an eviction at the end
  // that will trigger a commit.
  // Commit 2: Triggered by the eviction, let it go through and then set
  // needsCommit.
  // Commit 3: Triggered by the setNeedsCommit. In Layout(), post an eviction
  // task, which will be handled before the commit. Don't set needsCommit, it
  // should have been posted. A frame should not be drawn (note,
  // didCommitAndDrawFrame may be called anyway).
  // Commit 4: Triggered by the eviction, let it go through and then set
  // needsCommit.
  // Commit 5: Triggered by the setNeedsCommit, post an eviction task in
  // Layout(), a frame should not be drawn but a commit will be posted.
  // Commit 6: Triggered by the eviction, post an eviction task in
  // Layout(), which will be a noop, letting the commit (which recreates the
  // textures) go through and draw a frame, then end the test.
  //
  // Commits 1+2 test the eviction recovery path where eviction happens outside
  // of the beginFrame/commit pair.
  // Commits 3+4 test the eviction recovery path where eviction happens inside
  // the beginFrame/commit pair.
  // Commits 5+6 test the path where an eviction happens during the eviction
  // recovery path.
  virtual void DidCommitAndDrawFrame() OVERRIDE {
    switch (num_commits_) {
      case 1:
        EXPECT_TRUE(layer_->HaveBackingTexture());
        PostEvictTextures();
        break;
      case 2:
        EXPECT_TRUE(layer_->HaveBackingTexture());
        layer_tree_host()->SetNeedsCommit();
        break;
      case 3:
        break;
      case 4:
        EXPECT_TRUE(layer_->HaveBackingTexture());
        layer_tree_host()->SetNeedsCommit();
        break;
      case 5:
        break;
      case 6:
        EXPECT_TRUE(layer_->HaveBackingTexture());
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    impl_for_evict_textures_ = impl;
  }

  virtual void Layout() OVERRIDE {
    ++num_commits_;
    switch (num_commits_) {
      case 1:
      case 2:
        break;
      case 3:
        PostEvictTextures();
        break;
      case 4:
        // We couldn't check in didCommitAndDrawFrame on commit 3,
        // so check here.
        EXPECT_FALSE(layer_->HaveBackingTexture());
        break;
      case 5:
        PostEvictTextures();
        break;
      case 6:
        // We couldn't check in didCommitAndDrawFrame on commit 5,
        // so check here.
        EXPECT_FALSE(layer_->HaveBackingTexture());
        PostEvictTextures();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<EvictionTestLayer> layer_;
  LayerTreeHostImpl* impl_for_evict_textures_;
  int num_commits_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestEvictTextures);

class LayerTreeHostTestContinuousCommit : public LayerTreeHostTest {
 public:
  LayerTreeHostTestContinuousCommit()
      : num_commit_complete_(0), num_draw_layers_(0) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
    layer_tree_host()->root_layer()->SetBounds(gfx::Size(10, 10));

    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    if (num_draw_layers_ == 2)
      return;
    PostSetNeedsCommitToMainThread();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (num_draw_layers_ == 1)
      num_commit_complete_++;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_draw_layers_++;
    if (num_draw_layers_ == 2)
      EndTest();
  }

  virtual void AfterTest() OVERRIDE {
    // Check that we didn't commit twice between first and second draw.
    EXPECT_EQ(1, num_commit_complete_);
  }

 private:
  int num_commit_complete_;
  int num_draw_layers_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestContinuousCommit);

class LayerTreeHostTestContinuousInvalidate : public LayerTreeHostTest {
 public:
  LayerTreeHostTestContinuousInvalidate()
      : num_commit_complete_(0), num_draw_layers_(0) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
    layer_tree_host()->root_layer()->SetBounds(gfx::Size(10, 10));

    content_layer_ = ContentLayer::Create(&fake_delegate_);
    content_layer_->SetBounds(gfx::Size(10, 10));
    content_layer_->SetPosition(gfx::PointF(0.f, 0.f));
    content_layer_->SetAnchorPoint(gfx::PointF(0.f, 0.f));
    content_layer_->SetIsDrawable(true);
    layer_tree_host()->root_layer()->AddChild(content_layer_);

    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    if (num_draw_layers_ == 2)
      return;
    content_layer_->SetNeedsDisplay();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (num_draw_layers_ == 1)
      num_commit_complete_++;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_draw_layers_++;
    if (num_draw_layers_ == 2)
      EndTest();
  }

  virtual void AfterTest() OVERRIDE {
    // Check that we didn't commit twice between first and second draw.
    EXPECT_EQ(1, num_commit_complete_);

    // Clear layer references so LayerTreeHost dies.
    content_layer_ = NULL;
  }

 private:
  FakeContentLayerClient fake_delegate_;
  scoped_refptr<Layer> content_layer_;
  int num_commit_complete_;
  int num_draw_layers_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestContinuousInvalidate);

class LayerTreeHostTestDeferCommits : public LayerTreeHostTest {
 public:
  LayerTreeHostTestDeferCommits()
      : num_commits_deferred_(0), num_complete_commits_(0) {}

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidDeferCommit() OVERRIDE {
    num_commits_deferred_++;
    layer_tree_host()->SetDeferCommits(false);
  }

  virtual void DidCommit() OVERRIDE {
    num_complete_commits_++;
    switch (num_complete_commits_) {
      case 1:
        EXPECT_EQ(0, num_commits_deferred_);
        layer_tree_host()->SetDeferCommits(true);
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(1, num_commits_deferred_);
    EXPECT_EQ(2, num_complete_commits_);
  }

 private:
  int num_commits_deferred_;
  int num_complete_commits_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestDeferCommits);

class LayerTreeHostWithProxy : public LayerTreeHost {
 public:
  LayerTreeHostWithProxy(FakeLayerTreeHostClient* client,
                         const LayerTreeSettings& settings,
                         scoped_ptr<Proxy> proxy)
      : LayerTreeHost(client, settings) {
    EXPECT_TRUE(InitializeForTesting(proxy.Pass()));
  }
};

TEST(LayerTreeHostTest, LimitPartialUpdates) {
  // When partial updates are not allowed, max updates should be 0.
  {
    FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

    scoped_ptr<FakeProxy> proxy =
        make_scoped_ptr(new FakeProxy(scoped_ptr<Thread>()));
    proxy->GetRendererCapabilities().allow_partial_texture_updates = false;
    proxy->SetMaxPartialTextureUpdates(5);

    LayerTreeSettings settings;
    settings.max_partial_texture_updates = 10;

    LayerTreeHostWithProxy host(&client, settings, proxy.PassAs<Proxy>());
    EXPECT_TRUE(host.InitializeRendererIfNeeded());

    EXPECT_EQ(0u, host.settings().max_partial_texture_updates);
  }

  // When partial updates are allowed,
  // max updates should be limited by the proxy.
  {
    FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

    scoped_ptr<FakeProxy> proxy =
        make_scoped_ptr(new FakeProxy(scoped_ptr<Thread>()));
    proxy->GetRendererCapabilities().allow_partial_texture_updates = true;
    proxy->SetMaxPartialTextureUpdates(5);

    LayerTreeSettings settings;
    settings.max_partial_texture_updates = 10;

    LayerTreeHostWithProxy host(&client, settings, proxy.PassAs<Proxy>());
    EXPECT_TRUE(host.InitializeRendererIfNeeded());

    EXPECT_EQ(5u, host.settings().max_partial_texture_updates);
  }

  // When partial updates are allowed,
  // max updates should also be limited by the settings.
  {
    FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

    scoped_ptr<FakeProxy> proxy =
        make_scoped_ptr(new FakeProxy(scoped_ptr<Thread>()));
    proxy->GetRendererCapabilities().allow_partial_texture_updates = true;
    proxy->SetMaxPartialTextureUpdates(20);

    LayerTreeSettings settings;
    settings.max_partial_texture_updates = 10;

    LayerTreeHostWithProxy host(&client, settings, proxy.PassAs<Proxy>());
    EXPECT_TRUE(host.InitializeRendererIfNeeded());

    EXPECT_EQ(10u, host.settings().max_partial_texture_updates);
  }
}

TEST(LayerTreeHostTest, PartialUpdatesWithGLRenderer) {
  FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

  LayerTreeSettings settings;
  settings.max_partial_texture_updates = 4;

  scoped_ptr<LayerTreeHost> host =
      LayerTreeHost::Create(&client, settings, scoped_ptr<Thread>());
  EXPECT_TRUE(host->InitializeRendererIfNeeded());
  EXPECT_EQ(4u, host->settings().max_partial_texture_updates);
}

TEST(LayerTreeHostTest, PartialUpdatesWithSoftwareRenderer) {
  FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_SOFTWARE);

  LayerTreeSettings settings;
  settings.max_partial_texture_updates = 4;

  scoped_ptr<LayerTreeHost> host =
      LayerTreeHost::Create(&client, settings, scoped_ptr<Thread>());
  EXPECT_TRUE(host->InitializeRendererIfNeeded());
  EXPECT_EQ(4u, host->settings().max_partial_texture_updates);
}

TEST(LayerTreeHostTest, PartialUpdatesWithDelegatingRendererAndGLContent) {
  FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DELEGATED_3D);

  LayerTreeSettings settings;
  settings.max_partial_texture_updates = 4;

  scoped_ptr<LayerTreeHost> host =
      LayerTreeHost::Create(&client, settings, scoped_ptr<Thread>());
  EXPECT_TRUE(host->InitializeRendererIfNeeded());
  EXPECT_EQ(0u, host->settings().max_partial_texture_updates);
}

TEST(LayerTreeHostTest,
     PartialUpdatesWithDelegatingRendererAndSoftwareContent) {
  FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DELEGATED_SOFTWARE);

  LayerTreeSettings settings;
  settings.max_partial_texture_updates = 4;

  scoped_ptr<LayerTreeHost> host =
      LayerTreeHost::Create(&client, settings, scoped_ptr<Thread>());
  EXPECT_TRUE(host->InitializeRendererIfNeeded());
  EXPECT_EQ(0u, host->settings().max_partial_texture_updates);
}

class LayerTreeHostTestCapturePicture : public LayerTreeHostTest {
 public:
  LayerTreeHostTestCapturePicture()
      : bounds_(gfx::Size(100, 100)),
        layer_(PictureLayer::Create(&content_client_)) {}

  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->impl_side_painting = true;
  }

  class FillRectContentLayerClient : public ContentLayerClient {
   public:
    virtual void PaintContents(SkCanvas* canvas,
                               gfx::Rect clip,
                               gfx::RectF* opaque) OVERRIDE {
      SkPaint paint;
      paint.setColor(SK_ColorGREEN);

      SkRect rect = SkRect::MakeWH(canvas->getDeviceSize().width(),
                                   canvas->getDeviceSize().height());
      *opaque = gfx::RectF(rect.width(), rect.height());
      canvas->drawRect(rect, paint);
    }
    virtual void DidChangeLayerCanUseLCDText() OVERRIDE {}
  };

  virtual void BeginTest() OVERRIDE {
    layer_->SetIsDrawable(true);
    layer_->SetBounds(bounds_);
    layer_tree_host()->SetViewportSize(bounds_, bounds_);
    layer_tree_host()->SetRootLayer(layer_);

    EXPECT_TRUE(layer_tree_host()->InitializeRendererIfNeeded());
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    picture_ = layer_tree_host()->CapturePicture();
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(bounds_, gfx::Size(picture_->width(), picture_->height()));

    SkBitmap bitmap;
    bitmap.setConfig(
        SkBitmap::kARGB_8888_Config, bounds_.width(), bounds_.height());
    bitmap.allocPixels();
    bitmap.eraseARGB(0, 0, 0, 0);
    SkCanvas canvas(bitmap);

    picture_->draw(&canvas);

    bitmap.lockPixels();
    SkColor* pixels = reinterpret_cast<SkColor*>(bitmap.getPixels());
    EXPECT_EQ(SK_ColorGREEN, pixels[0]);
    bitmap.unlockPixels();
  }

 private:
  gfx::Size bounds_;
  FillRectContentLayerClient content_client_;
  scoped_refptr<PictureLayer> layer_;
  skia::RefPtr<SkPicture> picture_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestCapturePicture);

class LayerTreeHostTestMaxPendingFrames : public LayerTreeHostTest {
 public:
  LayerTreeHostTestMaxPendingFrames() : LayerTreeHostTest() {}

  virtual scoped_ptr<OutputSurface> CreateOutputSurface() OVERRIDE {
    if (delegating_renderer_)
      return FakeOutputSurface::CreateDelegating3d().PassAs<OutputSurface>();
    return FakeOutputSurface::Create3d().PassAs<OutputSurface>();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    DCHECK(host_impl->proxy()->HasImplThread());

    const ThreadProxy* proxy = static_cast<ThreadProxy*>(host_impl->proxy());
    if (delegating_renderer_) {
      EXPECT_EQ(1, proxy->MaxFramesPendingForTesting());
    } else {
      EXPECT_EQ(FrameRateController::DEFAULT_MAX_FRAMES_PENDING,
                proxy->MaxFramesPendingForTesting());
    }
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 protected:
  bool delegating_renderer_;
};

TEST_F(LayerTreeHostTestMaxPendingFrames, DelegatingRenderer) {
  delegating_renderer_ = true;
  RunTest(true);
}

TEST_F(LayerTreeHostTestMaxPendingFrames, GLRenderer) {
  delegating_renderer_ = false;
  RunTest(true);
}

class LayerTreeHostTestShutdownWithOnlySomeResourcesEvicted
    : public LayerTreeHostTest {
 public:
  LayerTreeHostTestShutdownWithOnlySomeResourcesEvicted()
      : root_layer_(FakeContentLayer::Create(&client_)),
        child_layer1_(FakeContentLayer::Create(&client_)),
        child_layer2_(FakeContentLayer::Create(&client_)),
        num_commits_(0) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(100, 100),
                                       gfx::Size(100, 100));
    root_layer_->SetBounds(gfx::Size(100, 100));
    child_layer1_->SetBounds(gfx::Size(100, 100));
    child_layer2_->SetBounds(gfx::Size(100, 100));
    root_layer_->AddChild(child_layer1_);
    root_layer_->AddChild(child_layer2_);
    layer_tree_host()->SetRootLayer(root_layer_);
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidSetVisibleOnImplTree(LayerTreeHostImpl* host_impl,
                                       bool visible) OVERRIDE {
    // One backing should remain unevicted.
    EXPECT_EQ(100 * 100 * 4 * 1,
              layer_tree_host()->contents_texture_manager()->MemoryUseBytes());
    // Make sure that contents textures are marked as having been
    // purged.
    EXPECT_TRUE(host_impl->active_tree()->ContentsTexturesPurged());
    // End the test in this state.
    EndTest();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    ++num_commits_;
    switch (num_commits_) {
      case 1:
        // All three backings should have memory.
        EXPECT_EQ(
            100 * 100 * 4 * 3,
            layer_tree_host()->contents_texture_manager()->MemoryUseBytes());
        // Set a new policy that will kick out 1 of the 3 resources.
        // Because a resource was evicted, a commit will be kicked off.
        host_impl->SetManagedMemoryPolicy(
            ManagedMemoryPolicy(100 * 100 * 4 * 2,
                                ManagedMemoryPolicy::CUTOFF_ALLOW_EVERYTHING,
                                100 * 100 * 4 * 1,
                                ManagedMemoryPolicy::CUTOFF_ALLOW_EVERYTHING));
        break;
      case 2:
        // Only two backings should have memory.
        EXPECT_EQ(
            100 * 100 * 4 * 2,
            layer_tree_host()->contents_texture_manager()->MemoryUseBytes());
        // Become backgrounded, which will cause 1 more resource to be
        // evicted.
        PostSetVisibleToMainThread(false);
        break;
      default:
        // No further commits should happen because this is not visible
        // anymore.
        NOTREACHED();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_layer_;
  scoped_refptr<FakeContentLayer> child_layer1_;
  scoped_refptr<FakeContentLayer> child_layer2_;
  int num_commits_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostTestShutdownWithOnlySomeResourcesEvicted);

class LayerTreeHostTestPinchZoomScrollbarCreation : public LayerTreeHostTest {
 public:
  LayerTreeHostTestPinchZoomScrollbarCreation()
      : root_layer_(ContentLayer::Create(&client_)) {}

  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->use_pinch_zoom_scrollbars = true;
  }

  virtual void BeginTest() OVERRIDE {
    root_layer_->SetIsDrawable(true);
    root_layer_->SetBounds(gfx::Size(100, 100));
    layer_tree_host()->SetRootLayer(root_layer_);
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    // We always expect two pinch-zoom scrollbar layers.
    ASSERT_EQ(2, root_layer_->children().size());

    // Pinch-zoom scrollbar layers always have invalid scrollLayerIds.
    ScrollbarLayer* layer1 = root_layer_->children()[0]->ToScrollbarLayer();
    ASSERT_TRUE(layer1);
    EXPECT_EQ(Layer::PINCH_ZOOM_ROOT_SCROLL_LAYER_ID,
              layer1->scroll_layer_id());
    EXPECT_EQ(0.f, layer1->opacity());
    EXPECT_TRUE(layer1->OpacityCanAnimateOnImplThread());
    EXPECT_TRUE(layer1->DrawsContent());

    ScrollbarLayer* layer2 = root_layer_->children()[1]->ToScrollbarLayer();
    ASSERT_TRUE(layer2);
    EXPECT_EQ(Layer::PINCH_ZOOM_ROOT_SCROLL_LAYER_ID,
              layer2->scroll_layer_id());
    EXPECT_EQ(0.f, layer2->opacity());
    EXPECT_TRUE(layer2->OpacityCanAnimateOnImplThread());
    EXPECT_TRUE(layer2->DrawsContent());

    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<ContentLayer> root_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestPinchZoomScrollbarCreation);

class LayerTreeHostTestPinchZoomScrollbarResize : public LayerTreeHostTest {
 public:
  LayerTreeHostTestPinchZoomScrollbarResize()
      : root_layer_(ContentLayer::Create(&client_)), num_commits_(0) {}

  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->use_pinch_zoom_scrollbars = true;
  }

  virtual void BeginTest() OVERRIDE {
    root_layer_->SetIsDrawable(true);
    root_layer_->SetBounds(gfx::Size(100, 100));
    layer_tree_host()->SetRootLayer(root_layer_);
    layer_tree_host()->SetViewportSize(gfx::Size(100, 100),
                                       gfx::Size(100, 100));
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    num_commits_++;

    ScrollbarLayer* layer1 = root_layer_->children()[0]->ToScrollbarLayer();
    ASSERT_TRUE(layer1);
    ScrollbarLayer* layer2 = root_layer_->children()[1]->ToScrollbarLayer();
    ASSERT_TRUE(layer2);

    // Get scrollbar thickness from horizontal scrollbar's height.
    int thickness = layer1->bounds().height();

    if (!layer1->Orientation() == WebKit::WebScrollbar::Horizontal)
      std::swap(layer1, layer2);

    gfx::Size viewport_size = layer_tree_host()->layout_viewport_size();
    EXPECT_EQ(viewport_size.width() - thickness, layer1->bounds().width());
    EXPECT_EQ(viewport_size.height() - thickness, layer2->bounds().height());

    switch (num_commits_) {
      case 1:
        // Resizing the viewport should also resize the pinch-zoom scrollbars.
        layer_tree_host()->SetViewportSize(gfx::Size(120, 150),
                                           gfx::Size(120, 150));
        break;
      default:
        EndTest();
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<ContentLayer> root_layer_;
  int num_commits_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestPinchZoomScrollbarResize);

class LayerTreeHostTestPinchZoomScrollbarNewRootLayer
    : public LayerTreeHostTest {
 public:
  LayerTreeHostTestPinchZoomScrollbarNewRootLayer()
      : root_layer_(ContentLayer::Create(&client_)), num_commits_(0) {}

  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->use_pinch_zoom_scrollbars = true;
  }

  virtual void BeginTest() OVERRIDE {
    root_layer_->SetIsDrawable(true);
    root_layer_->SetBounds(gfx::Size(100, 100));
    layer_tree_host()->SetRootLayer(root_layer_);
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    num_commits_++;

    // We always expect two pinch-zoom scrollbar layers.
    ASSERT_EQ(2, root_layer_->children().size());

    // Pinch-zoom scrollbar layers always have invalid scrollLayerIds.
    ScrollbarLayer* layer1 = root_layer_->children()[0]->ToScrollbarLayer();
    ASSERT_TRUE(layer1);
    EXPECT_EQ(Layer::PINCH_ZOOM_ROOT_SCROLL_LAYER_ID,
              layer1->scroll_layer_id());
    EXPECT_EQ(0.f, layer1->opacity());
    EXPECT_TRUE(layer1->DrawsContent());

    ScrollbarLayer* layer2 = root_layer_->children()[1]->ToScrollbarLayer();
    ASSERT_TRUE(layer2);
    EXPECT_EQ(Layer::PINCH_ZOOM_ROOT_SCROLL_LAYER_ID,
              layer2->scroll_layer_id());
    EXPECT_EQ(0.f, layer2->opacity());
    EXPECT_TRUE(layer2->DrawsContent());

    if (num_commits_ == 1) {
      // Create a new root layer and attach to tree to verify the pinch
      // zoom scrollbars get correctly re-attached.
      root_layer_ = ContentLayer::Create(&client_);
      root_layer_->SetIsDrawable(true);
      root_layer_->SetBounds(gfx::Size(100, 100));
      layer_tree_host()->SetRootLayer(root_layer_);
      PostSetNeedsCommitToMainThread();
    } else {
      EndTest();
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<ContentLayer> root_layer_;
  int num_commits_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestPinchZoomScrollbarNewRootLayer);

class LayerTreeHostTestLCDNotification : public LayerTreeHostTest {
 public:
  class NotificationClient : public ContentLayerClient {
   public:
    NotificationClient()
        : layer_(0), paint_count_(0), lcd_notification_count_(0) {}

    void set_layer(Layer* layer) { layer_ = layer; }
    int paint_count() const { return paint_count_; }
    int lcd_notification_count() const { return lcd_notification_count_; }

    virtual void PaintContents(SkCanvas* canvas,
                               gfx::Rect clip,
                               gfx::RectF* opaque) OVERRIDE {
      ++paint_count_;
    }
    virtual void DidChangeLayerCanUseLCDText() OVERRIDE {
      ++lcd_notification_count_;
      layer_->SetNeedsDisplay();
    }

   private:
    Layer* layer_;
    int paint_count_;
    int lcd_notification_count_;
  };

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<ContentLayer> root_layer = ContentLayer::Create(&client_);
    root_layer->SetIsDrawable(true);
    root_layer->SetBounds(gfx::Size(1, 1));

    layer_tree_host()->SetRootLayer(root_layer);
    client_.set_layer(root_layer.get());

    // The expecations are based on the assumption that the default
    // LCD settings are:
    EXPECT_TRUE(layer_tree_host()->settings().can_use_lcd_text);
    EXPECT_FALSE(root_layer->can_use_lcd_text());

    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }
  virtual void AfterTest() OVERRIDE {}

  virtual void DidCommit() OVERRIDE {
    switch (layer_tree_host()->commit_number()) {
      case 1:
        // The first update consists one LCD notification and one paint.
        EXPECT_EQ(1, client_.lcd_notification_count());
        EXPECT_EQ(1, client_.paint_count());
        // LCD text must have been enabled on the layer.
        EXPECT_TRUE(layer_tree_host()->root_layer()->can_use_lcd_text());
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        // Since nothing changed on layer, there should be no notification
        // or paint on the second update.
        EXPECT_EQ(1, client_.lcd_notification_count());
        EXPECT_EQ(1, client_.paint_count());
        // LCD text must not have changed.
        EXPECT_TRUE(layer_tree_host()->root_layer()->can_use_lcd_text());
        // Change layer opacity that should trigger lcd notification.
        layer_tree_host()->root_layer()->SetOpacity(.5f);
        // No need to request a commit - setting opacity will do it.
        break;
      default:
        // Verify that there is not extra commit due to layer invalidation.
        EXPECT_EQ(3, layer_tree_host()->commit_number());
        // LCD notification count should have incremented due to
        // change in layer opacity.
        EXPECT_EQ(2, client_.lcd_notification_count());
        // Paint count should be incremented due to invalidation.
        EXPECT_EQ(2, client_.paint_count());
        // LCD text must have been disabled on the layer due to opacity.
        EXPECT_FALSE(layer_tree_host()->root_layer()->can_use_lcd_text());
        EndTest();
        break;
    }
  }

 private:
  NotificationClient client_;
};

SINGLE_THREAD_TEST_F(LayerTreeHostTestLCDNotification);

// Verify that the vsync notification is used to initiate rendering.
class LayerTreeHostTestVSyncNotification : public LayerTreeHostTest {
 public:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->render_vsync_notification_enabled = true;
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    FakeOutputSurface* fake_output_surface =
        reinterpret_cast<FakeOutputSurface*>(host_impl->output_surface());

    // The vsync notification is turned off now but will get enabled once we
    // return, so post a task to trigger it.
    ASSERT_FALSE(fake_output_surface->vsync_notification_enabled());
    PostVSyncOnImplThread(fake_output_surface);
  }

  void PostVSyncOnImplThread(FakeOutputSurface* fake_output_surface) {
    DCHECK(ImplThread());
    ImplThread()->PostTask(
        base::Bind(&LayerTreeHostTestVSyncNotification::DidVSync,
                   base::Unretained(this),
                   base::Unretained(fake_output_surface)));
  }

  void DidVSync(FakeOutputSurface* fake_output_surface) {
    ASSERT_TRUE(fake_output_surface->vsync_notification_enabled());
    fake_output_surface->DidVSync(frame_time_);
  }

  virtual bool PrepareToDrawOnThread(
      LayerTreeHostImpl* host_impl,
      LayerTreeHostImpl::FrameData* frame,
      bool result) OVERRIDE {
    EndTest();
    return true;
  }

  virtual void AfterTest() OVERRIDE {
  }

 private:
  base::TimeTicks frame_time_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestVSyncNotification);

}  // namespace
}  // namespace cc
