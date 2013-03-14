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
#include "cc/test/pixel_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gl/gl_implementation.h"
#include "webkit/gpu/context_provider_in_process.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace cc {
namespace {

const int kDeviceViewportWidth = 200;
const int kDeviceViewportHeight = 200;

class FakeRendererClient : public RendererClient {
 public:
  FakeRendererClient()
  {
  }

  // RendererClient methods.
  virtual gfx::Size DeviceViewportSize() const OVERRIDE {
    return gfx::Size(kDeviceViewportWidth, kDeviceViewportHeight);
  }
  virtual const LayerTreeSettings& Settings() const OVERRIDE {
    static LayerTreeSettings fake_settings;
    return fake_settings;
  }
  virtual void DidLoseOutputSurface() OVERRIDE { }
  virtual void OnSwapBuffersComplete() OVERRIDE { }
  virtual void SetFullRootLayerDamage() OVERRIDE { }
  virtual void SetManagedMemoryPolicy(const ManagedMemoryPolicy&) OVERRIDE {}
  virtual void EnforceManagedMemoryPolicy(
      const ManagedMemoryPolicy&) OVERRIDE {}
  virtual bool HasImplThread() const OVERRIDE { return false; }
  virtual bool ShouldClearRootRenderPass() const OVERRIDE { return true; }
  virtual CompositorFrameMetadata MakeCompositorFrameMetadata() const
      OVERRIDE { return CompositorFrameMetadata(); }
};

class GLRendererPixelTest : public testing::Test {
 protected:
  GLRendererPixelTest() {}

  virtual void SetUp() {
    gfx::InitializeGLBindings(gfx::kGLImplementationOSMesaGL);
    scoped_ptr<webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl>
        context3d(
            new webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl);
    context3d->Initialize(WebKit::WebGraphicsContext3D::Attributes(), NULL);
    output_surface_.reset(new OutputSurface(
        context3d.PassAs<WebKit::WebGraphicsContext3D>()));
    resource_provider_ = ResourceProvider::Create(output_surface_.get());
    renderer_ = GLRenderer::Create(&fake_client_,
                                   output_surface_.get(),
                                   resource_provider_.get());

    scoped_refptr<cc::ContextProvider> offscreen_contexts =
        new webkit::gpu::ContextProviderInProcess(
            webkit::gpu::ContextProviderInProcess::IN_PROCESS_COMMAND_BUFFER);
    ASSERT_TRUE(offscreen_contexts->InitializeOnMainThread());
    resource_provider_->SetOffscreenContextProvider(offscreen_contexts);
  }

  bool PixelsMatchReference(base::FilePath ref_file) {
    gfx::Rect viewport_rect(kDeviceViewportWidth, kDeviceViewportHeight);

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                     viewport_rect.width(), viewport_rect.height());
    bitmap.allocPixels();
    unsigned char* pixels = static_cast<unsigned char*>(bitmap.getPixels());
    renderer_->GetFramebufferPixels(pixels, viewport_rect);

    base::FilePath test_data_dir;
    if (!PathService::Get(cc::DIR_TEST_DATA, &test_data_dir))
      return false;

    // To rebaseline:
    //return WritePNGFile(bitmap, test_data_dir.Append(ref_file));

    return IsSameAsPNGFile(bitmap, test_data_dir.Append(ref_file));
  }

  scoped_ptr<OutputSurface> output_surface_;
  FakeRendererClient fake_client_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<GLRenderer> renderer_;
};

scoped_ptr<RenderPass> CreateTestRootRenderPass(RenderPass::Id id,
                                                gfx::Rect rect) {
  scoped_ptr<RenderPass> pass = RenderPass::Create();
  const gfx::Rect output_rect = rect;
  const gfx::RectF damage_rect = rect;
  const gfx::Transform transform_to_root_target;
  pass->SetNew(id, output_rect, damage_rect, transform_to_root_target);
  return pass.Pass();
}

scoped_ptr<RenderPass> CreateTestRenderPass(
    RenderPass::Id id,
    gfx::Rect rect,
    const gfx::Transform& transform_to_root_target) {
  scoped_ptr<RenderPass> pass = RenderPass::Create();
  const gfx::Rect output_rect = rect;
  const gfx::RectF damage_rect = rect;
  pass->SetNew(id, output_rect, damage_rect, transform_to_root_target);
  return pass.Pass();
}

scoped_ptr<SharedQuadState> CreateTestSharedQuadState(
    gfx::Transform content_to_target_transform, gfx::Rect rect) {
  const gfx::Size content_bounds = rect.size();
  const gfx::Rect visible_content_rect = rect;
  const gfx::Rect clip_rect = rect;
  const bool is_clipped = false;
  const float opacity = 1.0f;
  scoped_ptr<SharedQuadState> shared_state = SharedQuadState::Create();
  shared_state->SetAll(content_to_target_transform,
                       content_bounds,
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
  gfx::Rect rect(kDeviceViewportWidth, kDeviceViewportHeight);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  gfx::Transform content_to_target_transform;
  scoped_ptr<SharedQuadState> shared_state =
      CreateTestSharedQuadState(content_to_target_transform, rect);

  scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
  color_quad->SetNew(shared_state.get(), rect, SK_ColorGREEN);

  pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  renderer_->DrawFrame(pass_list);

  EXPECT_TRUE(PixelsMatchReference(
      base::FilePath(FILE_PATH_LITERAL("green.png"))));
}

TEST_F(GLRendererPixelTest, RenderPassChangesSize) {
  gfx::Rect viewport_rect(kDeviceViewportWidth, kDeviceViewportHeight);

  RenderPass::Id root_pass_id(1, 1);
  scoped_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  RenderPass::Id child_pass_id(2, 2);
  gfx::Rect pass_rect(kDeviceViewportWidth, kDeviceViewportHeight);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform content_to_target_transform;
  scoped_ptr<SharedQuadState> shared_state =
      CreateTestSharedQuadState(content_to_target_transform, viewport_rect);

  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(shared_state.get(),
               gfx::Rect(0,
                         0,
                         kDeviceViewportWidth / 2,
                         kDeviceViewportHeight),
               SK_ColorBLUE);
  scoped_ptr<SolidColorDrawQuad> yellow = SolidColorDrawQuad::Create();
  yellow->SetNew(shared_state.get(),
               gfx::Rect(kDeviceViewportWidth / 2,
                         0,
                         kDeviceViewportWidth / 2,
                         kDeviceViewportHeight),
                 SK_ColorYELLOW);

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

  renderer_->SetEnlargePassTextureAmountForTesting(gfx::Vector2d(50, 75));
  renderer_->DecideRenderPassAllocationsForFrame(pass_list);
  renderer_->DrawFrame(pass_list);

  EXPECT_TRUE(PixelsMatchReference(
      base::FilePath(FILE_PATH_LITERAL("blue_yellow.png"))));
}

class GLRendererPixelTestWithBackgroundFilter : public GLRendererPixelTest {
 protected:
  void DrawFrame() {
    gfx::Rect device_viewport_rect(kDeviceViewportWidth, kDeviceViewportHeight);

    RenderPass::Id root_id(1, 1);
    scoped_ptr<RenderPass> root_pass =
        CreateTestRootRenderPass(root_id, device_viewport_rect);
    root_pass->has_transparent_background = false;

    gfx::Transform identity_content_to_target_transform;

    RenderPass::Id filter_pass_id(2, 1);
    gfx::Transform transform_to_root;
    scoped_ptr<RenderPass> filter_pass =
        CreateTestRenderPass(filter_pass_id,
                             filter_pass_content_rect_,
                             transform_to_root);

    // A non-visible quad in the filtering render pass.
    {
      scoped_ptr<SharedQuadState> shared_state =
          CreateTestSharedQuadState(identity_content_to_target_transform,
                                    filter_pass_content_rect_);
      scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(shared_state.get(),
                         filter_pass_content_rect_,
                         SK_ColorTRANSPARENT);
      filter_pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());
      filter_pass->shared_quad_state_list.push_back(shared_state.Pass());
    }

    {
      scoped_ptr<SharedQuadState> shared_state =
          CreateTestSharedQuadState(filter_pass_to_target_transform_,
                                    filter_pass_content_rect_);
      scoped_ptr<RenderPassDrawQuad> filter_pass_quad =
          RenderPassDrawQuad::Create();
      filter_pass_quad->SetNew(
          shared_state.get(),
          filter_pass_content_rect_,
          filter_pass_id,
          false,  // is_replica
          0,  // mask_resource_id
          filter_pass_content_rect_,  // contents_changed_since_last_frame
          gfx::RectF(), // mask_uv_rect
          WebKit::WebFilterOperations(),  // filters
          skia::RefPtr<SkImageFilter>(),  // filter
          background_filters_);
      root_pass->quad_list.push_back(filter_pass_quad.PassAs<DrawQuad>());
      root_pass->shared_quad_state_list.push_back(shared_state.Pass());
    }

    const int kColumnWidth = kDeviceViewportWidth / 3;

    gfx::Rect left_rect = gfx::Rect(0, 0, kColumnWidth, 20);
    for (int i = 0; left_rect.y() < kDeviceViewportHeight; ++i) {
      scoped_ptr<SharedQuadState> shared_state =
          CreateTestSharedQuadState(identity_content_to_target_transform,
                                    left_rect);
      scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(shared_state.get(), left_rect, SK_ColorGREEN);
      root_pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());
      root_pass->shared_quad_state_list.push_back(shared_state.Pass());
      left_rect += gfx::Vector2d(0, left_rect.height() + 1);
    }

    gfx::Rect middle_rect = gfx::Rect(kColumnWidth+1, 0, kColumnWidth, 20);
    for (int i = 0; middle_rect.y() < kDeviceViewportHeight; ++i) {
      scoped_ptr<SharedQuadState> shared_state =
          CreateTestSharedQuadState(identity_content_to_target_transform,
                                    middle_rect);
      scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(shared_state.get(), middle_rect, SK_ColorRED);
      root_pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());
      root_pass->shared_quad_state_list.push_back(shared_state.Pass());
      middle_rect += gfx::Vector2d(0, middle_rect.height() + 1);
    }

    gfx::Rect right_rect = gfx::Rect((kColumnWidth+1)*2, 0, kColumnWidth, 20);
    for (int i = 0; right_rect.y() < kDeviceViewportHeight; ++i) {
      scoped_ptr<SharedQuadState> shared_state =
          CreateTestSharedQuadState(identity_content_to_target_transform,
                                    right_rect);
      scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(shared_state.get(), right_rect, SK_ColorBLUE);
      root_pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());
      root_pass->shared_quad_state_list.push_back(shared_state.Pass());
      right_rect += gfx::Vector2d(0, right_rect.height() + 1);
    }

    scoped_ptr<SharedQuadState> shared_state =
        CreateTestSharedQuadState(identity_content_to_target_transform,
                                  device_viewport_rect);
    scoped_ptr<SolidColorDrawQuad> background_quad =
        SolidColorDrawQuad::Create();
    background_quad->SetNew(shared_state.get(),
                            device_viewport_rect,
                            SK_ColorWHITE);
    root_pass->quad_list.push_back(background_quad.PassAs<DrawQuad>());
    root_pass->shared_quad_state_list.push_back(shared_state.Pass());

    RenderPassList pass_list;
    pass_list.push_back(filter_pass.Pass());
    pass_list.push_back(root_pass.Pass());

    renderer_->DecideRenderPassAllocationsForFrame(pass_list);
    renderer_->DrawFrame(pass_list);
  }

  WebKit::WebFilterOperations background_filters_;
  gfx::Transform filter_pass_to_target_transform_;
  gfx::Rect filter_pass_content_rect_;
};

TEST_F(GLRendererPixelTestWithBackgroundFilter, InvertFilter) {
  background_filters_.append(
      WebKit::WebFilterOperation::createInvertFilter(1.f));

  filter_pass_content_rect_ =
      gfx::Rect(kDeviceViewportWidth, kDeviceViewportHeight);
  filter_pass_content_rect_.Inset(12, 14, 16, 18);

  DrawFrame();
  EXPECT_TRUE(PixelsMatchReference(
      base::FilePath(FILE_PATH_LITERAL("background_filter.png"))));
}

TEST_F(GLRendererPixelTest, AntiAliasing) {
  gfx::Rect rect(0, 0, 200, 200);

  RenderPass::Id id(1, 1);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> pass =
      CreateTestRenderPass(id, rect, transform_to_root);

  gfx::Transform red_content_to_target_transform;
  red_content_to_target_transform.Rotate(10);
  scoped_ptr<SharedQuadState> red_shared_state =
      CreateTestSharedQuadState(red_content_to_target_transform, rect);

  scoped_ptr<SolidColorDrawQuad> red = SolidColorDrawQuad::Create();
  red->SetNew(red_shared_state.get(), rect, SK_ColorRED);

  pass->quad_list.push_back(red.PassAs<DrawQuad>());

  gfx::Transform yellow_content_to_target_transform;
  yellow_content_to_target_transform.Rotate(5);
  scoped_ptr<SharedQuadState> yellow_shared_state =
      CreateTestSharedQuadState(yellow_content_to_target_transform, rect);

  scoped_ptr<SolidColorDrawQuad> yellow = SolidColorDrawQuad::Create();
  yellow->SetNew(yellow_shared_state.get(), rect, SK_ColorYELLOW);

  pass->quad_list.push_back(yellow.PassAs<DrawQuad>());

  gfx::Transform blue_content_to_target_transform;
  scoped_ptr<SharedQuadState> blue_shared_state =
      CreateTestSharedQuadState(blue_content_to_target_transform, rect);

  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(blue_shared_state.get(), rect, SK_ColorBLUE);

  pass->quad_list.push_back(blue.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  renderer_->DrawFrame(pass_list);

  EXPECT_TRUE(PixelsMatchReference(
      base::FilePath(FILE_PATH_LITERAL("anti_aliasing.png"))));
}
#endif

}  // namespace
}  // namespace cc
