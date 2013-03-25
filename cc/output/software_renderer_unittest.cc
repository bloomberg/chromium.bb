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

using namespace WebKit;

namespace cc {
namespace {

class SoftwareRendererTest : public testing::Test, public RendererClient {
 public:
  SoftwareRendererTest() : should_clear_root_render_pass_(true) {}

  void InitializeRenderer() {
    output_surface_ = FakeOutputSurface::CreateSoftware(
        make_scoped_ptr(new SoftwareOutputDevice));
    resource_provider_ = ResourceProvider::Create(output_surface_.get());
    renderer_ = SoftwareRenderer::Create(
        this, output_surface_.get(), resource_provider());
  }

  ResourceProvider* resource_provider() const {
    return resource_provider_.get();
  }

  SoftwareRenderer* renderer() const { return renderer_.get(); }

  void set_viewport_size(gfx::Size viewport_size) {
    viewport_size_ = viewport_size;
  }

  void set_should_clear_root_render_pass(bool clear_root_render_pass) {
    should_clear_root_render_pass_ = clear_root_render_pass;
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
  virtual bool ShouldClearRootRenderPass() const OVERRIDE {
    return should_clear_root_render_pass_;
  }
  virtual CompositorFrameMetadata MakeCompositorFrameMetadata() const OVERRIDE {
    return CompositorFrameMetadata();
  }

 protected:
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<SoftwareRenderer> renderer_;
  gfx::Size viewport_size_;
  LayerTreeSettings settings_;
  bool should_clear_root_render_pass_;
};

TEST_F(SoftwareRendererTest, SolidColorQuad) {
  gfx::Size outer_size(100, 100);
  int outer_pixels = outer_size.width() * outer_size.height();
  gfx::Size inner_size(98, 98);
  gfx::Rect outer_rect(outer_size);
  gfx::Rect inner_rect(gfx::Point(1, 1), inner_size);
  set_viewport_size(outer_size);

  InitializeRenderer();

  scoped_ptr<SharedQuadState> shared_quad_state = SharedQuadState::Create();
  shared_quad_state->SetAll(
      gfx::Transform(), outer_size, outer_rect, outer_rect, false, 1.0);
  RenderPass::Id root_render_pass_id = RenderPass::Id(1, 1);
  scoped_ptr<TestRenderPass> root_render_pass = TestRenderPass::Create();
  root_render_pass->SetNew(
      root_render_pass_id, outer_rect, outer_rect, gfx::Transform());
  scoped_ptr<SolidColorDrawQuad> outer_quad = SolidColorDrawQuad::Create();
  outer_quad->SetNew(shared_quad_state.get(), outer_rect, SK_ColorYELLOW);
  scoped_ptr<SolidColorDrawQuad> inner_quad = SolidColorDrawQuad::Create();
  inner_quad->SetNew(shared_quad_state.get(), inner_rect, SK_ColorCYAN);
  root_render_pass->AppendQuad(inner_quad.PassAs<DrawQuad>());
  root_render_pass->AppendQuad(outer_quad.PassAs<DrawQuad>());

  RenderPassList list;
  list.push_back(root_render_pass.PassAs<RenderPass>());
  renderer()->DrawFrame(list);

  scoped_array<SkColor> pixels(new SkColor[DeviceViewportSize().width() *
      DeviceViewportSize().height()]);
  renderer()->GetFramebufferPixels(pixels.get(), outer_rect);

// FIXME: This fails on Android. Endianness maybe?
// Yellow: expects 0xFFFFFF00, was 0xFF00FFFF on android.
// Cyan:   expects 0xFF00FFFF, was 0xFFFFFF00 on android.
// http://crbug.com/154528
#if !defined(OS_ANDROID)
  EXPECT_EQ(SK_ColorYELLOW, pixels[0]);
  EXPECT_EQ(SK_ColorYELLOW, pixels[outer_pixels - 1]);
  EXPECT_EQ(SK_ColorCYAN, pixels[outer_size.width() + 1]);
  EXPECT_EQ(SK_ColorCYAN, pixels[outer_pixels - outer_size.width() - 2]);
#endif
}

TEST_F(SoftwareRendererTest, TileQuad) {
  gfx::Size outer_size(100, 100);
  int outer_pixels = outer_size.width() * outer_size.height();
  gfx::Size inner_size(98, 98);
  int inner_pixels = inner_size.width() * inner_size.height();
  gfx::Rect outer_rect(outer_size);
  gfx::Rect inner_rect(gfx::Point(1, 1), inner_size);
  set_viewport_size(outer_size);
  InitializeRenderer();

  ResourceProvider::ResourceId resource_yellow =
      resource_provider()->CreateResource(
          outer_size, GL_RGBA, ResourceProvider::TextureUsageAny);
  ResourceProvider::ResourceId resource_cyan =
      resource_provider()->CreateResource(
          inner_size, GL_RGBA, ResourceProvider::TextureUsageAny);

  SkColor yellow = SK_ColorYELLOW;
  SkColor cyan = SK_ColorCYAN;
  scoped_array<SkColor> yellow_pixels(new SkColor[outer_pixels]);
  scoped_array<SkColor> cyan_pixels(new SkColor[inner_pixels]);
  for (int i = 0; i < outer_pixels; i++)
    yellow_pixels[i] = yellow;
  for (int i = 0; i < inner_pixels; i++)
    cyan_pixels[i] = cyan;

  resource_provider()->SetPixels(
      resource_yellow,
      reinterpret_cast<uint8_t*>(yellow_pixels.get()),
      gfx::Rect(outer_size),
      gfx::Rect(outer_size),
      gfx::Vector2d());
  resource_provider()->SetPixels(resource_cyan,
                                 reinterpret_cast<uint8_t*>(cyan_pixels.get()),
                                 gfx::Rect(inner_size),
                                 gfx::Rect(inner_size),
                                 gfx::Vector2d());

  gfx::Rect root_rect = gfx::Rect(DeviceViewportSize());

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
  renderer()->DrawFrame(list);

  scoped_array<SkColor> pixels(new SkColor[DeviceViewportSize().width() *
      DeviceViewportSize().height()]);
  renderer()->GetFramebufferPixels(pixels.get(), outer_rect);

  EXPECT_EQ(SK_ColorYELLOW, pixels[0]);
  EXPECT_EQ(SK_ColorYELLOW, pixels[outer_pixels - 1]);
  EXPECT_EQ(SK_ColorCYAN, pixels[outer_size.width() + 1]);
  EXPECT_EQ(SK_ColorCYAN, pixels[outer_pixels - outer_size.width() - 2]);
}

TEST_F(SoftwareRendererTest, ShouldClearRootRenderPass) {
  gfx::Rect viewport_rect(gfx::Size(100, 100));
  size_t viewport_pixels = viewport_rect.width() * viewport_rect.height();
  set_viewport_size(viewport_rect.size());
  set_should_clear_root_render_pass(false);
  InitializeRenderer();

  RenderPassList list;
  scoped_array<SkColor> pixels(new SkColor[viewport_pixels]);

  // Draw a fullscreen green quad in a first frame.
  RenderPass::Id root_clear_pass_id(1, 0);
  TestRenderPass* root_clear_pass =
      addRenderPass(list, root_clear_pass_id, viewport_rect, gfx::Transform());
  addQuad(root_clear_pass, viewport_rect, SK_ColorGREEN);

  renderer()->DecideRenderPassAllocationsForFrame(list);
  renderer()->DrawFrame(list);
  renderer()->GetFramebufferPixels(pixels.get(), viewport_rect);

  EXPECT_EQ(SK_ColorGREEN, pixels[0]);
  EXPECT_EQ(SK_ColorGREEN, pixels[viewport_pixels - 1]);

  list.clear();

  // Draw a smaller magenta rect without filling the viewport in a separate
  // frame.
  gfx::Rect smaller_rect(20, 20, 60, 60);

  RenderPass::Id root_smaller_pass_id(2, 0);
  TestRenderPass* root_smaller_pass = addRenderPass(
      list, root_smaller_pass_id, viewport_rect, gfx::Transform());
  addQuad(root_smaller_pass, smaller_rect, SK_ColorMAGENTA);

  renderer()->DecideRenderPassAllocationsForFrame(list);
  renderer()->DrawFrame(list);
  renderer()->GetFramebufferPixels(pixels.get(), viewport_rect);

  // If we didn't clear, the borders should still be green.
  EXPECT_EQ(SK_ColorGREEN, pixels[0]);
  EXPECT_EQ(SK_ColorGREEN, pixels[viewport_pixels - 1]);

  EXPECT_EQ(
      SK_ColorMAGENTA,
      pixels[smaller_rect.y() * viewport_rect.width() + smaller_rect.x()]);
  EXPECT_EQ(SK_ColorMAGENTA,
            pixels[(smaller_rect.bottom() - 1) * viewport_rect.width() +
                smaller_rect.right() - 1]);
}

}  // namespace
}  // namespace cc
