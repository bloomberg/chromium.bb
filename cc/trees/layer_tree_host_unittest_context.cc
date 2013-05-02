// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "base/basictypes.h"
#include "cc/base/thread_impl.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/io_surface_layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/scrollbar_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/layers/video_layer.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/test/fake_content_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_content_layer_impl.h"
#include "cc/test/fake_context_provider.h"
#include "cc/test/fake_delegated_renderer_layer.h"
#include "cc/test/fake_delegated_renderer_layer_impl.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_scrollbar_layer.h"
#include "cc/test/fake_scrollbar_theme_painter.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/fake_web_scrollbar.h"
#include "cc/test/fake_web_scrollbar_theme_geometry.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "media/base/media.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"

using media::VideoFrame;
using WebKit::WebGraphicsContext3D;

namespace cc {
namespace {

// These tests deal with losing the 3d graphics context.
class LayerTreeHostContextTest : public LayerTreeTest {
 public:
  LayerTreeHostContextTest()
      : LayerTreeTest(),
        context3d_(NULL),
        times_to_fail_create_(0),
        times_to_fail_initialize_(0),
        times_to_lose_on_create_(0),
        times_to_lose_during_commit_(0),
        times_to_lose_during_draw_(0),
        times_to_fail_recreate_(0),
        times_to_fail_reinitialize_(0),
        times_to_lose_on_recreate_(0),
        times_to_fail_create_offscreen_(0),
        times_to_fail_recreate_offscreen_(0),
        times_to_expect_create_failed_(0),
        times_create_failed_(0),
        times_offscreen_created_(0),
        committed_at_least_once_(false),
        context_should_support_io_surface_(false) {
    media::InitializeMediaLibraryForTesting();
  }

  void LoseContext() {
    context3d_->loseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                                    GL_INNOCENT_CONTEXT_RESET_ARB);
    context3d_ = NULL;
  }

  virtual scoped_ptr<TestWebGraphicsContext3D> CreateContext3d() {
    return TestWebGraphicsContext3D::Create();
  }

  virtual scoped_ptr<OutputSurface> CreateOutputSurface() OVERRIDE {
    if (times_to_fail_create_) {
      --times_to_fail_create_;
      ExpectCreateToFail();
      return scoped_ptr<OutputSurface>();
    }

    scoped_ptr<TestWebGraphicsContext3D> context3d = CreateContext3d();
    context3d_ = context3d.get();

    if (context_should_support_io_surface_) {
      context3d_->set_have_extension_io_surface(true);
      context3d_->set_have_extension_egl_image(true);
    }

    if (times_to_fail_initialize_) {
      --times_to_fail_initialize_;
      // Make the context get lost during reinitialization.
      // The number of times MakeCurrent succeeds is not important, and
      // can be changed if needed to make this pass with future changes.
      context3d_->set_times_make_current_succeeds(2);
      ExpectCreateToFail();
    } else if (times_to_lose_on_create_) {
      --times_to_lose_on_create_;
      LoseContext();
      ExpectCreateToFail();
    }

    return FakeOutputSurface::Create3d(
        context3d.PassAs<WebGraphicsContext3D>()).PassAs<OutputSurface>();
  }

  scoped_ptr<TestWebGraphicsContext3D> CreateOffscreenContext3d() {
    if (!context3d_)
      return scoped_ptr<TestWebGraphicsContext3D>();

    ++times_offscreen_created_;

    if (times_to_fail_create_offscreen_) {
      --times_to_fail_create_offscreen_;
      ExpectCreateToFail();
      return scoped_ptr<TestWebGraphicsContext3D>();
    }

    scoped_ptr<TestWebGraphicsContext3D> offscreen_context3d =
        TestWebGraphicsContext3D::Create().Pass();
    DCHECK(offscreen_context3d);
    context3d_->add_share_group_context(offscreen_context3d.get());

    return offscreen_context3d.Pass();
  }

  virtual scoped_refptr<cc::ContextProvider>
  OffscreenContextProviderForMainThread() OVERRIDE {
    DCHECK(!ImplThread());

    if (!offscreen_contexts_main_thread_ ||
        offscreen_contexts_main_thread_->DestroyedOnMainThread()) {
      offscreen_contexts_main_thread_ = FakeContextProvider::Create(
          base::Bind(&LayerTreeHostContextTest::CreateOffscreenContext3d,
                     base::Unretained(this)));
      if (offscreen_contexts_main_thread_ &&
          !offscreen_contexts_main_thread_->BindToCurrentThread())
        offscreen_contexts_main_thread_ = NULL;
    }
    return offscreen_contexts_main_thread_;
  }

  virtual scoped_refptr<cc::ContextProvider>
  OffscreenContextProviderForCompositorThread() OVERRIDE {
    DCHECK(ImplThread());

    if (!offscreen_contexts_compositor_thread_ ||
        offscreen_contexts_compositor_thread_->DestroyedOnMainThread()) {
      offscreen_contexts_compositor_thread_ = FakeContextProvider::Create(
          base::Bind(&LayerTreeHostContextTest::CreateOffscreenContext3d,
                     base::Unretained(this)));
    }
    return offscreen_contexts_compositor_thread_;
  }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame,
                                     bool result) OVERRIDE {
    EXPECT_TRUE(result);
    if (!times_to_lose_during_draw_)
      return result;

    --times_to_lose_during_draw_;
    context3d_->set_times_make_current_succeeds(0);

    times_to_fail_create_ = times_to_fail_recreate_;
    times_to_fail_recreate_ = 0;
    times_to_fail_initialize_ = times_to_fail_reinitialize_;
    times_to_fail_reinitialize_ = 0;
    times_to_lose_on_create_ = times_to_lose_on_recreate_;
    times_to_lose_on_recreate_ = 0;
    times_to_fail_create_offscreen_ = times_to_fail_recreate_offscreen_;
    times_to_fail_recreate_offscreen_ = 0;

    return result;
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    committed_at_least_once_ = true;

    if (!times_to_lose_during_commit_)
      return;
    --times_to_lose_during_commit_;
    LoseContext();

    times_to_fail_create_ = times_to_fail_recreate_;
    times_to_fail_recreate_ = 0;
    times_to_fail_initialize_ = times_to_fail_reinitialize_;
    times_to_fail_reinitialize_ = 0;
    times_to_lose_on_create_ = times_to_lose_on_recreate_;
    times_to_lose_on_recreate_ = 0;
    times_to_fail_create_offscreen_ = times_to_fail_recreate_offscreen_;
    times_to_fail_recreate_offscreen_ = 0;
  }

  virtual void DidFailToInitializeOutputSurface() OVERRIDE {
    ++times_create_failed_;
  }

  virtual void TearDown() OVERRIDE {
    LayerTreeTest::TearDown();
    EXPECT_EQ(times_to_expect_create_failed_, times_create_failed_);
  }

  void ExpectCreateToFail() {
    ++times_to_expect_create_failed_;
  }

 protected:
  TestWebGraphicsContext3D* context3d_;
  int times_to_fail_create_;
  int times_to_fail_initialize_;
  int times_to_lose_on_create_;
  int times_to_lose_during_commit_;
  int times_to_lose_during_draw_;
  int times_to_fail_recreate_;
  int times_to_fail_reinitialize_;
  int times_to_lose_on_recreate_;
  int times_to_fail_create_offscreen_;
  int times_to_fail_recreate_offscreen_;
  int times_to_expect_create_failed_;
  int times_create_failed_;
  int times_offscreen_created_;
  bool committed_at_least_once_;
  bool context_should_support_io_surface_;

  scoped_refptr<FakeContextProvider> offscreen_contexts_main_thread_;
  scoped_refptr<FakeContextProvider> offscreen_contexts_compositor_thread_;
};

class LayerTreeHostContextTestLostContextSucceeds
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextSucceeds()
      : LayerTreeHostContextTest(),
        test_case_(0),
        num_losses_(0),
        recovered_context_(true),
        first_initialized_(false) {}

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidRecreateOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);

    if (first_initialized_)
      ++num_losses_;
    else
      first_initialized_ = true;

    recovered_context_ = true;
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(10u, test_case_);
    EXPECT_EQ(8 + 10 + 10, num_losses_);
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
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
  }

  bool NextTestCase() {
    static const TestCase kTests[] = {
      // Losing the context and failing to recreate it (or losing it again
      // immediately) a small number of times should succeed.
      { 1,  // times_to_lose_during_commit
        0,  // times_to_lose_during_draw
        3,  // times_to_fail_reinitialize
        0,  // times_to_fail_recreate
        0,  // times_to_lose_on_recreate
        0,  // times_to_fail_recreate_offscreen
      },
      { 0,  // times_to_lose_during_commit
        1,  // times_to_lose_during_draw
        3,  // times_to_fail_reinitialize
        0,  // times_to_fail_recreate
        0,  // times_to_lose_on_recreate
        0,  // times_to_fail_recreate_offscreen
      },
      { 1,  // times_to_lose_during_commit
        0,  // times_to_lose_during_draw
        0,  // times_to_fail_reinitialize
        3,  // times_to_fail_recreate
        0,  // times_to_lose_on_recreate
        0,  // times_to_fail_recreate_offscreen
      },
      { 0,  // times_to_lose_during_commit
        1,  // times_to_lose_during_draw
        0,  // times_to_fail_reinitialize
        3,  // times_to_fail_recreate
        0,  // times_to_lose_on_recreate
        0,  // times_to_fail_recreate_offscreen
      },
      { 1,  // times_to_lose_during_commit
        0,  // times_to_lose_during_draw
        0,  // times_to_fail_reinitialize
        0,  // times_to_fail_recreate
        3,  // times_to_lose_on_recreate
        0,  // times_to_fail_recreate_offscreen
      },
      { 0,  // times_to_lose_during_commit
        1,  // times_to_lose_during_draw
        0,  // times_to_fail_reinitialize
        0,  // times_to_fail_recreate
        3,  // times_to_lose_on_recreate
        0,  // times_to_fail_recreate_offscreen
      },
      { 1,  // times_to_lose_during_commit
        0,  // times_to_lose_during_draw
        0,  // times_to_fail_reinitialize
        0,  // times_to_fail_recreate
        0,  // times_to_lose_on_recreate
        3,  // times_to_fail_recreate_offscreen
      },
      { 0,  // times_to_lose_during_commit
        1,  // times_to_lose_during_draw
        0,  // times_to_fail_reinitialize
        0,  // times_to_fail_recreate
        0,  // times_to_lose_on_recreate
        3,  // times_to_fail_recreate_offscreen
      },
      // Losing the context and recreating it any number of times should
      // succeed.
      { 10,  // times_to_lose_during_commit
        0,  // times_to_lose_during_draw
        0,  // times_to_fail_reinitialize
        0,  // times_to_fail_recreate
        0,  // times_to_lose_on_recreate
        0,  // times_to_fail_recreate_offscreen
      },
      { 0,  // times_to_lose_during_commit
        10,  // times_to_lose_during_draw
        0,  // times_to_fail_reinitialize
        0,  // times_to_fail_recreate
        0,  // times_to_lose_on_recreate
        0,  // times_to_fail_recreate_offscreen
      },
    };

    if (test_case_ >= arraysize(kTests))
      return false;

    times_to_lose_during_commit_ =
        kTests[test_case_].times_to_lose_during_commit;
    times_to_lose_during_draw_ =
        kTests[test_case_].times_to_lose_during_draw;
    times_to_fail_reinitialize_ = kTests[test_case_].times_to_fail_reinitialize;
    times_to_fail_recreate_ = kTests[test_case_].times_to_fail_recreate;
    times_to_lose_on_recreate_ = kTests[test_case_].times_to_lose_on_recreate;
    times_to_fail_recreate_offscreen_ =
        kTests[test_case_].times_to_fail_recreate_offscreen;
    ++test_case_;
    return true;
  }

  struct TestCase {
    int times_to_lose_during_commit;
    int times_to_lose_during_draw;
    int times_to_fail_reinitialize;
    int times_to_fail_recreate;
    int times_to_lose_on_recreate;
    int times_to_fail_recreate_offscreen;
  };

 protected:
  size_t test_case_;
  int num_losses_;
  bool recovered_context_;
  bool first_initialized_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestLostContextSucceeds);

class LayerTreeHostContextTestLostContextSucceedsWithContent
    : public LayerTreeHostContextTestLostContextSucceeds {
 public:
  LayerTreeHostContextTestLostContextSucceedsWithContent()
      : LayerTreeHostContextTestLostContextSucceeds() {}

  virtual void SetupTree() OVERRIDE {
    root_ = Layer::Create();
    root_->SetBounds(gfx::Size(10, 10));
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetIsDrawable(true);

    content_ = FakeContentLayer::Create(&client_);
    content_->SetBounds(gfx::Size(10, 10));
    content_->SetAnchorPoint(gfx::PointF());
    content_->SetIsDrawable(true);
    if (use_surface_) {
      content_->SetForceRenderSurface(true);
      // Filters require us to create an offscreen context.
      WebKit::WebFilterOperations filters;
      filters.append(WebKit::WebFilterOperation::createGrayscaleFilter(0.5f));
      content_->SetFilters(filters);
      content_->SetBackgroundFilters(filters);
    }

    root_->AddChild(content_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void InvalidateAndSetNeedsCommit() OVERRIDE {
    // Invalidate the render surface so we don't try to use a cached copy of the
    // surface.  We want to make sure to test the drawing paths for drawing to
    // a child surface.
    content_->SetNeedsDisplay();
    LayerTreeHostContextTestLostContextSucceeds::InvalidateAndSetNeedsCommit();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    FakeContentLayerImpl* content_impl = static_cast<FakeContentLayerImpl*>(
        host_impl->active_tree()->root_layer()->children()[0]);
    // Even though the context was lost, we should have a resource. The
    // TestWebGraphicsContext3D ensures that this resource is created with
    // the active context.
    EXPECT_TRUE(content_impl->HaveResourceForTileAt(0, 0));

    cc::ContextProvider* contexts =
        host_impl->resource_provider()->offscreen_context_provider();
    if (use_surface_) {
      EXPECT_TRUE(contexts->Context3d());
      // TODO(danakj): Make a fake GrContext.
      // EXPECT_TRUE(contexts->GrContext());
    } else {
      EXPECT_FALSE(contexts);
    }
  }

  virtual void AfterTest() OVERRIDE {
    LayerTreeHostContextTestLostContextSucceeds::AfterTest();
    if (use_surface_) {
      // 1 create to start with +
      // 6 from test cases that fail on initializing the renderer (after the
      // offscreen context is created) +
      // 6 from test cases that lose the offscreen context directly +
      // All the test cases that recreate both contexts only once
      // per time it is lost.
      EXPECT_EQ(6 + 6 + 1 + num_losses_, times_offscreen_created_);
    } else {
      EXPECT_EQ(0, times_offscreen_created_);
    }
  }

 protected:
  bool use_surface_;
  FakeContentLayerClient client_;
  scoped_refptr<Layer> root_;
  scoped_refptr<ContentLayer> content_;
};

TEST_F(LayerTreeHostContextTestLostContextSucceedsWithContent,
       NoSurface_SingleThread) {
  use_surface_ = false;
  RunTest(false);
}

TEST_F(LayerTreeHostContextTestLostContextSucceedsWithContent,
       NoSurface_MultiThread) {
  use_surface_ = false;
  RunTest(true);
}

TEST_F(LayerTreeHostContextTestLostContextSucceedsWithContent,
       WithSurface_SingleThread) {
  use_surface_ = true;
  RunTest(false);
}

TEST_F(LayerTreeHostContextTestLostContextSucceedsWithContent,
       WithSurface_MultiThread) {
  use_surface_ = true;
  RunTest(true);
}

class LayerTreeHostContextTestOffscreenContextFails
    : public LayerTreeHostContextTest {
 public:
  virtual void SetupTree() OVERRIDE {
    root_ = Layer::Create();
    root_->SetBounds(gfx::Size(10, 10));
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetIsDrawable(true);

    content_ = FakeContentLayer::Create(&client_);
    content_->SetBounds(gfx::Size(10, 10));
    content_->SetAnchorPoint(gfx::PointF());
    content_->SetIsDrawable(true);
    content_->SetForceRenderSurface(true);
    // Filters require us to create an offscreen context.
    WebKit::WebFilterOperations filters;
    filters.append(WebKit::WebFilterOperation::createGrayscaleFilter(0.5f));
    content_->SetFilters(filters);
    content_->SetBackgroundFilters(filters);

    root_->AddChild(content_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    times_to_fail_create_offscreen_ = 1;
    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    cc::ContextProvider* contexts =
        host_impl->resource_provider()->offscreen_context_provider();
    EXPECT_FALSE(contexts);

    // This did not lead to create failure.
    times_to_expect_create_failed_ = 0;
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 protected:
  FakeContentLayerClient client_;
  scoped_refptr<Layer> root_;
  scoped_refptr<ContentLayer> content_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestOffscreenContextFails);

class LayerTreeHostContextTestLostContextFails
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextFails()
      : LayerTreeHostContextTest(),
        num_commits_(0),
        first_initialized_(false) {
    times_to_lose_during_commit_ = 1;
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidRecreateOutputSurface(bool succeeded) OVERRIDE {
    if (first_initialized_) {
      EXPECT_FALSE(succeeded);
      EndTest();
    } else {
      first_initialized_ = true;
    }
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerTreeHostContextTest::CommitCompleteOnThread(host_impl);

    ++num_commits_;
    if (num_commits_ == 1) {
      // When the context is ok, we should have these things.
      EXPECT_TRUE(host_impl->output_surface());
      EXPECT_TRUE(host_impl->renderer());
      EXPECT_TRUE(host_impl->resource_provider());
      return;
    }

    // When context recreation fails we shouldn't be left with any of them.
    EXPECT_FALSE(host_impl->output_surface());
    EXPECT_FALSE(host_impl->renderer());
    EXPECT_FALSE(host_impl->resource_provider());
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_commits_;
  bool first_initialized_;
};

TEST_F(LayerTreeHostContextTestLostContextFails,
       FailReinitialize100_SingleThread) {
  times_to_fail_reinitialize_ = 100;
  times_to_fail_recreate_ = 0;
  times_to_lose_on_recreate_ = 0;
  RunTest(false);
}

TEST_F(LayerTreeHostContextTestLostContextFails,
       FailReinitialize100_MultiThread) {
  times_to_fail_reinitialize_ = 100;
  times_to_fail_recreate_ = 0;
  times_to_lose_on_recreate_ = 0;
  RunTest(true);
}

TEST_F(LayerTreeHostContextTestLostContextFails,
       FailRecreate100_SingleThread) {
  times_to_fail_reinitialize_ = 0;
  times_to_fail_recreate_ = 100;
  times_to_lose_on_recreate_ = 0;
  RunTest(false);
}

TEST_F(LayerTreeHostContextTestLostContextFails,
       FailRecreate100_MultiThread) {
  times_to_fail_reinitialize_ = 0;
  times_to_fail_recreate_ = 100;
  times_to_lose_on_recreate_ = 0;
  RunTest(true);
}

TEST_F(LayerTreeHostContextTestLostContextFails,
       LoseOnRecreate100_SingleThread) {
  times_to_fail_reinitialize_ = 0;
  times_to_fail_recreate_ = 0;
  times_to_lose_on_recreate_ = 100;
  RunTest(false);
}

TEST_F(LayerTreeHostContextTestLostContextFails,
       LoseOnRecreate100_MultiThread) {
  times_to_fail_reinitialize_ = 0;
  times_to_fail_recreate_ = 0;
  times_to_lose_on_recreate_ = 100;
  RunTest(true);
}

class LayerTreeHostContextTestFinishAllRenderingAfterLoss
    : public LayerTreeHostContextTest {
 public:
  virtual void BeginTest() OVERRIDE {
    // Lose the context until the compositor gives up on it.
    first_initialized_ = false;
    times_to_lose_during_commit_ = 1;
    times_to_fail_reinitialize_ = 10;
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidRecreateOutputSurface(bool succeeded) OVERRIDE {
    if (first_initialized_) {
      EXPECT_FALSE(succeeded);
      layer_tree_host()->FinishAllRendering();
      EndTest();
    } else {
      first_initialized_ = true;
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  bool first_initialized_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestFinishAllRenderingAfterLoss);

class LayerTreeHostContextTestLostContextAndEvictTextures
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextAndEvictTextures()
      : LayerTreeHostContextTest(),
        layer_(FakeContentLayer::Create(&client_)),
        impl_host_(0),
        num_commits_(0) {}

  virtual void SetupTree() OVERRIDE {
    layer_->SetBounds(gfx::Size(10, 20));
    layer_tree_host()->SetRootLayer(layer_);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  void PostEvictTextures() {
    if (ImplThread()) {
      ImplThread()->PostTask(
          base::Bind(
              &LayerTreeHostContextTestLostContextAndEvictTextures::
              EvictTexturesOnImplThread,
              base::Unretained(this)));
    } else {
      DebugScopedSetImplThread impl(proxy());
      EvictTexturesOnImplThread();
    }
  }

  void EvictTexturesOnImplThread() {
    impl_host_->EnforceManagedMemoryPolicy(ManagedMemoryPolicy(0));
    if (lose_after_evict_)
      LoseContext();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    if (num_commits_ > 1)
      return;
    EXPECT_TRUE(layer_->HaveBackingAt(0, 0));
    PostEvictTextures();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    if (num_commits_ > 1)
      return;
    ++num_commits_;
    if (!lose_after_evict_)
      LoseContext();
    impl_host_ = impl;
  }

  virtual void DidRecreateOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 protected:
  bool lose_after_evict_;
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> layer_;
  LayerTreeHostImpl* impl_host_;
  int num_commits_;
};

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseAfterEvict_SingleThread) {
  lose_after_evict_ = true;
  RunTest(false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseAfterEvict_MultiThread) {
  lose_after_evict_ = true;
  RunTest(true);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_SingleThread) {
  lose_after_evict_ = false;
  RunTest(false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_MultiThread) {
  lose_after_evict_ = false;
  RunTest(true);
}

class LayerTreeHostContextTestLostContextWhileUpdatingResources
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextWhileUpdatingResources()
      : parent_(FakeContentLayer::Create(&client_)),
        num_children_(50),
        times_to_lose_on_end_query_(3) {}

  virtual scoped_ptr<TestWebGraphicsContext3D> CreateContext3d() OVERRIDE {
    scoped_ptr<TestWebGraphicsContext3D> context =
        LayerTreeHostContextTest::CreateContext3d();
    if (times_to_lose_on_end_query_) {
      --times_to_lose_on_end_query_;
      context->set_times_end_query_succeeds(5);
    }
    return context.Pass();
  }

  virtual void SetupTree() OVERRIDE {
    parent_->SetBounds(gfx::Size(num_children_, 1));

    for (int i = 0; i < num_children_; i++) {
      scoped_refptr<FakeContentLayer> child =
          FakeContentLayer::Create(&client_);
      child->SetPosition(gfx::PointF(i, 0.f));
      child->SetBounds(gfx::Size(1, 1));
      parent_->AddChild(child);
    }

    layer_tree_host()->SetRootLayer(parent_);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    EndTest();
  }

  virtual void DidRecreateOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(0, times_to_lose_on_end_query_);
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> parent_;
  int num_children_;
  int times_to_lose_on_end_query_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestLostContextWhileUpdatingResources);

class LayerTreeHostContextTestLayersNotified
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLayersNotified()
      : LayerTreeHostContextTest(),
        num_commits_(0) {}

  virtual void SetupTree() OVERRIDE {
    root_ = FakeContentLayer::Create(&client_);
    child_ = FakeContentLayer::Create(&client_);
    grandchild_ = FakeContentLayer::Create(&client_);

    root_->AddChild(child_);
    child_->AddChild(grandchild_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerTreeHostContextTest::CommitCompleteOnThread(host_impl);

    FakeContentLayerImpl* root = static_cast<FakeContentLayerImpl*>(
        host_impl->active_tree()->root_layer());
    FakeContentLayerImpl* child = static_cast<FakeContentLayerImpl*>(
        root->children()[0]);
    FakeContentLayerImpl* grandchild = static_cast<FakeContentLayerImpl*>(
        child->children()[0]);

    ++num_commits_;
    switch (num_commits_) {
      case 1:
        EXPECT_EQ(0u, root->lost_output_surface_count());
        EXPECT_EQ(0u, child->lost_output_surface_count());
        EXPECT_EQ(0u, grandchild->lost_output_surface_count());
        // Lose the context and struggle to recreate it.
        LoseContext();
        times_to_fail_create_ = 1;
        break;
      case 2:
        EXPECT_EQ(1u, root->lost_output_surface_count());
        EXPECT_EQ(1u, child->lost_output_surface_count());
        EXPECT_EQ(1u, grandchild->lost_output_surface_count());
        // Lose the context and again during recreate.
        LoseContext();
        times_to_lose_on_create_ = 1;
        break;
      case 3:
        EXPECT_EQ(3u, root->lost_output_surface_count());
        EXPECT_EQ(3u, child->lost_output_surface_count());
        EXPECT_EQ(3u, grandchild->lost_output_surface_count());
        // Lose the context and again during reinitialization.
        LoseContext();
        times_to_fail_initialize_ = 1;
        break;
      case 4:
        EXPECT_EQ(5u, root->lost_output_surface_count());
        EXPECT_EQ(5u, child->lost_output_surface_count());
        EXPECT_EQ(5u, grandchild->lost_output_surface_count());
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_commits_;

  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_;
  scoped_refptr<FakeContentLayer> child_;
  scoped_refptr<FakeContentLayer> grandchild_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestLayersNotified);

class LayerTreeHostContextTestDontUseLostResources
    : public LayerTreeHostContextTest {
 public:
  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root_ = Layer::Create();
    root_->SetBounds(gfx::Size(10, 10));
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetIsDrawable(true);

    scoped_refptr<FakeDelegatedRendererLayer> delegated_ =
        FakeDelegatedRendererLayer::Create();
    delegated_->SetBounds(gfx::Size(10, 10));
    delegated_->SetAnchorPoint(gfx::PointF());
    delegated_->SetIsDrawable(true);
    root_->AddChild(delegated_);

    scoped_refptr<ContentLayer> content_ = ContentLayer::Create(&client_);
    content_->SetBounds(gfx::Size(10, 10));
    content_->SetAnchorPoint(gfx::PointF());
    content_->SetIsDrawable(true);
    root_->AddChild(content_);

    scoped_refptr<TextureLayer> texture_ = TextureLayer::Create(NULL);
    texture_->SetBounds(gfx::Size(10, 10));
    texture_->SetAnchorPoint(gfx::PointF());
    texture_->SetTextureId(TestWebGraphicsContext3D::kExternalTextureId);
    texture_->SetIsDrawable(true);
    root_->AddChild(texture_);

    scoped_refptr<ContentLayer> mask_ = ContentLayer::Create(&client_);
    mask_->SetBounds(gfx::Size(10, 10));
    mask_->SetAnchorPoint(gfx::PointF());

    scoped_refptr<ContentLayer> content_with_mask_ =
        ContentLayer::Create(&client_);
    content_with_mask_->SetBounds(gfx::Size(10, 10));
    content_with_mask_->SetAnchorPoint(gfx::PointF());
    content_with_mask_->SetIsDrawable(true);
    content_with_mask_->SetMaskLayer(mask_.get());
    root_->AddChild(content_with_mask_);

    scoped_refptr<VideoLayer> video_color_ = VideoLayer::Create(
        &color_frame_provider_);
    video_color_->SetBounds(gfx::Size(10, 10));
    video_color_->SetAnchorPoint(gfx::PointF());
    video_color_->SetIsDrawable(true);
    root_->AddChild(video_color_);

    scoped_refptr<VideoLayer> video_hw_ = VideoLayer::Create(
        &hw_frame_provider_);
    video_hw_->SetBounds(gfx::Size(10, 10));
    video_hw_->SetAnchorPoint(gfx::PointF());
    video_hw_->SetIsDrawable(true);
    root_->AddChild(video_hw_);

    scoped_refptr<VideoLayer> video_scaled_hw_ = VideoLayer::Create(
        &scaled_hw_frame_provider_);
    video_scaled_hw_->SetBounds(gfx::Size(10, 10));
    video_scaled_hw_->SetAnchorPoint(gfx::PointF());
    video_scaled_hw_->SetIsDrawable(true);
    root_->AddChild(video_scaled_hw_);

    scoped_refptr<IOSurfaceLayer> io_surface_ = IOSurfaceLayer::Create();
    io_surface_->SetBounds(gfx::Size(10, 10));
    io_surface_->SetAnchorPoint(gfx::PointF());
    io_surface_->SetIsDrawable(true);
    io_surface_->SetIOSurfaceProperties(1, gfx::Size(10, 10));
    root_->AddChild(io_surface_);

    // Enable the hud.
    LayerTreeDebugState debug_state;
    debug_state.show_property_changed_rects = true;
    layer_tree_host()->SetDebugState(debug_state);

    bool paint_scrollbar = true;
    bool has_thumb = true;
    scoped_refptr<ScrollbarLayer> scrollbar_ = ScrollbarLayer::Create(
        FakeWebScrollbar::Create().PassAs<WebKit::WebScrollbar>(),
        FakeScrollbarThemePainter::Create(paint_scrollbar)
            .PassAs<ScrollbarThemePainter>(),
        FakeWebScrollbarThemeGeometry::Create(has_thumb)
            .PassAs<WebKit::WebScrollbarThemeGeometry>(),
        content_->id());
    scrollbar_->SetBounds(gfx::Size(10, 10));
    scrollbar_->SetAnchorPoint(gfx::PointF());
    scrollbar_->SetIsDrawable(true);
    root_->AddChild(scrollbar_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    context_should_support_io_surface_ = true;
    PostSetNeedsCommitToMainThread();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerTreeHostContextTest::CommitCompleteOnThread(host_impl);

    ResourceProvider* resource_provider = host_impl->resource_provider();

    if (host_impl->active_tree()->source_frame_number() == 0) {
      // Set up impl resources on the first commit.

      scoped_ptr<TestRenderPass> pass_for_quad = TestRenderPass::Create();
      pass_for_quad->SetNew(
          // AppendOneOfEveryQuadType() makes a RenderPass quad with this id.
          RenderPass::Id(1, 1),
          gfx::Rect(0, 0, 10, 10),
          gfx::Rect(0, 0, 10, 10),
          gfx::Transform());

      scoped_ptr<TestRenderPass> pass = TestRenderPass::Create();
      pass->SetNew(
          RenderPass::Id(2, 1),
          gfx::Rect(0, 0, 10, 10),
          gfx::Rect(0, 0, 10, 10),
          gfx::Transform());
      pass->AppendOneOfEveryQuadType(resource_provider, RenderPass::Id(2, 1));

      ScopedPtrVector<RenderPass> pass_list;
      pass_list.push_back(pass_for_quad.PassAs<RenderPass>());
      pass_list.push_back(pass.PassAs<RenderPass>());

      // First child is the delegated layer.
      FakeDelegatedRendererLayerImpl* delegated_impl =
          static_cast<FakeDelegatedRendererLayerImpl*>(
              host_impl->active_tree()->root_layer()->children()[0]);
      delegated_impl->SetFrameDataForRenderPasses(&pass_list);
      EXPECT_TRUE(pass_list.empty());

      color_video_frame_ = VideoFrame::CreateColorFrame(
          gfx::Size(4, 4), 0x80, 0x80, 0x80, base::TimeDelta());
      hw_video_frame_ = VideoFrame::WrapNativeTexture(
          resource_provider->GraphicsContext3D()->createTexture(),
          GL_TEXTURE_2D,
          gfx::Size(4, 4), gfx::Rect(0, 0, 4, 4), gfx::Size(4, 4),
          base::TimeDelta(),
          VideoFrame::ReadPixelsCB(),
          base::Closure());
      scaled_hw_video_frame_ = VideoFrame::WrapNativeTexture(
          resource_provider->GraphicsContext3D()->createTexture(),
          GL_TEXTURE_2D,
          gfx::Size(4, 4), gfx::Rect(0, 0, 3, 2), gfx::Size(4, 4),
          base::TimeDelta(),
          VideoFrame::ReadPixelsCB(),
          base::Closure());

      color_frame_provider_.set_frame(color_video_frame_);
      hw_frame_provider_.set_frame(hw_video_frame_);
      scaled_hw_frame_provider_.set_frame(scaled_hw_video_frame_);
      return;
    }

    if (host_impl->active_tree()->source_frame_number() == 3) {
      // On the third commit we're recovering from context loss. Hardware
      // video frames should not be reused by the VideoFrameProvider, but
      // software frames can be.
      hw_frame_provider_.set_frame(NULL);
      scaled_hw_frame_provider_.set_frame(NULL);
    }
  }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame,
                                     bool result) OVERRIDE {
    if (host_impl->active_tree()->source_frame_number() == 2) {
      // Lose the context during draw on the second commit. This will cause
      // a third commit to recover.
      if (context3d_)
        context3d_->set_times_bind_texture_succeeds(4);
    }
    return true;
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    ASSERT_TRUE(layer_tree_host()->hud_layer());
    // End the test once we know the 3nd frame drew.
    if (layer_tree_host()->commit_number() == 4)
      EndTest();
    else
      layer_tree_host()->SetNeedsCommit();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;

  scoped_refptr<Layer> root_;
  scoped_refptr<DelegatedRendererLayer> delegated_;
  scoped_refptr<ContentLayer> content_;
  scoped_refptr<TextureLayer> texture_;
  scoped_refptr<ContentLayer> mask_;
  scoped_refptr<ContentLayer> content_with_mask_;
  scoped_refptr<VideoLayer> video_color_;
  scoped_refptr<VideoLayer> video_hw_;
  scoped_refptr<VideoLayer> video_scaled_hw_;
  scoped_refptr<IOSurfaceLayer> io_surface_;
  scoped_refptr<ScrollbarLayer> scrollbar_;

  scoped_refptr<VideoFrame> color_video_frame_;
  scoped_refptr<VideoFrame> hw_video_frame_;
  scoped_refptr<VideoFrame> scaled_hw_video_frame_;

  FakeVideoFrameProvider color_frame_provider_;
  FakeVideoFrameProvider hw_frame_provider_;
  FakeVideoFrameProvider scaled_hw_frame_provider_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestDontUseLostResources);

class LayerTreeHostContextTestLosesFirstOutputSurface
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLosesFirstOutputSurface() {
    // Always fail. This needs to be set before LayerTreeHost is created.
    times_to_lose_on_create_ = 1000;
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void AfterTest() OVERRIDE {}

  virtual void DidRecreateOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_FALSE(succeeded);

    // If we make it this far without crashing, we pass!
    EndTest();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    EXPECT_TRUE(false);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestLosesFirstOutputSurface);

class LayerTreeHostContextTestRetriesFirstInitializationAndSucceeds
    : public LayerTreeHostContextTest {
 public:
  virtual void AfterTest() OVERRIDE {}

  virtual void BeginTest() OVERRIDE {
    times_to_fail_initialize_ = 2;
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    EndTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestRetriesFirstInitializationAndSucceeds);

class LayerTreeHostContextTestRetryWorksWithForcedInit
    : public LayerTreeHostContextTestRetriesFirstInitializationAndSucceeds {
 public:
  virtual void DidFailToInitializeOutputSurface() OVERRIDE {
    LayerTreeHostContextTestRetriesFirstInitializationAndSucceeds
        ::DidFailToInitializeOutputSurface();

    if (times_create_failed_ == 1) {
      // CompositeAndReadback force recreates the output surface, which should
      // fail.
      char pixels[4];
      EXPECT_FALSE(layer_tree_host()->CompositeAndReadback(
            &pixels, gfx::Rect(1, 1)));
    }
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestRetryWorksWithForcedInit);

class LayerTreeHostContextTestCompositeAndReadbackBeforeOutputSurfaceInit
    : public LayerTreeHostContextTest {
 public:
  virtual void BeginTest() OVERRIDE {
    // This must be called immediately after creating LTH, before the first
    // OutputSurface is initialized.
    ASSERT_TRUE(layer_tree_host()->output_surface_lost());

    times_output_surface_created_ = 0;

    char pixels[4];
    EXPECT_TRUE(layer_tree_host()->CompositeAndReadback(
          &pixels, gfx::Rect(1, 1)));
    EXPECT_EQ(1, times_output_surface_created_);

    PostSetNeedsCommitToMainThread();
  }

  virtual void DidRecreateOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);
    ++times_output_surface_created_;
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {
    // Should not try to create output surface again after successfully
    // created by CompositeAndReadback.
    EXPECT_EQ(1, times_output_surface_created_);
  }

 private:
  int times_output_surface_created_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestCompositeAndReadbackBeforeOutputSurfaceInit);

class ImplSidePaintingLayerTreeHostContextTest
    : public LayerTreeHostContextTest {
 public:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->impl_side_painting = true;
  }
};

class LayerTreeHostContextTestImplSidePainting
    : public ImplSidePaintingLayerTreeHostContextTest {
 public:
  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetAnchorPoint(gfx::PointF());
    root->SetIsDrawable(true);

    scoped_refptr<PictureLayer> picture = PictureLayer::Create(&client_);
    picture->SetBounds(gfx::Size(10, 10));
    picture->SetAnchorPoint(gfx::PointF());
    picture->SetIsDrawable(true);
    root->AddChild(picture);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    times_to_lose_during_commit_ = 1;
    PostSetNeedsCommitToMainThread();
  }

  virtual void AfterTest() OVERRIDE {}

  virtual void DidRecreateOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);
    EndTest();
  }

 private:
  FakeContentLayerClient client_;
};

MULTI_THREAD_TEST_F(LayerTreeHostContextTestImplSidePainting);

class ScrollbarLayerLostContext : public LayerTreeHostContextTest {
 public:
  ScrollbarLayerLostContext() : commits_(0) {}

  virtual void BeginTest() OVERRIDE {
    scoped_refptr<Layer> scroll_layer = Layer::Create();
    scrollbar_layer_ = FakeScrollbarLayer::Create(
        false, true, scroll_layer->id());
    scrollbar_layer_->SetBounds(gfx::Size(10, 100));
    layer_tree_host()->root_layer()->AddChild(scrollbar_layer_);
    layer_tree_host()->root_layer()->AddChild(scroll_layer);
    PostSetNeedsCommitToMainThread();
  }

  virtual void AfterTest() OVERRIDE {}

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);

    ++commits_;
    size_t upload_count = scrollbar_layer_->last_update_full_upload_size() +
        scrollbar_layer_->last_update_partial_upload_size();
    switch (commits_) {
      case 1:
        // First (regular) update, we should upload 2 resources (thumb, and
        // backtrack).
        EXPECT_EQ(1, scrollbar_layer_->update_count());
        EXPECT_EQ(2u, upload_count);
        LoseContext();
        break;
      case 2:
        // Second update, after the lost context, we should still upload 2
        // resources even if the contents haven't changed.
        EXPECT_EQ(2, scrollbar_layer_->update_count());
        EXPECT_EQ(2u, upload_count);
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  int commits_;
  scoped_refptr<FakeScrollbarLayer> scrollbar_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(ScrollbarLayerLostContext);

class LayerTreeHostContextTestFailsToCreateSurface
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestFailsToCreateSurface()
      : LayerTreeHostContextTest(),
        failure_count_(0) {
    times_to_lose_on_create_ = 10;
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void AfterTest() OVERRIDE {}

  virtual void DidRecreateOutputSurface(bool success) OVERRIDE {
    EXPECT_FALSE(success);
    EXPECT_EQ(0, failure_count_);
    times_to_lose_on_create_ = 0;
    failure_count_++;
    // Normally, the embedder should stop trying to use the compositor at
    // this point, but let's force it back into action when we shouldn't.
    char pixels[4];
    EXPECT_FALSE(
        layer_tree_host()->CompositeAndReadback(pixels, gfx::Rect(1, 1)));
    // If we've made it this far without crashing, we've succeeded.
    EndTest();
  }

 private:
  int failure_count_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestFailsToCreateSurface);

// Not reusing LayerTreeTest because it expects creating LTH to always succeed.
class LayerTreeHostTestCannotCreateIfCannotCreateOutputSurface
    : public testing::Test,
      public FakeLayerTreeHostClient {
 public:
  LayerTreeHostTestCannotCreateIfCannotCreateOutputSurface()
      : FakeLayerTreeHostClient(FakeLayerTreeHostClient::DIRECT_3D) {}

  // FakeLayerTreeHostClient
  virtual scoped_ptr<OutputSurface> CreateOutputSurface() OVERRIDE {
    return scoped_ptr<OutputSurface>();
  }

  void RunTest(bool threaded) {
    scoped_ptr<base::Thread> impl_thread;
    scoped_ptr<cc::Thread> impl_ccthread(NULL);
    if (threaded) {
      impl_thread.reset(new base::Thread("LayerTreeTest"));
      impl_ccthread = cc::ThreadImpl::CreateForDifferentThread(
          impl_thread->message_loop_proxy());
      ASSERT_TRUE(impl_ccthread);
    }

    LayerTreeSettings settings;
    scoped_ptr<LayerTreeHost> layer_tree_host =
        LayerTreeHost::Create(this, settings, impl_ccthread.Pass());
    EXPECT_FALSE(layer_tree_host);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostTestCannotCreateIfCannotCreateOutputSurface);

}  // namespace
}  // namespace cc
