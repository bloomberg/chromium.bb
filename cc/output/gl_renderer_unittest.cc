// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/gl_renderer.h"

#include <stdint.h>

#include <set>
#include <vector>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/math_util.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/overlay_strategy_single_on_top.h"
#include "cc/output/overlay_strategy_underlay.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_resource_provider.h"
#include "cc/test/pixel_test.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/test_gles2_interface.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "ui/gfx/transform.h"
#include "ui/latency/latency_info.h"

using testing::_;
using testing::AnyNumber;
using testing::Args;
using testing::AtLeast;
using testing::ElementsAre;
using testing::Expectation;
using testing::InSequence;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

namespace cc {

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
class GLRendererShaderPixelTest : public GLRendererPixelTest {
 public:
  void SetUp() override {
    GLRendererPixelTest::SetUp();
    ASSERT_FALSE(renderer()->IsContextLost());
  }

  void TearDown() override {
    GLRendererPixelTest::TearDown();
    ASSERT_FALSE(renderer()->IsContextLost());
  }

  void TestShader(const ProgramKey& program_key) {
    renderer()->SetCurrentFrameForTesting(GLRenderer::DrawingFrame());
    const size_t kNumSrcColorSpaces = 4;
    gfx::ColorSpace src_color_spaces[kNumSrcColorSpaces] = {
        gfx::ColorSpace(), gfx::ColorSpace::CreateSRGB(),
        gfx::ColorSpace::CreateREC709(), gfx::ColorSpace::CreateExtendedSRGB(),
    };
    const size_t kNumDstColorSpaces = 3;
    gfx::ColorSpace dst_color_spaces[kNumDstColorSpaces] = {
        gfx::ColorSpace(), gfx::ColorSpace::CreateSRGB(),
        gfx::ColorSpace::CreateSCRGBLinear(),
    };
    for (size_t i = 0; i < kNumDstColorSpaces; ++i) {
      for (size_t j = 0; j < kNumSrcColorSpaces; ++j) {
        renderer()->SetUseProgram(program_key, src_color_spaces[j],
                                  dst_color_spaces[i]);
        EXPECT_TRUE(renderer()->current_program_->initialized());
      }
    }
  }

  void TestBasicShaders() {
    TestShader(ProgramKey::DebugBorder());
    TestShader(ProgramKey::SolidColor(NO_AA));
    TestShader(ProgramKey::SolidColor(USE_AA));
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
      renderer()->SetUseProgram(ProgramKey::SolidColor(NO_AA), src,
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
                                      NO_AA, NO_MASK, false, false));
    TestShader(ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode,
                                      USE_AA, NO_MASK, false, false));
  }

  void TestShadersWithPrecisionAndSampler(TexCoordPrecision precision,
                                          SamplerType sampler) {
    if (!context_provider()->ContextCapabilities().egl_image_external &&
        sampler == SAMPLER_TYPE_EXTERNAL_OES) {
      // This will likely be hit in tests due to usage of osmesa.
      return;
    }

    TestShader(
        ProgramKey::Texture(precision, sampler, PREMULTIPLIED_ALPHA, false));
    TestShader(
        ProgramKey::Texture(precision, sampler, PREMULTIPLIED_ALPHA, true));
    TestShader(ProgramKey::Texture(precision, sampler, NON_PREMULTIPLIED_ALPHA,
                                   false));
    TestShader(
        ProgramKey::Texture(precision, sampler, NON_PREMULTIPLIED_ALPHA, true));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, NO_SWIZZLE, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, DO_SWIZZLE, false));
    TestShader(ProgramKey::Tile(precision, sampler, USE_AA, NO_SWIZZLE, false));
    TestShader(ProgramKey::Tile(precision, sampler, USE_AA, DO_SWIZZLE, false));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, NO_SWIZZLE, true));
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, DO_SWIZZLE, true));

    // Iterate over alpha plane, nv12, and color_lut parameters.
    UVTextureMode uv_modes[2] = {UV_TEXTURE_MODE_UV, UV_TEXTURE_MODE_U_V};
    YUVAlphaTextureMode a_modes[2] = {YUV_NO_ALPHA_TEXTURE,
                                      YUV_HAS_ALPHA_TEXTURE};
    for (int j = 0; j < 2; j++) {
      for (int k = 0; k < 2; k++) {
        TestShader(
            ProgramKey::YUVVideo(precision, sampler, a_modes[j], uv_modes[k]));
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
                                      HAS_MASK, mask_for_background, false));
    TestShader(ProgramKey::RenderPass(precision, sampler, blend_mode, NO_AA,
                                      HAS_MASK, mask_for_background, true));
    TestShader(ProgramKey::RenderPass(precision, sampler, blend_mode, USE_AA,
                                      HAS_MASK, mask_for_background, false));
    TestShader(ProgramKey::RenderPass(precision, sampler, blend_mode, USE_AA,
                                      HAS_MASK, mask_for_background, true));
  }
};

namespace {

#if !defined(OS_ANDROID) && !defined(OS_WIN)
static const TexCoordPrecision kPrecisionList[] = {TEX_COORD_PRECISION_MEDIUM,
                                                   TEX_COORD_PRECISION_HIGH};

static const BlendMode kBlendModeList[LAST_BLEND_MODE + 1] = {
    BLEND_MODE_NONE,
    BLEND_MODE_NORMAL,
    BLEND_MODE_DESTINATION_IN,
    BLEND_MODE_SCREEN,
    BLEND_MODE_OVERLAY,
    BLEND_MODE_DARKEN,
    BLEND_MODE_LIGHTEN,
    BLEND_MODE_COLOR_DODGE,
    BLEND_MODE_COLOR_BURN,
    BLEND_MODE_HARD_LIGHT,
    BLEND_MODE_SOFT_LIGHT,
    BLEND_MODE_DIFFERENCE,
    BLEND_MODE_EXCLUSION,
    BLEND_MODE_MULTIPLY,
    BLEND_MODE_HUE,
    BLEND_MODE_SATURATION,
    BLEND_MODE_COLOR,
    BLEND_MODE_LUMINOSITY,
};

static const SamplerType kSamplerList[] = {
    SAMPLER_TYPE_2D,
    SAMPLER_TYPE_2D_RECT,
    SAMPLER_TYPE_EXTERNAL_OES,
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
          std::tr1::tuple<TexCoordPrecision, BlendMode>> {};

TEST_P(PrecisionBlendShaderPixelTest, ShadersCompile) {
  TestShadersWithPrecisionAndBlend(std::tr1::get<0>(GetParam()),
                                   std::tr1::get<1>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(
    PrecisionBlendShadersCompile,
    PrecisionBlendShaderPixelTest,
    ::testing::Combine(::testing::ValuesIn(kPrecisionList),
                       ::testing::ValuesIn(kBlendModeList)));

class PrecisionSamplerShaderPixelTest
    : public GLRendererShaderPixelTest,
      public ::testing::WithParamInterface<
          std::tr1::tuple<TexCoordPrecision, SamplerType>> {};

TEST_P(PrecisionSamplerShaderPixelTest, ShadersCompile) {
  TestShadersWithPrecisionAndSampler(std::tr1::get<0>(GetParam()),
                                     std::tr1::get<1>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(PrecisionSamplerShadersCompile,
                        PrecisionSamplerShaderPixelTest,
                        ::testing::Combine(::testing::ValuesIn(kPrecisionList),
                                           ::testing::ValuesIn(kSamplerList)));

class MaskShaderPixelTest
    : public GLRendererShaderPixelTest,
      public ::testing::WithParamInterface<
          std::tr1::tuple<TexCoordPrecision, SamplerType, BlendMode, bool>> {};

TEST_P(MaskShaderPixelTest, ShadersCompile) {
  TestShadersWithMasks(
      std::tr1::get<0>(GetParam()), std::tr1::get<1>(GetParam()),
      std::tr1::get<2>(GetParam()), std::tr1::get<3>(GetParam()));
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
                 ResourceProvider* resource_provider)
      : GLRenderer(settings, output_surface, resource_provider, nullptr) {}

  FakeRendererGL(const RendererSettings* settings,
                 OutputSurface* output_surface,
                 ResourceProvider* resource_provider,
                 TextureMailboxDeleter* texture_mailbox_deleter)
      : GLRenderer(settings,
                   output_surface,
                   resource_provider,
                   texture_mailbox_deleter) {}

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

    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ = FakeResourceProvider::Create(
        output_surface_->context_provider(), shared_bitmap_manager_.get());
    renderer_ = base::MakeUnique<FakeRendererGL>(
        &settings_, output_surface_.get(), resource_provider_.get());
    renderer_->Initialize();
    renderer_->SetVisible(true);
  }

  void SwapBuffers() { renderer_->SwapBuffers(std::vector<ui::LatencyInfo>()); }

  RendererSettings settings_;
  FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<ResourceProvider> resource_provider_;
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

    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ = FakeResourceProvider::Create(
        output_surface_->context_provider(), shared_bitmap_manager_.get());
    renderer_.reset(new FakeRendererGL(&settings_, output_surface_.get(),
                                       resource_provider_.get()));
    renderer_->Initialize();
    renderer_->SetVisible(true);
  }

  void TestRenderPassProgram(TexCoordPrecision precision,
                             BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode, NO_AA,
                               NO_MASK, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassColorMatrixProgram(TexCoordPrecision precision,
                                        BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode, NO_AA,
                               NO_MASK, false, true));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassMaskProgram(TexCoordPrecision precision,
                                 SamplerType sampler,
                                 BlendMode blend_mode) {
    const Program* program =
        renderer_->GetProgramIfInitialized(ProgramKey::RenderPass(
            precision, sampler, blend_mode, NO_AA, HAS_MASK, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassMaskColorMatrixProgram(TexCoordPrecision precision,
                                            SamplerType sampler,
                                            BlendMode blend_mode) {
    const Program* program =
        renderer_->GetProgramIfInitialized(ProgramKey::RenderPass(
            precision, sampler, blend_mode, NO_AA, HAS_MASK, false, true));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassProgramAA(TexCoordPrecision precision,
                               BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode, USE_AA,
                               NO_MASK, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassColorMatrixProgramAA(TexCoordPrecision precision,
                                          BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode, USE_AA,
                               NO_MASK, false, true));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassMaskProgramAA(TexCoordPrecision precision,
                                   SamplerType sampler,
                                   BlendMode blend_mode) {
    const Program* program =
        renderer_->GetProgramIfInitialized(ProgramKey::RenderPass(
            precision, sampler, blend_mode, USE_AA, HAS_MASK, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassMaskColorMatrixProgramAA(TexCoordPrecision precision,
                                              SamplerType sampler,
                                              BlendMode blend_mode) {
    const Program* program =
        renderer_->GetProgramIfInitialized(ProgramKey::RenderPass(
            precision, sampler, blend_mode, USE_AA, HAS_MASK, false, true));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestSolidColorProgramAA() {
    const Program* program =
        renderer_->GetProgramIfInitialized(ProgramKey::SolidColor(USE_AA));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  RendererSettings settings_;
  FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<ResourceProvider> resource_provider_;
  std::unique_ptr<FakeRendererGL> renderer_;
};

namespace {

TEST_F(GLRendererWithDefaultHarnessTest, ExternalStencil) {
  gfx::Size viewport_size(1, 1);
  EXPECT_FALSE(renderer_->stencil_enabled());

  output_surface_->set_has_external_stencil_test(true);

  RenderPass* root_pass =
      AddRenderPass(&render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
                    gfx::Transform(), FilterOperations());
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
  auto context = base::MakeUnique<ForbidSynchronousCallContext>();
  auto provider = TestContextProvider::Create(std::move(context));
  provider->BindToCurrentThread();

  FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  std::unique_ptr<ResourceProvider> resource_provider =
      FakeResourceProvider::Create(output_surface->context_provider(),
                                   shared_bitmap_manager.get());

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
  auto context = base::MakeUnique<LoseContextOnFirstGetContext>();
  auto provider = TestContextProvider::Create(std::move(context));
  provider->BindToCurrentThread();

  FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  std::unique_ptr<ResourceProvider> resource_provider =
      FakeResourceProvider::Create(output_surface->context_provider(),
                                   shared_bitmap_manager.get());

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

  FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  std::unique_ptr<ResourceProvider> resource_provider =
      FakeResourceProvider::Create(output_surface->context_provider(),
                                   shared_bitmap_manager.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(1, 1);
  RenderPass* root_pass =
      AddRenderPass(&render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
                    gfx::Transform(), FilterOperations());
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

  FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  std::unique_ptr<ResourceProvider> resource_provider =
      FakeResourceProvider::Create(output_surface->context_provider(),
                                   shared_bitmap_manager.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(1, 1);
  RenderPass* root_pass =
      AddRenderPass(&render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
                    gfx::Transform(), FilterOperations());
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

  FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::CreateOffscreen(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  std::unique_ptr<ResourceProvider> resource_provider =
      FakeResourceProvider::Create(output_surface->context_provider(),
                                   shared_bitmap_manager.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(1, 1);
  AddRenderPass(&render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
                gfx::Transform(), FilterOperations());

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

  FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  std::unique_ptr<ResourceProvider> resource_provider =
      FakeResourceProvider::Create(output_surface->context_provider(),
                                   shared_bitmap_manager.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  // During initialization we are allowed to set any texture parameters.
  EXPECT_CALL(*context, texParameteri(_, _, _)).Times(AnyNumber());

  RenderPass* root_pass =
      AddRenderPass(&render_passes_in_draw_order_, 1, gfx::Rect(100, 100),
                    gfx::Transform(), FilterOperations());
  gpu::SyncToken mailbox_sync_token;
  AddOneOfEveryQuadType(root_pass, resource_provider.get(), 0,
                        &mailbox_sync_token);

  renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // Set up expected texture filter state transitions that match the quads
  // created in AppendOneOfEveryQuadType().
  Mock::VerifyAndClearExpectations(context);
  {
    InSequence sequence;

    // The sync points for all quads are waited on first. This sync point is
    // for a texture quad drawn later in the frame.
    EXPECT_CALL(*context, waitSyncToken(MatchesSyncToken(mailbox_sync_token)))
        .Times(1);

    // yuv_quad is drawn with the default linear filter.
    EXPECT_CALL(*context, drawElements(_, _, _, _));

    // tile_quad is drawn with GL_NEAREST because it is not transformed or
    // scaled.
    EXPECT_CALL(
        *context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    EXPECT_CALL(
        *context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    EXPECT_CALL(*context, drawElements(_, _, _, _));

    // transformed_tile_quad uses GL_LINEAR.
    EXPECT_CALL(*context, drawElements(_, _, _, _));

    // scaled_tile_quad also uses GL_LINEAR.
    EXPECT_CALL(*context, drawElements(_, _, _, _));

    // The remaining quads also use GL_LINEAR because nearest neighbor
    // filtering is currently only used with tile quads.
    EXPECT_CALL(*context, drawElements(_, _, _, _)).Times(5);
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

  FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  std::unique_ptr<ResourceProvider> resource_provider =
      FakeResourceProvider::Create(output_surface->context_provider(),
                                   shared_bitmap_manager.get());

  RendererSettings settings;
  settings.should_clear_root_render_pass = false;

  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(10, 10);

  int child_pass_id = 2;
  RenderPass* child_pass = AddRenderPass(
      &render_passes_in_draw_order_, child_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), FilterOperations());
  AddQuad(child_pass, gfx::Rect(viewport_size), SK_ColorBLUE);

  int root_pass_id = 1;
  RenderPass* root_pass = AddRenderPass(&render_passes_in_draw_order_,
                                        root_pass_id, gfx::Rect(viewport_size),
                                        gfx::Transform(), FilterOperations());
  AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);

  AddRenderPassQuad(root_pass, child_pass);

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
  EXPECT_CALL(*mock_context, clear(clear_bits)).Times(0).After(
      first_render_pass);

  EXPECT_CALL(*mock_context, drawElements(_, _, _, _)).Times(AnyNumber()).After(
      first_render_pass);

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
  auto gl_owned = base::MakeUnique<ScissorTestOnClearCheckingGLES2Interface>();

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  std::unique_ptr<ResourceProvider> resource_provider =
      FakeResourceProvider::Create(output_surface->context_provider(),
                                   shared_bitmap_manager.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  EXPECT_FALSE(renderer.use_partial_swap());
  renderer.SetVisible(true);

  gfx::Size viewport_size(100, 100);

  gfx::Rect grand_child_rect(25, 25);
  int grand_child_pass_id = 3;
  RenderPass* grand_child_pass =
      AddRenderPass(&render_passes_in_draw_order_, grand_child_pass_id,
                    grand_child_rect, gfx::Transform(), FilterOperations());
  AddClippedQuad(grand_child_pass, grand_child_rect, SK_ColorYELLOW);

  gfx::Rect child_rect(50, 50);
  int child_pass_id = 2;
  RenderPass* child_pass =
      AddRenderPass(&render_passes_in_draw_order_, child_pass_id, child_rect,
                    gfx::Transform(), FilterOperations());
  AddQuad(child_pass, child_rect, SK_ColorBLUE);

  int root_pass_id = 1;
  RenderPass* root_pass = AddRenderPass(&render_passes_in_draw_order_,
                                        root_pass_id, gfx::Rect(viewport_size),
                                        gfx::Transform(), FilterOperations());
  AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);

  AddRenderPassQuad(root_pass, child_pass);
  AddRenderPassQuad(child_pass, grand_child_pass);

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
  auto gl_owned = base::MakeUnique<DiscardCheckingGLES2Interface>();
  auto* gl = gl_owned.get();

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  FakeOutputSurfaceClient output_surface_client;
  auto output_surface = FakeOutputSurface::Create3d(std::move(provider));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  std::unique_ptr<ResourceProvider> resource_provider =
      FakeResourceProvider::Create(output_surface->context_provider(),
                                   shared_bitmap_manager.get());

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
    RenderPass* root_pass = AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), FilterOperations());
    AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);
    root_pass->damage_rect = gfx::Rect(2, 2, 3, 3);

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);
    EXPECT_EQ(0, gl->discarded());
    gl->reset_discarded();
  }
  {
    // Full frame, should discard.
    int root_pass_id = 1;
    RenderPass* root_pass = AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), FilterOperations());
    AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);
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
    RenderPass* root_pass = AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), FilterOperations());
    AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);
    root_pass->damage_rect = root_pass->output_rect;
    root_pass->has_transparent_background = false;

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);
    EXPECT_EQ(0, gl->discarded());
    gl->reset_discarded();
    output_surface->set_has_external_stencil_test(false);
  }
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
    auto gl_owned = base::MakeUnique<StrictMock<DrawElementsGLES2Interface>>();
    gl_ = gl_owned.get();

    auto provider = TestContextProvider::Create(std::move(gl_owned));
    provider->BindToCurrentThread();

    output_surface_ = FakeOutputSurface::Create3d(std::move(provider));
    output_surface_->BindToClient(&output_surface_client_);

    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ = FakeResourceProvider::Create(
        output_surface_->context_provider(), shared_bitmap_manager_.get());
    settings_.partial_swap_enabled = true;
    renderer_ = base::MakeUnique<FakeRendererGL>(
        &settings_, output_surface_.get(), resource_provider_.get());
    renderer_->Initialize();
    renderer_->SetVisible(true);
  }

  StrictMock<DrawElementsGLES2Interface>* gl_;
  RendererSettings settings_;
  FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<ResourceProvider> resource_provider_;
  std::unique_ptr<FakeRendererGL> renderer_;
};

TEST_F(GLRendererSkipTest, DrawQuad) {
  EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(1);

  gfx::Size viewport_size(100, 100);
  gfx::Rect quad_rect = gfx::Rect(20, 20, 20, 20);

  int root_pass_id = 1;
  RenderPass* root_pass = AddRenderPass(&render_passes_in_draw_order_,
                                        root_pass_id, gfx::Rect(viewport_size),
                                        gfx::Transform(), FilterOperations());
  root_pass->damage_rect = gfx::Rect(0, 0, 25, 25);
  AddQuad(root_pass, quad_rect, SK_ColorGREEN);

  renderer_->DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(renderer_.get(), viewport_size);
}

TEST_F(GLRendererSkipTest, SkipVisibleRect) {
  gfx::Size viewport_size(100, 100);
  gfx::Rect quad_rect = gfx::Rect(0, 0, 40, 40);

  int root_pass_id = 1;
  RenderPass* root_pass = AddRenderPass(&render_passes_in_draw_order_,
                                        root_pass_id, gfx::Rect(viewport_size),
                                        gfx::Transform(), FilterOperations());
  root_pass->damage_rect = gfx::Rect(0, 0, 10, 10);
  AddQuad(root_pass, quad_rect, SK_ColorGREEN);
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
  RenderPass* root_pass = AddRenderPass(&render_passes_in_draw_order_,
                                        root_pass_id, gfx::Rect(viewport_size),
                                        gfx::Transform(), FilterOperations());
  root_pass->damage_rect = gfx::Rect(0, 0, 25, 25);
  AddClippedQuad(root_pass, quad_rect, SK_ColorGREEN);
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
  FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::Create3d());
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  std::unique_ptr<ResourceProvider> resource_provider =
      FakeResourceProvider::Create(output_surface->context_provider(),
                                   shared_bitmap_manager.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  EXPECT_FALSE(renderer.use_partial_swap());
  renderer.SetVisible(true);

  gfx::Size viewport_size(100, 100);
  gfx::Rect quad_rect = gfx::Rect(20, 20, 20, 20);

  int root_pass_id = 1;
  RenderPass* root_pass = AddRenderPass(&render_passes_in_draw_order_,
                                        root_pass_id, gfx::Rect(viewport_size),
                                        gfx::Transform(), FilterOperations());
  AddClippedQuad(root_pass, quad_rect, SK_ColorGREEN);

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

  ResourceId mask = resource_provider_->CreateResource(
      gfx::Size(20, 12), ResourceProvider::TEXTURE_HINT_IMMUTABLE,
      resource_provider_->best_texture_format(), gfx::ColorSpace());
  resource_provider_->AllocateForTesting(mask);

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
  FilterOperations filters;
  filters.Append(
      FilterOperation::CreateReferenceFilter(SkColorFilterImageFilter::Make(
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
        AddRenderPass(&render_passes_in_draw_order_, child_pass_id, child_rect,
                      gfx::Transform(), FilterOperations());

    root_pass = AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                              gfx::Rect(viewport_size), gfx::Transform(),
                              FilterOperations());

    AddRenderPassQuad(root_pass,
                      child_pass,
                      0,
                      gfx::Transform(),
                      xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassProgram(TEX_COORD_PRECISION_MEDIUM, blend_mode);

    // RenderPassColorMatrixProgram
    render_passes_in_draw_order_.clear();

    child_pass = AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                               child_rect, transform_causing_aa, filters);

    root_pass = AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                              gfx::Rect(viewport_size), gfx::Transform(),
                              FilterOperations());

    AddRenderPassQuad(root_pass, child_pass, 0, gfx::Transform(), xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassColorMatrixProgram(TEX_COORD_PRECISION_MEDIUM, blend_mode);

    // RenderPassMaskProgram
    render_passes_in_draw_order_.clear();

    child_pass =
        AddRenderPass(&render_passes_in_draw_order_, child_pass_id, child_rect,
                      gfx::Transform(), FilterOperations());

    root_pass = AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                              gfx::Rect(viewport_size), gfx::Transform(),
                              FilterOperations());

    AddRenderPassQuad(root_pass,
                      child_pass,
                      mask,
                      gfx::Transform(),
                      xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassMaskProgram(TEX_COORD_PRECISION_MEDIUM, SAMPLER_TYPE_2D,
                              blend_mode);

    // RenderPassMaskColorMatrixProgram
    render_passes_in_draw_order_.clear();

    child_pass = AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                               child_rect, gfx::Transform(), filters);

    root_pass = AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                              gfx::Rect(viewport_size), gfx::Transform(),
                              FilterOperations());

    AddRenderPassQuad(root_pass, child_pass, mask, gfx::Transform(), xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassMaskColorMatrixProgram(TEX_COORD_PRECISION_MEDIUM,
                                         SAMPLER_TYPE_2D, blend_mode);

    // RenderPassProgramAA
    render_passes_in_draw_order_.clear();

    child_pass =
        AddRenderPass(&render_passes_in_draw_order_, child_pass_id, child_rect,
                      transform_causing_aa, FilterOperations());

    root_pass = AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                              gfx::Rect(viewport_size), gfx::Transform(),
                              FilterOperations());

    AddRenderPassQuad(root_pass,
                      child_pass,
                      0,
                      transform_causing_aa,
                      xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassProgramAA(TEX_COORD_PRECISION_MEDIUM, blend_mode);

    // RenderPassColorMatrixProgramAA
    render_passes_in_draw_order_.clear();

    child_pass = AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                               child_rect, transform_causing_aa, filters);

    root_pass = AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                              gfx::Rect(viewport_size), gfx::Transform(),
                              FilterOperations());

    AddRenderPassQuad(root_pass, child_pass, 0, transform_causing_aa,
                      xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassColorMatrixProgramAA(TEX_COORD_PRECISION_MEDIUM, blend_mode);

    // RenderPassMaskProgramAA
    render_passes_in_draw_order_.clear();

    child_pass =
        AddRenderPass(&render_passes_in_draw_order_, child_pass_id, child_rect,
                      transform_causing_aa, FilterOperations());

    root_pass = AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                              gfx::Rect(viewport_size), gfx::Transform(),
                              FilterOperations());

    AddRenderPassQuad(root_pass,
                      child_pass,
                      mask,
                      transform_causing_aa,
                      xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassMaskProgramAA(TEX_COORD_PRECISION_MEDIUM, SAMPLER_TYPE_2D,
                                blend_mode);

    // RenderPassMaskColorMatrixProgramAA
    render_passes_in_draw_order_.clear();

    child_pass = AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                               child_rect, transform_causing_aa, filters);

    root_pass = AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                              gfx::Rect(viewport_size), transform_causing_aa,
                              FilterOperations());

    AddRenderPassQuad(root_pass, child_pass, mask, transform_causing_aa,
                      xfer_mode);

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
  MathUtil::MapQuad(transform_preventing_aa, gfx::QuadF(gfx::RectF(child_rect)),
                    &clipped);
  ASSERT_TRUE(clipped);

  child_pass =
      AddRenderPass(&render_passes_in_draw_order_, child_pass_id, child_rect,
                    transform_preventing_aa, FilterOperations());

  root_pass = AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                            gfx::Rect(viewport_size), gfx::Transform(),
                            FilterOperations());

  AddRenderPassQuad(root_pass, child_pass, 0, transform_preventing_aa,
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

  root_pass = AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                            gfx::Rect(viewport_size), gfx::Transform(),
                            FilterOperations());
  AddTransformedQuad(root_pass, gfx::Rect(quad_size), SK_ColorYELLOW,
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
  MOCK_CONST_METHOD0(SurfaceIsSuspendForRecycle, bool());
  MOCK_CONST_METHOD0(HasExternalStencilTest, bool());
  MOCK_METHOD0(ApplyExternalStencil, void());
};

class MockOutputSurfaceTest : public GLRendererTest {
 protected:
  void SetUp() override {
    auto context = base::MakeUnique<StrictMock<OutputSurfaceMockContext>>();
    context_ = context.get();
    auto provider = TestContextProvider::Create(std::move(context));
    provider->BindToCurrentThread();
    output_surface_ =
        base::MakeUnique<StrictMock<MockOutputSurface>>(std::move(provider));

    FakeOutputSurfaceClient output_surface_client_;
    output_surface_->BindToClient(&output_surface_client_);

    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ = FakeResourceProvider::Create(
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
    RenderPass* render_pass = AddRenderPass(
        &render_passes_in_draw_order_, render_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), FilterOperations());
    AddQuad(render_pass, gfx::Rect(viewport_size), SK_ColorGREEN);
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
  FakeOutputSurfaceClient output_surface_client_;
  OutputSurfaceMockContext* context_ = nullptr;
  std::unique_ptr<StrictMock<MockOutputSurface>> output_surface_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<ResourceProvider> resource_provider_;
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
    Strategy() {}
    ~Strategy() override {}
    MOCK_METHOD4(Attempt,
                 bool(ResourceProvider* resource_provider,
                      RenderPass* render_pass,
                      OverlayCandidateList* candidates,
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
    void CheckOverlaySupport(OverlayCandidateList* surfaces) {}
  };

  explicit TestOverlayProcessor(OutputSurface* surface)
      : OverlayProcessor(surface) {}
  ~TestOverlayProcessor() override {}
  void Initialize() override {
    strategy_ = new Strategy();
    strategies_.push_back(base::WrapUnique(strategy_));
  }

  Strategy* strategy_;
};

void MailboxReleased(const gpu::SyncToken& sync_token,
                     bool lost_resource,
                     BlockingTaskRunner* main_thread_task_runner) {}

void IgnoreCopyResult(std::unique_ptr<CopyOutputResult> result) {}

TEST_F(GLRendererTest, DontOverlayWithCopyRequests) {
  FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::Create3d());
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  std::unique_ptr<ResourceProvider> resource_provider =
      FakeResourceProvider::Create(output_surface->context_provider(),
                                   shared_bitmap_manager.get());
  std::unique_ptr<TextureMailboxDeleter> mailbox_deleter(
      new TextureMailboxDeleter(base::ThreadTaskRunnerHandle::Get()));

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get(), mailbox_deleter.get());
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
  RenderPass* root_pass =
      AddRenderPass(&render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
                    gfx::Transform(), FilterOperations());
  root_pass->has_transparent_background = false;
  root_pass->copy_requests.push_back(
      CopyOutputRequest::CreateRequest(base::Bind(&IgnoreCopyResult)));

  TextureMailbox mailbox =
      TextureMailbox(gpu::Mailbox::Generate(), gpu::SyncToken(), GL_TEXTURE_2D,
                     gfx::Size(256, 256), true, false);
  std::unique_ptr<SingleReleaseCallbackImpl> release_callback =
      SingleReleaseCallbackImpl::Create(base::Bind(&MailboxReleased));
  ResourceId resource_id = resource_provider->CreateResourceFromTextureMailbox(
      mailbox, std::move(release_callback));
  bool premultiplied_alpha = false;
  bool flipped = false;
  bool nearest_neighbor = false;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  TextureDrawQuad* overlay_quad =
      root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(
      root_pass->CreateAndAppendSharedQuadState(), gfx::Rect(viewport_size),
      gfx::Rect(viewport_size), gfx::Rect(viewport_size), resource_id,
      premultiplied_alpha, gfx::PointF(0, 0), gfx::PointF(1, 1),
      SK_ColorTRANSPARENT, vertex_opacity, flipped, nearest_neighbor, false);

  // DirectRenderer::DrawFrame calls into OverlayProcessor::ProcessForOverlays.
  // Attempt will be called for each strategy in OverlayProcessor. We have
  // added a fake strategy, so checking for Attempt calls checks if there was
  // any attempt to overlay, which there shouldn't be. We can't use the quad
  // list because the render pass is cleaned up by DrawFrame.
  EXPECT_CALL(*processor->strategy_, Attempt(_, _, _, _)).Times(0);
  EXPECT_CALL(*validator, AllowCALayerOverlays()).Times(0);
  EXPECT_CALL(*validator, AllowDCLayerOverlays()).Times(0);
  DrawFrame(&renderer, viewport_size);
  Mock::VerifyAndClearExpectations(processor->strategy_);
  Mock::VerifyAndClearExpectations(validator.get());

  // Without a copy request Attempt() should be called once.
  root_pass =
      AddRenderPass(&render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
                    gfx::Transform(), FilterOperations());
  root_pass->has_transparent_background = false;

  overlay_quad = root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(
      root_pass->CreateAndAppendSharedQuadState(), gfx::Rect(viewport_size),
      gfx::Rect(viewport_size), gfx::Rect(viewport_size), resource_id,
      premultiplied_alpha, gfx::PointF(0, 0), gfx::PointF(1, 1),
      SK_ColorTRANSPARENT, vertex_opacity, flipped, nearest_neighbor, false);
  EXPECT_CALL(*validator, AllowCALayerOverlays())
      .Times(1)
      .WillOnce(::testing::Return(false));
  EXPECT_CALL(*validator, AllowDCLayerOverlays())
      .Times(1)
      .WillOnce(::testing::Return(false));
  EXPECT_CALL(*processor->strategy_, Attempt(_, _, _, _)).Times(1);
  DrawFrame(&renderer, viewport_size);

  // If the CALayerOverlay path is taken, then the ordinary overlay path should
  // not be called.
  root_pass =
      AddRenderPass(&render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
                    gfx::Transform(), FilterOperations());
  root_pass->has_transparent_background = false;

  overlay_quad = root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(
      root_pass->CreateAndAppendSharedQuadState(), gfx::Rect(viewport_size),
      gfx::Rect(viewport_size), gfx::Rect(viewport_size), resource_id,
      premultiplied_alpha, gfx::PointF(0, 0), gfx::PointF(1, 1),
      SK_ColorTRANSPARENT, vertex_opacity, flipped, nearest_neighbor, false);
  EXPECT_CALL(*validator, AllowCALayerOverlays())
      .Times(1)
      .WillOnce(::testing::Return(true));
  EXPECT_CALL(*processor->strategy_, Attempt(_, _, _, _)).Times(0);
  DrawFrame(&renderer, viewport_size);
}

class SingleOverlayOnTopProcessor : public OverlayProcessor {
 public:
  class SingleOverlayValidator : public OverlayCandidateValidator {
   public:
    void GetStrategies(OverlayProcessor::StrategyList* strategies) override {
      strategies->push_back(base::MakeUnique<OverlayStrategySingleOnTop>(this));
      strategies->push_back(base::MakeUnique<OverlayStrategyUnderlay>(this));
    }

    bool AllowCALayerOverlays() override { return false; }
    bool AllowDCLayerOverlays() override { return false; }

    void CheckOverlaySupport(OverlayCandidateList* surfaces) override {
      ASSERT_EQ(1U, surfaces->size());
      OverlayCandidate& candidate = surfaces->back();
      candidate.overlay_handled = true;
    }
  };

  explicit SingleOverlayOnTopProcessor(OutputSurface* surface)
      : OverlayProcessor(surface) {}

  void Initialize() override {
    strategies_.push_back(
        base::MakeUnique<OverlayStrategySingleOnTop>(&validator_));
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

  FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  std::unique_ptr<ResourceProvider> resource_provider =
      FakeResourceProvider::Create(output_surface->context_provider(),
                                   shared_bitmap_manager.get());
  std::unique_ptr<TextureMailboxDeleter> mailbox_deleter(
      new TextureMailboxDeleter(base::ThreadTaskRunnerHandle::Get()));

  RendererSettings settings;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get(), mailbox_deleter.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  SingleOverlayOnTopProcessor* processor =
      new SingleOverlayOnTopProcessor(output_surface.get());
  processor->Initialize();
  renderer.SetOverlayProcessor(processor);

  gfx::Size viewport_size(1, 1);
  RenderPass* root_pass =
      AddRenderPass(&render_passes_in_draw_order_, 1, gfx::Rect(viewport_size),
                    gfx::Transform(), FilterOperations());
  root_pass->has_transparent_background = false;

  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO, 0,
                            gpu::CommandBufferId::FromUnsafeValue(0x123), 29);
  TextureMailbox mailbox =
      TextureMailbox(gpu::Mailbox::Generate(), sync_token, GL_TEXTURE_2D,
                     gfx::Size(256, 256), true, false);
  std::unique_ptr<SingleReleaseCallbackImpl> release_callback =
      SingleReleaseCallbackImpl::Create(base::Bind(&MailboxReleased));
  ResourceId resource_id = resource_provider->CreateResourceFromTextureMailbox(
      mailbox, std::move(release_callback));
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
                       false, 1, SkBlendMode::kSrcOver, 0);
  overlay_quad->SetNew(shared_state, gfx::Rect(viewport_size),
                       gfx::Rect(viewport_size), gfx::Rect(viewport_size),
                       resource_id, premultiplied_alpha, uv_top_left,
                       uv_bottom_right, SK_ColorTRANSPARENT, vertex_opacity,
                       flipped, nearest_neighbor, false);

  // Verify that overlay_quad actually gets turned into an overlay, and even
  // though it's not drawn, that its sync point is waited on.
  EXPECT_CALL(*context, waitSyncToken(MatchesSyncToken(sync_token))).Times(1);
  EXPECT_CALL(
      overlay_scheduler,
      Schedule(1, gfx::OVERLAY_TRANSFORM_NONE, _, gfx::Rect(viewport_size),
               BoundingRect(uv_top_left, uv_bottom_right)))
      .Times(1);

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
        base::MakeUnique<PartialSwapMockGLES2Interface>(set_draw_rectangle);
    auto* gl = gl_owned.get();

    auto provider = TestContextProvider::Create(std::move(gl_owned));
    provider->BindToCurrentThread();

    FakeOutputSurfaceClient output_surface_client;
    std::unique_ptr<FakeOutputSurface> output_surface(
        FakeOutputSurface::Create3d(std::move(provider)));
    output_surface->BindToClient(&output_surface_client);

    std::unique_ptr<ResourceProvider> resource_provider =
        FakeResourceProvider::Create(output_surface->context_provider(),
                                     nullptr);

    RendererSettings settings;
    settings.partial_swap_enabled = partial_swap;
    FakeRendererGL renderer(&settings, output_surface.get(),
                            resource_provider.get());
    renderer.Initialize();
    EXPECT_EQ(partial_swap, renderer.use_partial_swap());
    renderer.SetVisible(true);

    gfx::Size viewport_size(100, 100);

    for (int i = 0; i < 2; ++i) {
      int root_pass_id = 1;
      RenderPass* root_pass = AddRenderPass(
          &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
          gfx::Transform(), FilterOperations());
      AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);

      testing::Sequence seq;
      // A bunch of initialization that happens.
      EXPECT_CALL(*gl, Disable(GL_DEPTH_TEST)).InSequence(seq);
      EXPECT_CALL(*gl, Disable(GL_CULL_FACE)).InSequence(seq);
      EXPECT_CALL(*gl, Disable(GL_STENCIL_TEST)).InSequence(seq);
      EXPECT_CALL(*gl, Enable(GL_BLEND)).InSequence(seq);
      EXPECT_CALL(*gl, Disable(GL_SCISSOR_TEST)).InSequence(seq);
      EXPECT_CALL(*gl, Scissor(0, 0, 0, 0)).InSequence(seq);

      // Partial frame, we should use a scissor to swap only that part when
      // partial swap is enabled.
      root_pass->damage_rect = gfx::Rect(2, 2, 3, 3);
      // With SetDrawRectangle the first frame will have its damage expanded
      // to cover the entire output rect.
      bool frame_has_partial_damage =
          partial_swap && (!set_draw_rectangle || (i > 0));
      gfx::Rect output_rectangle = frame_has_partial_damage
                                       ? root_pass->damage_rect
                                       : gfx::Rect(viewport_size);

      if (partial_swap || set_draw_rectangle) {
        EXPECT_CALL(*gl, Enable(GL_SCISSOR_TEST)).InSequence(seq);
        // The scissor is flipped, so subtract the y coord and height from the
        // bottom of the GL viewport.
        EXPECT_CALL(
            *gl,
            Scissor(output_rectangle.x(),
                    viewport_size.height() - output_rectangle.y() -
                        output_rectangle.height(),
                    output_rectangle.width(), output_rectangle.height()))
            .InSequence(seq);
      }

      // The quad doesn't need blending.
      EXPECT_CALL(*gl, Disable(GL_BLEND)).InSequence(seq);

      // Blending is disabled at the end of the frame.
      EXPECT_CALL(*gl, Disable(GL_BLEND)).InSequence(seq);

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
  void CheckOverlaySupport(OverlayCandidateList* surfaces) override {}
};

// Test that SetEnableDCLayersCHROMIUM is properly called when enabling
// and disabling DC layers.
TEST_F(GLRendererTest, DCLayerOverlaySwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDirectCompositionUnderlays);
  auto gl_owned = base::MakeUnique<PartialSwapMockGLES2Interface>(true);
  auto* gl = gl_owned.get();

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  std::unique_ptr<ResourceProvider> resource_provider =
      FakeResourceProvider::Create(output_surface->context_provider(), nullptr);

  RendererSettings settings;
  settings.partial_swap_enabled = true;
  FakeRendererGL renderer(&settings, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);
  TestOverlayProcessor* processor =
      new TestOverlayProcessor(output_surface.get());
  processor->Initialize();
  renderer.SetOverlayProcessor(processor);
  std::unique_ptr<DCLayerValidator> validator(new DCLayerValidator);
  output_surface->SetOverlayCandidateValidator(validator.get());

  gfx::Size viewport_size(100, 100);

  TextureMailbox mailbox =
      TextureMailbox(gpu::Mailbox::Generate(), gpu::SyncToken(), GL_TEXTURE_2D,
                     gfx::Size(256, 256), true, false);
  std::unique_ptr<SingleReleaseCallbackImpl> release_callback =
      SingleReleaseCallbackImpl::Create(base::Bind(&MailboxReleased));
  ResourceId resource_id = resource_provider->CreateResourceFromTextureMailbox(
      mailbox, std::move(release_callback));

  for (int i = 0; i < 65; i++) {
    int root_pass_id = 1;
    RenderPass* root_pass = AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), FilterOperations());
    if (i == 0) {
      gfx::Rect rect(0, 0, 100, 100);
      gfx::RectF tex_coord_rect(0, 0, 1, 1);
      SharedQuadState* shared_state =
          root_pass->CreateAndAppendSharedQuadState();
      shared_state->SetAll(gfx::Transform(), rect, rect, rect, false, 1,
                           SkBlendMode::kSrcOver, 0);
      YUVVideoDrawQuad* quad =
          root_pass->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
      quad->SetNew(shared_state, rect, rect, rect, tex_coord_rect,
                   tex_coord_rect, rect.size(), rect.size(), resource_id,
                   resource_id, resource_id, resource_id,
                   YUVVideoDrawQuad::REC_601, gfx::ColorSpace(), 0, 1.0, 8);
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
    auto context_support = base::MakeUnique<MockContextSupport>();
    context_support_ptr_ = context_support.get();
    auto context_provider = TestContextProvider::Create(
        TestWebGraphicsContext3D::Create(), std::move(context_support));
    context_provider->BindToCurrentThread();
    output_surface_ = FakeOutputSurface::Create3d(std::move(context_provider));
    output_surface_->BindToClient(&output_surface_client_);
    resource_provider_ = FakeResourceProvider::Create(
        output_surface_->context_provider(), nullptr);
    renderer_ = base::MakeUnique<GLRenderer>(&settings_, output_surface_.get(),
                                             resource_provider_.get(), nullptr);
    renderer_->Initialize();
  }

  RendererSettings settings_;
  FakeOutputSurfaceClient output_surface_client_;
  MockContextSupport* context_support_ptr_;
  std::unique_ptr<OutputSurface> output_surface_;
  std::unique_ptr<ResourceProvider> resource_provider_;
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
    ~Strategy() override {}
    bool Attempt(ResourceProvider* resource_provider,
                 RenderPass* render_pass,
                 OverlayCandidateList* candidates,
                 std::vector<gfx::Rect>* content_bounds) override {
      content_bounds->insert(content_bounds->end(), content_bounds_.begin(),
                             content_bounds_.end());
      return true;
    }

    const std::vector<gfx::Rect> content_bounds_;
  };

  ContentBoundsOverlayProcessor(OutputSurface* surface,
                                const std::vector<gfx::Rect>& content_bounds)
      : OverlayProcessor(surface), content_bounds_(content_bounds) {}

  void Initialize() override {
    strategy_ = new Strategy(content_bounds_);
    strategies_.push_back(base::WrapUnique(strategy_));
  }

  Strategy* strategy_;
  const std::vector<gfx::Rect> content_bounds_;
};

class GLRendererSwapWithBoundsTest : public GLRendererTest {
 protected:
  void RunTest(const std::vector<gfx::Rect>& content_bounds) {
    auto gl_owned = base::MakeUnique<SwapWithBoundsMockGLES2Interface>();

    auto provider = TestContextProvider::Create(std::move(gl_owned));
    provider->BindToCurrentThread();

    FakeOutputSurfaceClient output_surface_client;
    std::unique_ptr<FakeOutputSurface> output_surface(
        FakeOutputSurface::Create3d(std::move(provider)));
    output_surface->BindToClient(&output_surface_client);

    std::unique_ptr<ResourceProvider> resource_provider =
        FakeResourceProvider::Create(output_surface->context_provider(),
                                     nullptr);

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
      AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                    gfx::Rect(viewport_size), gfx::Transform(),
                    FilterOperations());

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

}  // namespace
}  // namespace cc
