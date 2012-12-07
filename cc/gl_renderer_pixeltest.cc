// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/gl_renderer.h"

#include "base/file_util.h"
#include "base/path_service.h"
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
};


class GLRendererPixelTest : public testing::Test {
 protected:
  GLRendererPixelTest() {}

  virtual void SetUp() {
    gfx::InitializeGLBindings(gfx::kGLImplementationOSMesaGL);
    output_surface_ = PixelTestOutputSurface::create();
    resource_provider_ = ResourceProvider::create(output_surface_.get());
    renderer_ = GLRenderer::create(&fake_client_, resource_provider_.get());
  }

  scoped_ptr<OutputSurface> output_surface_;
  FakeRendererClient fake_client_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<GLRenderer> renderer_;
};

#if !defined(OS_ANDROID)
TEST_F(GLRendererPixelTest, simpleGreenRect) {
  gfx::Rect rect(0, 0, 200, 200);

  RenderPass::Id id(1, 1);
  const gfx::Rect output_rect = rect;
  const gfx::RectF damage_rect = rect;
  const gfx::Transform transform_to_root_target;
  scoped_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetNew(id, output_rect, damage_rect, transform_to_root_target);

  const gfx::Transform content_to_target_transform;
  const gfx::Rect visible_content_rect = rect;
  const gfx::Rect clipped_rect_in_target = rect;
  const gfx::Rect clip_rect = rect;
  const bool is_clipped = false;
  const float opacity = 1.0f;
  scoped_ptr<SharedQuadState> shared_state = SharedQuadState::Create();
  shared_state->SetAll(content_to_target_transform,
                       visible_content_rect,
                       clipped_rect_in_target,
                       clip_rect,
                       is_clipped,
                       opacity);

  scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
  color_quad->SetNew(shared_state.get(), rect, SK_ColorGREEN);

  pass->quad_list.append(color_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.get());
  RenderPassIdHashMap pass_map;
  pass_map.add(id, pass.PassAs<RenderPass>());

  renderer_->drawFrame(pass_list, pass_map);

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 200, 200);
  bitmap.allocPixels();
  unsigned char* pixels = static_cast<unsigned char*>(bitmap.getPixels());
  renderer_->getFramebufferPixels(pixels, gfx::Rect(0, 0, 200, 200));

  FilePath test_data_dir;
  ASSERT_TRUE(PathService::Get(cc::test::DIR_TEST_DATA, &test_data_dir));
  // test::WritePNGFile(bitmap, test_data_dir.AppendASCII("green.png"));
  EXPECT_TRUE(test::IsSameAsPNGFile(bitmap,
                                    test_data_dir.AppendASCII("green.png")));
}
#endif

}  // namespace
}  // namespace cc
