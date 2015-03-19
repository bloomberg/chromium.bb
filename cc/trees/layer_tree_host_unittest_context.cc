// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "base/basictypes.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/io_surface_layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/layers/video_layer.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/output/filter_operations.h"
#include "cc/resources/single_release_callback.h"
#include "cc/test/failure_output_surface.h"
#include "cc/test/fake_content_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_content_layer_impl.h"
#include "cc/test/fake_delegated_renderer_layer.h"
#include "cc/test/fake_delegated_renderer_layer_impl.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_painted_scrollbar_layer.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_scoped_ui_resource.h"
#include "cc/test/fake_scrollbar.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "media/base/media.h"

using media::VideoFrame;

namespace cc {
namespace {

// These tests deal with losing the 3d graphics context.
class LayerTreeHostContextTest : public LayerTreeTest {
 public:
  LayerTreeHostContextTest()
      : LayerTreeTest(),
        context3d_(NULL),
        times_to_fail_create_(0),
        times_to_lose_during_commit_(0),
        times_to_lose_during_draw_(0),
        times_to_fail_recreate_(0),
        times_to_expect_create_failed_(0),
        times_create_failed_(0),
        committed_at_least_once_(false),
        context_should_support_io_surface_(false),
        fallback_context_works_(false),
        async_output_surface_creation_(false) {
    media::InitializeMediaLibraryForTesting();
  }

  void LoseContext() {
    // For sanity-checking tests, they should only call this when the
    // context is not lost.
    CHECK(context3d_);
    context3d_->loseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                                    GL_INNOCENT_CONTEXT_RESET_ARB);
    context3d_ = NULL;
  }

  virtual scoped_ptr<TestWebGraphicsContext3D> CreateContext3d() {
    return TestWebGraphicsContext3D::Create();
  }

  scoped_ptr<FakeOutputSurface> CreateFakeOutputSurface() override {
    if (times_to_fail_create_) {
      --times_to_fail_create_;
      ExpectCreateToFail();
      return make_scoped_ptr(new FailureOutputSurface(delegating_renderer()));
    }

    scoped_ptr<TestWebGraphicsContext3D> context3d = CreateContext3d();
    context3d_ = context3d.get();

    if (context_should_support_io_surface_) {
      context3d_->set_have_extension_io_surface(true);
      context3d_->set_have_extension_egl_image(true);
    }

    if (delegating_renderer())
      return FakeOutputSurface::CreateDelegating3d(context3d.Pass());
    else
      return FakeOutputSurface::Create3d(context3d.Pass());
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame,
                                   DrawResult draw_result) override {
    if (draw_result == DRAW_ABORTED_MISSING_HIGH_RES_CONTENT) {
      // Only valid for single-threaded impl-side painting, which activates
      // immediately and will try to draw again when content has finished.
      DCHECK(!host_impl->proxy()->HasImplThread());
      DCHECK(host_impl->settings().impl_side_painting);
      return draw_result;
    }
    EXPECT_EQ(DRAW_SUCCESS, draw_result);
    if (!times_to_lose_during_draw_)
      return draw_result;

    --times_to_lose_during_draw_;
    LoseContext();

    times_to_fail_create_ = times_to_fail_recreate_;
    times_to_fail_recreate_ = 0;

    return draw_result;
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    committed_at_least_once_ = true;

    if (!times_to_lose_during_commit_)
      return;
    --times_to_lose_during_commit_;
    LoseContext();

    times_to_fail_create_ = times_to_fail_recreate_;
    times_to_fail_recreate_ = 0;
  }

  void DidFailToInitializeOutputSurface() override { ++times_create_failed_; }

  void TearDown() override {
    LayerTreeTest::TearDown();
    EXPECT_EQ(times_to_expect_create_failed_, times_create_failed_);
  }

  void ExpectCreateToFail() { ++times_to_expect_create_failed_; }

 protected:
  TestWebGraphicsContext3D* context3d_;
  int times_to_fail_create_;
  int times_to_lose_during_commit_;
  int times_to_lose_during_draw_;
  int times_to_fail_recreate_;
  int times_to_expect_create_failed_;
  int times_create_failed_;
  bool committed_at_least_once_;
  bool context_should_support_io_surface_;
  bool fallback_context_works_;
  bool async_output_surface_creation_;
};

class LayerTreeHostContextTestLostContextSucceeds
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextSucceeds()
      : LayerTreeHostContextTest(),
        test_case_(0),
        num_losses_(0),
        num_losses_last_test_case_(-1),
        recovered_context_(true),
        first_initialized_(false) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void RequestNewOutputSurface() override {
    if (async_output_surface_creation_) {
      MainThreadTaskRunner()->PostTask(
          FROM_HERE, base::Bind(&LayerTreeHostContextTestLostContextSucceeds::
                                    CreateAndSetOutputSurface,
                                base::Unretained(this)));
    } else {
      CreateAndSetOutputSurface();
    }
  }

  void CreateAndSetOutputSurface() {
    scoped_ptr<OutputSurface> surface(
        LayerTreeHostContextTest::CreateOutputSurface());
    CHECK(surface);
    layer_tree_host()->SetOutputSurface(surface.Pass());
  }

  void DidInitializeOutputSurface() override {
    if (first_initialized_)
      ++num_losses_;
    else
      first_initialized_ = true;

    recovered_context_ = true;
  }

  void AfterTest() override { EXPECT_EQ(11u, test_case_); }

  void DidCommitAndDrawFrame() override {
    // If the last frame had a context loss, then we'll commit again to
    // recover.
    if (!recovered_context_)
      return;
    if (times_to_lose_during_commit_)
      return;
    if (times_to_lose_during_draw_)
      return;

    recovered_context_ = false;
    if (NextTestCase())
      InvalidateAndSetNeedsCommit();
    else
      EndTest();
  }

  virtual void InvalidateAndSetNeedsCommit() {
    // Cause damage so we try to draw.
    layer_tree_host()->root_layer()->SetNeedsDisplay();
    layer_tree_host()->SetNeedsCommit();
  }

  bool NextTestCase() {
    static const TestCase kTests[] = {
        // Losing the context and failing to recreate it (or losing it again
        // immediately) a small number of times should succeed.
        {
         1,      // times_to_lose_during_commit
         0,      // times_to_lose_during_draw
         0,      // times_to_fail_recreate
         false,  // fallback_context_works
         false,  // async_output_surface_creation
        },
        {
         0,      // times_to_lose_during_commit
         1,      // times_to_lose_during_draw
         0,      // times_to_fail_recreate
         false,  // fallback_context_works
         false,  // async_output_surface_creation
        },
        {
         1,      // times_to_lose_during_commit
         0,      // times_to_lose_during_draw
         3,      // times_to_fail_recreate
         false,  // fallback_context_works
         false,  // async_output_surface_creation
        },
        {
         0,      // times_to_lose_during_commit
         1,      // times_to_lose_during_draw
         3,      // times_to_fail_recreate
         false,  // fallback_context_works
         false,  // async_output_surface_creation
        },
        {
         0,      // times_to_lose_during_commit
         1,      // times_to_lose_during_draw
         3,      // times_to_fail_recreate
         false,  // fallback_context_works
         true,   // async_output_surface_creation
        },
        // Losing the context and recreating it any number of times should
        // succeed.
        {
         10,     // times_to_lose_during_commit
         0,      // times_to_lose_during_draw
         0,      // times_to_fail_recreate
         false,  // fallback_context_works
         false,  // async_output_surface_creation
        },
        {
         0,      // times_to_lose_during_commit
         10,     // times_to_lose_during_draw
         0,      // times_to_fail_recreate
         false,  // fallback_context_works
         false,  // async_output_surface_creation
        },
        {
         10,     // times_to_lose_during_commit
         0,      // times_to_lose_during_draw
         0,      // times_to_fail_recreate
         false,  // fallback_context_works
         true,   // async_output_surface_creation
        },
        {
         0,      // times_to_lose_during_commit
         10,     // times_to_lose_during_draw
         0,      // times_to_fail_recreate
         false,  // fallback_context_works
         true,   // async_output_surface_creation
        },
        // Losing the context, failing to reinitialize it, and making a fallback
        // context should work.
        {
         0,      // times_to_lose_during_commit
         1,      // times_to_lose_during_draw
         0,      // times_to_fail_recreate
         true,   // fallback_context_works
         false,  // async_output_surface_creation
        },
        {
         0,     // times_to_lose_during_commit
         1,     // times_to_lose_during_draw
         0,     // times_to_fail_recreate
         true,  // fallback_context_works
         true,  // async_output_surface_creation
        },
    };

    if (test_case_ >= arraysize(kTests))
      return false;
    // Make sure that we lost our context at least once in the last test run so
    // the test did something.
    EXPECT_GT(num_losses_, num_losses_last_test_case_);
    num_losses_last_test_case_ = num_losses_;

    times_to_lose_during_commit_ =
        kTests[test_case_].times_to_lose_during_commit;
    times_to_lose_during_draw_ = kTests[test_case_].times_to_lose_during_draw;
    times_to_fail_recreate_ = kTests[test_case_].times_to_fail_recreate;
    fallback_context_works_ = kTests[test_case_].fallback_context_works;
    async_output_surface_creation_ =
        kTests[test_case_].async_output_surface_creation;
    ++test_case_;
    return true;
  }

  struct TestCase {
    int times_to_lose_during_commit;
    int times_to_lose_during_draw;
    int times_to_fail_recreate;
    bool fallback_context_works;
    bool async_output_surface_creation;
  };

 protected:
  size_t test_case_;
  int num_losses_;
  int num_losses_last_test_case_;
  bool recovered_context_;
  bool first_initialized_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestLostContextSucceeds);

class LayerTreeHostClientNotReadyDoesNotCreateOutputSurface
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostClientNotReadyDoesNotCreateOutputSurface()
      : LayerTreeHostContextTest() {}

  void WillBeginTest() override {
    // Override and do not signal SetLayerTreeHostClientReady.
  }

  void BeginTest() override {
    PostSetNeedsCommitToMainThread();
    EndTest();
  }

  scoped_ptr<OutputSurface> CreateOutputSurface() override {
    EXPECT_TRUE(false);
    return nullptr;
  }

  void DidInitializeOutputSurface() override { EXPECT_TRUE(false); }

  void AfterTest() override {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostClientNotReadyDoesNotCreateOutputSurface);

class MultipleCompositeDoesNotCreateOutputSurface
    : public LayerTreeHostContextTest {
 public:
  MultipleCompositeDoesNotCreateOutputSurface()
      : LayerTreeHostContextTest(), request_count_(0) {}

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->single_thread_proxy_scheduler = false;
  }

  void RequestNewOutputSurface() override {
    EXPECT_GE(1, ++request_count_);
    EndTest();
  }

  void BeginTest() override {
    layer_tree_host()->Composite(base::TimeTicks());
    layer_tree_host()->Composite(base::TimeTicks());
  }

  scoped_ptr<OutputSurface> CreateOutputSurface() override {
    EXPECT_TRUE(false);
    return nullptr;
  }

  void DidInitializeOutputSurface() override { EXPECT_TRUE(false); }

  void AfterTest() override {}

  int request_count_;
};

SINGLE_THREAD_NOIMPL_TEST_F(MultipleCompositeDoesNotCreateOutputSurface);

// This test makes sure that once a SingleThreadProxy issues a
// DidFailToInitializeOutputSurface, that future Composite calls will not
// trigger additional requests for output surfaces.
class FailedCreateDoesNotCreateExtraOutputSurface
    : public LayerTreeHostContextTest {
 public:
  FailedCreateDoesNotCreateExtraOutputSurface()
      : LayerTreeHostContextTest(), num_requests_(0), has_failed_(false) {}

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->single_thread_proxy_scheduler = false;
  }

  void RequestNewOutputSurface() override {
    num_requests_++;
    // There should be one initial request and then one request from
    // the LayerTreeTest test hooks DidFailToInitializeOutputSurface (which is
    // hard to skip).  This second request is just ignored and is test cruft.
    EXPECT_LE(num_requests_, 2);
    if (num_requests_ > 1)
      return;
    ExpectCreateToFail();
    layer_tree_host()->SetOutputSurface(
        make_scoped_ptr(new FailureOutputSurface(false)));
  }

  void BeginTest() override {
    // First composite tries to create a surface.
    layer_tree_host()->Composite(base::TimeTicks());
    EXPECT_EQ(num_requests_, 2);
    EXPECT_TRUE(has_failed_);

    // Second composite should not request or fail.
    layer_tree_host()->Composite(base::TimeTicks());
    EXPECT_EQ(num_requests_, 2);
    EndTest();
  }

  void DidInitializeOutputSurface() override { EXPECT_TRUE(false); }

  void DidFailToInitializeOutputSurface() override {
    LayerTreeHostContextTest::DidFailToInitializeOutputSurface();
    EXPECT_FALSE(has_failed_);
    has_failed_ = true;
  }

  void AfterTest() override {}

  int num_requests_;
  bool has_failed_;
};

SINGLE_THREAD_NOIMPL_TEST_F(FailedCreateDoesNotCreateExtraOutputSurface);

class LayerTreeHostContextTestCommitAfterDelayedOutputSurface
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestCommitAfterDelayedOutputSurface()
      : LayerTreeHostContextTest(), creating_output_(false) {}

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->single_thread_proxy_scheduler = false;
  }

  void RequestNewOutputSurface() override {
    MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&LayerTreeHostContextTestCommitAfterDelayedOutputSurface::
                       CreateAndSetOutputSurface,
                   base::Unretained(this)));
  }

  void CreateAndSetOutputSurface() {
    creating_output_ = true;
    layer_tree_host()->SetOutputSurface(
        LayerTreeHostContextTest::CreateOutputSurface());
  }

  void BeginTest() override { layer_tree_host()->Composite(base::TimeTicks()); }

  void ScheduleComposite() override {
    if (creating_output_)
      EndTest();
  }

  void AfterTest() override {}

  bool creating_output_;
};

SINGLE_THREAD_NOIMPL_TEST_F(
    LayerTreeHostContextTestCommitAfterDelayedOutputSurface);

class LayerTreeHostContextTestAvoidUnnecessaryComposite
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestAvoidUnnecessaryComposite()
      : LayerTreeHostContextTest(), in_composite_(false) {}

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->single_thread_proxy_scheduler = false;
  }

  void RequestNewOutputSurface() override {
    layer_tree_host()->SetOutputSurface(
        LayerTreeHostContextTest::CreateOutputSurface());
    EndTest();
  }

  void BeginTest() override {
    in_composite_ = true;
    layer_tree_host()->Composite(base::TimeTicks());
    in_composite_ = false;
  }

  void ScheduleComposite() override { EXPECT_FALSE(in_composite_); }

  void AfterTest() override {}

  bool in_composite_;
};

SINGLE_THREAD_NOIMPL_TEST_F(LayerTreeHostContextTestAvoidUnnecessaryComposite);

class LayerTreeHostContextTestLostContextSucceedsWithContent
    : public LayerTreeHostContextTestLostContextSucceeds {
 public:
  void SetupTree() override {
    root_ = Layer::Create();
    root_->SetBounds(gfx::Size(10, 10));
    root_->SetIsDrawable(true);

    // Paint non-solid color.
    SkPaint paint;
    paint.setColor(SkColorSetARGB(100, 80, 200, 200));
    client_.add_draw_rect(gfx::Rect(0, 0, 5, 5), paint);

    if (layer_tree_host()->settings().impl_side_painting)
      layer_ = FakePictureLayer::Create(&client_);
    else
      layer_ = FakeContentLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(10, 10));
    layer_->SetIsDrawable(true);

    root_->AddChild(layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostContextTest::SetupTree();
  }

  void InvalidateAndSetNeedsCommit() override {
    // Invalidate the render surface so we don't try to use a cached copy of the
    // surface.  We want to make sure to test the drawing paths for drawing to
    // a child surface.
    layer_->SetNeedsDisplay();
    LayerTreeHostContextTestLostContextSucceeds::InvalidateAndSetNeedsCommit();
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (!host_impl->settings().impl_side_painting) {
      FakeContentLayerImpl* content_impl = static_cast<FakeContentLayerImpl*>(
          host_impl->active_tree()->root_layer()->children()[0]);
      // Even though the context was lost, we should have a resource. The
      // TestWebGraphicsContext3D ensures that this resource is created with
      // the active context.
      EXPECT_TRUE(content_impl->HaveResourceForTileAt(0, 0));
    } else {
      FakePictureLayerImpl* picture_impl = static_cast<FakePictureLayerImpl*>(
          host_impl->active_tree()->root_layer()->children()[0]);
      EXPECT_TRUE(picture_impl->HighResTiling()->TileAt(0, 0)->IsReadyToDraw());
    }
  }

 protected:
  FakeContentLayerClient client_;
  scoped_refptr<Layer> root_;
  scoped_refptr<Layer> layer_;
};

// This test uses TiledLayer and PictureLayer to check for a working context.
SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestLostContextSucceedsWithContent);

class LayerTreeHostContextTestCreateOutputSurfaceFailsOnce
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestCreateOutputSurfaceFailsOnce()
      : times_to_fail_(1), times_initialized_(0) {
    times_to_fail_create_ = times_to_fail_;
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidInitializeOutputSurface() override { times_initialized_++; }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override { EndTest(); }

  void AfterTest() override {
    EXPECT_EQ(times_to_fail_, times_create_failed_);
    EXPECT_NE(0, times_initialized_);
  }

 private:
  int times_to_fail_;
  int times_initialized_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestCreateOutputSurfaceFailsOnce);

class LayerTreeHostContextTestLostContextAndEvictTextures
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextAndEvictTextures()
      : LayerTreeHostContextTest(),
        impl_host_(0),
        num_commits_(0),
        lost_context_(false) {}

  void SetupTree() override {
    // Paint non-solid color.
    SkPaint paint;
    paint.setColor(SkColorSetARGB(100, 80, 200, 200));
    client_.add_draw_rect(gfx::Rect(0, 0, 5, 5), paint);

    if (layer_tree_host()->settings().impl_side_painting) {
      picture_layer_ = FakePictureLayer::Create(&client_);
      picture_layer_->SetBounds(gfx::Size(10, 20));
      layer_tree_host()->SetRootLayer(picture_layer_);
    } else {
      content_layer_ = FakeContentLayer::Create(&client_);
      content_layer_->SetBounds(gfx::Size(10, 20));
      layer_tree_host()->SetRootLayer(content_layer_);
    }

    LayerTreeHostContextTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void PostEvictTextures() {
    if (HasImplThread()) {
      ImplThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(&LayerTreeHostContextTestLostContextAndEvictTextures::
                          EvictTexturesOnImplThread,
                     base::Unretained(this)));
    } else {
      DebugScopedSetImplThread impl(proxy());
      EvictTexturesOnImplThread();
    }
  }

  void EvictTexturesOnImplThread() {
    impl_host_->EvictTexturesForTesting();

    if (lose_after_evict_) {
      LoseContext();
      lost_context_ = true;
    }
  }

  void DidCommitAndDrawFrame() override {
    if (num_commits_ > 1)
      return;
    if (!layer_tree_host()->settings().impl_side_painting) {
      EXPECT_TRUE(content_layer_->HaveBackingAt(0, 0));
    }
    PostEvictTextures();
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    if (num_commits_ > 1)
      return;
    ++num_commits_;
    if (!lose_after_evict_) {
      LoseContext();
      lost_context_ = true;
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    if (impl->settings().impl_side_painting) {
      FakePictureLayerImpl* picture_impl =
          static_cast<FakePictureLayerImpl*>(impl->active_tree()->root_layer());
      EXPECT_TRUE(picture_impl->HighResTiling()->TileAt(0, 0)->IsReadyToDraw());
    } else {
      FakeContentLayerImpl* content_impl =
          static_cast<FakeContentLayerImpl*>(impl->active_tree()->root_layer());
      EXPECT_TRUE(content_impl->HaveResourceForTileAt(0, 0));
    }

    impl_host_ = impl;
    if (lost_context_)
      EndTest();
  }

  void DidInitializeOutputSurface() override {}

  void AfterTest() override {}

 protected:
  bool lose_after_evict_;
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> content_layer_;
  scoped_refptr<FakePictureLayer> picture_layer_;
  LayerTreeHostImpl* impl_host_;
  int num_commits_;
  bool lost_context_;
};

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseAfterEvict_SingleThread_DirectRenderer) {
  lose_after_evict_ = true;
  RunTest(false, false, false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseAfterEvict_SingleThread_DelegatingRenderer) {
  lose_after_evict_ = true;
  RunTest(false, true, false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseAfterEvict_MultiThread_DirectRenderer_MainThreadPaint) {
  lose_after_evict_ = true;
  RunTest(true, false, false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseAfterEvict_MultiThread_DelegatingRenderer_MainThreadPaint) {
  lose_after_evict_ = true;
  RunTest(true, true, false);
}

// Flaky on all platforms, http://crbug.com/310979
TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       DISABLED_LoseAfterEvict_MultiThread_DelegatingRenderer_ImplSidePaint) {
  lose_after_evict_ = true;
  RunTest(true, true, true);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_SingleThread_DirectRenderer) {
  lose_after_evict_ = false;
  RunTest(false, false, false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_SingleThread_DelegatingRenderer) {
  lose_after_evict_ = false;
  RunTest(false, true, false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_MultiThread_DirectRenderer_MainThreadPaint) {
  lose_after_evict_ = false;
  RunTest(true, false, false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_MultiThread_DirectRenderer_ImplSidePaint) {
  lose_after_evict_ = false;
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_MultiThread_DelegatingRenderer_MainThreadPaint) {
  lose_after_evict_ = false;
  RunTest(true, true, false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_MultiThread_DelegatingRenderer_ImplSidePaint) {
  lose_after_evict_ = false;
  RunTest(true, true, true);
}

class LayerTreeHostContextTestLostContextWhileUpdatingResources
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextWhileUpdatingResources()
      : num_children_(50), times_to_lose_on_end_query_(3) {}

  scoped_ptr<TestWebGraphicsContext3D> CreateContext3d() override {
    scoped_ptr<TestWebGraphicsContext3D> context =
        LayerTreeHostContextTest::CreateContext3d();
    if (times_to_lose_on_end_query_) {
      --times_to_lose_on_end_query_;
      context->set_times_end_query_succeeds(5);
    }
    return context.Pass();
  }

  void SetupTree() override {
    if (layer_tree_host()->settings().impl_side_painting)
      parent_ = FakePictureLayer::Create(&client_);
    else
      parent_ = FakeContentLayer::Create(&client_);

    parent_->SetBounds(gfx::Size(num_children_, 1));

    for (int i = 0; i < num_children_; i++) {
      scoped_refptr<Layer> child;
      if (layer_tree_host()->settings().impl_side_painting)
        child = FakePictureLayer::Create(&client_);
      else
        child = FakeContentLayer::Create(&client_);
      child->SetPosition(gfx::PointF(i, 0.f));
      child->SetBounds(gfx::Size(1, 1));
      parent_->AddChild(child);
    }

    layer_tree_host()->SetRootLayer(parent_);
    LayerTreeHostContextTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    EXPECT_EQ(0, times_to_lose_on_end_query_);
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(0, times_to_lose_on_end_query_); }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<Layer> parent_;
  int num_children_;
  int times_to_lose_on_end_query_;
};

SINGLE_AND_MULTI_THREAD_NOIMPL_TEST_F(
    LayerTreeHostContextTestLostContextWhileUpdatingResources);

class LayerTreeHostContextTestLayersNotified : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLayersNotified()
      : LayerTreeHostContextTest(), num_commits_(0) {}

  void SetupTree() override {
    if (layer_tree_host()->settings().impl_side_painting) {
      root_ = FakePictureLayer::Create(&client_);
      child_ = FakePictureLayer::Create(&client_);
      grandchild_ = FakePictureLayer::Create(&client_);
    } else {
      root_ = FakeContentLayer::Create(&client_);
      child_ = FakeContentLayer::Create(&client_);
      grandchild_ = FakeContentLayer::Create(&client_);
    }

    root_->AddChild(child_);
    child_->AddChild(grandchild_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostContextTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    LayerTreeHostContextTest::DidActivateTreeOnThread(host_impl);

    FakePictureLayerImpl* root_picture = NULL;
    FakePictureLayerImpl* child_picture = NULL;
    FakePictureLayerImpl* grandchild_picture = NULL;
    FakeContentLayerImpl* root_content = NULL;
    FakeContentLayerImpl* child_content = NULL;
    FakeContentLayerImpl* grandchild_content = NULL;

    if (host_impl->settings().impl_side_painting) {
      root_picture = static_cast<FakePictureLayerImpl*>(
          host_impl->active_tree()->root_layer());
      child_picture =
          static_cast<FakePictureLayerImpl*>(root_picture->children()[0]);
      grandchild_picture =
          static_cast<FakePictureLayerImpl*>(child_picture->children()[0]);

    } else {
      root_content = static_cast<FakeContentLayerImpl*>(
          host_impl->active_tree()->root_layer());
      child_content =
          static_cast<FakeContentLayerImpl*>(root_content->children()[0]);
      grandchild_content =
          static_cast<FakeContentLayerImpl*>(child_content->children()[0]);
    }

    ++num_commits_;
    switch (num_commits_) {
      case 1:
        if (host_impl->settings().impl_side_painting) {
          EXPECT_EQ(0u, root_picture->release_resources_count());
          EXPECT_EQ(0u, child_picture->release_resources_count());
          EXPECT_EQ(0u, grandchild_picture->release_resources_count());
        } else {
          EXPECT_EQ(0u, root_content->lost_output_surface_count());
          EXPECT_EQ(0u, child_content->lost_output_surface_count());
          EXPECT_EQ(0u, grandchild_content->lost_output_surface_count());
        }

        // Lose the context and struggle to recreate it.
        LoseContext();
        times_to_fail_create_ = 1;
        break;
      case 2:
        if (host_impl->settings().impl_side_painting) {
          EXPECT_TRUE(root_picture->release_resources_count());
          EXPECT_TRUE(child_picture->release_resources_count());
          EXPECT_TRUE(grandchild_picture->release_resources_count());
        } else {
          EXPECT_TRUE(root_content->lost_output_surface_count());
          EXPECT_TRUE(child_content->lost_output_surface_count());
          EXPECT_TRUE(grandchild_content->lost_output_surface_count());
        }

        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  void AfterTest() override {}

 private:
  int num_commits_;

  FakeContentLayerClient client_;
  scoped_refptr<Layer> root_;
  scoped_refptr<Layer> child_;
  scoped_refptr<Layer> grandchild_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestLayersNotified);

class LayerTreeHostContextTestDontUseLostResources
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestDontUseLostResources() : lost_context_(false) {
    context_should_support_io_surface_ = true;

    child_output_surface_ = FakeOutputSurface::Create3d();
    child_output_surface_->BindToClient(&output_surface_client_);
    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    child_resource_provider_ =
        ResourceProvider::Create(child_output_surface_.get(),
                                 shared_bitmap_manager_.get(),
                                 NULL,
                                 NULL,
                                 0,
                                 false,
                                 1);
  }

  static void EmptyReleaseCallback(unsigned sync_point, bool lost) {}

  void SetupTree() override {
    gpu::gles2::GLES2Interface* gl =
        child_output_surface_->context_provider()->ContextGL();

    scoped_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);

    scoped_ptr<TestRenderPass> pass_for_quad = TestRenderPass::Create();
    pass_for_quad->SetNew(
        // AppendOneOfEveryQuadType() makes a RenderPass quad with this id.
        RenderPassId(2, 1),
        gfx::Rect(0, 0, 10, 10),
        gfx::Rect(0, 0, 10, 10),
        gfx::Transform());

    scoped_ptr<TestRenderPass> pass = TestRenderPass::Create();
    pass->SetNew(RenderPassId(1, 1),
                 gfx::Rect(0, 0, 10, 10),
                 gfx::Rect(0, 0, 10, 10),
                 gfx::Transform());
    pass->AppendOneOfEveryQuadType(child_resource_provider_.get(),
                                   RenderPassId(2, 1));

    frame_data->render_pass_list.push_back(pass_for_quad.Pass());
    frame_data->render_pass_list.push_back(pass.Pass());

    delegated_resource_collection_ = new DelegatedFrameResourceCollection;
    delegated_frame_provider_ = new DelegatedFrameProvider(
        delegated_resource_collection_.get(), frame_data.Pass());

    ResourceProvider::ResourceId resource =
        child_resource_provider_->CreateResource(
            gfx::Size(4, 4), GL_CLAMP_TO_EDGE,
            ResourceProvider::TEXTURE_HINT_IMMUTABLE, RGBA_8888);
    ResourceProvider::ScopedWriteLockGL lock(child_resource_provider_.get(),
                                             resource);

    gpu::Mailbox mailbox;
    gl->GenMailboxCHROMIUM(mailbox.name);
    GLuint sync_point = gl->InsertSyncPointCHROMIUM();

    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetIsDrawable(true);

    scoped_refptr<FakeDelegatedRendererLayer> delegated =
        FakeDelegatedRendererLayer::Create(delegated_frame_provider_.get());
    delegated->SetBounds(gfx::Size(10, 10));
    delegated->SetIsDrawable(true);
    root->AddChild(delegated);

    scoped_refptr<Layer> layer;
    if (layer_tree_host()->settings().impl_side_painting)
      layer = PictureLayer::Create(&client_);
    else
      layer = ContentLayer::Create(&client_);
    layer->SetBounds(gfx::Size(10, 10));
    layer->SetIsDrawable(true);
    root->AddChild(layer);

    scoped_refptr<TextureLayer> texture = TextureLayer::CreateForMailbox(NULL);
    texture->SetBounds(gfx::Size(10, 10));
    texture->SetIsDrawable(true);
    texture->SetTextureMailbox(
        TextureMailbox(mailbox, GL_TEXTURE_2D, sync_point),
        SingleReleaseCallback::Create(
            base::Bind(&LayerTreeHostContextTestDontUseLostResources::
                            EmptyReleaseCallback)));
    root->AddChild(texture);

    scoped_refptr<Layer> mask;
    if (layer_tree_host()->settings().impl_side_painting)
      mask = PictureLayer::Create(&client_);
    else
      mask = ContentLayer::Create(&client_);
    mask->SetBounds(gfx::Size(10, 10));

    scoped_refptr<Layer> layer_with_mask;
    if (layer_tree_host()->settings().impl_side_painting)
      layer_with_mask = PictureLayer::Create(&client_);
    else
      layer_with_mask = ContentLayer::Create(&client_);
    layer_with_mask->SetBounds(gfx::Size(10, 10));
    layer_with_mask->SetIsDrawable(true);
    layer_with_mask->SetMaskLayer(mask.get());
    root->AddChild(layer_with_mask);

    scoped_refptr<VideoLayer> video_color =
        VideoLayer::Create(&color_frame_provider_, media::VIDEO_ROTATION_0);
    video_color->SetBounds(gfx::Size(10, 10));
    video_color->SetIsDrawable(true);
    root->AddChild(video_color);

    scoped_refptr<VideoLayer> video_hw =
        VideoLayer::Create(&hw_frame_provider_, media::VIDEO_ROTATION_0);
    video_hw->SetBounds(gfx::Size(10, 10));
    video_hw->SetIsDrawable(true);
    root->AddChild(video_hw);

    scoped_refptr<VideoLayer> video_scaled_hw =
        VideoLayer::Create(&scaled_hw_frame_provider_, media::VIDEO_ROTATION_0);
    video_scaled_hw->SetBounds(gfx::Size(10, 10));
    video_scaled_hw->SetIsDrawable(true);
    root->AddChild(video_scaled_hw);

    color_video_frame_ = VideoFrame::CreateColorFrame(
        gfx::Size(4, 4), 0x80, 0x80, 0x80, base::TimeDelta());
    hw_video_frame_ = VideoFrame::WrapNativeTexture(
        make_scoped_ptr(
            new gpu::MailboxHolder(mailbox, GL_TEXTURE_2D, sync_point)),
        media::VideoFrame::ReleaseMailboxCB(), gfx::Size(4, 4),
        gfx::Rect(0, 0, 4, 4), gfx::Size(4, 4), base::TimeDelta(), false);
    scaled_hw_video_frame_ = VideoFrame::WrapNativeTexture(
        make_scoped_ptr(
            new gpu::MailboxHolder(mailbox, GL_TEXTURE_2D, sync_point)),
        media::VideoFrame::ReleaseMailboxCB(), gfx::Size(4, 4),
        gfx::Rect(0, 0, 3, 2), gfx::Size(4, 4), base::TimeDelta(), false);

    color_frame_provider_.set_frame(color_video_frame_);
    hw_frame_provider_.set_frame(hw_video_frame_);
    scaled_hw_frame_provider_.set_frame(scaled_hw_video_frame_);

    if (!delegating_renderer()) {
      // TODO(danakj): IOSurface layer can not be transported. crbug.com/239335
      scoped_refptr<IOSurfaceLayer> io_surface = IOSurfaceLayer::Create();
      io_surface->SetBounds(gfx::Size(10, 10));
      io_surface->SetIsDrawable(true);
      io_surface->SetIOSurfaceProperties(1, gfx::Size(10, 10));
      root->AddChild(io_surface);
    }

    // Enable the hud.
    LayerTreeDebugState debug_state;
    debug_state.show_property_changed_rects = true;
    layer_tree_host()->SetDebugState(debug_state);

    scoped_refptr<PaintedScrollbarLayer> scrollbar =
        PaintedScrollbarLayer::Create(
            scoped_ptr<Scrollbar>(new FakeScrollbar).Pass(), layer->id());
    scrollbar->SetBounds(gfx::Size(10, 10));
    scrollbar->SetIsDrawable(true);
    root->AddChild(scrollbar);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostContextTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    LayerTreeHostContextTest::CommitCompleteOnThread(host_impl);

    if (host_impl->active_tree()->source_frame_number() == 3) {
      // On the third commit we're recovering from context loss. Hardware
      // video frames should not be reused by the VideoFrameProvider, but
      // software frames can be.
      hw_frame_provider_.set_frame(NULL);
      scaled_hw_frame_provider_.set_frame(NULL);
    }
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame,
                                   DrawResult draw_result) override {
    if (host_impl->active_tree()->source_frame_number() == 2) {
      // Lose the context during draw on the second commit. This will cause
      // a third commit to recover.
      context3d_->set_times_bind_texture_succeeds(0);
    }
    return draw_result;
  }

  scoped_ptr<FakeOutputSurface> CreateFakeOutputSurface() override {
    // This will get called twice:
    // First when we create the initial output surface...
    if (layer_tree_host()->source_frame_number() > 0) {
      // ... and then again after we forced the context to be lost.
      lost_context_ = true;
    }
    return LayerTreeHostContextTest::CreateFakeOutputSurface();
  }

  void DidCommitAndDrawFrame() override {
    ASSERT_TRUE(layer_tree_host()->hud_layer());
    // End the test once we know the 3nd frame drew.
    if (layer_tree_host()->source_frame_number() < 5) {
      layer_tree_host()->root_layer()->SetNeedsDisplay();
      layer_tree_host()->SetNeedsCommit();
    } else {
      EndTest();
    }
  }

  void AfterTest() override { EXPECT_TRUE(lost_context_); }

 private:
  FakeContentLayerClient client_;
  bool lost_context_;

  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> child_output_surface_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<ResourceProvider> child_resource_provider_;

  scoped_refptr<DelegatedFrameResourceCollection>
      delegated_resource_collection_;
  scoped_refptr<DelegatedFrameProvider> delegated_frame_provider_;

  scoped_refptr<VideoFrame> color_video_frame_;
  scoped_refptr<VideoFrame> hw_video_frame_;
  scoped_refptr<VideoFrame> scaled_hw_video_frame_;

  FakeVideoFrameProvider color_frame_provider_;
  FakeVideoFrameProvider hw_frame_provider_;
  FakeVideoFrameProvider scaled_hw_frame_provider_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestDontUseLostResources);

class ImplSidePaintingLayerTreeHostContextTest
    : public LayerTreeHostContextTest {
 public:
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->impl_side_painting = true;
  }
};

class LayerTreeHostContextTestImplSidePainting
    : public ImplSidePaintingLayerTreeHostContextTest {
 public:
  void SetupTree() override {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetIsDrawable(true);

    scoped_refptr<PictureLayer> picture = PictureLayer::Create(&client_);
    picture->SetBounds(gfx::Size(10, 10));
    picture->SetIsDrawable(true);
    root->AddChild(picture);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostContextTest::SetupTree();
  }

  void BeginTest() override {
    times_to_lose_during_commit_ = 1;
    PostSetNeedsCommitToMainThread();
  }

  void AfterTest() override {}

  void DidInitializeOutputSurface() override { EndTest(); }

 private:
  FakeContentLayerClient client_;
};

MULTI_THREAD_TEST_F(LayerTreeHostContextTestImplSidePainting);

class ScrollbarLayerLostContext : public LayerTreeHostContextTest {
 public:
  ScrollbarLayerLostContext() : commits_(0) {}

  void BeginTest() override {
    scoped_refptr<Layer> scroll_layer = Layer::Create();
    scrollbar_layer_ =
        FakePaintedScrollbarLayer::Create(false, true, scroll_layer->id());
    scrollbar_layer_->SetBounds(gfx::Size(10, 100));
    layer_tree_host()->root_layer()->AddChild(scrollbar_layer_);
    layer_tree_host()->root_layer()->AddChild(scroll_layer);
    PostSetNeedsCommitToMainThread();
  }

  void AfterTest() override {}

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);

    ++commits_;
    switch (commits_) {
      case 1:
        // First (regular) update, we should upload 2 resources (thumb, and
        // backtrack).
        EXPECT_EQ(1, scrollbar_layer_->update_count());
        LoseContext();
        break;
      case 2:
        // Second update, after the lost context, we should still upload 2
        // resources even if the contents haven't changed.
        EXPECT_EQ(2, scrollbar_layer_->update_count());
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  int commits_;
  scoped_refptr<FakePaintedScrollbarLayer> scrollbar_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(ScrollbarLayerLostContext);

class UIResourceLostTest : public LayerTreeHostContextTest {
 public:
  UIResourceLostTest() : time_step_(0) {}
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->renderer_settings.texture_id_allocation_chunk_size = 1;
  }
  void BeginTest() override { PostSetNeedsCommitToMainThread(); }
  void AfterTest() override {}

  // This is called on the main thread after each commit and
  // DidActivateTreeOnThread, with the value of time_step_ at the time
  // of the call to DidActivateTreeOnThread. Similar tests will do
  // work on the main thread in DidCommit but that is unsuitable because
  // the main thread work for these tests must happen after
  // DidActivateTreeOnThread, which happens after DidCommit with impl-side
  // painting.
  virtual void StepCompleteOnMainThread(int time_step) = 0;

  // Called after DidActivateTreeOnThread. If this is done during the commit,
  // the call to StepCompleteOnMainThread will not occur until after
  // the commit completes, because the main thread is blocked.
  void PostStepCompleteToMainThread() {
    proxy()->MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&UIResourceLostTest::StepCompleteOnMainThreadInternal,
                   base::Unretained(this),
                   time_step_));
  }

  void PostLoseContextToImplThread() {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&LayerTreeHostContextTest::LoseContext,
                   base::Unretained(this)));
  }

 protected:
  int time_step_;
  scoped_ptr<FakeScopedUIResource> ui_resource_;

 private:
  void StepCompleteOnMainThreadInternal(int step) {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    StepCompleteOnMainThread(step);
  }
};

class UIResourceLostTestSimple : public UIResourceLostTest {
 public:
  // This is called when the commit is complete and the new layer tree has been
  // activated.
  virtual void StepCompleteOnImplThread(LayerTreeHostImpl* impl) = 0;

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    if (!impl->settings().impl_side_painting) {
      StepCompleteOnImplThread(impl);
      PostStepCompleteToMainThread();
      ++time_step_;
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    if (impl->settings().impl_side_painting) {
      StepCompleteOnImplThread(impl);
      PostStepCompleteToMainThread();
      ++time_step_;
    }
  }
};

// Losing context after an UI resource has been created.
class UIResourceLostAfterCommit : public UIResourceLostTestSimple {
 public:
  void StepCompleteOnMainThread(int step) override {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    switch (step) {
      case 0:
        ui_resource_ = FakeScopedUIResource::Create(layer_tree_host());
        // Expects a valid UIResourceId.
        EXPECT_NE(0, ui_resource_->id());
        PostSetNeedsCommitToMainThread();
        break;
      case 4:
        // Release resource before ending the test.
        ui_resource_ = nullptr;
        EndTest();
        break;
      case 5:
        NOTREACHED();
        break;
    }
  }

  void StepCompleteOnImplThread(LayerTreeHostImpl* impl) override {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    switch (time_step_) {
      case 1:
        // The resource should have been created on LTHI after the commit.
        EXPECT_NE(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        LoseContext();
        break;
      case 3:
        // The resources should have been recreated. The bitmap callback should
        // have been called once with the resource_lost flag set to true.
        EXPECT_EQ(1, ui_resource_->lost_resource_count);
        // Resource Id on the impl-side have been recreated as well. Note
        // that the same UIResourceId persists after the context lost.
        EXPECT_NE(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        PostSetNeedsCommitToMainThread();
        break;
    }
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(UIResourceLostAfterCommit);

// Losing context before UI resource requests can be commited.  Three sequences
// of creation/deletion are considered:
// 1. Create one resource -> Context Lost => Expect the resource to have been
// created.
// 2. Delete an exisiting resource (test_id0_) -> create a second resource
// (test_id1_) -> Context Lost => Expect the test_id0_ to be removed and
// test_id1_ to have been created.
// 3. Create one resource -> Delete that same resource -> Context Lost => Expect
// the resource to not exist in the manager.
class UIResourceLostBeforeCommit : public UIResourceLostTestSimple {
 public:
  UIResourceLostBeforeCommit() : test_id0_(0), test_id1_(0) {}

  void StepCompleteOnMainThread(int step) override {
    switch (step) {
      case 0:
        ui_resource_ = FakeScopedUIResource::Create(layer_tree_host());
        // Lose the context on the impl thread before the commit.
        PostLoseContextToImplThread();
        break;
      case 2:
        // Sequence 2:
        // Currently one resource has been created.
        test_id0_ = ui_resource_->id();
        // Delete this resource.
        ui_resource_ = nullptr;
        // Create another resource.
        ui_resource_ = FakeScopedUIResource::Create(layer_tree_host());
        test_id1_ = ui_resource_->id();
        // Sanity check that two resource creations return different ids.
        EXPECT_NE(test_id0_, test_id1_);
        // Lose the context on the impl thread before the commit.
        PostLoseContextToImplThread();
        break;
      case 3:
        // Clear the manager of resources.
        ui_resource_ = nullptr;
        PostSetNeedsCommitToMainThread();
        break;
      case 4:
        // Sequence 3:
        ui_resource_ = FakeScopedUIResource::Create(layer_tree_host());
        test_id0_ = ui_resource_->id();
        // Sanity check the UIResourceId should not be 0.
        EXPECT_NE(0, test_id0_);
        // Usually ScopedUIResource are deleted from the manager in their
        // destructor (so usually ui_resource_ = nullptr).  But here we need
        // ui_resource_ for the next step, so call DeleteUIResource directly.
        layer_tree_host()->DeleteUIResource(test_id0_);
        // Delete the resouce and then lose the context.
        PostLoseContextToImplThread();
        break;
      case 5:
        // Release resource before ending the test.
        ui_resource_ = nullptr;
        EndTest();
        break;
      case 6:
        NOTREACHED();
        break;
    }
  }

  void StepCompleteOnImplThread(LayerTreeHostImpl* impl) override {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    switch (time_step_) {
      case 1:
        // Sequence 1 (continued):
        // The first context lost happens before the resources were created,
        // and because it resulted in no resources being destroyed, it does not
        // trigger resource re-creation.
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        // Resource Id on the impl-side has been created.
        PostSetNeedsCommitToMainThread();
        break;
      case 3:
        // Sequence 2 (continued):
        // The previous resource should have been deleted.
        EXPECT_EQ(0u, impl->ResourceIdForUIResource(test_id0_));
        // The second resource should have been created.
        EXPECT_NE(0u, impl->ResourceIdForUIResource(test_id1_));
        // The second resource called the resource callback once and since the
        // context is lost, a "resource lost" callback was also issued.
        EXPECT_EQ(2, ui_resource_->resource_create_count);
        EXPECT_EQ(1, ui_resource_->lost_resource_count);
        break;
      case 5:
        // Sequence 3 (continued):
        // Expect the resource callback to have been called once.
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        // No "resource lost" callbacks.
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        // The UI resource id should not be valid
        EXPECT_EQ(0u, impl->ResourceIdForUIResource(test_id0_));
        break;
    }
  }

 private:
  UIResourceId test_id0_;
  UIResourceId test_id1_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(UIResourceLostBeforeCommit);

// Losing UI resource before the pending trees is activated but after the
// commit.  Impl-side-painting only.
class UIResourceLostBeforeActivateTree : public UIResourceLostTest {
  void StepCompleteOnMainThread(int step) override {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    switch (step) {
      case 0:
        ui_resource_ = FakeScopedUIResource::Create(layer_tree_host());
        PostSetNeedsCommitToMainThread();
        break;
      case 3:
        test_id_ = ui_resource_->id();
        ui_resource_ = nullptr;
        PostSetNeedsCommitToMainThread();
        break;
      case 5:
        // Release resource before ending the test.
        ui_resource_ = nullptr;
        EndTest();
        break;
      case 6:
        // Make sure no extra commits happened.
        NOTREACHED();
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    switch (time_step_) {
      case 2:
        PostSetNeedsCommitToMainThread();
        break;
      case 4:
        PostSetNeedsCommitToMainThread();
        break;
    }
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    switch (time_step_) {
      case 1:
        // The resource creation callback has been called.
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        // The resource is not yet lost (sanity check).
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        // The resource should not have been created yet on the impl-side.
        EXPECT_EQ(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        LoseContext();
        break;
      case 3:
        LoseContext();
        break;
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    LayerTreeHostContextTest::DidActivateTreeOnThread(impl);
    switch (time_step_) {
      case 1:
        // The pending requests on the impl-side should have been processed.
        EXPECT_NE(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        break;
      case 2:
        // The "lost resource" callback should have been called once.
        EXPECT_EQ(1, ui_resource_->lost_resource_count);
        break;
      case 4:
        // The resource is deleted and should not be in the manager.  Use
        // test_id_ since ui_resource_ has been deleted.
        EXPECT_EQ(0u, impl->ResourceIdForUIResource(test_id_));
        break;
    }

    PostStepCompleteToMainThread();
    ++time_step_;
  }

 private:
  UIResourceId test_id_;
};

TEST_F(UIResourceLostBeforeActivateTree,
       RunMultiThread_DirectRenderer_ImplSidePaint) {
  RunTest(true, false, true);
}

TEST_F(UIResourceLostBeforeActivateTree,
       RunMultiThread_DelegatingRenderer_ImplSidePaint) {
  RunTest(true, true, true);
}

// Resources evicted explicitly and by visibility changes.
class UIResourceLostEviction : public UIResourceLostTestSimple {
 public:
  void StepCompleteOnMainThread(int step) override {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    switch (step) {
      case 0:
        ui_resource_ = FakeScopedUIResource::Create(layer_tree_host());
        EXPECT_NE(0, ui_resource_->id());
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        // Make the tree not visible.
        PostSetVisibleToMainThread(false);
        break;
      case 3:
        // Release resource before ending the test.
        ui_resource_ = nullptr;
        EndTest();
        break;
      case 4:
        NOTREACHED();
    }
  }

  void DidSetVisibleOnImplTree(LayerTreeHostImpl* impl, bool visible) override {
    TestWebGraphicsContext3D* context = TestContext();
    if (!visible) {
      // All resources should have been evicted.
      ASSERT_EQ(0u, context->NumTextures());
      EXPECT_EQ(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
      EXPECT_EQ(2, ui_resource_->resource_create_count);
      EXPECT_EQ(1, ui_resource_->lost_resource_count);
      // Drawing is disabled both because of the evicted resources and
      // because the renderer is not visible.
      EXPECT_FALSE(impl->CanDraw());
      // Make the renderer visible again.
      PostSetVisibleToMainThread(true);
    }
  }

  void StepCompleteOnImplThread(LayerTreeHostImpl* impl) override {
    TestWebGraphicsContext3D* context = TestContext();
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    switch (time_step_) {
      case 1:
        // The resource should have been created on LTHI after the commit.
        ASSERT_EQ(1u, context->NumTextures());
        EXPECT_NE(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        EXPECT_TRUE(impl->CanDraw());
        // Evict all UI resources. This will trigger a commit.
        impl->EvictAllUIResources();
        ASSERT_EQ(0u, context->NumTextures());
        EXPECT_EQ(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        EXPECT_FALSE(impl->CanDraw());
        break;
      case 2:
        // The resource should have been recreated.
        ASSERT_EQ(1u, context->NumTextures());
        EXPECT_NE(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_EQ(2, ui_resource_->resource_create_count);
        EXPECT_EQ(1, ui_resource_->lost_resource_count);
        EXPECT_TRUE(impl->CanDraw());
        break;
      case 3:
        // The resource should have been recreated after visibility was
        // restored.
        ASSERT_EQ(1u, context->NumTextures());
        EXPECT_NE(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_EQ(3, ui_resource_->resource_create_count);
        EXPECT_EQ(2, ui_resource_->lost_resource_count);
        EXPECT_TRUE(impl->CanDraw());
        break;
    }
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(UIResourceLostEviction);

class LayerTreeHostContextTestSurfaceCreateCallback
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestSurfaceCreateCallback()
      : LayerTreeHostContextTest() {}

  void SetupTree() override {
    if (layer_tree_host()->settings().impl_side_painting) {
      picture_layer_ = FakePictureLayer::Create(&client_);
      picture_layer_->SetBounds(gfx::Size(10, 20));
      layer_tree_host()->SetRootLayer(picture_layer_);
    } else {
      content_layer_ = FakeContentLayer::Create(&client_);
      content_layer_->SetBounds(gfx::Size(10, 20));
      layer_tree_host()->SetRootLayer(content_layer_);
    }

    LayerTreeHostContextTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        if (layer_tree_host()->settings().impl_side_painting)
          EXPECT_EQ(1u, picture_layer_->output_surface_created_count());
        else
          EXPECT_EQ(1u, content_layer_->output_surface_created_count());
        layer_tree_host()->SetNeedsCommit();
        break;
      case 2:
        if (layer_tree_host()->settings().impl_side_painting)
          EXPECT_EQ(1u, picture_layer_->output_surface_created_count());
        else
          EXPECT_EQ(1u, content_layer_->output_surface_created_count());
        layer_tree_host()->SetNeedsCommit();
        break;
      case 3:
        if (layer_tree_host()->settings().impl_side_painting)
          EXPECT_EQ(1u, picture_layer_->output_surface_created_count());
        else
          EXPECT_EQ(1u, content_layer_->output_surface_created_count());
        break;
      case 4:
        if (layer_tree_host()->settings().impl_side_painting)
          EXPECT_EQ(2u, picture_layer_->output_surface_created_count());
        else
          EXPECT_EQ(2u, content_layer_->output_surface_created_count());
        layer_tree_host()->SetNeedsCommit();
        break;
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    switch (LastCommittedSourceFrameNumber(impl)) {
      case 0:
        break;
      case 1:
        break;
      case 2:
        LoseContext();
        break;
      case 3:
        EndTest();
        break;
    }
  }

  void AfterTest() override {}

 protected:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_layer_;
  scoped_refptr<FakeContentLayer> content_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestSurfaceCreateCallback);

class LayerTreeHostContextTestLoseAfterSendingBeginMainFrame
    : public LayerTreeHostContextTest {
 protected:
  void BeginTest() override {
    deferred_ = false;
    PostSetNeedsCommitToMainThread();
  }

  void ScheduledActionWillSendBeginMainFrame() override {
    if (deferred_)
      return;
    deferred_ = true;

    // Defer commits before the BeginFrame arrives, causing it to be delayed.
    PostSetDeferCommitsToMainThread(true);
    // Meanwhile, lose the context while we are in defer commits.
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&LayerTreeHostContextTestLoseAfterSendingBeginMainFrame::
                       LoseContextOnImplThread,
                   base::Unretained(this)));
  }

  void LoseContextOnImplThread() {
    LoseContext();

    // After losing the context, stop deferring commits.
    PostSetDeferCommitsToMainThread(false);
  }

  void WillBeginMainFrame() override {
    // Don't begin a frame with a lost surface.
    EXPECT_FALSE(layer_tree_host()->output_surface_lost());
  }

  void DidCommitAndDrawFrame() override { EndTest(); }

  void AfterTest() override {}

  bool deferred_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestLoseAfterSendingBeginMainFrame);

}  // namespace
}  // namespace cc
