// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/gl_renderer.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "cc/compositor_frame_metadata.h"
#include "cc/draw_quad.h"
#include "cc/prioritized_resource_manager.h"
#include "cc/resource_provider.h"
#include "cc/test/paths.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/pixel_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gl/gl_implementation.h"

namespace cc {
namespace {

class FakeRendererClient : public RendererClient {
 public:
  FakeRendererClient()
  {
  }

  // RendererClient methods.
  virtual const gfx::Size& deviceViewportSize() const OVERRIDE {
    static gfx::Size fake_size(200, 200);
    return fake_size;
  }
  virtual const LayerTreeSettings& settings() const OVERRIDE {
    static LayerTreeSettings fake_settings;
    return fake_settings;
  }
  virtual void didLoseOutputSurface() OVERRIDE { }
  virtual void onSwapBuffersComplete() OVERRIDE { }
  virtual void setFullRootLayerDamage() OVERRIDE { }
  virtual void setManagedMemoryPolicy(const ManagedMemoryPolicy&) OVERRIDE {}
  virtual void enforceManagedMemoryPolicy(
      const ManagedMemoryPolicy&) OVERRIDE {}
  virtual bool hasImplThread() const OVERRIDE { return false; }
  virtual bool shouldClearRootRenderPass() const { return true; }
  virtual CompositorFrameMetadata makeCompositorFrameMetadata() const
      OVERRIDE { return CompositorFrameMetadata(); }
};

class GLRendererPixelTest : public testing::Test {
 protected:
  GLRendererPixelTest() {}

  virtual void SetUp() {
    gfx::InitializeGLBindings(gfx::kGLImplementationOSMesaGL);
    output_surface_ = PixelTestOutputSurface::create();
    resource_provider_ = ResourceProvider::create(output_surface_.get());
    renderer_ = GLRenderer::create(&fake_client_,
                                   output_surface_.get(),
                                   resource_provider_.get());
  }

  bool PixelsMatchReference(FilePath ref_file, gfx::Rect viewport_rect) {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                     viewport_rect.width(), viewport_rect.height());
    bitmap.allocPixels();
    unsigned char* pixels = static_cast<unsigned char*>(bitmap.getPixels());
    renderer_->getFramebufferPixels(pixels, viewport_rect);

    FilePath test_data_dir;
    if (!PathService::Get(cc::DIR_TEST_DATA, &test_data_dir))
      return false;

    return IsSameAsPNGFile(bitmap, test_data_dir.Append(ref_file));
  }

  scoped_ptr<OutputSurface> output_surface_;
  FakeRendererClient fake_client_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<GLRenderer> renderer_;
};

scoped_ptr<RenderPass> CreateTestRenderPass(RenderPass::Id id, gfx::Rect rect) {
  scoped_ptr<RenderPass> pass = RenderPass::Create();
  const gfx::Rect output_rect = rect;
  const gfx::RectF damage_rect = rect;
  const gfx::Transform transform_to_root_target;
  pass->SetNew(id, output_rect, damage_rect, transform_to_root_target);
  return pass.Pass();
}

scoped_ptr<SharedQuadState> CreateTestSharedQuadState(
    gfx::Transform content_to_target_transform, gfx::Rect rect) {
  const gfx::Rect visible_content_rect = rect;
  const gfx::Rect clip_rect = rect;
  const bool is_clipped = false;
  const float opacity = 1.0f;
  scoped_ptr<SharedQuadState> shared_state = SharedQuadState::Create();
  shared_state->SetAll(content_to_target_transform,
                       visible_content_rect,
                       clip_rect,
                       is_clipped,
                       opacity);
  return shared_state.Pass();
}

scoped_ptr<DrawQuad> CreateTestRenderPassDrawQuad(
    SharedQuadState* shared_state, gfx::Rect rect, RenderPass::Id pass_id) {
  scoped_ptr<RenderPassDrawQuad> quad = RenderPassDrawQuad::Create();
  quad->SetNew(shared_state,
               rect,
               pass_id,
               false,         // is_replica
               0,             // mask_resource_id
               rect,          // contents_changed_since_last_frame
               gfx::RectF(),  // mask_uv_rect
               WebKit::WebFilterOperations(),   // foreground filters
               skia::RefPtr<SkImageFilter>(),   // foreground filter
               WebKit::WebFilterOperations());  // background filters

  return quad.PassAs<DrawQuad>();
}


#if !defined(OS_ANDROID)
TEST_F(GLRendererPixelTest, simpleGreenRect) {
  gfx::Rect rect(0, 0, 200, 200);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRenderPass(id, rect);

  gfx::Transform content_to_target_transform;
  scoped_ptr<SharedQuadState> shared_state =
      CreateTestSharedQuadState(content_to_target_transform, rect);

  scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
  color_quad->SetNew(shared_state.get(), rect, SK_ColorGREEN);

  pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  renderer_->drawFrame(pass_list);

  EXPECT_TRUE(PixelsMatchReference(FilePath(FILE_PATH_LITERAL("green.png")),
                                   rect));
}

TEST_F(GLRendererPixelTest, RenderPassChangesSize) {
  gfx::Rect viewport_rect(200, 200);

  RenderPass::Id root_pass_id(1, 1);
  scoped_ptr<RenderPass> root_pass =
      CreateTestRenderPass(root_pass_id, viewport_rect);

  RenderPass::Id child_pass_id(2, 2);
  gfx::Rect pass_rect(200, 200);
  scoped_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect);

  gfx::Transform content_to_target_transform;
  scoped_ptr<SharedQuadState> shared_state =
      CreateTestSharedQuadState(content_to_target_transform, viewport_rect);

  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(shared_state.get(), gfx::Rect(0, 0, 100, 200), SK_ColorBLUE);
  scoped_ptr<SolidColorDrawQuad> yellow = SolidColorDrawQuad::Create();
  yellow->SetNew(shared_state.get(), gfx::Rect(100, 0, 100, 200), SK_ColorYELLOW);

  child_pass->quad_list.push_back(blue.PassAs<DrawQuad>());
  child_pass->quad_list.push_back(yellow.PassAs<DrawQuad>());

  scoped_ptr<SharedQuadState> pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect);
  root_pass->quad_list.push_back(
      CreateTestRenderPassDrawQuad(pass_shared_state.get(),
                                   pass_rect,
                                   child_pass_id));

  RenderPassList pass_list;
  pass_list.push_back(child_pass.Pass());
  pass_list.push_back(root_pass.Pass());

  renderer_->setEnlargePassTextureAmountForTesting(gfx::Vector2d(50, 75));
  renderer_->decideRenderPassAllocationsForFrame(pass_list);
  renderer_->drawFrame(pass_list);

  EXPECT_TRUE(PixelsMatchReference(
      FilePath(FILE_PATH_LITERAL("blue_yellow.png")), viewport_rect));
}
#endif

}  // namespace
}  // namespace cc
