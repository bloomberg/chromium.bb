// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/software_renderer.h"

#include "cc/layers/quad_sink.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/software_output_device.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/render_pass_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace cc {
namespace {

class SoftwareRendererTest : public testing::Test, public RendererClient {
 public:
  SoftwareRendererTest() : should_clear_root_render_pass_(true) {}

  void InitializeRenderer(
      scoped_ptr<SoftwareOutputDevice> software_output_device) {
    output_surface_ = FakeOutputSurface::CreateSoftware(
        software_output_device.Pass());
    resource_provider_ = ResourceProvider::Create(output_surface_.get(), 0);
    renderer_ = SoftwareRenderer::Create(
        this, output_surface_.get(), resource_provider());
  }

  ResourceProvider* resource_provider() const {
    return resource_provider_.get();
  }

  SoftwareRenderer* renderer() const { return renderer_.get(); }

  void set_viewport(gfx::Rect viewport) {
    viewport_ = viewport;
  }

  void set_should_clear_root_render_pass(bool clear_root_render_pass) {
    should_clear_root_render_pass_ = clear_root_render_pass;
  }

  // RendererClient implementation.
  virtual gfx::Rect DeviceViewport() const OVERRIDE {
    return viewport_;
  }
  virtual float DeviceScaleFactor() const OVERRIDE {
    return 1.f;
  }
  virtual const LayerTreeSettings& Settings() const OVERRIDE {
    return settings_;
  }
  virtual void SetFullRootLayerDamage() OVERRIDE {}
  virtual bool HasImplThread() const OVERRIDE { return false; }
  virtual bool ShouldClearRootRenderPass() const OVERRIDE {
    return should_clear_root_render_pass_;
  }
  virtual CompositorFrameMetadata MakeCompositorFrameMetadata() const OVERRIDE {
    return CompositorFrameMetadata();
  }
  virtual bool AllowPartialSwap() const OVERRIDE {
    return true;
  }
  virtual bool ExternalStencilTestEnabled() const OVERRIDE { return false; }

 protected:
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<SoftwareRenderer> renderer_;
  gfx::Rect viewport_;
  LayerTreeSettings settings_;
  bool should_clear_root_render_pass_;
};

TEST_F(SoftwareRendererTest, SolidColorQuad) {
  gfx::Size outer_size(100, 100);
  gfx::Size inner_size(98, 98);
  gfx::Rect outer_rect(outer_size);
  gfx::Rect inner_rect(gfx::Point(1, 1), inner_size);
  set_viewport(gfx::Rect(outer_size));

  InitializeRenderer(make_scoped_ptr(new SoftwareOutputDevice));

  scoped_ptr<SharedQuadState> shared_quad_state = SharedQuadState::Create();
  shared_quad_state->SetAll(
      gfx::Transform(), outer_size, outer_rect, outer_rect, false, 1.0);
  RenderPass::Id root_render_pass_id = RenderPass::Id(1, 1);
  scoped_ptr<TestRenderPass> root_render_pass = TestRenderPass::Create();
  root_render_pass->SetNew(
      root_render_pass_id, outer_rect, outer_rect, gfx::Transform());
  scoped_ptr<SolidColorDrawQuad> outer_quad = SolidColorDrawQuad::Create();
  outer_quad->SetNew(
      shared_quad_state.get(), outer_rect, SK_ColorYELLOW, false);
  scoped_ptr<SolidColorDrawQuad> inner_quad = SolidColorDrawQuad::Create();
  inner_quad->SetNew(shared_quad_state.get(), inner_rect, SK_ColorCYAN, false);
  root_render_pass->AppendQuad(inner_quad.PassAs<DrawQuad>());
  root_render_pass->AppendQuad(outer_quad.PassAs<DrawQuad>());

  RenderPassList list;
  list.push_back(root_render_pass.PassAs<RenderPass>());
  renderer()->DrawFrame(&list);

  SkBitmap output;
  output.setConfig(SkBitmap::kARGB_8888_Config,
                   DeviceViewport().width(),
                   DeviceViewport().height());
  output.allocPixels();
  renderer()->GetFramebufferPixels(output.getPixels(), outer_rect);

  EXPECT_EQ(SK_ColorYELLOW, output.getColor(0, 0));
  EXPECT_EQ(SK_ColorYELLOW,
            output.getColor(outer_size.width() - 1, outer_size.height() - 1));
  EXPECT_EQ(SK_ColorCYAN, output.getColor(1, 1));
  EXPECT_EQ(SK_ColorCYAN,
            output.getColor(inner_size.width() - 1, inner_size.height() - 1));
}

TEST_F(SoftwareRendererTest, TileQuad) {
  gfx::Size outer_size(100, 100);
  gfx::Size inner_size(98, 98);
  gfx::Rect outer_rect(outer_size);
  gfx::Rect inner_rect(gfx::Point(1, 1), inner_size);
  set_viewport(gfx::Rect(outer_size));
  InitializeRenderer(make_scoped_ptr(new SoftwareOutputDevice));

  ResourceProvider::ResourceId resource_yellow =
      resource_provider()->CreateResource(
          outer_size, GL_RGBA, ResourceProvider::TextureUsageAny);
  ResourceProvider::ResourceId resource_cyan =
      resource_provider()->CreateResource(
          inner_size, GL_RGBA, ResourceProvider::TextureUsageAny);

  SkBitmap yellow_tile;
  yellow_tile.setConfig(
      SkBitmap::kARGB_8888_Config, outer_size.width(), outer_size.height());
  yellow_tile.allocPixels();
  yellow_tile.eraseColor(SK_ColorYELLOW);

  SkBitmap cyan_tile;
  cyan_tile.setConfig(
      SkBitmap::kARGB_8888_Config, inner_size.width(), inner_size.height());
  cyan_tile.allocPixels();
  cyan_tile.eraseColor(SK_ColorCYAN);

  resource_provider()->SetPixels(
      resource_yellow,
      static_cast<uint8_t*>(yellow_tile.getPixels()),
      gfx::Rect(outer_size),
      gfx::Rect(outer_size),
      gfx::Vector2d());
  resource_provider()->SetPixels(resource_cyan,
                                 static_cast<uint8_t*>(cyan_tile.getPixels()),
                                 gfx::Rect(inner_size),
                                 gfx::Rect(inner_size),
                                 gfx::Vector2d());

  gfx::Rect root_rect = DeviceViewport();

  scoped_ptr<SharedQuadState> shared_quad_state = SharedQuadState::Create();
  shared_quad_state->SetAll(
      gfx::Transform(), outer_size, outer_rect, outer_rect, false, 1.0);
  RenderPass::Id root_render_pass_id = RenderPass::Id(1, 1);
  scoped_ptr<TestRenderPass> root_render_pass = TestRenderPass::Create();
  root_render_pass->SetNew(
      root_render_pass_id, root_rect, root_rect, gfx::Transform());
  scoped_ptr<TileDrawQuad> outer_quad = TileDrawQuad::Create();
  outer_quad->SetNew(shared_quad_state.get(),
                     outer_rect,
                     outer_rect,
                     resource_yellow,
                     gfx::RectF(outer_size),
                     outer_size,
                     false);
  scoped_ptr<TileDrawQuad> inner_quad = TileDrawQuad::Create();
  inner_quad->SetNew(shared_quad_state.get(),
                     inner_rect,
                     inner_rect,
                     resource_cyan,
                     gfx::RectF(inner_size),
                     inner_size,
                     false);
  root_render_pass->AppendQuad(inner_quad.PassAs<DrawQuad>());
  root_render_pass->AppendQuad(outer_quad.PassAs<DrawQuad>());

  RenderPassList list;
  list.push_back(root_render_pass.PassAs<RenderPass>());
  renderer()->DrawFrame(&list);

  SkBitmap output;
  output.setConfig(SkBitmap::kARGB_8888_Config,
                   DeviceViewport().width(),
                   DeviceViewport().height());
  output.allocPixels();
  renderer()->GetFramebufferPixels(output.getPixels(), outer_rect);

  EXPECT_EQ(SK_ColorYELLOW, output.getColor(0, 0));
  EXPECT_EQ(SK_ColorYELLOW,
            output.getColor(outer_size.width() - 1, outer_size.height() - 1));
  EXPECT_EQ(SK_ColorCYAN, output.getColor(1, 1));
  EXPECT_EQ(SK_ColorCYAN,
            output.getColor(inner_size.width() - 1, inner_size.height() - 1));
}

TEST_F(SoftwareRendererTest, ShouldClearRootRenderPass) {
  gfx::Rect viewport_rect(0, 0, 100, 100);
  set_viewport(viewport_rect);
  set_should_clear_root_render_pass(false);
  InitializeRenderer(make_scoped_ptr(new SoftwareOutputDevice));

  RenderPassList list;

  SkBitmap output;
  output.setConfig(SkBitmap::kARGB_8888_Config,
                   viewport_rect.width(),
                   viewport_rect.height());
  output.allocPixels();

  // Draw a fullscreen green quad in a first frame.
  RenderPass::Id root_clear_pass_id(1, 0);
  TestRenderPass* root_clear_pass = AddRenderPass(
      &list, root_clear_pass_id, viewport_rect, gfx::Transform());
  AddQuad(root_clear_pass, viewport_rect, SK_ColorGREEN);

  renderer()->DecideRenderPassAllocationsForFrame(list);
  renderer()->DrawFrame(&list);
  renderer()->GetFramebufferPixels(output.getPixels(), viewport_rect);

  EXPECT_EQ(SK_ColorGREEN, output.getColor(0, 0));
  EXPECT_EQ(SK_ColorGREEN,
      output.getColor(viewport_rect.width() - 1, viewport_rect.height() - 1));

  list.clear();

  // Draw a smaller magenta rect without filling the viewport in a separate
  // frame.
  gfx::Rect smaller_rect(20, 20, 60, 60);

  RenderPass::Id root_smaller_pass_id(2, 0);
  TestRenderPass* root_smaller_pass = AddRenderPass(
      &list, root_smaller_pass_id, viewport_rect, gfx::Transform());
  AddQuad(root_smaller_pass, smaller_rect, SK_ColorMAGENTA);

  renderer()->DecideRenderPassAllocationsForFrame(list);
  renderer()->DrawFrame(&list);
  renderer()->GetFramebufferPixels(output.getPixels(), viewport_rect);

  // If we didn't clear, the borders should still be green.
  EXPECT_EQ(SK_ColorGREEN, output.getColor(0, 0));
  EXPECT_EQ(SK_ColorGREEN,
      output.getColor(viewport_rect.width() - 1, viewport_rect.height() - 1));

  EXPECT_EQ(SK_ColorMAGENTA,
            output.getColor(smaller_rect.x(), smaller_rect.y()));
  EXPECT_EQ(SK_ColorMAGENTA,
      output.getColor(smaller_rect.right() - 1, smaller_rect.bottom() - 1));
}

}  // namespace
}  // namespace cc
