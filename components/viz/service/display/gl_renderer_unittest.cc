// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/gl_renderer.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include "base/location.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/resources/resource_provider.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_resource_provider.h"
#include "cc/test/pixel_test.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "components/viz/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "ui/gfx/transform.h"
#include "ui/latency/latency_info.h"

using testing::_;
using testing::AnyNumber;
using testing::Args;
using testing::AtLeast;
using testing::Contains;
using testing::ElementsAre;
using testing::Expectation;
using testing::InSequence;
using testing::Invoke;
using testing::Mock;
using testing::Not;
using testing::Pointee;
using testing::Return;
using testing::StrictMock;

namespace viz {

MATCHER_P(MatchesSyncToken, sync_token, "") {
  gpu::SyncToken other;
  memcpy(&other, arg, sizeof(other));
  return other == sync_token;
}

class GLRendererTest : public testing::Test {
 protected:
  RenderPass* root_render_pass() {
    return render_passes_in_draw_order_.back().get();
  }
  void DrawFrame(GLRenderer* renderer, const gfx::Size& viewport_size) {
    renderer->DrawFrame(&render_passes_in_draw_order_, 1.f, viewport_size);
  }

  static const Program* current_program(GLRenderer* renderer) {
    return renderer->current_program_;
  }

  RenderPassList render_passes_in_draw_order_;
};

#define EXPECT_PROGRAM_VALID(program_binding)      \
  do {                                             \
    ASSERT_TRUE(program_binding);                  \
    EXPECT_TRUE((program_binding)->program());     \
    EXPECT_TRUE((program_binding)->initialized()); \
  } while (false)

static inline SkBlendMode BlendModeToSkXfermode(BlendMode blend_mode) {
  switch (blend_mode) {
    case BLEND_MODE_NONE:
    case BLEND_MODE_NORMAL:
      return SkBlendMode::kSrcOver;
    case BLEND_MODE_DESTINATION_IN:
      return SkBlendMode::kDstIn;
    case BLEND_MODE_SCREEN:
      return SkBlendMode::kScreen;
    case BLEND_MODE_OVERLAY:
      return SkBlendMode::kOverlay;
    case BLEND_MODE_DARKEN:
      return SkBlendMode::kDarken;
    case BLEND_MODE_LIGHTEN:
      return SkBlendMode::kLighten;
    case BLEND_MODE_COLOR_DODGE:
      return SkBlendMode::kColorDodge;
    case BLEND_MODE_COLOR_BURN:
      return SkBlendMode::kColorBurn;
    case BLEND_MODE_HARD_LIGHT:
      return SkBlendMode::kHardLight;
    case BLEND_MODE_SOFT_LIGHT:
      return SkBlendMode::kSoftLight;
    case BLEND_MODE_DIFFERENCE:
      return SkBlendMode::kDifference;
    case BLEND_MODE_EXCLUSION:
      return SkBlendMode::kExclusion;
    case BLEND_MODE_MULTIPLY:
      return SkBlendMode::kMultiply;
    case BLEND_MODE_HUE:
      return SkBlendMode::kHue;
    case BLEND_MODE_SATURATION:
      return SkBlendMode::kSaturation;
    case BLEND_MODE_COLOR:
      return SkBlendMode::kColor;
    case BLEND_MODE_LUMINOSITY:
      return SkBlendMode::kLuminosity;
  }
  return SkBlendMode::kSrcOver;
}

// Explicitly named to be a friend in GLRenderer for shader access.
class GLRendererShaderPixelTest : public cc::GLRendererPixelTest {
 public:
  void SetUp() override {
    cc::GLRendererPixelTest::SetUp();
    ASSERT_FALSE(renderer()->IsContextLost());
  }

  void TearDown() override {
    cc::GLRendererPixelTest::TearDown();
    ASSERT_FALSE(renderer()->IsContextLost());
  }

  void TestShaderWithDrawingFrame(
      const ProgramKey& program_key,
      const DirectRenderer::DrawingFrame& drawing_frame,
      bool validate_output_color_matrix) {
    renderer()->SetCurrentFrameForTesting(drawing_frame);
    const size_t kNumSrcColorSpaces = 4;
    gfx::ColorSpace src_color_spaces[kNumSrcColorSpaces] = {
        gfx::ColorSpace::CreateSRGB(),
        gfx::ColorSpace(gfx::ColorSpace::PrimaryID::ADOBE_RGB,
                        gfx::ColorSpace::TransferID::GAMMA28),
        gfx::ColorSpace::CreateREC709(), gfx::ColorSpace::CreateExtendedSRGB(),
    };
    const size_t kNumDstColorSpaces = 3;
    gfx::ColorSpace dst_color_spaces[kNumDstColorSpaces] = {
        gfx::ColorSpace::CreateSRGB(),
        gfx::ColorSpace(gfx::ColorSpace::PrimaryID::ADOBE_RGB,
                        gfx::ColorSpace::TransferID::GAMMA18),
        gfx::ColorSpace::CreateSCRGBLinear(),
    };
    for (size_t i = 0; i < kNumDstColorSpaces; ++i) {
      for (size_t j = 0; j < kNumSrcColorSpaces; ++j) {
        renderer()->SetUseProgram(program_key, src_color_spaces[j],
                                  dst_color_spaces[i]);
        EXPECT_TRUE(renderer()->current_program_->initialized());
        if (validate_output_color_matrix) {
          EXPECT_NE(
              -1, renderer()->current_program_->output_color_matrix_location());
        }
      }
    }
  }

  void TestShader(const ProgramKey& program_key) {
    TestShaderWithDrawingFrame(program_key, GLRenderer::DrawingFrame(), false);
  }

  void TestShadersWithOutputColorMatrix(const ProgramKey& program_key) {
    GLRenderer::DrawingFrame frame;

    RenderPassList render_passes_in_draw_order;
    gfx::Size viewport_size(100, 100);
    RenderPassId root_pass_id = 1;
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    root_pass->damage_rect = gfx::Rect(0, 0, 25, 25);

    frame.root_render_pass = root_pass;
    frame.current_render_pass = root_pass;
    frame.render_passes_in_draw_order = &render_passes_in_draw_order;

    // Set a non-identity color matrix on the output surface.
    SkMatrix44 color_matrix(SkMatrix44::kIdentity_Constructor);
    color_matrix.set(0, 0, 0.7f);
    color_matrix.set(1, 1, 0.4f);
    color_matrix.set(2, 2, 0.5f);
    renderer()->output_surface_->set_color_matrix(color_matrix);

    TestShaderWithDrawingFrame(program_key, frame, true);
  }

  void TestBasicShaders() {
    TestShader(ProgramKey::DebugBorder());
    TestShader(ProgramKey::SolidColor(NO_AA, false));
    TestShader(ProgramKey::SolidColor(USE_AA, false));

    TestShadersWithOutputColorMatrix(ProgramKey::DebugBorder());
    TestShadersWithOutputColorMatrix(ProgramKey::SolidColor(NO_AA, false));
    TestShadersWithOutputColorMatrix(ProgramKey::SolidColor(USE_AA, false));

    TestShader(ProgramKey::SolidColor(NO_AA, true));
    TestShadersWithOutputColorMatrix(ProgramKey::SolidColor(NO_AA, true));
  }

  void TestColorShaders() {
    const size_t kNumTransferFns = 7;
    SkColorSpaceTransferFn transfer_fns[kNumTransferFns] = {
        // The identity.
        {1.f, 1.f, 0.f, 1.f, 0.f, 0.f, 0.f},
        // The identity, with an if statement.
        {1.f, 1.f, 0.f, 1.f, 0.5f, 0.f, 0.f},
        // Just the power function.
        {1.1f, 1.f, 0.f, 1.f, 0.f, 0.f, 0.f},
        // Everything but the power function, nonlinear only.
        {1.f, 0.9f, 0.1f, 0.9f, 0.f, 0.1f, 0.1f},
        // Everything, nonlinear only.
        {1.1f, 0.9f, 0.1f, 0.9f, 0.f, 0.1f, 0.1f},
        // Everything but the power function.
        {1.f, 0.9f, 0.1f, 0.9f, 0.5f, 0.1f, 0.1f},
        // Everything.
        {1.1f, 0.9f, 0.1f, 0.9f, 0.5f, 0.1f, 0.1f},
    };

    for (size_t i = 0; i < kNumTransferFns; ++i) {
      SkMatrix44 primaries;
      gfx::ColorSpace::CreateSRGB().GetPrimaryMatrix(&primaries);
      gfx::ColorSpace src =
          gfx::ColorSpace::CreateCustom(primaries, transfer_fns[i]);

      renderer()->SetCurrentFrameForTesting(GLRenderer::DrawingFrame());
      renderer()->SetUseProgram(ProgramKey::SolidColor(NO_AA, false), src,
                                gfx::ColorSpace::CreateXYZD50());
      EXPECT_TRUE(renderer()->current_program_->initialized());
    }
  }

  void TestShadersWithPrecision(TexCoordPrecision precision) {
    // This program uses external textures and sampler, so it won't compile
    // everywhere.
    if (context_provider()->ContextCapabilities().egl_image_external) {
      TestShader(ProgramKey::VideoStream(precision));
    }
  }

  void TestShadersWithPrecisionAndBlend(TexCoordPrecision precision,
                                        BlendMode blend_mode) {
    TestShader(ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode,
                                      NO_AA, NO_MASK, false, false, false));
    TestShader(ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode,
                                      USE_AA, NO_MASK, false, false, false));
  }

  void TestShadersWithPrecisionAndSampler(TexCoordPrecision precision,
                                          SamplerType sampler) {
    if (!context_provider()->ContextCapabilities().egl_image_external &&
        sampler == SAMPLER_TYPE_EXTERNAL_OES) {
      // This will likely be hit in tests due to usage of osmesa.
      return;
    }

    TestShader(ProgramKey::Texture(precision, sampler, PREMULTIPLIED_ALPHA,
                                   false, true, false));
    TestShader(ProgramKey::Texture(precision, sampler, PREMULTIPLIED_ALPHA,
                                   false, false, false));
    TestShader(ProgramKey::Texture(precision, sampler, PREMULTIPLIED_ALPHA,
                                   true, true, false));
    TestShader(ProgramKey::Texture(precision, sampler, PREMULTIPLIED_ALPHA,
                                   true, false, false));
    TestShader(ProgramKey::Texture(precision, sampler, NON_PREMULTIPLIED_ALPHA,
                                   false, true, false));
    TestShader(ProgramKey::Texture(precision, sampler, NON_PREMULTIPLIED_ALPHA,
                                   false, false, false));
    TestShader(ProgramKey::Texture(precision, sampler, NON_PREMULTIPLIED_ALPHA,
                                   true, true, false));
    TestShader(ProgramKey::Texture(precision, sampler, NON_PREMULTIPLIED_ALPHA,
                                   true, false, false));

    TestShader(ProgramKey::Tile(precision, sampler, USE_AA, NO_SWIZZLE,
                                PREMULTIPLIED_ALPHA, false, false, false));
    TestShader(ProgramKey::Tile(precision, sampler, USE_AA, DO_SWIZZLE,
                                PREMULTIPLIED_ALPHA, false, false, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, NO_SWIZZLE,
                                PREMULTIPLIED_ALPHA, false, false, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, DO_SWIZZLE,
                                PREMULTIPLIED_ALPHA, false, false, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, NO_SWIZZLE,
                                PREMULTIPLIED_ALPHA, true, false, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, DO_SWIZZLE,
                                PREMULTIPLIED_ALPHA, true, false, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, NO_SWIZZLE,
                                PREMULTIPLIED_ALPHA, false, true, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, DO_SWIZZLE,
                                PREMULTIPLIED_ALPHA, false, true, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, NO_SWIZZLE,
                                PREMULTIPLIED_ALPHA, true, true, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, DO_SWIZZLE,
                                PREMULTIPLIED_ALPHA, true, true, false));
    TestShader(ProgramKey::Tile(precision, sampler, USE_AA, NO_SWIZZLE,
                                NON_PREMULTIPLIED_ALPHA, false, false, false));
    TestShader(ProgramKey::Tile(precision, sampler, USE_AA, DO_SWIZZLE,
                                NON_PREMULTIPLIED_ALPHA, false, false, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, NO_SWIZZLE,
                                NON_PREMULTIPLIED_ALPHA, false, false, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, DO_SWIZZLE,
                                NON_PREMULTIPLIED_ALPHA, false, false, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, NO_SWIZZLE,
                                NON_PREMULTIPLIED_ALPHA, true, false, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, DO_SWIZZLE,
                                NON_PREMULTIPLIED_ALPHA, true, false, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, NO_SWIZZLE,
                                NON_PREMULTIPLIED_ALPHA, false, true, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, DO_SWIZZLE,
                                NON_PREMULTIPLIED_ALPHA, false, true, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, NO_SWIZZLE,
                                NON_PREMULTIPLIED_ALPHA, true, true, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, DO_SWIZZLE,
                                NON_PREMULTIPLIED_ALPHA, true, true, false));

    // Iterate over alpha plane, nv12, and color_lut parameters.
    UVTextureMode uv_modes[2] = {UV_TEXTURE_MODE_UV, UV_TEXTURE_MODE_U_V};
    YUVAlphaTextureMode a_modes[2] = {YUV_NO_ALPHA_TEXTURE,
                                      YUV_HAS_ALPHA_TEXTURE};
    for (int j = 0; j < 2; j++) {
      for (int k = 0; k < 2; k++) {
        TestShader(ProgramKey::YUVVideo(precision, sampler, a_modes[j],
                                        uv_modes[k], false));
      }
    }
  }

  void TestShadersWithMasks(TexCoordPrecision precision,
                            SamplerType sampler,
                            BlendMode blend_mode,
                            bool mask_for_background) {
    if (!context_provider()->ContextCapabilities().egl_image_external &&
        sampler == SAMPLER_TYPE_EXTERNAL_OES) {
      // This will likely be hit in tests due to usage of osmesa.
      return;
    }

    TestShader(ProgramKey::RenderPass(precision, sampler, blend_mode, NO_AA,
                                      HAS_MASK, mask_for_background, false,
                                      false));
    TestShader(ProgramKey::RenderPass(precision, sampler, blend_mode, NO_AA,
                                      HAS_MASK, mask_for_background, true,
                                      false));
    TestShader(ProgramKey::RenderPass(precision, sampler, blend_mode, USE_AA,
                                      HAS_MASK, mask_for_background, false,
                                      false));
    TestShader(ProgramKey::RenderPass(precision, sampler, blend_mode, USE_AA,
                                      HAS_MASK, mask_for_background, true,
                                      false));
  }
};

namespace {

#if !defined(OS_ANDROID) && !defined(OS_WIN)
static const TexCoordPrecision kPrecisionList[] = {TEX_COORD_PRECISION_MEDIUM,
                                                   TEX_COORD_PRECISION_HIGH};

static const BlendMode kBlendModeList[LAST_BLEND_MODE + 1] = {
    BLEND_MODE_NONE,       BLEND_MODE_NORMAL,      BLEND_MODE_DESTINATION_IN,
    BLEND_MODE_SCREEN,     BLEND_MODE_OVERLAY,     BLEND_MODE_DARKEN,
    BLEND_MODE_LIGHTEN,    BLEND_MODE_COLOR_DODGE, BLEND_MODE_COLOR_BURN,
    BLEND_MODE_HARD_LIGHT, BLEND_MODE_SOFT_LIGHT,  BLEND_MODE_DIFFERENCE,
    BLEND_MODE_EXCLUSION,  BLEND_MODE_MULTIPLY,    BLEND_MODE_HUE,
    BLEND_MODE_SATURATION, BLEND_MODE_COLOR,       BLEND_MODE_LUMINOSITY,
};

static const SamplerType kSamplerList[] = {
    SAMPLER_TYPE_2D, SAMPLER_TYPE_2D_RECT, SAMPLER_TYPE_EXTERNAL_OES,
};

TEST_F(GLRendererShaderPixelTest, BasicShadersCompile) {
  TestBasicShaders();
}

TEST_F(GLRendererShaderPixelTest, TestColorShadersCompile) {
  TestColorShaders();
}

class PrecisionShaderPixelTest
    : public GLRendererShaderPixelTest,
      public ::testing::WithParamInterface<TexCoordPrecision> {};

TEST_P(PrecisionShaderPixelTest, ShadersCompile) {
  TestShadersWithPrecision(GetParam());
}

INSTANTIATE_TEST_CASE_P(PrecisionShadersCompile,
                        PrecisionShaderPixelTest,
                        ::testing::ValuesIn(kPrecisionList));

class PrecisionBlendShaderPixelTest
    : public GLRendererShaderPixelTest,
      public ::testing::WithParamInterface<
          std::tuple<TexCoordPrecision, BlendMode>> {};

TEST_P(PrecisionBlendShaderPixelTest, ShadersCompile) {
  TestShadersWithPrecisionAndBlend(std::get<0>(GetParam()),
                                   std::get<1>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(
    PrecisionBlendShadersCompile,
    PrecisionBlendShaderPixelTest,
    ::testing::Combine(::testing::ValuesIn(kPrecisionList),
                       ::testing::ValuesIn(kBlendModeList)));

class PrecisionSamplerShaderPixelTest
    : public GLRendererShaderPixelTest,
      public ::testing::WithParamInterface<
          std::tuple<TexCoordPrecision, SamplerType>> {};

TEST_P(PrecisionSamplerShaderPixelTest, ShadersCompile) {
  TestShadersWithPrecisionAndSampler(std::get<0>(GetParam()),
                                     std::get<1>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(PrecisionSamplerShadersCompile,
                        PrecisionSamplerShaderPixelTest,
                        ::testing::Combine(::testing::ValuesIn(kPrecisionList),
                                           ::testing::ValuesIn(kSamplerList)));

class MaskShaderPixelTest
    : public GLRendererShaderPixelTest,
      public ::testing::WithParamInterface<
          std::tuple<TexCoordPrecision, SamplerType, BlendMode, bool>> {};

TEST_P(MaskShaderPixelTest, ShadersCompile) {
  TestShadersWithMasks(std::get<0>(GetParam()), std::get<1>(GetParam()),
                       std::get<2>(GetParam()), std::get<3>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(MaskShadersCompile,
                        MaskShaderPixelTest,
                        ::testing::Combine(::testing::ValuesIn(kPrecisionList),
                                           ::testing::ValuesIn(kSamplerList),
                                           ::testing::ValuesIn(kBlendModeList),
                                           ::testing::Bool()));

#endif

class FakeRendererGL : public GLRenderer {
 public:
  FakeRendererGL(const RendererSettings* settings,
                 OutputSurface* output_surface,
                 cc::DisplayResourceProvider* resource_provider)
      : GLRenderer(settings, output_surface, resource_provider, nullptr) {}

  FakeRendererGL(
      const RendererSettings* settings,
      OutputSurface* output_surface,
      cc::DisplayResourceProvider* resource_provider,
      scoped_refptr<base::SingleThreadTaskRunner> current_task_runner)
      : GLRenderer(settings,
                   output_surface,
                   resource_provider,
                   std::move(current_task_runner)) {}

  void SetOverlayProcessor(OverlayProcessor* processor) {
    overlay_processor_.reset(processor);
  }

  // GLRenderer methods.

  // Changing visibility to public.
  using GLRenderer::DoDrawQuad;
  using GLRenderer::BeginDrawingFrame;
  using GLRenderer::FinishDrawingQuadList;
  using GLRenderer::stencil_enabled;
};

class GLRendererWithDefaultHarnessTest : public GLRendererTest {
 protected:
  GLRendererWithDefaultHarnessTest() {
    output_surface_ = FakeOutputSurface::Create3d();
    output_surface_->BindToClient(&output_surface_client_);

    shared_bitmap_manager_ = std::make_unique<TestSharedBitmapManager>();
    resource_provider_ =
        cc::FakeResourceProvider::CreateDisplayResourceProvider(
            output_surface_->context_provider(), shared_bitmap_manager_.get());
    renderer_ = std::make_unique<FakeRendererGL>(
        &settings_, output_surface_.get(), resource_provider_.get());
    renderer_->Initialize();
    renderer_->SetVisible(true);
  }

  void SwapBuffers() { renderer_->SwapBuffers(std::vector<ui::LatencyInfo>()); }

  RendererSettings settings_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider_;
  std::unique_ptr<FakeRendererGL> renderer_;
};

// Closing the namespace here so that GLRendererShaderTest can take advantage
// of the friend relationship with GLRenderer and all of the mock classes
// declared above it.
}  // namespace

class GLRendererShaderTest : public GLRendererTest {
 protected:
  GLRendererShaderTest() {
    output_surface_ = FakeOutputSurface::Create3d();
    output_surface_->BindToClient(&output_surface_client_);

    shared_bitmap_manager_ = std::make_unique<TestSharedBitmapManager>();
    resource_provider_ =
        cc::FakeResourceProvider::CreateDisplayResourceProvider(
            output_surface_->context_provider(), shared_bitmap_manager_.get());
    renderer_.reset(new FakeRendererGL(&settings_, output_surface_.get(),
                                       resource_provider_.get()));
    renderer_->Initialize();
    renderer_->SetVisible(true);

    child_context_provider_ = TestContextProvider::Create();
    child_context_provider_->BindToCurrentThread();
    child_resource_provider_ =
        cc::FakeResourceProvider::CreateLayerTreeResourceProvider(
            child_context_provider_.get(), shared_bitmap_manager_.get());
  }

  void TestRenderPassProgram(TexCoordPrecision precision,
                             BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode, NO_AA,
                               NO_MASK, false, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassColorMatrixProgram(TexCoordPrecision precision,
                                        BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode, NO_AA,
                               NO_MASK, false, true, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassMaskProgram(TexCoordPrecision precision,
                                 SamplerType sampler,
                                 BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, sampler, blend_mode, NO_AA, HAS_MASK,
                               false, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassMaskColorMatrixProgram(TexCoordPrecision precision,
                                            SamplerType sampler,
                                            BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, sampler, blend_mode, NO_AA, HAS_MASK,
                               false, true, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassProgramAA(TexCoordPrecision precision,
                               BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode, USE_AA,
                               NO_MASK, false, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassColorMatrixProgramAA(TexCoordPrecision precision,
                                          BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode, USE_AA,
                               NO_MASK, false, true, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassMaskProgramAA(TexCoordPrecision precision,
                                   SamplerType sampler,
                                   BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, sampler, blend_mode, USE_AA, HAS_MASK,
                               false, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassMaskColorMatrixProgramAA(TexCoordPrecision precision,
                                              SamplerType sampler,
                                              BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, sampler, blend_mode, USE_AA, HAS_MASK,
                               false, true, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestSolidColorProgramAA() {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::SolidColor(USE_AA, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  RendererSettings settings_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider_;
  scoped_refptr<TestContextProvider> child_context_provider_;
  std::unique_ptr<cc::LayerTreeResourceProvider> child_resource_provider_;
  std::unique_ptr<FakeRendererGL> renderer_;
};

namespace {

TEST_F(GLRendererWithDefaultHarnessTest, ExternalStencil) {
  gfx::Size viewport_size(1, 1);
  EXPECT_FALSE(renderer_->stencil_enabled());

  output_surface_->set_has_external_stencil_test(true);

  RenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->has_transparent_background = false;

  DrawFrame(renderer_.get(), viewport_size);
  EXPECT_TRUE(renderer_->stencil_enabled());
}

class ForbidSynchronousCallContext : public TestWebGraphicsContext3D {
 public:
  ForbidSynchronousCallContext() {}

  void getAttachedShaders(GLuint program,
                          GLsizei max_count,
                          GLsizei* count,
                          GLuint* shaders) override {
    ADD_FAILURE();
  }
  GLint getAttribLocation(GLuint program, const GLchar* name) override {
    ADD_FAILURE();
    return 0;
  }
  void getBooleanv(GLenum pname, GLboolean* value) override { ADD_FAILURE(); }
  void getBufferParameteriv(GLenum target,
                            GLenum pname,
                            GLint* value) override {
    ADD_FAILURE();
  }
  GLenum getError() override {
    ADD_FAILURE();
    return GL_NO_ERROR;
  }
  void getFloatv(GLenum pname, GLfloat* value) override { ADD_FAILURE(); }
  void getFramebufferAttachmentParameteriv(GLenum target,
                                           GLenum attachment,
                                           GLenum pname,
                                           GLint* value) override {
    ADD_FAILURE();
  }
  void getIntegerv(GLenum pname, GLint* value) override {
    if (pname == GL_MAX_TEXTURE_SIZE) {
      // MAX_TEXTURE_SIZE is cached client side, so it's OK to query.
      *value = 1024;
    } else {
      ADD_FAILURE();
    }
  }

  // We allow querying the shader compilation and program link status in debug
  // mode, but not release.
  void getProgramiv(GLuint program, GLenum pname, GLint* value) override {
#ifndef NDEBUG
    *value = 1;
#else
    ADD_FAILURE();
#endif
  }

  void getShaderiv(GLuint shader, GLenum pname, GLint* value) override {
#ifndef NDEBUG
    *value = 1;
#else
    ADD_FAILURE();
#endif
  }

  void getRenderbufferParameteriv(GLenum target,
                                  GLenum pname,
                                  GLint* value) override {
    ADD_FAILURE();
  }

  void getShaderPrecisionFormat(GLenum shadertype,
                                GLenum precisiontype,
                                GLint* range,
                                GLint* precision) override {
    ADD_FAILURE();
  }
  void getTexParameterfv(GLenum target, GLenum pname, GLfloat* value) override {
    ADD_FAILURE();
  }
  void getTexParameteriv(GLenum target, GLenum pname, GLint* value) override {
    ADD_FAILURE();
  }
  void getUniformfv(GLuint program, GLint location, GLfloat* value) override {
    ADD_FAILURE();
  }
  void getUniformiv(GLuint program, GLint location, GLint* value) override {
    ADD_FAILURE();
  }
  GLint getUniformLocation(GLuint program, const GLchar* name) override {
    ADD_FAILURE();
    return 0;
  }
  void getVertexAttribfv(GLuint index, GLenum pname, GLfloat* value) override {
    ADD_FAILURE();
  }
  void getVertexAttribiv(GLuint index, GLenum pname, GLint* value) override {
    ADD_FAILURE();
  }
  GLsizeiptr getVertexAttribOffset(GLuint index, GLenum pname) override {
    ADD_FAILURE();
    return 0;
  }
};
TEST_F(GLRendererTest, InitializationDoesNotMakeSynchronousCalls) {
  auto context = std::make_unique<ForbidSynchronousCallContext>();
  auto provider = TestContextProvider::Create(std::move(context));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager =
      std::make_unique<TestSharedBitmapManager>();
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), shared_bitmap_manager.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
}

class LoseContextOnFirstGetContext : public TestWebGraphicsContext3D {
 public:
  LoseContextOnFirstGetContext() {}

  void getProgramiv(GLuint program, GLenum pname, GLint* value) override {
    context_lost_ = true;
    *value = 0;
  }

  void getShaderiv(GLuint shader, GLenum pname, GLint* value) override {
    context_lost_ = true;
    *value = 0;
  }
};

TEST_F(GLRendererTest, InitializationWithQuicklyLostContextDoesNotAssert) {
  auto context = std::make_unique<LoseContextOnFirstGetContext>();
  auto provider = TestContextProvider::Create(std::move(context));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager =
      std::make_unique<TestSharedBitmapManager>();
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), shared_bitmap_manager.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
}

class ClearCountingContext : public TestWebGraphicsContext3D {
 public:
  ClearCountingContext() { test_capabilities_.discard_framebuffer = true; }

  MOCK_METHOD3(discardFramebufferEXT,
               void(GLenum target,
                    GLsizei numAttachments,
                    const GLenum* attachments));
  MOCK_METHOD1(clear, void(GLbitfield mask));
};

TEST_F(GLRendererTest, OpaqueBackground) {
  std::unique_ptr<ClearCountingContext> context_owned(new ClearCountingContext);
  ClearCountingContext* context = context_owned.get();

  auto provider = TestContextProvider::Create(std::move(context_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager =
      std::make_unique<TestSharedBitmapManager>();
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), shared_bitmap_manager.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(1, 1);
  RenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->has_transparent_background = false;

  // On DEBUG builds, render passes with opaque background clear to blue to
  // easily see regions that were not drawn on the screen.
  EXPECT_CALL(*context, discardFramebufferEXT(GL_FRAMEBUFFER, _, _))
      .With(Args<2, 1>(ElementsAre(GL_COLOR_EXT)))
      .Times(1);
#ifdef NDEBUG
  EXPECT_CALL(*context, clear(_)).Times(0);
#else
  EXPECT_CALL(*context, clear(_)).Times(1);
#endif
  DrawFrame(&renderer, viewport_size);
  Mock::VerifyAndClearExpectations(context);
}

TEST_F(GLRendererTest, TransparentBackground) {
  std::unique_ptr<ClearCountingContext> context_owned(new ClearCountingContext);
  ClearCountingContext* context = context_owned.get();

  auto provider = TestContextProvider::Create(std::move(context_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager =
      std::make_unique<TestSharedBitmapManager>();
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), shared_bitmap_manager.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(1, 1);
  RenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->has_transparent_background = true;

  EXPECT_CALL(*context, discardFramebufferEXT(GL_FRAMEBUFFER, 1, _)).Times(1);
  EXPECT_CALL(*context, clear(_)).Times(1);
  DrawFrame(&renderer, viewport_size);

  Mock::VerifyAndClearExpectations(context);
}

TEST_F(GLRendererTest, OffscreenOutputSurface) {
  std::unique_ptr<ClearCountingContext> context_owned(new ClearCountingContext);
  ClearCountingContext* context = context_owned.get();

  auto provider = TestContextProvider::Create(std::move(context_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::CreateOffscreen(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager =
      std::make_unique<TestSharedBitmapManager>();
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), shared_bitmap_manager.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(1, 1);
  cc::AddRenderPass(&render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
                    gfx::Transform(), cc::FilterOperations());

  EXPECT_CALL(*context, discardFramebufferEXT(GL_FRAMEBUFFER, _, _))
      .With(Args<2, 1>(ElementsAre(GL_COLOR_ATTACHMENT0)))
      .Times(1);
  EXPECT_CALL(*context, clear(_)).Times(AnyNumber());
  DrawFrame(&renderer, viewport_size);
  Mock::VerifyAndClearExpectations(context);
}

class TextureStateTrackingContext : public TestWebGraphicsContext3D {
 public:
  TextureStateTrackingContext() : active_texture_(GL_INVALID_ENUM) {
    test_capabilities_.egl_image_external = true;
  }

  MOCK_METHOD1(waitSyncToken, void(const GLbyte* sync_token));
  MOCK_METHOD3(texParameteri, void(GLenum target, GLenum pname, GLint param));
  MOCK_METHOD4(drawElements,
               void(GLenum mode, GLsizei count, GLenum type, GLintptr offset));

  virtual void activeTexture(GLenum texture) {
    EXPECT_NE(texture, active_texture_);
    active_texture_ = texture;
  }

  GLenum active_texture() const { return active_texture_; }

 private:
  GLenum active_texture_;
};

TEST_F(GLRendererTest, ActiveTextureState) {
  std::unique_ptr<TextureStateTrackingContext> context_owned(
      new TextureStateTrackingContext);
  TextureStateTrackingContext* context = context_owned.get();

  auto provider = TestContextProvider::Create(std::move(context_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager =
      std::make_unique<TestSharedBitmapManager>();
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), shared_bitmap_manager.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  // During initialization we are allowed to set any texture parameters.
  EXPECT_CALL(*context, texParameteri(_, _, _)).Times(AnyNumber());

  std::unique_ptr<TextureStateTrackingContext> child_context_owned(
      new TextureStateTrackingContext);

  auto child_context_provider =
      TestContextProvider::Create(std::move(child_context_owned));
  child_context_provider->BindToCurrentThread();
  auto child_resource_provider =
      cc::FakeResourceProvider::CreateLayerTreeResourceProvider(
          child_context_provider.get(), shared_bitmap_manager.get());

  RenderPass* root_pass =
      cc::AddRenderPass(&render_passes_in_draw_order_, 1, gfx::Rect(100, 100),
                        gfx::Transform(), cc::FilterOperations());
  gpu::SyncToken mailbox_sync_token;
  AddOneOfEveryQuadTypeInDisplayResourceProvider(
      root_pass, resource_provider.get(), child_resource_provider.get(), 0,
      &mailbox_sync_token);

  EXPECT_EQ(12u, resource_provider->num_resources());
  renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // Set up expected texture filter state transitions that match the quads
  // created in AppendOneOfEveryQuadType().
  Mock::VerifyAndClearExpectations(context);
  {
    InSequence sequence;
    // The verified flush flag will be set by
    // cc::LayerTreeResourceProvider::PrepareSendToParent. Before checking if
    // the gpu::SyncToken matches, set this flag first.
    mailbox_sync_token.SetVerifyFlush();
    // In AddOneOfEveryQuadTypeInDisplayResourceProvider, resources are added
    // into RenderPass with the below order: resource6, resource1, resource8
    // (with mailbox), resource2, resource3, resource4, resource9, resource10,
    // resource11, resource12. resource8 has its own mailbox mailbox_sync_token.
    // The rest resources share a common default sync token.
    EXPECT_CALL(*context, waitSyncToken(_)).Times(2);
    EXPECT_CALL(*context, waitSyncToken(MatchesSyncToken(mailbox_sync_token)))
        .Times(1);
    EXPECT_CALL(*context, waitSyncToken(_)).Times(7);

    // yuv_quad is drawn with the default linear filter.
    EXPECT_CALL(*context, drawElements(_, _, _, _));

    // tile_quad is drawn with GL_NEAREST because it is not transformed or
    // scaled.
    EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                        GL_NEAREST));
    EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                        GL_NEAREST));
    // The remaining quads also use GL_LINEAR because nearest neighbor
    // filtering is currently only used with tile quads.
    EXPECT_CALL(*context, drawElements(_, _, _, _)).Times(8);
  }

  gfx::Size viewport_size(100, 100);
  DrawFrame(&renderer, viewport_size);
  Mock::VerifyAndClearExpectations(context);
}

class NoClearRootRenderPassMockContext : public TestWebGraphicsContext3D {
 public:
  MOCK_METHOD1(clear, void(GLbitfield mask));
  MOCK_METHOD4(drawElements,
               void(GLenum mode, GLsizei count, GLenum type, GLintptr offset));
};

TEST_F(GLRendererTest, ShouldClearRootRenderPass) {
  std::unique_ptr<NoClearRootRenderPassMockContext> mock_context_owned(
      new NoClearRootRenderPassMockContext);
  NoClearRootRenderPassMockContext* mock_context = mock_context_owned.get();

  auto provider = TestContextProvider::Create(std::move(mock_context_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager =
      std::make_unique<TestSharedBitmapManager>();
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), shared_bitmap_manager.get());

  RendererSettings settings;
  settings.should_clear_root_render_pass = false;

  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(10, 10);

  int child_pass_id = 2;
  RenderPass* child_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, child_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(child_pass, gfx::Rect(viewport_size), SK_ColorBLUE);

  int root_pass_id = 1;
  RenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);

  cc::AddRenderPassQuad(root_pass, child_pass);

#ifdef NDEBUG
  GLint clear_bits = GL_COLOR_BUFFER_BIT;
#else
  GLint clear_bits = GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
#endif

  // First render pass is not the root one, clearing should happen.
  EXPECT_CALL(*mock_context, clear(clear_bits)).Times(AtLeast(1));

  Expectation first_render_pass =
      EXPECT_CALL(*mock_context, drawElements(_, _, _, _)).Times(1);

  // The second render pass is the root one, clearing should be prevented.
  EXPECT_CALL(*mock_context, clear(clear_bits))
      .Times(0)
      .After(first_render_pass);

  EXPECT_CALL(*mock_context, drawElements(_, _, _, _))
      .Times(AnyNumber())
      .After(first_render_pass);

  renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(&renderer, viewport_size);

  // In multiple render passes all but the root pass should clear the
  // framebuffer.
  Mock::VerifyAndClearExpectations(&mock_context);
}

class ScissorTestOnClearCheckingGLES2Interface : public TestGLES2Interface {
 public:
  ScissorTestOnClearCheckingGLES2Interface() = default;

  void Clear(GLbitfield) override { EXPECT_FALSE(scissor_enabled_); }

  void Enable(GLenum cap) override {
    if (cap == GL_SCISSOR_TEST)
      scissor_enabled_ = true;
  }

  void Disable(GLenum cap) override {
    if (cap == GL_SCISSOR_TEST)
      scissor_enabled_ = false;
  }

 private:
  bool scissor_enabled_ = false;
};

TEST_F(GLRendererTest, ScissorTestWhenClearing) {
  auto gl_owned = std::make_unique<ScissorTestOnClearCheckingGLES2Interface>();

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager =
      std::make_unique<TestSharedBitmapManager>();
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), shared_bitmap_manager.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  EXPECT_FALSE(renderer.use_partial_swap());
  renderer.SetVisible(true);

  gfx::Size viewport_size(100, 100);

  gfx::Rect grand_child_rect(25, 25);
  int grand_child_pass_id = 3;
  RenderPass* grand_child_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, grand_child_pass_id, grand_child_rect,
      gfx::Transform(), cc::FilterOperations());
  cc::AddClippedQuad(grand_child_pass, grand_child_rect, SK_ColorYELLOW);

  gfx::Rect child_rect(50, 50);
  int child_pass_id = 2;
  RenderPass* child_pass =
      cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                        child_rect, gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(child_pass, child_rect, SK_ColorBLUE);

  int root_pass_id = 1;
  RenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);

  cc::AddRenderPassQuad(root_pass, child_pass);
  cc::AddRenderPassQuad(child_pass, grand_child_pass);

  renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(&renderer, viewport_size);
}

class DiscardCheckingGLES2Interface : public TestGLES2Interface {
 public:
  DiscardCheckingGLES2Interface() = default;

  void InitializeTestContext(TestWebGraphicsContext3D* context) override {
    context->set_have_post_sub_buffer(true);
    context->set_have_discard_framebuffer(true);
  }

  void DiscardFramebufferEXT(GLenum target,
                             GLsizei numAttachments,
                             const GLenum* attachments) override {
    ++discarded_;
  }

  int discarded() const { return discarded_; }
  void reset_discarded() { discarded_ = 0; }

 private:
  int discarded_ = 0;
};

TEST_F(GLRendererTest, NoDiscardOnPartialUpdates) {
  auto gl_owned = std::make_unique<DiscardCheckingGLES2Interface>();
  auto* gl = gl_owned.get();

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  auto output_surface = FakeOutputSurface::Create3d(std::move(provider));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager =
      std::make_unique<TestSharedBitmapManager>();
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), shared_bitmap_manager.get());

  RendererSettings settings;
  settings.partial_swap_enabled = true;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  EXPECT_TRUE(renderer.use_partial_swap());
  renderer.SetVisible(true);

  gfx::Size viewport_size(100, 100);

  {
    // Partial frame, should not discard.
    int root_pass_id = 1;
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);
    root_pass->damage_rect = gfx::Rect(2, 2, 3, 3);

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);
    EXPECT_EQ(0, gl->discarded());
    gl->reset_discarded();
  }
  {
    // Full frame, should discard.
    int root_pass_id = 1;
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);
    root_pass->damage_rect = root_pass->output_rect;

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);
    EXPECT_EQ(1, gl->discarded());
    gl->reset_discarded();
  }
  {
    // Full frame, external scissor is set, should not discard.
    output_surface->set_has_external_stencil_test(true);
    int root_pass_id = 1;
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);
    root_pass->damage_rect = root_pass->output_rect;
    root_pass->has_transparent_background = false;

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);
    EXPECT_EQ(0, gl->discarded());
    gl->reset_discarded();
    output_surface->set_has_external_stencil_test(false);
  }
}

class ResourceTrackingGLES2Interface : public TestGLES2Interface {
 public:
  ResourceTrackingGLES2Interface() = default;
  ~ResourceTrackingGLES2Interface() override { CheckNoResources(); }

  void CheckNoResources() {
    EXPECT_TRUE(textures_.empty());
    EXPECT_TRUE(buffers_.empty());
    EXPECT_TRUE(framebuffers_.empty());
    EXPECT_TRUE(renderbuffers_.empty());
    EXPECT_TRUE(queries_.empty());
    EXPECT_TRUE(shaders_.empty());
    EXPECT_TRUE(programs_.empty());
  }

  void GenTextures(GLsizei n, GLuint* textures) override {
    GenIds(&textures_, n, textures);
  }

  void GenBuffers(GLsizei n, GLuint* buffers) override {
    GenIds(&buffers_, n, buffers);
  }

  void GenFramebuffers(GLsizei n, GLuint* framebuffers) override {
    GenIds(&framebuffers_, n, framebuffers);
  }

  void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) override {
    GenIds(&renderbuffers_, n, renderbuffers);
  }

  void GenQueriesEXT(GLsizei n, GLuint* queries) override {
    GenIds(&queries_, n, queries);
  }

  GLuint CreateProgram() override { return GenId(&programs_); }

  GLuint CreateShader(GLenum type) override { return GenId(&shaders_); }

  void BindTexture(GLenum target, GLuint texture) override {
    CheckId(&textures_, texture);
  }

  void BindBuffer(GLenum target, GLuint buffer) override {
    CheckId(&buffers_, buffer);
  }

  void BindRenderbuffer(GLenum target, GLuint renderbuffer) override {
    CheckId(&renderbuffers_, renderbuffer);
  }

  void BindFramebuffer(GLenum target, GLuint framebuffer) override {
    CheckId(&framebuffers_, framebuffer);
  }

  void UseProgram(GLuint program) override { CheckId(&programs_, program); }

  void DeleteTextures(GLsizei n, const GLuint* textures) override {
    DeleteIds(&textures_, n, textures);
  }

  void DeleteBuffers(GLsizei n, const GLuint* buffers) override {
    DeleteIds(&buffers_, n, buffers);
  }

  void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) override {
    DeleteIds(&framebuffers_, n, framebuffers);
  }

  void DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) override {
    DeleteIds(&renderbuffers_, n, renderbuffers);
  }

  void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override {
    DeleteIds(&queries_, n, queries);
  }

  void DeleteProgram(GLuint program) override { DeleteId(&programs_, program); }

  void DeleteShader(GLuint shader) override { DeleteId(&shaders_, shader); }

  void BufferData(GLenum target,
                  GLsizeiptr size,
                  const void* data,
                  GLenum usage) override {}

 private:
  GLuint GenId(std::set<GLuint>* resource_set) {
    GLuint id = next_id_++;
    resource_set->insert(id);
    return id;
  }

  void GenIds(std::set<GLuint>* resource_set, GLsizei n, GLuint* ids) {
    for (GLsizei i = 0; i < n; ++i)
      ids[i] = GenId(resource_set);
  }

  void CheckId(std::set<GLuint>* resource_set, GLuint id) {
    if (id == 0)
      return;
    EXPECT_TRUE(resource_set->find(id) != resource_set->end());
  }

  void DeleteId(std::set<GLuint>* resource_set, GLuint id) {
    if (id == 0)
      return;
    size_t num_erased = resource_set->erase(id);
    EXPECT_EQ(1u, num_erased);
  }

  void DeleteIds(std::set<GLuint>* resource_set, GLsizei n, const GLuint* ids) {
    for (GLsizei i = 0; i < n; ++i)
      DeleteId(resource_set, ids[i]);
  }

  GLuint next_id_ = 1;
  std::set<GLuint> textures_;
  std::set<GLuint> buffers_;
  std::set<GLuint> framebuffers_;
  std::set<GLuint> renderbuffers_;
  std::set<GLuint> queries_;
  std::set<GLuint> shaders_;
  std::set<GLuint> programs_;
};

TEST_F(GLRendererTest, NoResourceLeak) {
  auto gl_owned = std::make_unique<ResourceTrackingGLES2Interface>();
  auto* gl = gl_owned.get();

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  auto output_surface = FakeOutputSurface::Create3d(std::move(provider));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager =
      std::make_unique<TestSharedBitmapManager>();
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), shared_bitmap_manager.get());

  {
    RendererSettings settings;
    FakeRendererGL renderer(&settings, output_surface.get(),
                            resource_provider.get());
    renderer.Initialize();
    renderer.SetVisible(true);

    gfx::Size viewport_size(100, 100);

    int root_pass_id = 1;
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);
    root_pass->damage_rect = gfx::Rect(2, 2, 3, 3);

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);
  }
  gl->CheckNoResources();
}

class DrawElementsGLES2Interface : public TestGLES2Interface {
 public:
  void InitializeTestContext(TestWebGraphicsContext3D* context) override {
    context->set_have_post_sub_buffer(true);
  }

  MOCK_METHOD4(
      DrawElements,
      void(GLenum mode, GLsizei count, GLenum type, const void* indices));
};

class GLRendererSkipTest : public GLRendererTest {
 protected:
  GLRendererSkipTest() {
    auto gl_owned = std::make_unique<StrictMock<DrawElementsGLES2Interface>>();
    gl_ = gl_owned.get();

    auto provider = TestContextProvider::Create(std::move(gl_owned));
    provider->BindToCurrentThread();

    output_surface_ = FakeOutputSurface::Create3d(std::move(provider));
    output_surface_->BindToClient(&output_surface_client_);

    shared_bitmap_manager_ = std::make_unique<TestSharedBitmapManager>();
    resource_provider_ =
        cc::FakeResourceProvider::CreateDisplayResourceProvider(
            output_surface_->context_provider(), shared_bitmap_manager_.get());
    settings_.partial_swap_enabled = true;
    renderer_ = std::make_unique<FakeRendererGL>(
        &settings_, output_surface_.get(), resource_provider_.get());
    renderer_->Initialize();
    renderer_->SetVisible(true);
  }

  StrictMock<DrawElementsGLES2Interface>* gl_;
  RendererSettings settings_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider_;
  std::unique_ptr<FakeRendererGL> renderer_;
};

TEST_F(GLRendererSkipTest, DrawQuad) {
  EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(1);

  gfx::Size viewport_size(100, 100);
  gfx::Rect quad_rect = gfx::Rect(20, 20, 20, 20);

  int root_pass_id = 1;
  RenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = gfx::Rect(0, 0, 25, 25);
  cc::AddQuad(root_pass, quad_rect, SK_ColorGREEN);

  renderer_->DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(renderer_.get(), viewport_size);
}

TEST_F(GLRendererSkipTest, SkipVisibleRect) {
  gfx::Size viewport_size(100, 100);
  gfx::Rect quad_rect = gfx::Rect(0, 0, 40, 40);

  int root_pass_id = 1;
  RenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = gfx::Rect(0, 0, 10, 10);
  cc::AddQuad(root_pass, quad_rect, SK_ColorGREEN);
  root_pass->shared_quad_state_list.front()->is_clipped = true;
  root_pass->shared_quad_state_list.front()->clip_rect =
      gfx::Rect(0, 0, 40, 40);
  root_pass->quad_list.front()->visible_rect = gfx::Rect(20, 20, 20, 20);

  renderer_->DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(renderer_.get(), viewport_size);
  // DrawElements should not be called because the visible rect is outside the
  // scissor, even though the clip rect and quad rect intersect the scissor.
}

TEST_F(GLRendererSkipTest, SkipClippedQuads) {
  gfx::Size viewport_size(100, 100);
  gfx::Rect quad_rect = gfx::Rect(25, 25, 90, 90);

  int root_pass_id = 1;
  RenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = gfx::Rect(0, 0, 25, 25);
  cc::AddClippedQuad(root_pass, quad_rect, SK_ColorGREEN);
  root_pass->quad_list.front()->rect = gfx::Rect(20, 20, 20, 20);

  renderer_->DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(renderer_.get(), viewport_size);
  // DrawElements should not be called because the clip rect is outside the
  // scissor.
}

TEST_F(GLRendererTest, DrawFramePreservesFramebuffer) {
  // When using render-to-FBO to display the surface, all rendering is done
  // to a non-zero FBO. Make sure that the framebuffer is always restored to
  // the correct framebuffer during rendering, if changed.
  // Note: there is one path that will set it to 0, but that is after the render
  // has finished.
  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::Create3d());
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager =
      std::make_unique<TestSharedBitmapManager>();
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), shared_bitmap_manager.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  EXPECT_FALSE(renderer.use_partial_swap());
  renderer.SetVisible(true);

  gfx::Size viewport_size(100, 100);
  gfx::Rect quad_rect = gfx::Rect(20, 20, 20, 20);

  int root_pass_id = 1;
  RenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  cc::AddClippedQuad(root_pass, quad_rect, SK_ColorGREEN);

  unsigned fbo;
  gpu::gles2::GLES2Interface* gl =
      output_surface->context_provider()->ContextGL();
  gl->GenFramebuffers(1, &fbo);
  output_surface->set_framebuffer(fbo, GL_RGB);

  renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(&renderer, viewport_size);

  int bound_fbo;
  gl->GetIntegerv(GL_FRAMEBUFFER_BINDING, &bound_fbo);
  EXPECT_EQ(static_cast<int>(fbo), bound_fbo);
}

TEST_F(GLRendererShaderTest, DrawRenderPassQuadShaderPermutations) {
  gfx::Size viewport_size(60, 75);

  gfx::Rect child_rect(50, 50);
  int child_pass_id = 2;
  RenderPass* child_pass;

  int root_pass_id = 1;
  RenderPass* root_pass;

  ResourceId mask = child_resource_provider_->CreateGpuTextureResource(
      gfx::Size(20, 12), ResourceTextureHint::kDefault,
      child_resource_provider_->best_texture_format(), gfx::ColorSpace());
  child_resource_provider_->AllocateForTesting(mask);

  // Return the mapped resource id.
  cc::ResourceProvider::ResourceIdMap resource_map =
      SendResourceAndGetChildToParentMap({mask}, resource_provider_.get(),
                                         child_resource_provider_.get());
  ResourceId mapped_mask = resource_map[mask];

  SkScalar matrix[20];
  float amount = 0.5f;
  matrix[0] = 0.213f + 0.787f * amount;
  matrix[1] = 0.715f - 0.715f * amount;
  matrix[2] = 1.f - (matrix[0] + matrix[1]);
  matrix[3] = matrix[4] = 0;
  matrix[5] = 0.213f - 0.213f * amount;
  matrix[6] = 0.715f + 0.285f * amount;
  matrix[7] = 1.f - (matrix[5] + matrix[6]);
  matrix[8] = matrix[9] = 0;
  matrix[10] = 0.213f - 0.213f * amount;
  matrix[11] = 0.715f - 0.715f * amount;
  matrix[12] = 1.f - (matrix[10] + matrix[11]);
  matrix[13] = matrix[14] = 0;
  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
  matrix[18] = 1;
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateReferenceFilter(
      sk_make_sp<cc::ColorFilterPaintFilter>(
          SkColorFilter::MakeMatrixFilterRowMajor255(matrix), nullptr)));

  gfx::Transform transform_causing_aa;
  transform_causing_aa.Rotate(20.0);

  for (int i = 0; i <= LAST_BLEND_MODE; ++i) {
    BlendMode blend_mode = static_cast<BlendMode>(i);
    SkBlendMode xfer_mode = BlendModeToSkXfermode(blend_mode);
    settings_.force_blending_with_shaders = (blend_mode != BLEND_MODE_NONE);
    // RenderPassProgram
    render_passes_in_draw_order_.clear();
    child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          child_rect, gfx::Transform(), cc::FilterOperations());

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassProgram(TEX_COORD_PRECISION_MEDIUM, blend_mode);

    // RenderPassColorMatrixProgram
    render_passes_in_draw_order_.clear();

    child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                                   child_rect, transform_causing_aa, filters);

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassColorMatrixProgram(TEX_COORD_PRECISION_MEDIUM, blend_mode);

    // RenderPassMaskProgram
    render_passes_in_draw_order_.clear();

    child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          child_rect, gfx::Transform(), cc::FilterOperations());

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, mapped_mask, gfx::Transform(),
                          xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassMaskProgram(TEX_COORD_PRECISION_MEDIUM, SAMPLER_TYPE_2D,
                              blend_mode);

    // RenderPassMaskColorMatrixProgram
    render_passes_in_draw_order_.clear();

    child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                                   child_rect, gfx::Transform(), filters);

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, mapped_mask, gfx::Transform(),
                          xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassMaskColorMatrixProgram(TEX_COORD_PRECISION_MEDIUM,
                                         SAMPLER_TYPE_2D, blend_mode);

    // RenderPassProgramAA
    render_passes_in_draw_order_.clear();

    child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                                   child_rect, transform_causing_aa,
                                   cc::FilterOperations());

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, 0, transform_causing_aa,
                          xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassProgramAA(TEX_COORD_PRECISION_MEDIUM, blend_mode);

    // RenderPassColorMatrixProgramAA
    render_passes_in_draw_order_.clear();

    child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                                   child_rect, transform_causing_aa, filters);

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, 0, transform_causing_aa,
                          xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassColorMatrixProgramAA(TEX_COORD_PRECISION_MEDIUM, blend_mode);

    // RenderPassMaskProgramAA
    render_passes_in_draw_order_.clear();

    child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                                   child_rect, transform_causing_aa,
                                   cc::FilterOperations());

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, mapped_mask,
                          transform_causing_aa, xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassMaskProgramAA(TEX_COORD_PRECISION_MEDIUM, SAMPLER_TYPE_2D,
                                blend_mode);

    // RenderPassMaskColorMatrixProgramAA
    render_passes_in_draw_order_.clear();

    child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                                   child_rect, transform_causing_aa, filters);

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size),
                                  transform_causing_aa, cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, mapped_mask,
                          transform_causing_aa, xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassMaskColorMatrixProgramAA(TEX_COORD_PRECISION_MEDIUM,
                                           SAMPLER_TYPE_2D, blend_mode);
  }
}

// At this time, the AA code path cannot be taken if the surface's rect would
// project incorrectly by the given transform, because of w<0 clipping.
TEST_F(GLRendererShaderTest, DrawRenderPassQuadSkipsAAForClippingTransform) {
  gfx::Rect child_rect(50, 50);
  int child_pass_id = 2;
  RenderPass* child_pass;

  gfx::Size viewport_size(100, 100);
  int root_pass_id = 1;
  RenderPass* root_pass;

  gfx::Transform transform_preventing_aa;
  transform_preventing_aa.ApplyPerspectiveDepth(40.0);
  transform_preventing_aa.RotateAboutYAxis(-20.0);
  transform_preventing_aa.Scale(30.0, 1.0);

  // Verify that the test transform and test rect actually do cause the clipped
  // flag to trigger. Otherwise we are not testing the intended scenario.
  bool clipped = false;
  cc::MathUtil::MapQuad(transform_preventing_aa,
                        gfx::QuadF(gfx::RectF(child_rect)), &clipped);
  ASSERT_TRUE(clipped);

  child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                                 child_rect, transform_preventing_aa,
                                 cc::FilterOperations());

  root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                gfx::Rect(viewport_size), gfx::Transform(),
                                cc::FilterOperations());

  cc::AddRenderPassQuad(root_pass, child_pass, 0, transform_preventing_aa,
                        SkBlendMode::kSrcOver);

  renderer_->DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(renderer_.get(), viewport_size);

  // If use_aa incorrectly ignores clipping, it will use the
  // RenderPassProgramAA shader instead of the RenderPassProgram.
  TestRenderPassProgram(TEX_COORD_PRECISION_MEDIUM, BLEND_MODE_NONE);
}

TEST_F(GLRendererShaderTest, DrawSolidColorShader) {
  gfx::Size viewport_size(30, 30);  // Don't translate out of the viewport.
  gfx::Size quad_size(3, 3);
  int root_pass_id = 1;
  RenderPass* root_pass;

  gfx::Transform pixel_aligned_transform_causing_aa;
  pixel_aligned_transform_causing_aa.Translate(25.5f, 25.5f);
  pixel_aligned_transform_causing_aa.Scale(0.5f, 0.5f);

  root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                gfx::Rect(viewport_size), gfx::Transform(),
                                cc::FilterOperations());
  cc::AddTransformedQuad(root_pass, gfx::Rect(quad_size), SK_ColorYELLOW,
                         pixel_aligned_transform_causing_aa);

  renderer_->DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(renderer_.get(), viewport_size);

  TestSolidColorProgramAA();
}

class OutputSurfaceMockContext : public TestWebGraphicsContext3D {
 public:
  OutputSurfaceMockContext() { test_capabilities_.post_sub_buffer = true; }

  // Specifically override methods even if they are unused (used in conjunction
  // with StrictMock). We need to make sure that GLRenderer does not issue
  // framebuffer-related GLuint calls directly. Instead these are supposed to go
  // through the OutputSurface abstraction.
  MOCK_METHOD2(bindFramebuffer, void(GLenum target, GLuint framebuffer));
  MOCK_METHOD3(reshapeWithScaleFactor,
               void(int width, int height, float scale_factor));
  MOCK_METHOD4(drawElements,
               void(GLenum mode, GLsizei count, GLenum type, GLintptr offset));
};

class MockOutputSurface : public OutputSurface {
 public:
  explicit MockOutputSurface(scoped_refptr<ContextProvider> provider)
      : OutputSurface(std::move(provider)) {}
  virtual ~MockOutputSurface() {}

  void BindToClient(OutputSurfaceClient*) override {}

  MOCK_METHOD0(EnsureBackbuffer, void());
  MOCK_METHOD0(DiscardBackbuffer, void());
  MOCK_METHOD5(Reshape,
               void(const gfx::Size& size,
                    float scale_factor,
                    const gfx::ColorSpace& color_space,
                    bool has_alpha,
                    bool use_stencil));
  MOCK_METHOD0(BindFramebuffer, void());
  MOCK_METHOD1(SetDrawRectangle, void(const gfx::Rect&));
  MOCK_METHOD0(GetFramebufferCopyTextureFormat, GLenum());
  MOCK_METHOD1(SwapBuffers_, void(OutputSurfaceFrame& frame));  // NOLINT
  void SwapBuffers(OutputSurfaceFrame frame) override { SwapBuffers_(frame); }
  MOCK_CONST_METHOD0(GetOverlayCandidateValidator,
                     OverlayCandidateValidator*());
  MOCK_CONST_METHOD0(IsDisplayedAsOverlayPlane, bool());
  MOCK_CONST_METHOD0(GetOverlayTextureId, unsigned());
  MOCK_CONST_METHOD0(GetOverlayBufferFormat, gfx::BufferFormat());
  MOCK_CONST_METHOD0(SurfaceIsSuspendForRecycle, bool());
  MOCK_CONST_METHOD0(HasExternalStencilTest, bool());
  MOCK_METHOD0(ApplyExternalStencil, void());
};

class MockOutputSurfaceTest : public GLRendererTest {
 protected:
  void SetUp() override {
    auto context = std::make_unique<StrictMock<OutputSurfaceMockContext>>();
    context_ = context.get();
    auto provider = TestContextProvider::Create(std::move(context));
    provider->BindToCurrentThread();
    output_surface_ =
        std::make_unique<StrictMock<MockOutputSurface>>(std::move(provider));

    cc::FakeOutputSurfaceClient output_surface_client_;
    output_surface_->BindToClient(&output_surface_client_);

    shared_bitmap_manager_ = std::make_unique<TestSharedBitmapManager>();
    resource_provider_ =
        cc::FakeResourceProvider::CreateDisplayResourceProvider(
            output_surface_->context_provider(), shared_bitmap_manager_.get());

    renderer_.reset(new FakeRendererGL(&settings_, output_surface_.get(),
                                       resource_provider_.get()));
    EXPECT_CALL(*output_surface_, GetOverlayCandidateValidator()).Times(1);
    renderer_->Initialize();

    EXPECT_CALL(*output_surface_, EnsureBackbuffer()).Times(1);
    renderer_->SetVisible(true);
    Mock::VerifyAndClearExpectations(output_surface_.get());
  }

  void SwapBuffers() { renderer_->SwapBuffers(std::vector<ui::LatencyInfo>()); }

  void DrawFrame(float device_scale_factor,
                 const gfx::Size& viewport_size,
                 bool transparent) {
    int render_pass_id = 1;
    RenderPass* render_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, render_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(render_pass, gfx::Rect(viewport_size), SK_ColorGREEN);
    render_pass->has_transparent_background = transparent;

    EXPECT_CALL(*output_surface_, EnsureBackbuffer()).WillRepeatedly(Return());

    EXPECT_CALL(*output_surface_,
                Reshape(viewport_size, device_scale_factor, _, transparent, _))
        .Times(1);

    EXPECT_CALL(*output_surface_, BindFramebuffer()).Times(1);

    EXPECT_CALL(*context_, drawElements(_, _, _, _)).Times(1);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    renderer_->DrawFrame(&render_passes_in_draw_order_, device_scale_factor,
                         viewport_size);
  }

  RendererSettings settings_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  OutputSurfaceMockContext* context_ = nullptr;
  std::unique_ptr<StrictMock<MockOutputSurface>> output_surface_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider_;
  std::unique_ptr<FakeRendererGL> renderer_;
};

TEST_F(MockOutputSurfaceTest, BackbufferDiscard) {
  // Drop backbuffer on hide.
  EXPECT_CALL(*output_surface_, DiscardBackbuffer()).Times(1);
  renderer_->SetVisible(false);
  Mock::VerifyAndClearExpectations(output_surface_.get());

  // Restore backbuffer on show.
  EXPECT_CALL(*output_surface_, EnsureBackbuffer()).Times(1);
  renderer_->SetVisible(true);
  Mock::VerifyAndClearExpectations(output_surface_.get());
}

class TestOverlayProcessor : public OverlayProcessor {
 public:
  class Strategy : public OverlayProcessor::Strategy {
   public:
    Strategy() = default;
    ~Strategy() override = default;

    MOCK_METHOD5(Attempt,
                 bool(const SkMatrix44& output_color_matrix,
                      cc::DisplayResourceProvider* resource_provider,
                      RenderPass* render_pass,
                      cc::OverlayCandidateList* candidates,
                      std::vector<gfx::Rect>* content_bounds));
  };

  class Validator : public OverlayCandidateValidator {
   public:
    void GetStrategies(OverlayProcessor::StrategyList* strategies) override {}

    // Returns true if draw quads can be represented as CALayers (Mac only).
    MOCK_METHOD0(AllowCALayerOverlays, bool());
    MOCK_METHOD0(AllowDCLayerOverlays, bool());

    // A list of possible overlay candidates is presented to this function.
    // The expected result is that those candidates that can be in a separate
    // plane are marked with |overlay_handled| set to true, otherwise they are
    // to be traditionally composited. Candidates with |overlay_handled| set to
    // true must also have their |display_rect| converted to integer
    // coordinates if necessary.
    void CheckOverlaySupport(cc::OverlayCandidateList* surfaces) {}
  };

  explicit TestOverlayProcessor(OutputSurface* surface)
      : OverlayProcessor(surface) {}
  ~TestOverlayProcessor() override = default;

  void Initialize() override {
    strategies_.push_back(std::make_unique<Strategy>());
  }

  Strategy& strategy() { return static_cast<Strategy&>(*strategies_.back()); }
};

void MailboxReleased(const gpu::SyncToken& sync_token, bool lost_resource) {}

static void CollectResources(std::vector<ReturnedResource>* array,
                             const std::vector<ReturnedResource>& returned) {
  array->insert(array->end(), returned.begin(), returned.end());
}

TEST_F(GLRendererTest, DontOverlayWithCopyRequests) {
  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::Create3d());
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager =
      std::make_unique<TestSharedBitmapManager>();
  auto parent_resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), shared_bitmap_manager.get());

  auto child_context_provider = TestContextProvider::Create();
  child_context_provider->BindToCurrentThread();
  auto child_resource_provider =
      cc::FakeResourceProvider::CreateLayerTreeResourceProvider(
          child_context_provider.get(), shared_bitmap_manager.get());

  auto transfer_resource = TransferableResource::MakeGLOverlay(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken(),
      gfx::Size(256, 256), true);
  auto release_callback =
      SingleReleaseCallback::Create(base::Bind(&MailboxReleased));
  ResourceId resource_id = child_resource_provider->ImportResource(
      transfer_resource, std::move(release_callback));

  std::vector<ReturnedResource> returned_to_child;
  int child_id = parent_resource_provider->CreateChild(
      base::Bind(&CollectResources, &returned_to_child));

  // Transfer resource to the parent.
  cc::ResourceProvider::ResourceIdArray resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource_id);
  std::vector<TransferableResource> list;
  child_resource_provider->PrepareSendToParent(resource_ids_to_transfer, &list);
  parent_resource_provider->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  cc::ResourceProvider::ResourceIdMap resource_map =
      parent_resource_provider->GetChildToParentMap(child_id);
  ResourceId parent_resource_id = resource_map[list[0].id];

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          parent_resource_provider.get(),
                          base::ThreadTaskRunnerHandle::Get());
  renderer.Initialize();
  renderer.SetVisible(true);

  TestOverlayProcessor* processor =
      new TestOverlayProcessor(output_surface.get());
  processor->Initialize();
  renderer.SetOverlayProcessor(processor);
  std::unique_ptr<TestOverlayProcessor::Validator> validator(
      new TestOverlayProcessor::Validator);
  output_surface->SetOverlayCandidateValidator(validator.get());

  gfx::Size viewport_size(1, 1);
  RenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->has_transparent_background = false;
  root_pass->copy_requests.push_back(CopyOutputRequest::CreateStubForTesting());

  bool needs_blending = false;
  bool premultiplied_alpha = false;
  bool flipped = false;
  bool nearest_neighbor = false;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  TextureDrawQuad* overlay_quad =
      root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(
      root_pass->CreateAndAppendSharedQuadState(), gfx::Rect(viewport_size),
      gfx::Rect(viewport_size), needs_blending, parent_resource_id,
      premultiplied_alpha, gfx::PointF(0, 0), gfx::PointF(1, 1),
      SK_ColorTRANSPARENT, vertex_opacity, flipped, nearest_neighbor, false);

  // DirectRenderer::DrawFrame calls into OverlayProcessor::ProcessForOverlays.
  // Attempt will be called for each strategy in OverlayProcessor. We have
  // added a fake strategy, so checking for Attempt calls checks if there was
  // any attempt to overlay, which there shouldn't be. We can't use the quad
  // list because the render pass is cleaned up by DrawFrame.
  EXPECT_CALL(processor->strategy(), Attempt(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*validator, AllowCALayerOverlays()).Times(0);
  EXPECT_CALL(*validator, AllowDCLayerOverlays()).Times(0);
  DrawFrame(&renderer, viewport_size);
  Mock::VerifyAndClearExpectations(&processor->strategy());
  Mock::VerifyAndClearExpectations(validator.get());

  // Without a copy request Attempt() should be called once.
  root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, 1,
                                gfx::Rect(viewport_size), gfx::Transform(),
                                cc::FilterOperations());
  root_pass->has_transparent_background = false;

  overlay_quad = root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(
      root_pass->CreateAndAppendSharedQuadState(), gfx::Rect(viewport_size),
      gfx::Rect(viewport_size), needs_blending, parent_resource_id,
      premultiplied_alpha, gfx::PointF(0, 0), gfx::PointF(1, 1),
      SK_ColorTRANSPARENT, vertex_opacity, flipped, nearest_neighbor, false);
  EXPECT_CALL(*validator, AllowCALayerOverlays())
      .Times(1)
      .WillOnce(::testing::Return(false));
  EXPECT_CALL(*validator, AllowDCLayerOverlays())
      .Times(1)
      .WillOnce(::testing::Return(false));
  EXPECT_CALL(processor->strategy(), Attempt(_, _, _, _, _)).Times(1);
  DrawFrame(&renderer, viewport_size);

  // If the CALayerOverlay path is taken, then the ordinary overlay path should
  // not be called.
  root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, 1,
                                gfx::Rect(viewport_size), gfx::Transform(),
                                cc::FilterOperations());
  root_pass->has_transparent_background = false;

  overlay_quad = root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(
      root_pass->CreateAndAppendSharedQuadState(), gfx::Rect(viewport_size),
      gfx::Rect(viewport_size), needs_blending, parent_resource_id,
      premultiplied_alpha, gfx::PointF(0, 0), gfx::PointF(1, 1),
      SK_ColorTRANSPARENT, vertex_opacity, flipped, nearest_neighbor, false);
  EXPECT_CALL(*validator, AllowCALayerOverlays())
      .Times(1)
      .WillOnce(::testing::Return(true));
  EXPECT_CALL(processor->strategy(), Attempt(_, _, _, _, _)).Times(0);
  DrawFrame(&renderer, viewport_size);

  // Transfer resources back from the parent to the child. Set no resources as
  // being in use.
  parent_resource_provider->DeclareUsedResourcesFromChild(child_id,
                                                          ResourceIdSet());
}

class SingleOverlayOnTopProcessor : public OverlayProcessor {
 public:
  class SingleOverlayValidator : public OverlayCandidateValidator {
   public:
    void GetStrategies(OverlayProcessor::StrategyList* strategies) override {
      strategies->push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
      strategies->push_back(std::make_unique<OverlayStrategyUnderlay>(this));
    }

    bool AllowCALayerOverlays() override { return false; }
    bool AllowDCLayerOverlays() override { return false; }

    void CheckOverlaySupport(cc::OverlayCandidateList* surfaces) override {
      ASSERT_EQ(1U, surfaces->size());
      cc::OverlayCandidate& candidate = surfaces->back();
      candidate.overlay_handled = true;
    }
  };

  explicit SingleOverlayOnTopProcessor(OutputSurface* surface)
      : OverlayProcessor(surface) {}

  void Initialize() override {
    strategies_.push_back(
        std::make_unique<OverlayStrategySingleOnTop>(&validator_));
  }

  SingleOverlayValidator validator_;
};

class WaitSyncTokenCountingContext : public TestWebGraphicsContext3D {
 public:
  MOCK_METHOD1(waitSyncToken, void(const GLbyte* sync_token));
};

class MockOverlayScheduler {
 public:
  MOCK_METHOD5(Schedule,
               void(int plane_z_order,
                    gfx::OverlayTransform plane_transform,
                    unsigned overlay_texture_id,
                    const gfx::Rect& display_bounds,
                    const gfx::RectF& uv_rect));
};

TEST_F(GLRendererTest, OverlaySyncTokensAreProcessed) {
  std::unique_ptr<WaitSyncTokenCountingContext> context_owned(
      new WaitSyncTokenCountingContext);
  WaitSyncTokenCountingContext* context = context_owned.get();

  auto provider = TestContextProvider::Create(std::move(context_owned));
  provider->BindToCurrentThread();

  MockOverlayScheduler overlay_scheduler;
  provider->support()->SetScheduleOverlayPlaneCallback(base::Bind(
      &MockOverlayScheduler::Schedule, base::Unretained(&overlay_scheduler)));

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager =
      std::make_unique<TestSharedBitmapManager>();
  auto parent_resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), shared_bitmap_manager.get());

  auto child_context_provider = TestContextProvider::Create();
  child_context_provider->BindToCurrentThread();
  auto child_resource_provider =
      cc::FakeResourceProvider::CreateLayerTreeResourceProvider(
          child_context_provider.get(), shared_bitmap_manager.get());

  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            gpu::CommandBufferId::FromUnsafeValue(0x123), 29);
  auto transfer_resource = TransferableResource::MakeGLOverlay(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, sync_token,
      gfx::Size(256, 256), true);
  auto release_callback =
      SingleReleaseCallback::Create(base::Bind(&MailboxReleased));
  ResourceId resource_id = child_resource_provider->ImportResource(
      transfer_resource, std::move(release_callback));

  std::vector<ReturnedResource> returned_to_child;
  int child_id = parent_resource_provider->CreateChild(
      base::Bind(&CollectResources, &returned_to_child));

  // Transfer resource to the parent.
  cc::ResourceProvider::ResourceIdArray resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource_id);
  std::vector<TransferableResource> list;
  child_resource_provider->PrepareSendToParent(resource_ids_to_transfer, &list);
  parent_resource_provider->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  cc::ResourceProvider::ResourceIdMap resource_map =
      parent_resource_provider->GetChildToParentMap(child_id);
  ResourceId parent_resource_id = resource_map[list[0].id];

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          parent_resource_provider.get(),
                          base::ThreadTaskRunnerHandle::Get());
  renderer.Initialize();
  renderer.SetVisible(true);

  SingleOverlayOnTopProcessor* processor =
      new SingleOverlayOnTopProcessor(output_surface.get());
  processor->Initialize();
  renderer.SetOverlayProcessor(processor);

  gfx::Size viewport_size(1, 1);
  RenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->has_transparent_background = false;

  bool needs_blending = false;
  bool premultiplied_alpha = false;
  bool flipped = false;
  bool nearest_neighbor = false;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  gfx::PointF uv_top_left(0, 0);
  gfx::PointF uv_bottom_right(1, 1);

  TextureDrawQuad* overlay_quad =
      root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  SharedQuadState* shared_state = root_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), gfx::Rect(viewport_size),
                       gfx::Rect(viewport_size), gfx::Rect(viewport_size),
                       false, false, 1, SkBlendMode::kSrcOver, 0);
  overlay_quad->SetNew(shared_state, gfx::Rect(viewport_size),
                       gfx::Rect(viewport_size), needs_blending,
                       parent_resource_id, premultiplied_alpha, uv_top_left,
                       uv_bottom_right, SK_ColorTRANSPARENT, vertex_opacity,
                       flipped, nearest_neighbor, false);

  // The verified flush flag will be set by
  // cc::LayerTreeResourceProvider::PrepareSendToParent. Before checking if the
  // gpu::SyncToken matches, set this flag first.
  sync_token.SetVerifyFlush();

  // Verify that overlay_quad actually gets turned into an overlay, and even
  // though it's not drawn, that its sync point is waited on.
  EXPECT_CALL(*context, waitSyncToken(MatchesSyncToken(sync_token))).Times(1);

  EXPECT_CALL(
      overlay_scheduler,
      Schedule(1, gfx::OVERLAY_TRANSFORM_NONE, _, gfx::Rect(viewport_size),
               BoundingRect(uv_top_left, uv_bottom_right)))
      .Times(1);

  DrawFrame(&renderer, viewport_size);

  // Transfer resources back from the parent to the child. Set no resources as
  // being in use.
  parent_resource_provider->DeclareUsedResourcesFromChild(child_id,
                                                          ResourceIdSet());
}

class OutputColorMatrixMockGLES2Interface : public TestGLES2Interface {
 public:
  OutputColorMatrixMockGLES2Interface() = default;

  MOCK_METHOD4(UniformMatrix4fv,
               void(GLint location,
                    GLsizei count,
                    GLboolean transpose,
                    const GLfloat* value));
};

TEST_F(GLRendererTest, OutputColorMatrixTest) {
  // Initialize the mock GL interface, the output surface and the renderer.
  auto gl_owned = std::make_unique<OutputColorMatrixMockGLES2Interface>();
  auto* gl = gl_owned.get();
  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();
  std::unique_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  cc::FakeOutputSurfaceClient output_surface_client;
  output_surface->BindToClient(&output_surface_client);
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), nullptr);
  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  // Set a non-identity color matrix on the output surface.
  SkMatrix44 color_matrix(SkMatrix44::kIdentity_Constructor);
  color_matrix.set(0, 0, 0.7f);
  color_matrix.set(1, 1, 0.4f);
  color_matrix.set(2, 2, 0.5f);
  output_surface->set_color_matrix(color_matrix);

  // Create a root and a child passes to test that the output color matrix is
  // registered only for the root pass.
  gfx::Size viewport_size(100, 100);
  RenderPassId child_pass_id = 2;
  RenderPass* child_pass =
      cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                        gfx::Rect(viewport_size) + gfx::Vector2d(1, 2),
                        gfx::Transform(), cc::FilterOperations());
  RenderPassId root_pass_id = 1;
  RenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = gfx::Rect(0, 0, 25, 25);
  cc::AddRenderPassQuad(root_pass, child_pass);

  // Verify that UniformMatrix4fv() is called only once on the root pass with
  // the correct matrix values.
  int call_count = 0;
  bool output_color_matrix_invoked = false;
  EXPECT_CALL(*gl, UniformMatrix4fv(_, 1, false, _))
      .WillRepeatedly(testing::WithArgs<0, 3>(testing::Invoke(
          [&color_matrix, &renderer, &call_count, &output_color_matrix_invoked](
              int matrix_location, const GLfloat* gl_matrix) {
            DCHECK(current_program(&renderer));
            const int color_matrix_location =
                current_program(&renderer)->output_color_matrix_location();

            if (matrix_location != color_matrix_location)
              return;

            call_count++;
            output_color_matrix_invoked = true;
            float expected_matrix[16];
            color_matrix.asColMajorf(expected_matrix);
            for (int i = 0; i < 16; ++i)
              EXPECT_FLOAT_EQ(expected_matrix[i], gl_matrix[i]);
          })));

  renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(&renderer, viewport_size);

  EXPECT_EQ(1, call_count);
  EXPECT_TRUE(output_color_matrix_invoked);
}

class GenerateMipmapMockGLESInterface : public TestGLES2Interface {
 public:
  GenerateMipmapMockGLESInterface() = default;

  MOCK_METHOD3(TexParameteri, void(GLenum target, GLenum pname, GLint param));
  MOCK_METHOD1(GenerateMipmap, void(GLenum target));
};

// TODO(crbug.com/803286): Currently npot texture always return false on ubuntu
// desktop.  The npot texture check is probably failing on desktop GL. This test
// crashes DCHECK npot texture to catch this. When
// GLRendererPixelTest.DISABLED_TrilinearFiltering got passed, can remove this.
TEST_F(GLRendererTest, GenerateMipmap) {
  // Initialize the mock GL interface, the output surface and the renderer.
  auto gl_owned = std::make_unique<GenerateMipmapMockGLESInterface>();
  auto* gl = gl_owned.get();
  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();
  provider->TestContext3d()->set_support_texture_npot(true);

  std::unique_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  cc::FakeOutputSurfaceClient output_surface_client;
  output_surface->BindToClient(&output_surface_client);
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), nullptr);
  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(100, 100);
  RenderPassId child_pass_id = 2;
  // Create a child pass with mipmap to verify that npot texture is enabled.
  RenderPass* child_pass =
      cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                        gfx::Rect(viewport_size) + gfx::Vector2d(1, 2),
                        gfx::Transform(), cc::FilterOperations());
  child_pass->generate_mipmap = true;

  RenderPassId root_pass_id = 1;
  RenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = gfx::Rect(0, 0, 25, 25);
  cc::AddRenderPassQuad(root_pass, child_pass);
  renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  EXPECT_CALL(*gl, TexParameteri(_, _, _)).Times(4);
  EXPECT_CALL(*gl, GenerateMipmap(GL_TEXTURE_2D)).Times(1);
  // When generate_mipmap enabled, the GL_TEXTURE_MIN_FILTER should be
  // GL_LINEAR_MIPMAP_LINEAR.
  EXPECT_CALL(*gl, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                 GL_LINEAR_MIPMAP_LINEAR));
  DrawFrame(&renderer, viewport_size);
}

class PartialSwapMockGLES2Interface : public TestGLES2Interface {
 public:
  explicit PartialSwapMockGLES2Interface(bool support_dc_layers)
      : support_dc_layers_(support_dc_layers) {}

  void InitializeTestContext(TestWebGraphicsContext3D* context) override {
    context->set_have_post_sub_buffer(true);
    context->set_enable_dc_layers(support_dc_layers_);
  }

  MOCK_METHOD1(Enable, void(GLenum cap));
  MOCK_METHOD1(Disable, void(GLenum cap));
  MOCK_METHOD4(Scissor, void(GLint x, GLint y, GLsizei width, GLsizei height));
  MOCK_METHOD1(SetEnableDCLayersCHROMIUM, void(GLboolean enable));

 private:
  bool support_dc_layers_;
};

class GLRendererPartialSwapTest : public GLRendererTest {
 protected:
  void RunTest(bool partial_swap, bool set_draw_rectangle) {
    auto gl_owned =
        std::make_unique<PartialSwapMockGLES2Interface>(set_draw_rectangle);
    auto* gl = gl_owned.get();

    auto provider = TestContextProvider::Create(std::move(gl_owned));
    provider->BindToCurrentThread();

    cc::FakeOutputSurfaceClient output_surface_client;
    std::unique_ptr<FakeOutputSurface> output_surface(
        FakeOutputSurface::Create3d(std::move(provider)));
    output_surface->BindToClient(&output_surface_client);

    std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
        cc::FakeResourceProvider::CreateDisplayResourceProvider(
            output_surface->context_provider(), nullptr);

    RendererSettings settings;
    settings.partial_swap_enabled = partial_swap;
    FakeRendererGL renderer(&settings, output_surface.get(),
                            resource_provider.get());
    renderer.Initialize();
    EXPECT_EQ(partial_swap, renderer.use_partial_swap());
    renderer.SetVisible(true);

    gfx::Size viewport_size(100, 100);
    gfx::Rect root_pass_output_rect(80, 80);
    gfx::Rect root_pass_damage_rect(2, 2, 3, 3);

    for (int i = 0; i < 2; ++i) {
      int root_pass_id = 1;
      RenderPass* root_pass = cc::AddRenderPassWithDamage(
          &render_passes_in_draw_order_, root_pass_id, root_pass_output_rect,
          root_pass_damage_rect, gfx::Transform(), cc::FilterOperations());
      cc::AddQuad(root_pass, gfx::Rect(root_pass_output_rect), SK_ColorGREEN);

      InSequence seq;

      // A bunch of initialization that happens.
      EXPECT_CALL(*gl, Disable(GL_DEPTH_TEST));
      EXPECT_CALL(*gl, Disable(GL_CULL_FACE));
      EXPECT_CALL(*gl, Disable(GL_STENCIL_TEST));
      EXPECT_CALL(*gl, Enable(GL_BLEND));
      EXPECT_CALL(*gl, Disable(GL_SCISSOR_TEST));
      EXPECT_CALL(*gl, Scissor(0, 0, 0, 0));

      // Partial frame, we should use a scissor to swap only that part when
      // partial swap is enabled. With SetDrawRectangle the first frame will
      // have its damage expanded to cover the entire output rect.
      bool draw_rectangle_needs_full_damage = set_draw_rectangle && (i == 0);
      bool frame_has_partial_damage =
          partial_swap && !draw_rectangle_needs_full_damage;
      gfx::Rect output_rectangle = frame_has_partial_damage
                                       ? root_pass_damage_rect
                                       : gfx::Rect(viewport_size);

      if (partial_swap || set_draw_rectangle) {
        EXPECT_CALL(*gl, Enable(GL_SCISSOR_TEST));
        // The scissor is flipped, so subtract the y coord and height from the
        // bottom of the GL viewport.
        EXPECT_CALL(
            *gl, Scissor(output_rectangle.x(),
                         viewport_size.height() - output_rectangle.y() -
                             output_rectangle.height(),
                         output_rectangle.width(), output_rectangle.height()));
      }

      // The quad doesn't need blending.
      EXPECT_CALL(*gl, Disable(GL_BLEND));

      // Blending is disabled at the end of the frame.
      EXPECT_CALL(*gl, Disable(GL_BLEND));

      renderer.DecideRenderPassAllocationsForFrame(
          render_passes_in_draw_order_);
      DrawFrame(&renderer, viewport_size);

      if (set_draw_rectangle) {
        EXPECT_EQ(output_rectangle, output_surface->last_set_draw_rectangle());
      }

      Mock::VerifyAndClearExpectations(gl);
    }
  }
};

TEST_F(GLRendererPartialSwapTest, PartialSwap) {
  RunTest(true, false);
}

TEST_F(GLRendererPartialSwapTest, NoPartialSwap) {
  RunTest(false, false);
}

TEST_F(GLRendererPartialSwapTest, SetDrawRectangle_PartialSwap) {
  RunTest(true, true);
}

TEST_F(GLRendererPartialSwapTest, SetDrawRectangle_NoPartialSwap) {
  RunTest(false, true);
}

class DCLayerValidator : public OverlayCandidateValidator {
 public:
  void GetStrategies(OverlayProcessor::StrategyList* strategies) override {}
  bool AllowCALayerOverlays() override { return false; }
  bool AllowDCLayerOverlays() override { return true; }
  void CheckOverlaySupport(cc::OverlayCandidateList* surfaces) override {}
};

// Test that SetEnableDCLayersCHROMIUM is properly called when enabling
// and disabling DC layers.
TEST_F(GLRendererTest, DCLayerOverlaySwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDirectCompositionUnderlays);
  auto gl_owned = std::make_unique<PartialSwapMockGLES2Interface>(true);
  auto* gl = gl_owned.get();

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  auto parent_resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), nullptr);

  auto child_context_provider = TestContextProvider::Create();
  child_context_provider->BindToCurrentThread();
  auto child_resource_provider =
      cc::FakeResourceProvider::CreateLayerTreeResourceProvider(
          child_context_provider.get(), nullptr);

  auto transfer_resource = TransferableResource::MakeGLOverlay(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken(),
      gfx::Size(256, 256), true);
  auto release_callback =
      SingleReleaseCallback::Create(base::Bind(&MailboxReleased));
  ResourceId resource_id = child_resource_provider->ImportResource(
      transfer_resource, std::move(release_callback));

  std::vector<ReturnedResource> returned_to_child;
  int child_id = parent_resource_provider->CreateChild(
      base::Bind(&CollectResources, &returned_to_child));

  // Transfer resource to the parent.
  cc::ResourceProvider::ResourceIdArray resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource_id);
  std::vector<TransferableResource> list;
  child_resource_provider->PrepareSendToParent(resource_ids_to_transfer, &list);
  parent_resource_provider->ReceiveFromChild(child_id, list);
  // In DisplayResourceProvider's namespace, use the mapped resource id.
  cc::ResourceProvider::ResourceIdMap resource_map =
      parent_resource_provider->GetChildToParentMap(child_id);
  ResourceId parent_resource_id = resource_map[list[0].id];

  RendererSettings settings;
  settings.partial_swap_enabled = true;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          parent_resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);
  TestOverlayProcessor* processor =
      new TestOverlayProcessor(output_surface.get());
  processor->Initialize();
  renderer.SetOverlayProcessor(processor);
  std::unique_ptr<DCLayerValidator> validator(new DCLayerValidator);
  output_surface->SetOverlayCandidateValidator(validator.get());

  gfx::Size viewport_size(100, 100);

  for (int i = 0; i < 65; i++) {
    int root_pass_id = 1;
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    if (i == 0) {
      gfx::Rect rect(0, 0, 100, 100);
      bool needs_blending = false;
      gfx::RectF tex_coord_rect(0, 0, 1, 1);
      SharedQuadState* shared_state =
          root_pass->CreateAndAppendSharedQuadState();
      shared_state->SetAll(gfx::Transform(), rect, rect, rect, false, false, 1,
                           SkBlendMode::kSrcOver, 0);
      YUVVideoDrawQuad* quad =
          root_pass->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
      quad->SetNew(shared_state, rect, rect, needs_blending, tex_coord_rect,
                   tex_coord_rect, rect.size(), rect.size(), parent_resource_id,
                   parent_resource_id, parent_resource_id, parent_resource_id,
                   gfx::ColorSpace::CreateREC601(), 0, 1.0, 8);
    }

    // A bunch of initialization that happens.
    EXPECT_CALL(*gl, Disable(_)).Times(AnyNumber());
    EXPECT_CALL(*gl, Enable(_)).Times(AnyNumber());
    EXPECT_CALL(*gl, Scissor(_, _, _, _)).Times(AnyNumber());

    // Partial frame, we should use a scissor to swap only that part when
    // partial swap is enabled.
    root_pass->damage_rect = gfx::Rect(2, 2, 3, 3);
    // Frame 0 should be completely damaged because it's the first.
    // Frame 1 should be because it changed. Frame 60 should be
    // because it's disabling DC layers.
    gfx::Rect output_rectangle = (i == 0 || i == 1 || i == 60)
                                     ? root_pass->output_rect
                                     : root_pass->damage_rect;

    // Frame 0 should have DC Layers enabled because of the overlay.
    // After 60 frames of no overlays DC layers should be disabled again.
    if (i < 60)
      EXPECT_CALL(*gl, SetEnableDCLayersCHROMIUM(GL_TRUE));
    else
      EXPECT_CALL(*gl, SetEnableDCLayersCHROMIUM(GL_FALSE));

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);
    EXPECT_EQ(output_rectangle, output_surface->last_set_draw_rectangle());
    testing::Mock::VerifyAndClearExpectations(gl);
  }

  // Transfer resources back from the parent to the child. Set no resources as
  // being in use.
  parent_resource_provider->DeclareUsedResourcesFromChild(child_id,
                                                          ResourceIdSet());
}

class GLRendererWithMockContextTest : public ::testing::Test {
 protected:
  class MockContextSupport : public TestContextSupport {
   public:
    MockContextSupport() {}
    MOCK_METHOD1(SetAggressivelyFreeResources,
                 void(bool aggressively_free_resources));
  };

  void SetUp() override {
    auto context_support = std::make_unique<MockContextSupport>();
    context_support_ptr_ = context_support.get();
    auto context_provider = TestContextProvider::Create(
        TestWebGraphicsContext3D::Create(), std::move(context_support));
    ASSERT_EQ(context_provider->BindToCurrentThread(),
              gpu::ContextResult::kSuccess);
    output_surface_ = FakeOutputSurface::Create3d(std::move(context_provider));
    output_surface_->BindToClient(&output_surface_client_);
    resource_provider_ =
        cc::FakeResourceProvider::CreateDisplayResourceProvider(
            output_surface_->context_provider(), nullptr);
    renderer_ = std::make_unique<GLRenderer>(&settings_, output_surface_.get(),
                                             resource_provider_.get(), nullptr);
    renderer_->Initialize();
  }

  RendererSettings settings_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  MockContextSupport* context_support_ptr_;
  std::unique_ptr<OutputSurface> output_surface_;
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider_;
  std::unique_ptr<GLRenderer> renderer_;
};

TEST_F(GLRendererWithMockContextTest,
       ContextPurgedWhenRendererBecomesInvisible) {
  EXPECT_CALL(*context_support_ptr_, SetAggressivelyFreeResources(false));
  renderer_->SetVisible(true);
  Mock::VerifyAndClearExpectations(context_support_ptr_);

  EXPECT_CALL(*context_support_ptr_, SetAggressivelyFreeResources(true));
  renderer_->SetVisible(false);
  Mock::VerifyAndClearExpectations(context_support_ptr_);
}

class SwapWithBoundsMockGLES2Interface : public TestGLES2Interface {
 public:
  void InitializeTestContext(TestWebGraphicsContext3D* context) override {
    context->set_have_swap_buffers_with_bounds(true);
  }
};

class ContentBoundsOverlayProcessor : public OverlayProcessor {
 public:
  class Strategy : public OverlayProcessor::Strategy {
   public:
    explicit Strategy(const std::vector<gfx::Rect>& content_bounds)
        : content_bounds_(content_bounds) {}
    ~Strategy() override = default;

    bool Attempt(const SkMatrix44& output_color_matrix,
                 cc::DisplayResourceProvider* resource_provider,
                 RenderPass* render_pass,
                 cc::OverlayCandidateList* candidates,
                 std::vector<gfx::Rect>* content_bounds) override {
      content_bounds->insert(content_bounds->end(), content_bounds_.begin(),
                             content_bounds_.end());
      return true;
    }

   private:
    const std::vector<gfx::Rect> content_bounds_;
  };

  ContentBoundsOverlayProcessor(OutputSurface* surface,
                                const std::vector<gfx::Rect>& content_bounds)
      : OverlayProcessor(surface), content_bounds_(content_bounds) {}

  void Initialize() override {
    strategies_.push_back(
        std::make_unique<Strategy>(std::move(content_bounds_)));
  }

  Strategy& strategy() { return static_cast<Strategy&>(*strategies_.back()); }

 private:
  std::vector<gfx::Rect> content_bounds_;
};

class GLRendererSwapWithBoundsTest : public GLRendererTest {
 protected:
  void RunTest(const std::vector<gfx::Rect>& content_bounds) {
    auto gl_owned = std::make_unique<SwapWithBoundsMockGLES2Interface>();

    auto provider = TestContextProvider::Create(std::move(gl_owned));
    provider->BindToCurrentThread();

    cc::FakeOutputSurfaceClient output_surface_client;
    std::unique_ptr<FakeOutputSurface> output_surface(
        FakeOutputSurface::Create3d(std::move(provider)));
    output_surface->BindToClient(&output_surface_client);

    std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
        cc::FakeResourceProvider::CreateDisplayResourceProvider(
            output_surface->context_provider(), nullptr);

    RendererSettings settings;
    FakeRendererGL renderer(&settings, output_surface.get(),
                            resource_provider.get());
    renderer.Initialize();
    EXPECT_EQ(true, renderer.use_swap_with_bounds());
    renderer.SetVisible(true);

    OverlayProcessor* processor =
        new ContentBoundsOverlayProcessor(output_surface.get(), content_bounds);
    processor->Initialize();
    renderer.SetOverlayProcessor(processor);

    gfx::Size viewport_size(100, 100);

    {
      int root_pass_id = 1;
      cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                        gfx::Rect(viewport_size), gfx::Transform(),
                        cc::FilterOperations());

      renderer.DecideRenderPassAllocationsForFrame(
          render_passes_in_draw_order_);
      DrawFrame(&renderer, viewport_size);
      renderer.SwapBuffers(std::vector<ui::LatencyInfo>());

      std::vector<gfx::Rect> expected_content_bounds;
      EXPECT_EQ(content_bounds,
                output_surface->last_sent_frame()->content_bounds);
    }
  }
};

TEST_F(GLRendererSwapWithBoundsTest, EmptyContent) {
  std::vector<gfx::Rect> content_bounds;
  RunTest(content_bounds);
}

TEST_F(GLRendererSwapWithBoundsTest, NonEmpty) {
  std::vector<gfx::Rect> content_bounds;
  content_bounds.push_back(gfx::Rect(0, 0, 10, 10));
  content_bounds.push_back(gfx::Rect(20, 20, 30, 30));
  RunTest(content_bounds);
}

class CALayerValidator : public OverlayCandidateValidator {
 public:
  void GetStrategies(OverlayProcessor::StrategyList* strategies) override {}
  bool AllowCALayerOverlays() override { return true; }
  bool AllowDCLayerOverlays() override { return false; }
  void CheckOverlaySupport(cc::OverlayCandidateList* surfaces) override {}
};

class MockCALayerGLES2Interface : public TestGLES2Interface {
 public:
  MOCK_METHOD5(ScheduleCALayerSharedStateCHROMIUM,
               void(GLfloat opacity,
                    GLboolean is_clipped,
                    const GLfloat* clip_rect,
                    GLint sorting_context_id,
                    const GLfloat* transform));
  MOCK_METHOD6(ScheduleCALayerCHROMIUM,
               void(GLuint contents_texture_id,
                    const GLfloat* contents_rect,
                    GLuint background_color,
                    GLuint edge_aa_mask,
                    const GLfloat* bounds_rect,
                    GLuint filter));
  MOCK_METHOD2(ScheduleCALayerInUseQueryCHROMIUM,
               void(GLsizei count, const GLuint* textures));

  void InitializeTestContext(TestWebGraphicsContext3D* context) override {
    // Support image storage for GpuMemoryBuffers, needed for
    // CALayers/IOSurfaces backed by textures.
    context->set_support_texture_storage_image(true);

    // Allow the renderer to make an empty SwapBuffers - skipping even the
    // root RenderPass.
    context->set_have_commit_overlay_planes(true);
  }
};

class CALayerGLRendererTest : public GLRendererTest {
 protected:
  void SetUp() override {
    // A mock GLES2Interface that can watch CALayer stuff happen.
    auto gles2_interface = std::make_unique<MockCALayerGLES2Interface>();
    gl_ = gles2_interface.get();

    auto provider = TestContextProvider::Create(std::move(gles2_interface));
    provider->BindToCurrentThread();

    cc::FakeOutputSurfaceClient output_surface_client;
    output_surface_ = FakeOutputSurface::Create3d(std::move(provider));
    output_surface_->BindToClient(&output_surface_client);

    // This validator allows the renderer to make CALayer overlays. If all
    // quads can be turned into CALayer overlays, then all damage is removed and
    // we can skip the root RenderPass, swapping empty.
    output_surface_->SetOverlayCandidateValidator(&validator_);

    display_resource_provider_ =
        cc::FakeResourceProvider::CreateDisplayResourceProvider(
            output_surface_->context_provider(), nullptr);

    settings_ = std::make_unique<RendererSettings>();
    // This setting is enabled to use CALayer overlays.
    settings_->release_overlay_resources_after_gpu_query = true;
    renderer_ = std::make_unique<FakeRendererGL>(
        settings_.get(), output_surface_.get(),
        display_resource_provider_.get(), base::ThreadTaskRunnerHandle::Get());
    renderer_->Initialize();
    renderer_->SetVisible(true);

    TestOverlayProcessor* processor =
        new TestOverlayProcessor(output_surface_.get());
    processor->Initialize();
    renderer_->SetOverlayProcessor(processor);
  }

  void TearDown() override {
    renderer_.reset();
    display_resource_provider_.reset();
    output_surface_.reset();
  }

  MockCALayerGLES2Interface& gl() const { return *gl_; }
  FakeRendererGL& renderer() const { return *renderer_; }
  FakeOutputSurface& output_surface() const { return *output_surface_; }

 private:
  MockCALayerGLES2Interface* gl_;
  CALayerValidator validator_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<cc::DisplayResourceProvider> display_resource_provider_;
  std::unique_ptr<RendererSettings> settings_;
  std::unique_ptr<FakeRendererGL> renderer_;
};

TEST_F(CALayerGLRendererTest, CALayerOverlaysWithAllQuadsPromoted) {
  gfx::Size viewport_size(10, 10);

  // This frame has a root pass with a RenderPassDrawQuad pointing to a child
  // pass that is at 1,2 to make it identifiable.
  RenderPassId child_pass_id = 2;
  RenderPassId root_pass_id = 1;
  {
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(viewport_size) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child pass is drawn, promoted to an overlay, and scheduled as a
  // CALayer.
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([](GLuint contents_texture_id, const GLfloat* contents_rect,
                      GLuint background_color, GLuint edge_aa_mask,
                      const GLfloat* bounds_rect, GLuint filter) {
              // This is the child RenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());

  renderer().SwapBuffers(std::vector<ui::LatencyInfo>());

  // The damage was eliminated when everything was promoted to CALayers.
  ASSERT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect);
  EXPECT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect->IsEmpty());

  // Frame number 2. Same inputs, except...
  {
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(viewport_size) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);

    // Use a cached RenderPass for the child.
    child_pass->cache_render_pass = true;
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child RenderPassDrawQuad gets promoted again, but importantly it
  // did not itself have to be drawn this time as it can use the cached texture.
  // Because we can skip the child pass, and the root pass (all quads were
  // promoted), this exposes edge cases in GLRenderer if it assumes we draw
  // at least one RenderPass. This still works, doesn't crash, etc, and the
  // RenderPassDrawQuad is emitted.
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());

  renderer().SwapBuffers(std::vector<ui::LatencyInfo>());
}

TEST_F(CALayerGLRendererTest, CALayerOverlaysReusesTextureWithDifferentSizes) {
  gfx::Size viewport_size(300, 300);

  // This frame has a root pass with a RenderPassDrawQuad pointing to a child
  // pass that is at 1,2 to make it identifiable.
  // The child's size is 250x251, but it will be rounded up to a multiple of 64
  // in order to promote easier texture reuse. See https://crbug.com/146070.
  RenderPassId child_pass_id = 2;
  RenderPassId root_pass_id = 1;
  {
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(250, 251) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child pass is drawn, promoted to an overlay, and scheduled as a
  // CALayer. The bounds of the texture are rounded up to 256x256. We save the
  // texture ID to make sure we reuse it correctly.
  uint32_t saved_texture_id = 0;
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // This is the child RenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              // The size is rounded to a multiple of 64.
              EXPECT_EQ(256, bounds_rect[2]);
              EXPECT_EQ(256, bounds_rect[3]);
              saved_texture_id = contents_texture_id;
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(std::vector<ui::LatencyInfo>());

  // ScheduleCALayerCHROMIUM happened and used a non-0 texture.
  EXPECT_NE(saved_texture_id, 0u);

  // The damage was eliminated when everything was promoted to CALayers.
  ASSERT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect);
  EXPECT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect->IsEmpty());

  // The texture will be checked to verify if it is free yet.
  EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(1, _));
  renderer().SwapBuffersComplete();
  Mock::VerifyAndClearExpectations(&gl());

  // Frame number 2. We change the size of the child RenderPass to be smaller
  // than the next multiple of 64, but larger than half the previous size so
  // that our texture reuse heuristics will reuse the texture if it is free.
  // For now, it is not.
  {
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(190, 191) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child RenderPass will use a new 192x192 texture, since the last texture
  // is still in use.
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // New texture id.
              EXPECT_NE(saved_texture_id, contents_texture_id);
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              // The texture is 192x192 since we snap up to multiples of 64.
              EXPECT_EQ(192, bounds_rect[2]);
              EXPECT_EQ(192, bounds_rect[3]);
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(std::vector<ui::LatencyInfo>());

  // There are now 2 textures to check if they are free.
  EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(2, _));
  renderer().SwapBuffersComplete();
  Mock::VerifyAndClearExpectations(&gl());

  // The first (256x256) texture is returned to the GLRenderer.
  renderer().DidReceiveTextureInUseResponses({{saved_texture_id, false}});

  // Frame number 3 looks just like frame number 2. The child RenderPass is
  // smaller than the next multiple of 64 from the released texture, but larger
  // than half of its size so that our texture reuse heuristics will kick in.
  {
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(190, 191) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child RenderPass would try to use a 192x192 texture, but since we have
  // an existing 256x256 texture, we can reuse that.
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // The first texture is reused.
              EXPECT_EQ(saved_texture_id, contents_texture_id);
              // This is the child RenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              // The size here is the size of the texture being used, not
              // the size we tried to use (192x192).
              EXPECT_EQ(256, bounds_rect[2]);
              EXPECT_EQ(256, bounds_rect[3]);
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(std::vector<ui::LatencyInfo>());
}

TEST_F(CALayerGLRendererTest, CALayerOverlaysDontReuseTooBigTexture) {
  gfx::Size viewport_size(300, 300);

  // This frame has a root pass with a RenderPassDrawQuad pointing to a child
  // pass that is at 1,2 to make it identifiable.
  // The child's size is 250x251, but it will be rounded up to a multiple of 64
  // in order to promote easier texture reuse. See https://crbug.com/146070.
  RenderPassId child_pass_id = 2;
  RenderPassId root_pass_id = 1;
  {
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(250, 251) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child pass is drawn, promoted to an overlay, and scheduled as a
  // CALayer. The bounds of the texture are rounded up to 256x256. We save the
  // texture ID to make sure we reuse it correctly.
  uint32_t saved_texture_id = 0;
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // This is the child RenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              // The size is rounded to a multiple of 64.
              EXPECT_EQ(256, bounds_rect[2]);
              EXPECT_EQ(256, bounds_rect[3]);
              saved_texture_id = contents_texture_id;
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(std::vector<ui::LatencyInfo>());

  // ScheduleCALayerCHROMIUM happened and used a non-0 texture.
  EXPECT_NE(saved_texture_id, 0u);

  // The damage was eliminated when everything was promoted to CALayers.
  ASSERT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect);
  EXPECT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect->IsEmpty());

  // The texture will be checked to verify if it is free yet.
  EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(1, _));
  renderer().SwapBuffersComplete();
  Mock::VerifyAndClearExpectations(&gl());

  // Frame number 2. We change the size of the child RenderPass to be much
  // smaller.
  {
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(20, 21) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child RenderPass will use a new 64x64 texture, since the last texture
  // is still in use.
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // New texture id.
              EXPECT_NE(saved_texture_id, contents_texture_id);
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              // The texture is 64x64 since we snap up to multiples of 64.
              EXPECT_EQ(64, bounds_rect[2]);
              EXPECT_EQ(64, bounds_rect[3]);
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(std::vector<ui::LatencyInfo>());

  // There are now 2 textures to check if they are free.
  EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(2, _));
  renderer().SwapBuffersComplete();
  Mock::VerifyAndClearExpectations(&gl());

  // The first (256x256) texture is returned to the GLRenderer.
  renderer().DidReceiveTextureInUseResponses({{saved_texture_id, false}});

  // Frame number 3 looks just like frame number 2. The child RenderPass is
  // too small to reuse the old texture.
  {
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(20, 21) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child RenderPass would try to use a 64x64 texture. We have a free and
  // existing 256x256 texture, but it's too large for us to reuse it.
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // The first texture is not reused.
              EXPECT_NE(saved_texture_id, contents_texture_id);
              // This is the child RenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              // The new texture has a smaller size.
              EXPECT_EQ(64, bounds_rect[2]);
              EXPECT_EQ(64, bounds_rect[3]);
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(std::vector<ui::LatencyInfo>());
}

TEST_F(CALayerGLRendererTest, CALayerOverlaysReuseAfterNoSwapBuffers) {
  gfx::Size viewport_size(300, 300);

  // This frame has a root pass with a RenderPassDrawQuad pointing to a child
  // pass that is at 1,2 to make it identifiable.
  RenderPassId child_pass_id = 2;
  RenderPassId root_pass_id = 1;
  {
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(100, 100) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child pass is drawn, promoted to an overlay, and scheduled as a
  // CALayer. We save the texture ID to make sure we reuse it correctly.
  uint32_t saved_texture_id = 0;
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // This is the child RenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              saved_texture_id = contents_texture_id;
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());

  // ScheduleCALayerCHROMIUM happened and used a non-0 texture.
  EXPECT_NE(saved_texture_id, 0u);

  // SwapBuffers() is *not* called though! Display can do this sometimes.

  // Frame number 2. We can not reuse the texture since the last one isn't
  // returned yet. We use a different size so we can control which texture gets
  // reused later.
  {
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(200, 200) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  uint32_t second_saved_texture_id = 0;
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // New texture id.
              EXPECT_NE(saved_texture_id, contents_texture_id);
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              second_saved_texture_id = contents_texture_id;
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());

  // SwapBuffers() *does* happen this time.
  renderer().SwapBuffers(std::vector<ui::LatencyInfo>());

  // There are 2 textures to check if they are free.
  EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(2, _));
  renderer().SwapBuffersComplete();
  Mock::VerifyAndClearExpectations(&gl());

  // Both textures get returned and the 2nd one can be reused.
  renderer().DidReceiveTextureInUseResponses(
      {{saved_texture_id, false}, {second_saved_texture_id, false}});

  // Frame number 3 looks just like frame number 2.
  {
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(200, 200) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The 2nd texture that we sent has been returned so we can reuse it. We
  // verify that happened.
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // The second texture is reused.
              EXPECT_EQ(second_saved_texture_id, contents_texture_id);
              // This is the child RenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(std::vector<ui::LatencyInfo>());
}

TEST_F(CALayerGLRendererTest, CALayerOverlaysReuseManyIfReturnedSlowly) {
  gfx::Size viewport_size(300, 300);

  // Each frame has a root pass with a RenderPassDrawQuad pointing to a child
  // pass. We generate a bunch of frames and swap them, each with a different
  // child RenderPass id, without getting any of the resources back from the OS.
  RenderPassId root_pass_id = 1;

  // The number is at least 2 larger than the number of textures we expect to
  // reuse, so that we can leave one in the OS, and have 1 texture returned but
  // not reused.
  const int kNumSendManyTextureIds = 7;
  uint32_t sent_texture_ids[kNumSendManyTextureIds];
  for (int i = 0; i < kNumSendManyTextureIds; ++i) {
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, i + 2,
                          gfx::Rect(250, 251) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);

    renderer().DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);

    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // This is the child RenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              sent_texture_ids[i] = contents_texture_id;
            }));
    DrawFrame(&renderer(), viewport_size);
    Mock::VerifyAndClearExpectations(&gl());
    renderer().SwapBuffers(std::vector<ui::LatencyInfo>());

    // ScheduleCALayerCHROMIUM happened and used a non-0 texture.
    EXPECT_NE(sent_texture_ids[i], 0u);

    // The damage was eliminated when everything was promoted to CALayers.
    ASSERT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect);
    EXPECT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect->IsEmpty());

    // All sent textures will be checked to verify if they are free yet.
    EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(i + 1, _));
    renderer().SwapBuffersComplete();
    Mock::VerifyAndClearExpectations(&gl());
  }

  // Now all but 1 texture get returned by the OS, so they are all inserted
  // into the cache for reuse.
  std::vector<uint32_t> returned_texture_ids;
  for (int i = 0; i < kNumSendManyTextureIds - 1; ++i) {
    uint32_t id = sent_texture_ids[i];
    renderer().DidReceiveTextureInUseResponses({{id, false}});
    returned_texture_ids.push_back(id);
  }

  // We should keep *some* of these textures around to reuse them across
  // multiple frames. https://crbug.com/146070 motivates this, and empirical
  // testing found 5 to be a good number.
  const int kNumSendReusedTextures = 5;
  // See comment on |kNumSendManyTextureIds|.
  ASSERT_LT(kNumSendReusedTextures, kNumSendManyTextureIds - 1);

  for (int i = 0; i < kNumSendReusedTextures + 1; ++i) {
    // We use different RenderPass ids to ensure that the cache allows reuse
    // even if they don't match.
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, i + 100,
                          gfx::Rect(250, 251) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);

    renderer().DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);

    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(Invoke([&](GLuint contents_texture_id,
                             const GLfloat* contents_rect,
                             GLuint background_color, GLuint edge_aa_mask,
                             const GLfloat* bounds_rect, GLuint filter) {
          // This is the child RenderPassDrawQuad.
          EXPECT_EQ(1, bounds_rect[0]);
          EXPECT_EQ(2, bounds_rect[1]);

          if (i < kNumSendReusedTextures) {
            // The texture id should be from the set of returned ones.
            EXPECT_THAT(returned_texture_ids, Contains(contents_texture_id));
            base::Erase(returned_texture_ids, contents_texture_id);
          } else {
            // More textures were returned at once than we expect to reuse
            // so eventually we should be making a new texture to show we're
            // not just keeping infinity textures in the cache.
            EXPECT_THAT(returned_texture_ids,
                        Not(Contains(contents_texture_id)));
            // This shows that there was some returned id that we didn't use.
            EXPECT_FALSE(returned_texture_ids.empty());
          }
        }));
    DrawFrame(&renderer(), viewport_size);
    Mock::VerifyAndClearExpectations(&gl());
    renderer().SwapBuffers(std::vector<ui::LatencyInfo>());

    // All sent textures will be checked to verify if they are free yet. There's
    // also 1 outstanding texture to check for that wasn't returned yet from the
    // above loop.
    EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(i + 2, _));
    renderer().SwapBuffersComplete();
    Mock::VerifyAndClearExpectations(&gl());
  }
}

TEST_F(CALayerGLRendererTest, CALayerOverlaysCachedTexturesAreFreed) {
  gfx::Size viewport_size(300, 300);

  // Each frame has a root pass with a RenderPassDrawQuad pointing to a child
  // pass. We generate a bunch of frames and swap them, each with a different
  // child RenderPass id, without getting any of the resources back from the OS.
  RenderPassId child_pass_id = 2;
  RenderPassId root_pass_id = 1;

  // We send a whole bunch of textures as overlays to the OS.
  const int kNumSendManyTextureIds = 7;
  uint32_t sent_texture_ids[kNumSendManyTextureIds];
  for (int i = 0; i < kNumSendManyTextureIds; ++i) {
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, i + 2,
                          gfx::Rect(250, 251) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);

    renderer().DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);

    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // This is the child RenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              sent_texture_ids[i] = contents_texture_id;
            }));
    DrawFrame(&renderer(), viewport_size);
    Mock::VerifyAndClearExpectations(&gl());
    renderer().SwapBuffers(std::vector<ui::LatencyInfo>());

    // ScheduleCALayerCHROMIUM happened and used a non-0 texture.
    EXPECT_NE(sent_texture_ids[i], 0u);

    // The damage was eliminated when everything was promoted to CALayers.
    ASSERT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect);
    EXPECT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect->IsEmpty());

    // All sent textures will be checked to verify if they are free yet.
    EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(i + 1, _));
    renderer().SwapBuffersComplete();
    Mock::VerifyAndClearExpectations(&gl());
  }

  // Now all but 1 texture get returned by the OS, so they are all inserted
  // into the cache for reuse.
  std::vector<uint32_t> returned_texture_ids;
  for (int i = 0; i < kNumSendManyTextureIds - 1; ++i) {
    uint32_t id = sent_texture_ids[i];
    renderer().DidReceiveTextureInUseResponses({{id, false}});
    returned_texture_ids.push_back(id);
  }

  // We generate a bunch of frames that don't use the cache, one less than the
  // number of textures returned.
  for (int i = 0; i < kNumSendManyTextureIds - 2; ++i) {
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(100, 100), SK_ColorRED);

    renderer().DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);

    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _));
    DrawFrame(&renderer(), viewport_size);
    Mock::VerifyAndClearExpectations(&gl());
    renderer().SwapBuffers(std::vector<ui::LatencyInfo>());

    // There's just 1 outstanding RenderPass texture to query for.
    EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(1, _));
    renderer().SwapBuffersComplete();
    Mock::VerifyAndClearExpectations(&gl());
  }

  // By now the cache should be empty, to show that we don't keep cached
  // textures that won't be used forever. We generate a frame with a
  // RenderPassDrawQuad and verify that it does not reuse a texture from the
  // (empty) cache.
  {
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(250, 251) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  InSequence sequence;
  EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _));
  EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
      .WillOnce(Invoke([&](GLuint contents_texture_id,
                           const GLfloat* contents_rect,
                           GLuint background_color, GLuint edge_aa_mask,
                           const GLfloat* bounds_rect, GLuint filter) {
        // This is the child RenderPassDrawQuad.
        EXPECT_EQ(1, bounds_rect[0]);
        EXPECT_EQ(2, bounds_rect[1]);

        // More textures were returned at once than we expect to reuse
        // so eventually we should be making a new texture to show we're
        // not just keeping infinity textures in the cache.
        EXPECT_THAT(returned_texture_ids, Not(Contains(contents_texture_id)));
        // This shows that there was some returned id that we didn't use.
        EXPECT_FALSE(returned_texture_ids.empty());
      }));
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(std::vector<ui::LatencyInfo>());
}

class FramebufferWatchingGLRenderer : public FakeRendererGL {
 public:
  FramebufferWatchingGLRenderer(RendererSettings* settings,
                                OutputSurface* output_surface,
                                cc::DisplayResourceProvider* resource_provider)
      : FakeRendererGL(settings, output_surface, resource_provider) {}

  void BindFramebufferToOutputSurface() override {
    ++bind_root_framebuffer_calls_;
    FakeRendererGL::BindFramebufferToOutputSurface();
  }

  void BindFramebufferToTexture(const RenderPassId render_pass_id) override {
    ++bind_child_framebuffer_calls_;
    FakeRendererGL::BindFramebufferToTexture(render_pass_id);
  }

  int bind_root_framebuffer_calls() const {
    return bind_root_framebuffer_calls_;
  }
  int bind_child_framebuffer_calls() const {
    return bind_child_framebuffer_calls_;
  }

  void ResetBindCalls() {
    bind_root_framebuffer_calls_ = bind_child_framebuffer_calls_ = 0;
  }

 private:
  int bind_root_framebuffer_calls_ = 0;
  int bind_child_framebuffer_calls_ = 0;
};

TEST_F(GLRendererTest, UndamagedRenderPassStillDrawnWhenNoPartialSwap) {
  auto provider = TestContextProvider::Create();
  provider->UnboundTestContext3d()->set_have_post_sub_buffer(true);
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  auto output_surface = FakeOutputSurface::Create3d(std::move(provider));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<cc::DisplayResourceProvider> resource_provider =
      cc::FakeResourceProvider::CreateDisplayResourceProvider(
          output_surface->context_provider(), nullptr);

  for (int i = 0; i < 2; ++i) {
    bool use_partial_swap = i == 0;
    SCOPED_TRACE(use_partial_swap);

    RendererSettings settings;
    settings.partial_swap_enabled = use_partial_swap;
    FramebufferWatchingGLRenderer renderer(&settings, output_surface.get(),
                                           resource_provider.get());
    renderer.Initialize();
    EXPECT_EQ(use_partial_swap, renderer.use_partial_swap());
    renderer.SetVisible(true);

    gfx::Size viewport_size(100, 100);
    gfx::Rect child_rect(10, 10);

    // First frame, the child and root RenderPass each have damage.
    RenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, 2, child_rect,
                          gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(child_pass, child_rect, SK_ColorGREEN);
    child_pass->damage_rect = child_rect;

    RenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorRED);
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);
    root_pass->damage_rect = gfx::Rect(viewport_size);

    EXPECT_EQ(0, renderer.bind_root_framebuffer_calls());
    EXPECT_EQ(0, renderer.bind_child_framebuffer_calls());

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);

    // We had to draw the root, and the child.
    EXPECT_EQ(1, renderer.bind_child_framebuffer_calls());
    // When the RenderPassDrawQuad in the root is drawn, we may re-bind the root
    // framebuffer. So it can be bound more than once.
    EXPECT_GE(renderer.bind_root_framebuffer_calls(), 1);

    // Reset counting.
    renderer.ResetBindCalls();

    // Second frame, the child RenderPass has no damage in it.
    child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, 2, child_rect,
                                   gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(child_pass, child_rect, SK_ColorGREEN);
    child_pass->damage_rect = gfx::Rect();

    // Root RenderPass has some damage that doesn't intersect the child.
    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, 1,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorRED);
    cc::AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(),
                          SkBlendMode::kSrcOver);
    root_pass->damage_rect = gfx::Rect(child_rect.right(), 0, 10, 10);

    EXPECT_EQ(0, renderer.bind_root_framebuffer_calls());
    EXPECT_EQ(0, renderer.bind_child_framebuffer_calls());

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);

    if (use_partial_swap) {
      // Without damage overlapping the child, it didn't need to be drawn (it
      // may choose to anyway but that'd be a waste). So we don't check for
      // |bind_child_framebuffer_calls|. But the root should have been drawn.
      EXPECT_EQ(renderer.bind_root_framebuffer_calls(), 1);
    } else {
      // Without partial swap, we have to draw the child still, this means
      // the child is bound as the framebuffer.
      EXPECT_EQ(1, renderer.bind_child_framebuffer_calls());
      // When the RenderPassDrawQuad in the root is drawn, as it must be since
      // we must draw the entire output, we may re-bind the root framebuffer. So
      // it can be bound more than once.
      EXPECT_GE(renderer.bind_root_framebuffer_calls(), 1);
    }
  }
}

}  // namespace
}  // namespace viz
