// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/gl_renderer.h"

#include <set>

#include "cc/base/math_util.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/sync_point_helper.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/pixel_test.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "ui/gfx/transform.h"

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Expectation;
using testing::InSequence;
using testing::Mock;
using testing::Return;
using testing::StrictMock;
using WebKit::WebGLId;
using WebKit::WebString;
using WebKit::WGC3Dbitfield;
using WebKit::WGC3Dboolean;
using WebKit::WGC3Dchar;
using WebKit::WGC3Denum;
using WebKit::WGC3Dfloat;
using WebKit::WGC3Dint;
using WebKit::WGC3Dintptr;
using WebKit::WGC3Dsizei;
using WebKit::WGC3Dsizeiptr;
using WebKit::WGC3Duint;

namespace cc {

#define EXPECT_PROGRAM_VALID(program_binding)                                  \
  do {                                                                         \
    EXPECT_TRUE(program_binding->program());                                   \
    EXPECT_TRUE(program_binding->initialized());                               \
  } while (false)

// Explicitly named to be a friend in GLRenderer for shader access.
class GLRendererShaderPixelTest : public GLRendererPixelTest {
 public:
  void TestShaders() {
    ASSERT_FALSE(renderer()->IsContextLost());
    EXPECT_PROGRAM_VALID(renderer()->GetTileCheckerboardProgram());
    EXPECT_PROGRAM_VALID(renderer()->GetDebugBorderProgram());
    EXPECT_PROGRAM_VALID(renderer()->GetSolidColorProgram());
    EXPECT_PROGRAM_VALID(renderer()->GetSolidColorProgramAA());
    TestShadersWithTexCoordPrecision(TexCoordPrecisionMedium);
    TestShadersWithTexCoordPrecision(TexCoordPrecisionHigh);
    ASSERT_FALSE(renderer()->IsContextLost());
  }

  void TestShadersWithTexCoordPrecision(TexCoordPrecision precision) {
    EXPECT_PROGRAM_VALID(renderer()->GetTileProgram(precision));
    EXPECT_PROGRAM_VALID(renderer()->GetTileProgramOpaque(precision));
    EXPECT_PROGRAM_VALID(renderer()->GetTileProgramAA(precision));
    EXPECT_PROGRAM_VALID(renderer()->GetTileProgramSwizzle(precision));
    EXPECT_PROGRAM_VALID(renderer()->GetTileProgramSwizzleOpaque(precision));
    EXPECT_PROGRAM_VALID(renderer()->GetTileProgramSwizzleAA(precision));
    EXPECT_PROGRAM_VALID(renderer()->GetRenderPassProgram(precision));
    EXPECT_PROGRAM_VALID(renderer()->GetRenderPassProgramAA(precision));
    EXPECT_PROGRAM_VALID(renderer()->GetRenderPassMaskProgram(precision));
    EXPECT_PROGRAM_VALID(renderer()->GetRenderPassMaskProgramAA(precision));
    EXPECT_PROGRAM_VALID(
        renderer()->GetRenderPassColorMatrixProgram(precision));
    EXPECT_PROGRAM_VALID(
        renderer()->GetRenderPassMaskColorMatrixProgramAA(precision));
    EXPECT_PROGRAM_VALID(
        renderer()->GetRenderPassColorMatrixProgramAA(precision));
    EXPECT_PROGRAM_VALID(
        renderer()->GetRenderPassMaskColorMatrixProgram(precision));
    EXPECT_PROGRAM_VALID(renderer()->GetTextureProgram(precision));
    EXPECT_PROGRAM_VALID(
        renderer()->GetNonPremultipliedTextureProgram(precision));
    EXPECT_PROGRAM_VALID(renderer()->GetTextureBackgroundProgram(precision));
    EXPECT_PROGRAM_VALID(
        renderer()->GetNonPremultipliedTextureBackgroundProgram(precision));
    EXPECT_PROGRAM_VALID(renderer()->GetTextureIOSurfaceProgram(precision));
    EXPECT_PROGRAM_VALID(renderer()->GetVideoYUVProgram(precision));
    EXPECT_PROGRAM_VALID(renderer()->GetVideoYUVAProgram(precision));
    // This is unlikely to be ever true in tests due to usage of osmesa.
    if (renderer()->Capabilities().using_egl_image)
      EXPECT_PROGRAM_VALID(renderer()->GetVideoStreamTextureProgram(precision));
    else
      EXPECT_FALSE(renderer()->GetVideoStreamTextureProgram(precision));
  }
};

namespace {

#if !defined(OS_ANDROID)
TEST_F(GLRendererShaderPixelTest, AllShadersCompile) { TestShaders(); }
#endif

class FrameCountingContext : public TestWebGraphicsContext3D {
 public:
  FrameCountingContext() : frame_(0) {}

  // WebGraphicsContext3D methods.

  // This method would normally do a glSwapBuffers under the hood.
  virtual void prepareTexture() { frame_++; }
  virtual WebString getString(WebKit::WGC3Denum name) {
    if (name == GL_EXTENSIONS)
      return WebString(
          "GL_CHROMIUM_set_visibility GL_CHROMIUM_gpu_memory_manager "
          "GL_CHROMIUM_discard_backbuffer");
    return WebString();
  }

  // Methods added for test.
  int frame_count() { return frame_; }

 private:
  int frame_;
};

class FakeRendererClient : public RendererClient {
 public:
  FakeRendererClient()
      : host_impl_(&proxy_),
        set_full_root_layer_damage_count_(0),
        root_layer_(LayerImpl::Create(host_impl_.active_tree(), 1)),
        viewport_size_(gfx::Size(1, 1)),
        scale_factor_(1.f),
        external_stencil_test_enabled_(false) {
    root_layer_->CreateRenderSurface();
    RenderPass::Id render_pass_id =
        root_layer_->render_surface()->RenderPassId();
    scoped_ptr<RenderPass> root_render_pass = RenderPass::Create();
    root_render_pass->SetNew(
        render_pass_id, gfx::Rect(), gfx::Rect(), gfx::Transform());
    render_passes_in_draw_order_.push_back(root_render_pass.Pass());
  }

  // RendererClient methods.
  virtual gfx::Rect DeviceViewport() const OVERRIDE {
    static gfx::Size fake_size(1, 1);
    return gfx::Rect(fake_size);
  }
  virtual float DeviceScaleFactor() const OVERRIDE {
    return scale_factor_;
  }
  virtual const LayerTreeSettings& Settings() const OVERRIDE {
    static LayerTreeSettings fake_settings;
    return fake_settings;
  }
  virtual void SetFullRootLayerDamage() OVERRIDE {
    set_full_root_layer_damage_count_++;
  }
  virtual bool HasImplThread() const OVERRIDE { return false; }
  virtual bool ShouldClearRootRenderPass() const OVERRIDE { return true; }
  virtual CompositorFrameMetadata MakeCompositorFrameMetadata() const OVERRIDE {
    return CompositorFrameMetadata();
  }
  virtual bool AllowPartialSwap() const OVERRIDE {
    return true;
  }
  virtual bool ExternalStencilTestEnabled() const OVERRIDE {
    return external_stencil_test_enabled_;
  }

  void EnableExternalStencilTest() {
    external_stencil_test_enabled_ = true;
  }

  // Methods added for test.
  int set_full_root_layer_damage_count() const {
    return set_full_root_layer_damage_count_;
  }
  void set_viewport_and_scale(
      gfx::Size viewport_size, float scale_factor) {
    viewport_size_ = viewport_size;
    scale_factor_ = scale_factor;
  }

  RenderPass* root_render_pass() { return render_passes_in_draw_order_.back(); }
  RenderPassList* render_passes_in_draw_order() {
    return &render_passes_in_draw_order_;
  }

 private:
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  int set_full_root_layer_damage_count_;
  scoped_ptr<LayerImpl> root_layer_;
  RenderPassList render_passes_in_draw_order_;
  gfx::Size viewport_size_;
  float scale_factor_;
  bool external_stencil_test_enabled_;
};

class FakeRendererGL : public GLRenderer {
 public:
  FakeRendererGL(RendererClient* client,
                 OutputSurface* output_surface,
                 ResourceProvider* resource_provider)
      : GLRenderer(client, output_surface, resource_provider, 0) {}

  // GLRenderer methods.

  // Changing visibility to public.
  using GLRenderer::Initialize;
  using GLRenderer::IsBackbufferDiscarded;
  using GLRenderer::DoDrawQuad;
  using GLRenderer::BeginDrawingFrame;
  using GLRenderer::FinishDrawingQuadList;
  using GLRenderer::stencil_enabled;
};

class GLRendererTest : public testing::Test {
 protected:
  GLRendererTest()
      : output_surface_(FakeOutputSurface::Create3d(
            scoped_ptr<WebKit::WebGraphicsContext3D>(
                new FrameCountingContext()))),
        resource_provider_(ResourceProvider::Create(output_surface_.get(), 0)),
        renderer_(&mock_client_,
                  output_surface_.get(),
                  resource_provider_.get()) {}

  virtual void SetUp() { renderer_.Initialize(); }

  void SwapBuffers() { renderer_.SwapBuffers(); }

  FrameCountingContext* Context() {
    return static_cast<FrameCountingContext*>(output_surface_->context3d());
  }

  scoped_ptr<OutputSurface> output_surface_;
  FakeRendererClient mock_client_;
  scoped_ptr<ResourceProvider> resource_provider_;
  FakeRendererGL renderer_;
};

// Closing the namespace here so that GLRendererShaderTest can take advantage
// of the friend relationship with GLRenderer and all of the mock classes
// declared above it.
}  // namespace


// Gives unique shader ids and unique program ids for tests that need them.
class ShaderCreatorMockGraphicsContext : public TestWebGraphicsContext3D {
 public:
  ShaderCreatorMockGraphicsContext()
      : next_program_id_number_(10000),
        next_shader_id_number_(1) {}

  bool hasShader(WebGLId shader) {
    return shader_set_.find(shader) != shader_set_.end();
  }

  bool hasProgram(WebGLId program) {
    return program_set_.find(program) != program_set_.end();
  }

  virtual WebGLId createProgram() {
    unsigned program = next_program_id_number_;
    program_set_.insert(program);
    next_program_id_number_++;
    return program;
  }

  virtual void deleteProgram(WebGLId program) {
    ASSERT_TRUE(hasProgram(program));
    program_set_.erase(program);
  }

  virtual void useProgram(WebGLId program) {
    if (!program)
      return;
    ASSERT_TRUE(hasProgram(program));
  }

  virtual WebKit::WebGLId createShader(WebKit::WGC3Denum) {
    unsigned shader = next_shader_id_number_;
    shader_set_.insert(shader);
    next_shader_id_number_++;
    return shader;
  }

  virtual void deleteShader(WebKit::WebGLId shader) {
    ASSERT_TRUE(hasShader(shader));
    shader_set_.erase(shader);
  }

  virtual void attachShader(WebGLId program, WebGLId shader) {
    ASSERT_TRUE(hasProgram(program));
    ASSERT_TRUE(hasShader(shader));
  }

 protected:
  unsigned next_program_id_number_;
  unsigned next_shader_id_number_;
  std::set<unsigned> program_set_;
  std::set<unsigned> shader_set_;
};

class GLRendererShaderTest : public testing::Test {
 protected:
  GLRendererShaderTest()
      : output_surface_(FakeOutputSurface::Create3d(
            scoped_ptr<WebKit::WebGraphicsContext3D>(
                new ShaderCreatorMockGraphicsContext()))),
        resource_provider_(ResourceProvider::Create(output_surface_.get(), 0)),
        renderer_(scoped_ptr<FakeRendererGL>(
            new FakeRendererGL(&mock_client_,
                               output_surface_.get(),
                               resource_provider_.get()))) {
    renderer_->Initialize();
  }

  void TestRenderPassProgram() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_program_);
    EXPECT_EQ(renderer_->render_pass_program_->program(),
              renderer_->program_shadow_);
  }

  void TestRenderPassColorMatrixProgram() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_color_matrix_program_);
    EXPECT_EQ(renderer_->render_pass_color_matrix_program_->program(),
              renderer_->program_shadow_);
  }

  void TestRenderPassMaskProgram() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_mask_program_);
    EXPECT_EQ(renderer_->render_pass_mask_program_->program(),
              renderer_->program_shadow_);
  }

  void TestRenderPassMaskColorMatrixProgram() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_mask_color_matrix_program_);
    EXPECT_EQ(renderer_->render_pass_mask_color_matrix_program_->program(),
              renderer_->program_shadow_);
  }

  void TestRenderPassProgramAA() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_program_aa_);
    EXPECT_EQ(renderer_->render_pass_program_aa_->program(),
              renderer_->program_shadow_);
  }

  void TestRenderPassColorMatrixProgramAA() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_color_matrix_program_aa_);
    EXPECT_EQ(renderer_->render_pass_color_matrix_program_aa_->program(),
              renderer_->program_shadow_);
  }

  void TestRenderPassMaskProgramAA() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_mask_program_aa_);
    EXPECT_EQ(renderer_->render_pass_mask_program_aa_->program(),
              renderer_->program_shadow_);
  }

  void TestRenderPassMaskColorMatrixProgramAA() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_mask_color_matrix_program_aa_);
    EXPECT_EQ(renderer_->render_pass_mask_color_matrix_program_aa_->program(),
              renderer_->program_shadow_);
  }

  void TestSolidColorProgramAA() {
    EXPECT_PROGRAM_VALID(renderer_->solid_color_program_aa_);
    EXPECT_EQ(renderer_->solid_color_program_aa_->program(),
              renderer_->program_shadow_);
  }

  scoped_ptr<OutputSurface> output_surface_;
  FakeRendererClient mock_client_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<FakeRendererGL> renderer_;
};

namespace {

// Test GLRenderer discardBackbuffer functionality:
// Suggest recreating framebuffer when one already exists.
// Expected: it does nothing.
TEST_F(GLRendererTest, SuggestBackbufferYesWhenItAlreadyExistsShouldDoNothing) {
  renderer_.SetDiscardBackBufferWhenNotVisible(false);
  EXPECT_EQ(0, mock_client_.set_full_root_layer_damage_count());
  EXPECT_FALSE(renderer_.IsBackbufferDiscarded());

  SwapBuffers();
  EXPECT_EQ(1, Context()->frame_count());
}

// Test GLRenderer DiscardBackbuffer functionality:
// Suggest discarding framebuffer when one exists and the renderer is not
// visible.
// Expected: it is discarded and damage tracker is reset.
TEST_F(
    GLRendererTest,
    SuggestBackbufferNoShouldDiscardBackbufferAndDamageRootLayerIfNotVisible) {
  renderer_.SetVisible(false);
  renderer_.SetDiscardBackBufferWhenNotVisible(true);
  EXPECT_EQ(1, mock_client_.set_full_root_layer_damage_count());
  EXPECT_TRUE(renderer_.IsBackbufferDiscarded());
}

// Test GLRenderer DiscardBackbuffer functionality:
// Suggest discarding framebuffer when one exists and the renderer is visible.
// Expected: the allocation is ignored.
TEST_F(GLRendererTest, SuggestBackbufferNoDoNothingWhenVisible) {
  renderer_.SetVisible(true);
  renderer_.SetDiscardBackBufferWhenNotVisible(true);
  EXPECT_EQ(0, mock_client_.set_full_root_layer_damage_count());
  EXPECT_FALSE(renderer_.IsBackbufferDiscarded());
}

// Test GLRenderer DiscardBackbuffer functionality:
// Suggest discarding framebuffer when one does not exist.
// Expected: it does nothing.
TEST_F(GLRendererTest, SuggestBackbufferNoWhenItDoesntExistShouldDoNothing) {
  renderer_.SetVisible(false);
  renderer_.SetDiscardBackBufferWhenNotVisible(true);
  EXPECT_EQ(1, mock_client_.set_full_root_layer_damage_count());
  EXPECT_TRUE(renderer_.IsBackbufferDiscarded());

  renderer_.SetDiscardBackBufferWhenNotVisible(true);
  EXPECT_EQ(1, mock_client_.set_full_root_layer_damage_count());
  EXPECT_TRUE(renderer_.IsBackbufferDiscarded());
}

// Test GLRenderer DiscardBackbuffer functionality:
// Begin drawing a frame while a framebuffer is discarded.
// Expected: will recreate framebuffer.
TEST_F(GLRendererTest, DiscardedBackbufferIsRecreatedForScopeDuration) {
  renderer_.SetVisible(false);
  renderer_.SetDiscardBackBufferWhenNotVisible(true);
  EXPECT_TRUE(renderer_.IsBackbufferDiscarded());
  EXPECT_EQ(1, mock_client_.set_full_root_layer_damage_count());

  renderer_.SetVisible(true);
  renderer_.DrawFrame(mock_client_.render_passes_in_draw_order());
  EXPECT_FALSE(renderer_.IsBackbufferDiscarded());

  SwapBuffers();
  EXPECT_EQ(1, Context()->frame_count());
}

TEST_F(GLRendererTest, FramebufferDiscardedAfterReadbackWhenNotVisible) {
  renderer_.SetVisible(false);
  renderer_.SetDiscardBackBufferWhenNotVisible(true);
  EXPECT_TRUE(renderer_.IsBackbufferDiscarded());
  EXPECT_EQ(1, mock_client_.set_full_root_layer_damage_count());

  char pixels[4];
  renderer_.DrawFrame(mock_client_.render_passes_in_draw_order());
  EXPECT_FALSE(renderer_.IsBackbufferDiscarded());

  renderer_.GetFramebufferPixels(pixels, gfx::Rect(0, 0, 1, 1));
  EXPECT_TRUE(renderer_.IsBackbufferDiscarded());
  EXPECT_EQ(2, mock_client_.set_full_root_layer_damage_count());
}

TEST_F(GLRendererTest, ExternalStencil) {
  EXPECT_FALSE(renderer_.stencil_enabled());

  mock_client_.EnableExternalStencilTest();
  mock_client_.root_render_pass()->has_transparent_background = false;

  renderer_.DrawFrame(mock_client_.render_passes_in_draw_order());
  EXPECT_TRUE(renderer_.stencil_enabled());
}

class ForbidSynchronousCallContext : public TestWebGraphicsContext3D {
 public:
  ForbidSynchronousCallContext() {}

  virtual bool getActiveAttrib(WebGLId program,
                               WGC3Duint index,
                               ActiveInfo& info) {
    ADD_FAILURE();
    return false;
  }
  virtual bool getActiveUniform(WebGLId program,
                                WGC3Duint index,
                                ActiveInfo& info) {
    ADD_FAILURE();
    return false;
  }
  virtual void getAttachedShaders(WebGLId program,
                                  WGC3Dsizei max_count,
                                  WGC3Dsizei* count,
                                  WebGLId* shaders) {
    ADD_FAILURE();
  }
  virtual WGC3Dint getAttribLocation(WebGLId program, const WGC3Dchar* name) {
    ADD_FAILURE();
    return 0;
  }
  virtual void getBooleanv(WGC3Denum pname, WGC3Dboolean* value) {
    ADD_FAILURE();
  }
  virtual void getBufferParameteriv(WGC3Denum target,
                                    WGC3Denum pname,
                                    WGC3Dint* value) {
    ADD_FAILURE();
  }
  virtual Attributes getContextAttributes() {
    ADD_FAILURE();
    return attributes_;
  }
  virtual WGC3Denum getError() {
    ADD_FAILURE();
    return 0;
  }
  virtual void getFloatv(WGC3Denum pname, WGC3Dfloat* value) { ADD_FAILURE(); }
  virtual void getFramebufferAttachmentParameteriv(WGC3Denum target,
                                                   WGC3Denum attachment,
                                                   WGC3Denum pname,
                                                   WGC3Dint* value) {
    ADD_FAILURE();
  }
  virtual void getIntegerv(WGC3Denum pname, WGC3Dint* value) {
    if (pname == GL_MAX_TEXTURE_SIZE) {
      // MAX_TEXTURE_SIZE is cached client side, so it's OK to query.
      *value = 1024;
    } else {
      ADD_FAILURE();
    }
  }

  // We allow querying the shader compilation and program link status in debug
  // mode, but not release.
  virtual void getProgramiv(WebGLId program, WGC3Denum pname, WGC3Dint* value) {
#ifndef NDEBUG
    *value = 1;
#else
    ADD_FAILURE();
#endif
  }

  virtual void getShaderiv(WebGLId shader, WGC3Denum pname, WGC3Dint* value) {
#ifndef NDEBUG
    *value = 1;
#else
    ADD_FAILURE();
#endif
  }

  virtual WebString getString(WGC3Denum name) {
    // We allow querying the extension string.
    // TODO(enne): It'd be better to check that we only do this before starting
    // any other expensive work (like starting a compilation)
    if (name != GL_EXTENSIONS)
      ADD_FAILURE();
    return WebString();
  }

  virtual WebString getProgramInfoLog(WebGLId program) {
    ADD_FAILURE();
    return WebString();
  }
  virtual void getRenderbufferParameteriv(WGC3Denum target,
                                          WGC3Denum pname,
                                          WGC3Dint* value) {
    ADD_FAILURE();
  }

  virtual WebString getShaderInfoLog(WebGLId shader) {
    ADD_FAILURE();
    return WebString();
  }
  virtual void getShaderPrecisionFormat(WGC3Denum shadertype,
                                        WGC3Denum precisiontype,
                                        WGC3Dint* range,
                                        WGC3Dint* precision) {
    ADD_FAILURE();
  }
  virtual WebString getShaderSource(WebGLId shader) {
    ADD_FAILURE();
    return WebString();
  }
  virtual void getTexParameterfv(WGC3Denum target,
                                 WGC3Denum pname,
                                 WGC3Dfloat* value) {
    ADD_FAILURE();
  }
  virtual void getTexParameteriv(WGC3Denum target,
                                 WGC3Denum pname,
                                 WGC3Dint* value) {
    ADD_FAILURE();
  }
  virtual void getUniformfv(WebGLId program,
                            WGC3Dint location,
                            WGC3Dfloat* value) {
    ADD_FAILURE();
  }
  virtual void getUniformiv(WebGLId program,
                            WGC3Dint location,
                            WGC3Dint* value) {
    ADD_FAILURE();
  }
  virtual WGC3Dint getUniformLocation(WebGLId program, const WGC3Dchar* name) {
    ADD_FAILURE();
    return 0;
  }
  virtual void getVertexAttribfv(WGC3Duint index,
                                 WGC3Denum pname,
                                 WGC3Dfloat* value) {
    ADD_FAILURE();
  }
  virtual void getVertexAttribiv(WGC3Duint index,
                                 WGC3Denum pname,
                                 WGC3Dint* value) {
    ADD_FAILURE();
  }
  virtual WGC3Dsizeiptr getVertexAttribOffset(WGC3Duint index,
                                              WGC3Denum pname) {
    ADD_FAILURE();
    return 0;
  }
};

// This test isn't using the same fixture as GLRendererTest, and you can't mix
// TEST() and TEST_F() with the same name, Hence LRC2.
TEST(GLRendererTest2, InitializationDoesNotMakeSynchronousCalls) {
  FakeRendererClient mock_client;
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new ForbidSynchronousCallContext)));
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0));
  FakeRendererGL renderer(
      &mock_client, output_surface.get(), resource_provider.get());

  EXPECT_TRUE(renderer.Initialize());
}

class LoseContextOnFirstGetContext : public TestWebGraphicsContext3D {
 public:
  LoseContextOnFirstGetContext() : context_lost_(false) {}

  virtual bool makeContextCurrent() OVERRIDE { return !context_lost_; }

  virtual void getProgramiv(WebGLId program, WGC3Denum pname, WGC3Dint* value)
      OVERRIDE {
    context_lost_ = true;
    *value = 0;
  }

  virtual void getShaderiv(WebGLId shader, WGC3Denum pname, WGC3Dint* value)
      OVERRIDE {
    context_lost_ = true;
    *value = 0;
  }

  virtual WGC3Denum getGraphicsResetStatusARB() OVERRIDE {
    return context_lost_ ? 1 : 0;
  }

 private:
  bool context_lost_;
};

TEST(GLRendererTest2, InitializationWithQuicklyLostContextDoesNotAssert) {
  FakeRendererClient mock_client;
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new LoseContextOnFirstGetContext)));
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0));
  FakeRendererGL renderer(
      &mock_client, output_surface.get(), resource_provider.get());

  renderer.Initialize();
}

class ClearCountingContext : public TestWebGraphicsContext3D {
 public:
  ClearCountingContext() : clear_(0) {}

  virtual void clear(WGC3Dbitfield) { clear_++; }

  int clear_count() const { return clear_; }

 private:
  int clear_;
};

TEST(GLRendererTest2, OpaqueBackground) {
  FakeRendererClient mock_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      scoped_ptr<WebKit::WebGraphicsContext3D>(new ClearCountingContext)));
  ClearCountingContext* context =
      static_cast<ClearCountingContext*>(output_surface->context3d());
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0));
  FakeRendererGL renderer(
      &mock_client, output_surface.get(), resource_provider.get());

  mock_client.root_render_pass()->has_transparent_background = false;

  EXPECT_TRUE(renderer.Initialize());

  renderer.DrawFrame(mock_client.render_passes_in_draw_order());

// On DEBUG builds, render passes with opaque background clear to blue to
// easily see regions that were not drawn on the screen.
#ifdef NDEBUG
  EXPECT_EQ(0, context->clear_count());
#else
  EXPECT_EQ(1, context->clear_count());
#endif
}

TEST(GLRendererTest2, TransparentBackground) {
  FakeRendererClient mock_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      scoped_ptr<WebKit::WebGraphicsContext3D>(new ClearCountingContext)));
  ClearCountingContext* context =
      static_cast<ClearCountingContext*>(output_surface->context3d());
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0));
  FakeRendererGL renderer(
      &mock_client, output_surface.get(), resource_provider.get());

  mock_client.root_render_pass()->has_transparent_background = true;

  EXPECT_TRUE(renderer.Initialize());

  renderer.DrawFrame(mock_client.render_passes_in_draw_order());

  EXPECT_EQ(1, context->clear_count());
}

class VisibilityChangeIsLastCallTrackingContext
    : public TestWebGraphicsContext3D {
 public:
  VisibilityChangeIsLastCallTrackingContext()
      : last_call_was_set_visibility_(false) {}

  // WebGraphicsContext3D methods.
  virtual void setVisibilityCHROMIUM(bool visible) {
    DCHECK(last_call_was_set_visibility_ == false);
    last_call_was_set_visibility_ = true;
  }
  virtual void flush() {
    last_call_was_set_visibility_ = false;
  }
  virtual void deleteTexture(WebGLId) {
    last_call_was_set_visibility_ = false;
  }
  virtual void deleteFramebuffer(WebGLId) {
    last_call_was_set_visibility_ = false;
  }
  virtual void deleteQueryEXT(WebGLId) {
    last_call_was_set_visibility_ = false;
  }
  virtual void deleteRenderbuffer(WebGLId) {
    last_call_was_set_visibility_ = false;
  }
  virtual void discardBackbufferCHROMIUM() {
    last_call_was_set_visibility_ = false;
  }
  virtual void ensureBackbufferCHROMIUM() {
    last_call_was_set_visibility_ = false;
  }

  // This method would normally do a glSwapBuffers under the hood.
  virtual WebString getString(WebKit::WGC3Denum name) {
    if (name == GL_EXTENSIONS)
      return WebString(
          "GL_CHROMIUM_set_visibility GL_CHROMIUM_gpu_memory_manager "
          "GL_CHROMIUM_discard_backbuffer");
    return WebString();
  }

  // Methods added for test.
  bool last_call_was_set_visibility() const {
    return last_call_was_set_visibility_;
  }

 private:
  bool last_call_was_set_visibility_;
};

TEST(GLRendererTest2, VisibilityChangeIsLastCall) {
  FakeRendererClient mock_client;
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new VisibilityChangeIsLastCallTrackingContext)));
  VisibilityChangeIsLastCallTrackingContext* context =
      static_cast<VisibilityChangeIsLastCallTrackingContext*>(
          output_surface->context3d());
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0));
  FakeRendererGL renderer(
      &mock_client, output_surface.get(), resource_provider.get());

  EXPECT_TRUE(renderer.Initialize());

  // Ensure that the call to setVisibilityCHROMIUM is the last call issue to the
  // GPU process, after glFlush is called, and after the RendererClient's
  // SetManagedMemoryPolicy is called. Plumb this tracking between both the
  // RenderClient and the Context by giving them both a pointer to a variable on
  // the stack.
  renderer.SetVisible(true);
  renderer.DrawFrame(mock_client.render_passes_in_draw_order());
  renderer.SetVisible(false);
  EXPECT_TRUE(context->last_call_was_set_visibility());
}

class TextureStateTrackingContext : public TestWebGraphicsContext3D {
 public:
  TextureStateTrackingContext() : active_texture_(GL_INVALID_ENUM) {}

  virtual WebString getString(WGC3Denum name) {
    if (name == GL_EXTENSIONS)
      return WebString("GL_OES_EGL_image_external");
    return WebString();
  }

  MOCK_METHOD3(texParameteri,
               void(WGC3Denum target, WGC3Denum pname, WGC3Dint param));
  MOCK_METHOD4(drawElements,
               void(WGC3Denum mode,
                    WGC3Dsizei count,
                    WGC3Denum type,
                    WGC3Dintptr offset));

  virtual void activeTexture(WGC3Denum texture) {
    EXPECT_NE(texture, active_texture_);
    active_texture_ = texture;
  }

  WGC3Denum active_texture() const { return active_texture_; }

 private:
  WGC3Denum active_texture_;
};

TEST(GLRendererTest2, ActiveTextureState) {
  FakeRendererClient fake_client;
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new TextureStateTrackingContext)));
  TextureStateTrackingContext* context =
      static_cast<TextureStateTrackingContext*>(output_surface->context3d());
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0));
  FakeRendererGL renderer(
      &fake_client, output_surface.get(), resource_provider.get());

  // During initialization we are allowed to set any texture parameters.
  EXPECT_CALL(*context, texParameteri(_, _, _)).Times(AnyNumber());
  EXPECT_TRUE(renderer.Initialize());

  cc::RenderPass::Id id(1, 1);
  scoped_ptr<TestRenderPass> pass = TestRenderPass::Create();
  pass->SetNew(id,
               gfx::Rect(0, 0, 100, 100),
               gfx::Rect(0, 0, 100, 100),
               gfx::Transform());
  pass->AppendOneOfEveryQuadType(resource_provider.get(), RenderPass::Id(2, 1));

  // Set up expected texture filter state transitions that match the quads
  // created in AppendOneOfEveryQuadType().
  Mock::VerifyAndClearExpectations(context);
  {
    InSequence sequence;

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
    EXPECT_CALL(*context, drawElements(_, _, _, _)).Times(6);
  }

  cc::DirectRenderer::DrawingFrame drawing_frame;
  renderer.BeginDrawingFrame(&drawing_frame);
  EXPECT_EQ(static_cast<unsigned>(GL_TEXTURE0), context->active_texture());

  for (cc::QuadList::BackToFrontIterator
           it = pass->quad_list.BackToFrontBegin();
       it != pass->quad_list.BackToFrontEnd();
       ++it) {
    renderer.DoDrawQuad(&drawing_frame, *it);
  }
  renderer.FinishDrawingQuadList();
  EXPECT_EQ(static_cast<unsigned>(GL_TEXTURE0), context->active_texture());
  Mock::VerifyAndClearExpectations(context);
}

class NoClearRootRenderPassFakeClient : public FakeRendererClient {
 public:
  virtual bool ShouldClearRootRenderPass() const OVERRIDE { return false; }
};

class NoClearRootRenderPassMockContext : public TestWebGraphicsContext3D {
 public:
  MOCK_METHOD1(clear, void(WGC3Dbitfield mask));
  MOCK_METHOD4(drawElements,
               void(WGC3Denum mode,
                    WGC3Dsizei count,
                    WGC3Denum type,
                    WGC3Dintptr offset));
};

TEST(GLRendererTest2, ShouldClearRootRenderPass) {
  NoClearRootRenderPassFakeClient mock_client;
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new NoClearRootRenderPassMockContext)));
  NoClearRootRenderPassMockContext* mock_context =
      static_cast<NoClearRootRenderPassMockContext*>(
          output_surface->context3d());
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0));
  FakeRendererGL renderer(
      &mock_client, output_surface.get(), resource_provider.get());
  EXPECT_TRUE(renderer.Initialize());

  gfx::Rect viewport_rect(mock_client.DeviceViewport());
  ScopedPtrVector<RenderPass>& render_passes =
      *mock_client.render_passes_in_draw_order();
  render_passes.clear();

  RenderPass::Id root_pass_id(1, 0);
  TestRenderPass* root_pass = AddRenderPass(
      &render_passes, root_pass_id, viewport_rect, gfx::Transform());
  AddQuad(root_pass, viewport_rect, SK_ColorGREEN);

  RenderPass::Id child_pass_id(2, 0);
  TestRenderPass* child_pass = AddRenderPass(
      &render_passes, child_pass_id, viewport_rect, gfx::Transform());
  AddQuad(child_pass, viewport_rect, SK_ColorBLUE);

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
  EXPECT_CALL(*mock_context, clear(clear_bits)).Times(0)
      .After(first_render_pass);

  EXPECT_CALL(*mock_context, drawElements(_, _, _, _)).Times(AnyNumber())
      .After(first_render_pass);

  renderer.DecideRenderPassAllocationsForFrame(
      *mock_client.render_passes_in_draw_order());
  renderer.DrawFrame(mock_client.render_passes_in_draw_order());

  // In multiple render passes all but the root pass should clear the
  // framebuffer.
  Mock::VerifyAndClearExpectations(&mock_context);
}

class ScissorTestOnClearCheckingContext : public TestWebGraphicsContext3D {
 public:
  ScissorTestOnClearCheckingContext() : scissor_enabled_(false) {}

  virtual void clear(WGC3Dbitfield) { EXPECT_FALSE(scissor_enabled_); }

  virtual void enable(WGC3Denum cap) {
    if (cap == GL_SCISSOR_TEST)
      scissor_enabled_ = true;
  }

  virtual void disable(WGC3Denum cap) {
    if (cap == GL_SCISSOR_TEST)
      scissor_enabled_ = false;
  }

 private:
  bool scissor_enabled_;
};

TEST(GLRendererTest2, ScissorTestWhenClearing) {
  FakeRendererClient mock_client;
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new ScissorTestOnClearCheckingContext)));
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0));
  FakeRendererGL renderer(
      &mock_client, output_surface.get(), resource_provider.get());
  EXPECT_TRUE(renderer.Initialize());
  EXPECT_FALSE(renderer.Capabilities().using_partial_swap);

  gfx::Rect viewport_rect(mock_client.DeviceViewport());
  ScopedPtrVector<RenderPass>& render_passes =
      *mock_client.render_passes_in_draw_order();
  render_passes.clear();

  gfx::Rect grand_child_rect(25, 25);
  RenderPass::Id grand_child_pass_id(3, 0);
  TestRenderPass* grand_child_pass = AddRenderPass(
      &render_passes, grand_child_pass_id, grand_child_rect, gfx::Transform());
  AddClippedQuad(grand_child_pass, grand_child_rect, SK_ColorYELLOW);

  gfx::Rect child_rect(50, 50);
  RenderPass::Id child_pass_id(2, 0);
  TestRenderPass* child_pass = AddRenderPass(
      &render_passes, child_pass_id, child_rect, gfx::Transform());
  AddQuad(child_pass, child_rect, SK_ColorBLUE);

  RenderPass::Id root_pass_id(1, 0);
  TestRenderPass* root_pass = AddRenderPass(
      &render_passes, root_pass_id, viewport_rect, gfx::Transform());
  AddQuad(root_pass, viewport_rect, SK_ColorGREEN);

  AddRenderPassQuad(root_pass, child_pass);
  AddRenderPassQuad(child_pass, grand_child_pass);

  renderer.DecideRenderPassAllocationsForFrame(
      *mock_client.render_passes_in_draw_order());
  renderer.DrawFrame(mock_client.render_passes_in_draw_order());
}

class NonReshapableOutputSurface : public FakeOutputSurface {
 public:
  explicit NonReshapableOutputSurface(
      scoped_ptr<WebKit::WebGraphicsContext3D> context3d)
      : FakeOutputSurface(context3d.Pass(), false) {}
  virtual gfx::Size SurfaceSize() const OVERRIDE { return gfx::Size(500, 500); }
};

class OffsetViewportRendererClient : public FakeRendererClient {
 public:
  virtual gfx::Rect DeviceViewport() const OVERRIDE {
    return gfx::Rect(10, 10, 100, 100);
  }
};

class FlippedScissorAndViewportContext : public TestWebGraphicsContext3D {
 public:
  FlippedScissorAndViewportContext()
      : did_call_viewport_(false), did_call_scissor_(false) {}
  virtual ~FlippedScissorAndViewportContext() {
    EXPECT_TRUE(did_call_viewport_);
    EXPECT_TRUE(did_call_scissor_);
  }

  virtual void viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    EXPECT_EQ(10, x);
    EXPECT_EQ(390, y);
    EXPECT_EQ(100, width);
    EXPECT_EQ(100, height);
    did_call_viewport_ = true;
  }

  virtual void scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    EXPECT_EQ(30, x);
    EXPECT_EQ(450, y);
    EXPECT_EQ(20, width);
    EXPECT_EQ(20, height);
    did_call_scissor_ = true;
  }

 private:
  bool did_call_viewport_;
  bool did_call_scissor_;
};

TEST(GLRendererTest2, ScissorAndViewportWithinNonreshapableSurface) {
  // In Android WebView, the OutputSurface is unable to respect reshape() calls
  // and maintains a fixed size. This test verifies that glViewport and
  // glScissor's Y coordinate is flipped correctly in this environment, and that
  // the glViewport can be at a nonzero origin within the surface.
  OffsetViewportRendererClient mock_client;
  scoped_ptr<OutputSurface> output_surface(make_scoped_ptr(
      new NonReshapableOutputSurface(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new FlippedScissorAndViewportContext))));
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), 0));
  FakeRendererGL renderer(
      &mock_client, output_surface.get(), resource_provider.get());
  EXPECT_TRUE(renderer.Initialize());
  EXPECT_FALSE(renderer.Capabilities().using_partial_swap);

  gfx::Rect viewport_rect(mock_client.DeviceViewport().size());
  gfx::Rect quad_rect = gfx::Rect(20, 20, 20, 20);
  ScopedPtrVector<RenderPass>& render_passes =
      *mock_client.render_passes_in_draw_order();
  render_passes.clear();

  RenderPass::Id root_pass_id(1, 0);
  TestRenderPass* root_pass = AddRenderPass(
      &render_passes, root_pass_id, viewport_rect, gfx::Transform());
  AddClippedQuad(root_pass, quad_rect, SK_ColorGREEN);

  renderer.DecideRenderPassAllocationsForFrame(
      *mock_client.render_passes_in_draw_order());
  renderer.DrawFrame(mock_client.render_passes_in_draw_order());
}

TEST_F(GLRendererShaderTest, DrawRenderPassQuadShaderPermutations) {
  gfx::Rect viewport_rect(mock_client_.DeviceViewport());
  ScopedPtrVector<RenderPass>* render_passes =
      mock_client_.render_passes_in_draw_order();

  gfx::Rect child_rect(50, 50);
  RenderPass::Id child_pass_id(2, 0);
  TestRenderPass* child_pass;

  RenderPass::Id root_pass_id(1, 0);
  TestRenderPass* root_pass;

  cc::ResourceProvider::ResourceId mask =
  resource_provider_->CreateResource(gfx::Size(20, 12),
                                     resource_provider_->best_texture_format(),
                                     ResourceProvider::TextureUsageAny);
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
  skia::RefPtr<SkColorFilter> color_filter(
      skia::AdoptRef(new SkColorMatrixFilter(matrix)));
  skia::RefPtr<SkImageFilter> filter = skia::AdoptRef(
      SkColorFilterImageFilter::Create(color_filter.get(), NULL));

  gfx::Transform transform_causing_aa;
  transform_causing_aa.Rotate(20.0);

  // RenderPassProgram
  render_passes->clear();

  child_pass = AddRenderPass(
      render_passes, child_pass_id, child_rect, gfx::Transform());

  root_pass = AddRenderPass(
      render_passes, root_pass_id, viewport_rect, gfx::Transform());

  AddRenderPassQuad(root_pass,
                    child_pass,
                    0,
                    skia::RefPtr<SkImageFilter>(),
                    gfx::Transform());

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassProgram();

  // RenderPassColorMatrixProgram
  render_passes->clear();

  child_pass = AddRenderPass(
      render_passes, child_pass_id, child_rect, transform_causing_aa);

  root_pass = AddRenderPass(
      render_passes, root_pass_id, viewport_rect, gfx::Transform());

  AddRenderPassQuad(root_pass, child_pass, 0, filter, gfx::Transform());

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassColorMatrixProgram();

  // RenderPassMaskProgram
  render_passes->clear();

  child_pass = AddRenderPass(
      render_passes, child_pass_id, child_rect, gfx::Transform());

  root_pass = AddRenderPass(
      render_passes, root_pass_id, viewport_rect, gfx::Transform());

  AddRenderPassQuad(root_pass,
                    child_pass,
                    mask,
                    skia::RefPtr<SkImageFilter>(),
                    gfx::Transform());

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassMaskProgram();

  // RenderPassMaskColorMatrixProgram
  render_passes->clear();

  child_pass = AddRenderPass(
      render_passes, child_pass_id, child_rect, gfx::Transform());

  root_pass = AddRenderPass(
      render_passes, root_pass_id, viewport_rect, gfx::Transform());

  AddRenderPassQuad(root_pass, child_pass, mask, filter, gfx::Transform());

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassMaskColorMatrixProgram();

  // RenderPassProgramAA
  render_passes->clear();

  child_pass = AddRenderPass(
      render_passes, child_pass_id, child_rect, transform_causing_aa);

  root_pass = AddRenderPass(
      render_passes, root_pass_id, viewport_rect, gfx::Transform());

  AddRenderPassQuad(root_pass,
                    child_pass,
                    0,
                    skia::RefPtr<SkImageFilter>(),
                    transform_causing_aa);

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassProgramAA();

  // RenderPassColorMatrixProgramAA
  render_passes->clear();

  child_pass = AddRenderPass(
      render_passes, child_pass_id, child_rect, transform_causing_aa);

  root_pass = AddRenderPass(
      render_passes, root_pass_id, viewport_rect, gfx::Transform());

  AddRenderPassQuad(root_pass, child_pass, 0, filter, transform_causing_aa);

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassColorMatrixProgramAA();

  // RenderPassMaskProgramAA
  render_passes->clear();

  child_pass = AddRenderPass(render_passes, child_pass_id, child_rect,
      transform_causing_aa);

  root_pass = AddRenderPass(render_passes, root_pass_id, viewport_rect,
      gfx::Transform());

  AddRenderPassQuad(root_pass, child_pass, mask, skia::RefPtr<SkImageFilter>(),
      transform_causing_aa);

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassMaskProgramAA();

  // RenderPassMaskColorMatrixProgramAA
  render_passes->clear();

  child_pass = AddRenderPass(render_passes, child_pass_id, child_rect,
      transform_causing_aa);

  root_pass = AddRenderPass(render_passes, root_pass_id, viewport_rect,
      transform_causing_aa);

  AddRenderPassQuad(root_pass, child_pass, mask, filter, transform_causing_aa);

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassMaskColorMatrixProgramAA();
}

// At this time, the AA code path cannot be taken if the surface's rect would
// project incorrectly by the given transform, because of w<0 clipping.
TEST_F(GLRendererShaderTest, DrawRenderPassQuadSkipsAAForClippingTransform) {
  gfx::Rect child_rect(50, 50);
  RenderPass::Id child_pass_id(2, 0);
  TestRenderPass* child_pass;

  gfx::Rect viewport_rect(mock_client_.DeviceViewport());
  RenderPass::Id root_pass_id(1, 0);
  TestRenderPass* root_pass;

  gfx::Transform transform_preventing_aa;
  transform_preventing_aa.ApplyPerspectiveDepth(40.0);
  transform_preventing_aa.RotateAboutYAxis(-20.0);
  transform_preventing_aa.Scale(30.0, 1.0);

  // Verify that the test transform and test rect actually do cause the clipped
  // flag to trigger. Otherwise we are not testing the intended scenario.
  bool clipped = false;
  MathUtil::MapQuad(transform_preventing_aa,
                    gfx::QuadF(child_rect),
                    &clipped);
  ASSERT_TRUE(clipped);

  // Set up the render pass quad to be drawn
  ScopedPtrVector<RenderPass>* render_passes =
      mock_client_.render_passes_in_draw_order();

  render_passes->clear();

  child_pass = AddRenderPass(
      render_passes, child_pass_id, child_rect, transform_preventing_aa);

  root_pass = AddRenderPass(
      render_passes, root_pass_id, viewport_rect, gfx::Transform());

  AddRenderPassQuad(root_pass,
                    child_pass,
                    0,
                    skia::RefPtr<SkImageFilter>(),
                    transform_preventing_aa);

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());

  // If use_aa incorrectly ignores clipping, it will use the
  // RenderPassProgramAA shader instead of the RenderPassProgram.
  TestRenderPassProgram();
}

TEST_F(GLRendererShaderTest, DrawSolidColorShader) {
  gfx::Rect viewport_rect(mock_client_.DeviceViewport());
  ScopedPtrVector<RenderPass>* render_passes =
      mock_client_.render_passes_in_draw_order();

  RenderPass::Id root_pass_id(1, 0);
  TestRenderPass* root_pass;

  gfx::Transform pixel_aligned_transform_causing_aa;
  pixel_aligned_transform_causing_aa.Translate(25.5f, 25.5f);
  pixel_aligned_transform_causing_aa.Scale(0.5f, 0.5f);

  render_passes->clear();

  root_pass = AddRenderPass(
      render_passes, root_pass_id, viewport_rect, gfx::Transform());
  AddTransformedQuad(root_pass,
                     viewport_rect,
                     SK_ColorYELLOW,
                     pixel_aligned_transform_causing_aa);

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());

  TestSolidColorProgramAA();
}

class OutputSurfaceMockContext : public TestWebGraphicsContext3D {
 public:
  // Specifically override methods even if they are unused (used in conjunction
  // with StrictMock). We need to make sure that GLRenderer does not issue
  // framebuffer-related GL calls directly. Instead these are supposed to go
  // through the OutputSurface abstraction.
  MOCK_METHOD0(ensureBackbufferCHROMIUM, void());
  MOCK_METHOD0(discardBackbufferCHROMIUM, void());
  MOCK_METHOD2(bindFramebuffer, void(WGC3Denum target, WebGLId framebuffer));
  MOCK_METHOD0(prepareTexture, void());
  MOCK_METHOD3(reshapeWithScaleFactor,
               void(int width, int height, float scale_factor));
  MOCK_METHOD4(drawElements,
               void(WGC3Denum mode,
                    WGC3Dsizei count,
                    WGC3Denum type,
                    WGC3Dintptr offset));

  virtual WebString getString(WebKit::WGC3Denum name) {
    if (name == GL_EXTENSIONS)
      return WebString(
          "GL_CHROMIUM_post_sub_buffer GL_CHROMIUM_discard_backbuffer");
    return WebString();
  }
};

class MockOutputSurface : public OutputSurface {
 public:
  MockOutputSurface()
      : OutputSurface(scoped_ptr<WebKit::WebGraphicsContext3D>(
            new StrictMock<OutputSurfaceMockContext>)) {
    surface_size_ = gfx::Size(100, 100);
  }
  virtual ~MockOutputSurface() {}

  MOCK_METHOD0(EnsureBackbuffer, void());
  MOCK_METHOD0(DiscardBackbuffer, void());
  MOCK_METHOD2(Reshape, void(gfx::Size size, float scale_factor));
  MOCK_METHOD0(BindFramebuffer, void());
  MOCK_METHOD1(SwapBuffers, void(CompositorFrame* frame));
};

class MockOutputSurfaceTest : public testing::Test, public FakeRendererClient {
 protected:
  MockOutputSurfaceTest()
      : resource_provider_(ResourceProvider::Create(&output_surface_, 0)),
        renderer_(this, &output_surface_, resource_provider_.get()) {}

  virtual void SetUp() { EXPECT_TRUE(renderer_.Initialize()); }

  void SwapBuffers() { renderer_.SwapBuffers(); }

  void DrawFrame() {
    gfx::Rect viewport_rect(DeviceViewport());
    ScopedPtrVector<RenderPass>* render_passes = render_passes_in_draw_order();
    render_passes->clear();

    RenderPass::Id render_pass_id(1, 0);
    TestRenderPass* render_pass = AddRenderPass(
        render_passes, render_pass_id, viewport_rect, gfx::Transform());
    AddQuad(render_pass, viewport_rect, SK_ColorGREEN);

    EXPECT_CALL(output_surface_, EnsureBackbuffer()).WillRepeatedly(Return());

    EXPECT_CALL(output_surface_,
                Reshape(DeviceViewport().size(), DeviceScaleFactor())).Times(1);

    EXPECT_CALL(output_surface_, BindFramebuffer()).Times(1);

    EXPECT_CALL(*Context(), drawElements(_, _, _, _)).Times(1);

    renderer_.DecideRenderPassAllocationsForFrame(
        *render_passes_in_draw_order());
    renderer_.DrawFrame(render_passes_in_draw_order());
  }

  OutputSurfaceMockContext* Context() {
    return static_cast<OutputSurfaceMockContext*>(output_surface_.context3d());
  }

  StrictMock<MockOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  FakeRendererGL renderer_;
};

TEST_F(MockOutputSurfaceTest, DrawFrameAndSwap) {
  DrawFrame();

  EXPECT_CALL(output_surface_, SwapBuffers(_)).Times(1);
  renderer_.SwapBuffers();
}

TEST_F(MockOutputSurfaceTest, DrawFrameAndResizeAndSwap) {
  DrawFrame();
  EXPECT_CALL(output_surface_, SwapBuffers(_)).Times(1);
  renderer_.SwapBuffers();

  set_viewport_and_scale(gfx::Size(2, 2), 2.f);
  renderer_.ViewportChanged();

  DrawFrame();
  EXPECT_CALL(output_surface_, SwapBuffers(_)).Times(1);
  renderer_.SwapBuffers();

  DrawFrame();
  EXPECT_CALL(output_surface_, SwapBuffers(_)).Times(1);
  renderer_.SwapBuffers();

  set_viewport_and_scale(gfx::Size(1, 1), 1.f);
  renderer_.ViewportChanged();

  DrawFrame();
  EXPECT_CALL(output_surface_, SwapBuffers(_)).Times(1);
  renderer_.SwapBuffers();
}

class GLRendererTestSyncPoint : public GLRendererPixelTest {
 protected:
  static void SyncPointCallback(int* callback_count) {
    ++(*callback_count);
    base::MessageLoop::current()->QuitWhenIdle();
  }

  static void OtherCallback(int* callback_count) {
    ++(*callback_count);
    base::MessageLoop::current()->QuitWhenIdle();
  }
};

#if !defined(OS_ANDROID)
TEST_F(GLRendererTestSyncPoint, SignalSyncPointOnLostContext) {
  int sync_point_callback_count = 0;
  int other_callback_count = 0;
  unsigned sync_point = output_surface_->context3d()->insertSyncPoint();

  output_surface_->context3d()->loseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);

  SyncPointHelper::SignalSyncPoint(
      output_surface_->context3d(),
      sync_point,
      base::Bind(&SyncPointCallback, &sync_point_callback_count));
  EXPECT_EQ(0, sync_point_callback_count);
  EXPECT_EQ(0, other_callback_count);

  // Make the sync point happen.
  output_surface_->context3d()->finish();
  // Post a task after the sync point.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&OtherCallback, &other_callback_count));

  base::MessageLoop::current()->Run();

  // The sync point shouldn't have happened since the context was lost.
  EXPECT_EQ(0, sync_point_callback_count);
  EXPECT_EQ(1, other_callback_count);
}

TEST_F(GLRendererTestSyncPoint, SignalSyncPoint) {
  int sync_point_callback_count = 0;
  int other_callback_count = 0;
  unsigned sync_point = output_surface_->context3d()->insertSyncPoint();

  SyncPointHelper::SignalSyncPoint(
      output_surface_->context3d(),
      sync_point,
      base::Bind(&SyncPointCallback, &sync_point_callback_count));
  EXPECT_EQ(0, sync_point_callback_count);
  EXPECT_EQ(0, other_callback_count);

  // Make the sync point happen.
  output_surface_->context3d()->finish();
  // Post a task after the sync point.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&OtherCallback, &other_callback_count));

  base::MessageLoop::current()->Run();

  // The sync point should have happened.
  EXPECT_EQ(1, sync_point_callback_count);
  EXPECT_EQ(1, other_callback_count);
}
#endif  // OS_ANDROID

}  // namespace
}  // namespace cc
