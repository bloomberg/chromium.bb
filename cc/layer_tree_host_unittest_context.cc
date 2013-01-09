// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host.h"

#include "base/basictypes.h"
#include "cc/content_layer.h"
#include "cc/delegated_renderer_layer.h"
#include "cc/delegated_renderer_layer_impl.h"
#include "cc/heads_up_display_layer.h"
#include "cc/io_surface_layer.h"
#include "cc/layer_impl.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/layer_tree_impl.h"
#include "cc/scrollbar_layer.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_content_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_content_layer_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_scrollbar_theme_painter.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/fake_web_graphics_context_3d.h"
#include "cc/test/fake_web_scrollbar.h"
#include "cc/test/fake_web_scrollbar_theme_geometry.h"
#include "cc/test/layer_tree_test_common.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/texture_layer.h"
#include "cc/video_layer.h"
#include "cc/video_layer_impl.h"
#include "media/base/media.h"

using media::VideoFrame;
using WebKit::WebGraphicsContext3D;

namespace cc {
namespace {

// These tests deal with losing the 3d graphics context.
class LayerTreeHostContextTest : public ThreadedTest {
 public:
  LayerTreeHostContextTest()
      : ThreadedTest(),
        context3d_(NULL),
        times_to_fail_create_(0),
        times_to_create_and_lose_(0),
        times_to_lose_during_commit_(0),
        times_to_repeat_loss_(0),
        times_to_fail_recreate_(0) {
    media::InitializeMediaLibraryForTesting();
  }

  void LoseContext() {
    context3d_->loseContextCHROMIUM();
    context3d_ = NULL;
  }

  virtual scoped_ptr<FakeWebGraphicsContext3D> CreateContext3d() {
    return FakeWebGraphicsContext3D::Create();
  }

  virtual scoped_ptr<OutputSurface> createOutputSurface() OVERRIDE {
    if (times_to_fail_create_) {
      --times_to_fail_create_;
      return scoped_ptr<OutputSurface>();
    }

    scoped_ptr<FakeWebGraphicsContext3D> context3d = CreateContext3d();
    context3d_ = context3d.get();

    if (times_to_create_and_lose_) {
      --times_to_create_and_lose_;
      // Make the context get lost during reinitialization.
      // The number of times MakeCurrent succeeds is not important, and
      // can be changed if needed to make this pass with future changes.
      context3d_->set_times_make_current_succeeds(2);
    }

    return FakeOutputSurface::Create3d(
        context3d.PassAs<WebGraphicsContext3D>()).PassAs<OutputSurface>();
  }

  virtual void commitCompleteOnThread(LayerTreeHostImpl *host_impl) OVERRIDE {
    if (!times_to_lose_during_commit_)
      return;
    --times_to_lose_during_commit_;
    LoseContext();

    times_to_create_and_lose_ = times_to_repeat_loss_;
    times_to_repeat_loss_ = 0;
    times_to_fail_create_ = times_to_fail_recreate_;
    times_to_fail_recreate_ = 0;
  }

 protected:
  FakeWebGraphicsContext3D* context3d_;
  int times_to_fail_create_;
  int times_to_create_and_lose_;
  int times_to_lose_during_commit_;
  int times_to_repeat_loss_;
  int times_to_fail_recreate_;
};

class LayerTreeHostContextTestLostContextSucceeds :
      public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextSucceeds()
      : LayerTreeHostContextTest(),
        test_case_(0),
        num_losses_(0) {
  }

  virtual void beginTest() OVERRIDE {
    postSetNeedsCommitToMainThread();
  }

  virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);
    ++num_losses_;
  }

  virtual void afterTest() OVERRIDE {
    EXPECT_EQ(3, test_case_);
    EXPECT_EQ(3, num_losses_);
  }

  bool SourceFrameHasContextLoss(int source_frame) const {
    return source_frame % 2 == 1;
  }

  virtual void didCommitAndDrawFrame() OVERRIDE {
    // If the last frame had a context loss, then we'll commit again to
    // recover.
    if (SourceFrameHasContextLoss(m_layerTreeHost->commitNumber()) - 1)
      return;

    if (NextTestCase())
      m_layerTreeHost->setNeedsCommit();
    else
      endTest();
  }

  bool NextTestCase() {
    static const TestCase kTests[] = {
      // Losing the context and failing to recreate it (or losing it again
      // immediately) a small number of times should succeed.
      { 1, // times_to_lose_during_commit
        0, // times_to_repeat_loss
        0, // times_to_fail_recreate
      },
      { 1,
        3, // times_to_repeat_loss
        0, // times_to_fail_recreate
      },
      { 1,
        0, // times_to_repeat_loss
        3, // times_to_fail_recreate
      },
    };

    if (test_case_ >= arraysize(kTests))
      return false;

    times_to_lose_during_commit_ =
        kTests[test_case_].times_to_lose_during_commit;
    times_to_repeat_loss_ = kTests[test_case_].times_to_repeat_loss;
    times_to_fail_recreate_ = kTests[test_case_].times_to_fail_recreate;
    ++test_case_;
    return true;
  }

  struct TestCase {
    int times_to_lose_during_commit;
    int times_to_repeat_loss;
    int times_to_fail_recreate;
  };

 private:
  size_t test_case_;
  int num_losses_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestLostContextSucceeds)

class LayerTreeHostContextTestLostContextSucceedsWithContent :
    public LayerTreeHostContextTestLostContextSucceeds {
 public:

  LayerTreeHostContextTestLostContextSucceedsWithContent()
      : LayerTreeHostContextTestLostContextSucceeds() {
  }

  virtual void setupTree() OVERRIDE {
    scoped_refptr<Layer> root_ = Layer::create();
    root_->setBounds(gfx::Size(10, 10));
    root_->setAnchorPoint(gfx::PointF());
    root_->setIsDrawable(true);

    scoped_refptr<FakeContentLayer> content_ =
        FakeContentLayer::Create(&client_);
    content_->setBounds(gfx::Size(10, 10));
    content_->setAnchorPoint(gfx::PointF());
    content_->setIsDrawable(true);
    if (use_surface_)
      content_->setForceRenderSurface(true);
    root_->addChild(content_);

    m_layerTreeHost->setRootLayer(root_);
    LayerTreeHostContextTest::setupTree();
  }

  virtual void drawLayersOnThread(LayerTreeHostImpl* host_impl) {
    FakeContentLayerImpl* content_impl = static_cast<FakeContentLayerImpl*>(
        host_impl->rootLayer()->children()[0]);
    // Even though the context was lost, we should have a resource. The
    // FakeWebGraphicsContext3D ensures that this resource is created with
    // the active context.
    EXPECT_TRUE(content_impl->HaveResourceForTileAt(0, 0));
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
  runTest(false);
}

TEST_F(LayerTreeHostContextTestLostContextSucceedsWithContent,
       NoSurface_MultiThread) {
  use_surface_ = false;
  runTest(true);
}

TEST_F(LayerTreeHostContextTestLostContextSucceedsWithContent,
       WithSurface_SingleThread) {
  use_surface_ = true;
  runTest(false);
}

TEST_F(LayerTreeHostContextTestLostContextSucceedsWithContent,
       WithSurface_MultiThread) {
  use_surface_ = true;
  runTest(true);
}

class LayerTreeHostContextTestLostContextFails :
    public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextFails()
      : LayerTreeHostContextTest(),
        num_commits_(0) {
    times_to_lose_during_commit_ = 1;
  }

  virtual void beginTest() OVERRIDE {
    postSetNeedsCommitToMainThread();
  }

  virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_FALSE(succeeded);
    endTest();
  }

  virtual void commitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerTreeHostContextTest::commitCompleteOnThread(host_impl);

    ++num_commits_;
    if (num_commits_ == 1) {
      // When the context is ok, we should have these things.
      EXPECT_TRUE(host_impl->outputSurface());
      EXPECT_TRUE(host_impl->renderer());
      EXPECT_TRUE(host_impl->resourceProvider());
      return;
    }

    // When context recreation fails we shouldn't be left with any of them.
    EXPECT_FALSE(host_impl->outputSurface());
    EXPECT_FALSE(host_impl->renderer());
    EXPECT_FALSE(host_impl->resourceProvider());
  }

  virtual void afterTest() OVERRIDE {}

 private:
  int num_commits_;
};

TEST_F(LayerTreeHostContextTestLostContextFails, RepeatLoss100_SingleThread) {
  times_to_repeat_loss_ = 100;
  times_to_fail_recreate_ = 0;
  runTest(false);
}

TEST_F(LayerTreeHostContextTestLostContextFails, RepeatLoss100_MultiThread) {
  times_to_repeat_loss_ = 100;
  times_to_fail_recreate_ = 0;
  runTest(true);
}

TEST_F(LayerTreeHostContextTestLostContextFails, FailRecreate100_SingleThread) {
  times_to_repeat_loss_ = 0;
  times_to_fail_recreate_ = 100;
  runTest(false);
}

TEST_F(LayerTreeHostContextTestLostContextFails, FailRecreate100_MultiThread) {
  times_to_repeat_loss_ = 0;
  times_to_fail_recreate_ = 100;
  runTest(true);
}

class LayerTreeHostContextTestFinishAllRenderingAfterLoss :
      public LayerTreeHostContextTest {
 public:
  virtual void beginTest() OVERRIDE {
    // Lose the context until the compositor gives up on it.
    times_to_lose_during_commit_ = 1;
    times_to_repeat_loss_ = 10;
    postSetNeedsCommitToMainThread();
  }

  virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_FALSE(succeeded);
    m_layerTreeHost->finishAllRendering();
    endTest();
  }

  virtual void afterTest() OVERRIDE {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestFinishAllRenderingAfterLoss)

class LayerTreeHostContextTestLostContextAndEvictTextures :
    public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextAndEvictTextures()
      : LayerTreeHostContextTest(),
        layer_(FakeContentLayer::Create(&client_)),
        impl_host_(0),
        num_commits_(0) {
  }

  virtual void setupTree() OVERRIDE {
    layer_->setBounds(gfx::Size(10, 20));
    m_layerTreeHost->setRootLayer(layer_);
    LayerTreeHostContextTest::setupTree();
  }

  virtual void beginTest() OVERRIDE {
    postSetNeedsCommitToMainThread();
  }

  void PostEvictTextures() {
    if (implThread()) {
      implThread()->postTask(
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
    impl_host_->enforceManagedMemoryPolicy(ManagedMemoryPolicy(0));
    if (lose_after_evict_)
      LoseContext();
  }

  virtual void didCommitAndDrawFrame() OVERRIDE {
    if (num_commits_ > 1)
      return;
    EXPECT_TRUE(layer_->HaveBackingAt(0, 0));
    PostEvictTextures();
  }

  virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (num_commits_ > 1)
      return;
    ++num_commits_;
    if (!lose_after_evict_)
      LoseContext();
    impl_host_ = impl;
  }

  virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);
    endTest();
  }

  virtual void afterTest() OVERRIDE {}

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
  runTest(false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseAfterEvict_MultiThread) {
  lose_after_evict_ = true;
  runTest(true);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_SingleThread) {
  lose_after_evict_ = false;
  runTest(false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_MultiThread) {
  lose_after_evict_ = false;
  runTest(true);
}

class LayerTreeHostContextTestLostContextWhileUpdatingResources :
    public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextWhileUpdatingResources()
      : parent_(FakeContentLayer::Create(&client_)),
        num_children_(50),
        times_to_lose_on_end_query_(3) {
  }

  virtual scoped_ptr<FakeWebGraphicsContext3D> CreateContext3d() {
    scoped_ptr<FakeWebGraphicsContext3D> context =
        LayerTreeHostContextTest::CreateContext3d();
    if (times_to_lose_on_end_query_) {
      --times_to_lose_on_end_query_;
      context->set_times_end_query_succeeds(5);
    }
    return context.Pass();
  }

  virtual void setupTree() {
    parent_->setBounds(gfx::Size(num_children_, 1));

    for (int i = 0; i < num_children_; i++) {
      scoped_refptr<FakeContentLayer> child =
          FakeContentLayer::Create(&client_);
      child->setPosition(gfx::PointF(i, 0.f));
      child->setBounds(gfx::Size(1, 1));
      parent_->addChild(child);
    }

    m_layerTreeHost->setRootLayer(parent_);
    LayerTreeHostContextTest::setupTree();
  }

  virtual void beginTest() {
    postSetNeedsCommitToMainThread();
  }

  virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) {
    endTest();
  }

  virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);
  }

  virtual void afterTest() {
    EXPECT_EQ(0, times_to_lose_on_end_query_);
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> parent_;
  int num_children_;
  int times_to_lose_on_end_query_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestLostContextWhileUpdatingResources)

class LayerTreeHostContextTestLayersNotified :
    public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLayersNotified()
      : LayerTreeHostContextTest(),
        num_commits_(0) {
  }

  virtual void setupTree() OVERRIDE {
    root_ = FakeContentLayer::Create(&client_);
    child_ = FakeContentLayer::Create(&client_);
    grandchild_ = FakeContentLayer::Create(&client_);

    root_->addChild(child_);
    child_->addChild(grandchild_);

    m_layerTreeHost->setRootLayer(root_);
    LayerTreeHostContextTest::setupTree();
  }

  virtual void beginTest() OVERRIDE {
    postSetNeedsCommitToMainThread();
  }

  virtual void commitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    FakeContentLayerImpl* root = static_cast<FakeContentLayerImpl*>(
        host_impl->rootLayer());
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
        times_to_create_and_lose_ = 1;
        break;
      case 3:
        EXPECT_EQ(3u, root->lost_output_surface_count());
        EXPECT_EQ(3u, child->lost_output_surface_count());
        EXPECT_EQ(3u, grandchild->lost_output_surface_count());
        endTest();
        break;
      default:
        NOTREACHED();
    }
  }

  virtual void afterTest() OVERRIDE {}

 private:
  int num_commits_;

  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_;
  scoped_refptr<FakeContentLayer> child_;
  scoped_refptr<FakeContentLayer> grandchild_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestLayersNotified)

class LayerTreeHostContextTestDontUseLostResources :
    public LayerTreeHostContextTest {
 public:
  virtual void setupTree() OVERRIDE {
    context3d_->set_have_extension_io_surface(true);
    context3d_->set_have_extension_egl_image(true);

    scoped_refptr<Layer> root_ = Layer::create();
    root_->setBounds(gfx::Size(10, 10));
    root_->setAnchorPoint(gfx::PointF());
    root_->setIsDrawable(true);

    scoped_refptr<DelegatedRendererLayer> delegated_ =
        DelegatedRendererLayer::create();
    delegated_->setBounds(gfx::Size(10, 10));
    delegated_->setAnchorPoint(gfx::PointF());
    delegated_->setIsDrawable(true);
    root_->addChild(delegated_);

    scoped_refptr<ContentLayer> content_ = ContentLayer::create(&client_);
    content_->setBounds(gfx::Size(10, 10));
    content_->setAnchorPoint(gfx::PointF());
    content_->setIsDrawable(true);
    root_->addChild(content_);

    scoped_refptr<TextureLayer> texture_ = TextureLayer::create(NULL);
    texture_->setBounds(gfx::Size(10, 10));
    texture_->setAnchorPoint(gfx::PointF());
    texture_->setTextureId(FakeWebGraphicsContext3D::kExternalTextureId);
    texture_->setIsDrawable(true);
    root_->addChild(texture_);

    scoped_refptr<ContentLayer> mask_ = ContentLayer::create(&client_);
    mask_->setBounds(gfx::Size(10, 10));
    mask_->setAnchorPoint(gfx::PointF());

    scoped_refptr<ContentLayer> content_with_mask_ =
        ContentLayer::create(&client_);
    content_with_mask_->setBounds(gfx::Size(10, 10));
    content_with_mask_->setAnchorPoint(gfx::PointF());
    content_with_mask_->setIsDrawable(true);
    content_with_mask_->setMaskLayer(mask_.get());
    root_->addChild(content_with_mask_);

    scoped_refptr<VideoLayer> video_color_ = VideoLayer::create(
        &color_frame_provider_);
    video_color_->setBounds(gfx::Size(10, 10));
    video_color_->setAnchorPoint(gfx::PointF());
    video_color_->setIsDrawable(true);
    root_->addChild(video_color_);

    scoped_refptr<VideoLayer> video_hw_ = VideoLayer::create(
        &hw_frame_provider_);
    video_hw_->setBounds(gfx::Size(10, 10));
    video_hw_->setAnchorPoint(gfx::PointF());
    video_hw_->setIsDrawable(true);
    root_->addChild(video_hw_);

    scoped_refptr<VideoLayer> video_scaled_hw_ = VideoLayer::create(
        &scaled_hw_frame_provider_);
    video_scaled_hw_->setBounds(gfx::Size(10, 10));
    video_scaled_hw_->setAnchorPoint(gfx::PointF());
    video_scaled_hw_->setIsDrawable(true);
    root_->addChild(video_scaled_hw_);

    scoped_refptr<IOSurfaceLayer> io_surface_ = IOSurfaceLayer::create();
    io_surface_->setBounds(gfx::Size(10, 10));
    io_surface_->setAnchorPoint(gfx::PointF());
    io_surface_->setIsDrawable(true);
    io_surface_->setIOSurfaceProperties(1, gfx::Size(10, 10));
    root_->addChild(io_surface_);

    scoped_refptr<HeadsUpDisplayLayer> hud_ = HeadsUpDisplayLayer::create();
    hud_->setBounds(gfx::Size(10, 10));
    hud_->setAnchorPoint(gfx::PointF());
    hud_->setIsDrawable(true);
    root_->addChild(hud_);
    // Enable the hud.
    LayerTreeDebugState debug_state;
    debug_state.showFPSCounter = true;
    m_layerTreeHost->setDebugState(debug_state);

    bool paint_scrollbar = true;
    bool has_thumb = true;
    scoped_refptr<ScrollbarLayer> scrollbar_ = ScrollbarLayer::create(
        FakeWebScrollbar::create().PassAs<WebKit::WebScrollbar>(),
        FakeScrollbarThemePainter::Create(paint_scrollbar)
        .PassAs<ScrollbarThemePainter>(),
        FakeWebScrollbarThemeGeometry::create(has_thumb)
        .PassAs<WebKit::WebScrollbarThemeGeometry>(),
        content_->id());
    scrollbar_->setBounds(gfx::Size(10, 10));
    scrollbar_->setAnchorPoint(gfx::PointF());
    scrollbar_->setIsDrawable(true);
    root_->addChild(scrollbar_);

    m_layerTreeHost->setRootLayer(root_);
    LayerTreeHostContextTest::setupTree();
  }

  virtual void beginTest() OVERRIDE {
    postSetNeedsCommitToMainThread();
  }

  virtual void commitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    ResourceProvider* resource_provider = host_impl->resourceProvider();

    if (host_impl->activeTree()->source_frame_number() == 0) {
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
      pass->AppendOneOfEveryQuadType(resource_provider);

      ScopedPtrVector<RenderPass> pass_list;
      pass_list.append(pass_for_quad.PassAs<RenderPass>());
      pass_list.append(pass.PassAs<RenderPass>());

      // First child is the delegated layer.
      DelegatedRendererLayerImpl* delegated_impl =
          static_cast<DelegatedRendererLayerImpl*>(
              host_impl->rootLayer()->children()[0]);
      delegated_impl->setRenderPasses(pass_list);
      EXPECT_TRUE(pass_list.isEmpty());

      color_video_frame_ = VideoFrame::CreateColorFrame(
          gfx::Size(4, 4), 0x80, 0x80, 0x80, base::TimeDelta());
      hw_video_frame_ = VideoFrame::WrapNativeTexture(
          resource_provider->graphicsContext3D()->createTexture(),
          GL_TEXTURE_2D,
          gfx::Size(4, 4), gfx::Rect(0, 0, 4, 4), gfx::Size(4, 4),
          base::TimeDelta(),
          VideoFrame::ReadPixelsCB(),
          base::Closure());
      scaled_hw_video_frame_ = VideoFrame::WrapNativeTexture(
          resource_provider->graphicsContext3D()->createTexture(),
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

    if (host_impl->activeTree()->source_frame_number() == 3) {
      // On the third commit we're recovering from context loss. Hardware
      // video frames should not be reused by the VideoFrameProvider, but
      // software frames can be.
      hw_frame_provider_.set_frame(NULL);
      scaled_hw_frame_provider_.set_frame(NULL);
    }
  }

  virtual bool prepareToDrawOnThread(LayerTreeHostImpl* host_impl) {
    if (host_impl->activeTree()->source_frame_number() == 2) {
      // Lose the context during draw on the second commit. This will cause
      // a third commit to recover.
      if (context3d_)
        context3d_->set_times_bind_texture_succeeds(4);
    }
    return true;
  }

  virtual void didCommitAndDrawFrame() OVERRIDE {
    // End the test once we know the 3nd frame drew.
    if (m_layerTreeHost->commitNumber() == 4)
      endTest();
  }

  virtual void afterTest() OVERRIDE {}

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
  scoped_refptr<HeadsUpDisplayLayer> hud_;
  scoped_refptr<ScrollbarLayer> scrollbar_;

  scoped_refptr<VideoFrame> color_video_frame_;
  scoped_refptr<VideoFrame> hw_video_frame_;
  scoped_refptr<VideoFrame> scaled_hw_video_frame_;

  FakeVideoFrameProvider color_frame_provider_;
  FakeVideoFrameProvider hw_frame_provider_;
  FakeVideoFrameProvider scaled_hw_frame_provider_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestDontUseLostResources)

class LayerTreeHostContextTestFailsImmediately :
    public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestFailsImmediately()
      : LayerTreeHostContextTest() {
  }

  virtual ~LayerTreeHostContextTestFailsImmediately() {}

  virtual void beginTest() OVERRIDE {
    postSetNeedsCommitToMainThread();
  }

  virtual void afterTest() OVERRIDE {
  }

  virtual scoped_ptr<FakeWebGraphicsContext3D> CreateContext3d() OVERRIDE {
    scoped_ptr<FakeWebGraphicsContext3D> context =
        LayerTreeHostContextTest::CreateContext3d();
    context->loseContextCHROMIUM();
    return context.Pass();
  }

  virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_FALSE(succeeded);
    // If we make it this far without crashing, we pass!
    endTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestFailsImmediately);

}  // namespace
}  // namespace cc
