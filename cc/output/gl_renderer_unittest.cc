// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/gl_renderer.h"

#include <set>

#include "cc/base/math_util.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource_provider.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/pixel_test.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
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
using testing::Args;
using testing::AtLeast;
using testing::ElementsAre;
using testing::Expectation;
using testing::InSequence;
using testing::Mock;
using testing::Return;
using testing::StrictMock;
using blink::WebGLId;
using blink::WebString;
using blink::WGC3Dbitfield;
using blink::WGC3Dboolean;
using blink::WGC3Dchar;
using blink::WGC3Denum;
using blink::WGC3Dfloat;
using blink::WGC3Dint;
using blink::WGC3Dintptr;
using blink::WGC3Dsizei;
using blink::WGC3Dsizeiptr;
using blink::WGC3Duint;

namespace cc {

#define EXPECT_PROGRAM_VALID(program_binding)                                  \
  do {                                                                         \
    EXPECT_TRUE((program_binding)->program());                                 \
    EXPECT_TRUE((program_binding)->initialized());                             \
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
  FrameCountingContext()
      : frame_(0) {
    test_capabilities_.set_visibility = true;
    test_capabilities_.discard_backbuffer = true;
  }

  // WebGraphicsContext3D methods.

  // This method would normally do a glSwapBuffers under the hood.
  virtual void prepareTexture() { frame_++; }

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
        viewport_(gfx::Rect(0, 0, 1, 1)),
        clip_(gfx::Rect(0, 0, 1, 1)) {
    root_layer_->CreateRenderSurface();
    RenderPass::Id render_pass_id =
        root_layer_->render_surface()->RenderPassId();
    scoped_ptr<RenderPass> root_render_pass = RenderPass::Create();
    root_render_pass->SetNew(
        render_pass_id, gfx::Rect(), gfx::Rect(), gfx::Transform());
    render_passes_in_draw_order_.push_back(root_render_pass.Pass());
  }

  // RendererClient methods.
  virtual gfx::Rect DeviceViewport() const OVERRIDE { return viewport_; }
  virtual gfx::Rect DeviceClip() const OVERRIDE { return clip_; }
  virtual void SetFullRootLayerDamage() OVERRIDE {
    set_full_root_layer_damage_count_++;
  }
  virtual CompositorFrameMetadata MakeCompositorFrameMetadata() const OVERRIDE {
    return CompositorFrameMetadata();
  }

  // Methods added for test.
  int set_full_root_layer_damage_count() const {
    return set_full_root_layer_damage_count_;
  }
  void set_viewport(gfx::Rect viewport) { viewport_ = viewport; }
  void set_clip(gfx::Rect clip) { clip_ = clip; }

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
  gfx::Rect viewport_;
  gfx::Rect clip_;
};

class FakeRendererGL : public GLRenderer {
 public:
  FakeRendererGL(RendererClient* client,
                 const LayerTreeSettings* settings,
                 OutputSurface* output_surface,
                 ResourceProvider* resource_provider)
      : GLRenderer(client,
                   settings,
                   output_surface,
                   resource_provider,
                   NULL,
                   0) {}

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
  GLRendererTest() {
    scoped_ptr<FrameCountingContext> context3d(new FrameCountingContext);
    context3d_ = context3d.get();

    output_surface_ = FakeOutputSurface::Create3d(
        context3d.PassAs<TestWebGraphicsContext3D>()).Pass();
    CHECK(output_surface_->BindToClient(&output_surface_client_));

    resource_provider_ = ResourceProvider::Create(
        output_surface_.get(), NULL, 0, false, 1).Pass();
    renderer_ = make_scoped_ptr(new FakeRendererGL(&renderer_client_,
                                                   &settings_,
                                                   output_surface_.get(),
                                                   resource_provider_.get()));
  }

  virtual void SetUp() { renderer_->Initialize(); }

  void SwapBuffers() { renderer_->SwapBuffers(); }

  LayerTreeSettings settings_;
  FrameCountingContext* context3d_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  FakeRendererClient renderer_client_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<FakeRendererGL> renderer_;
};

// Closing the namespace here so that GLRendererShaderTest can take advantage
// of the friend relationship with GLRenderer and all of the mock classes
// declared above it.
}  // namespace

class GLRendererShaderTest : public testing::Test {
 protected:
  GLRendererShaderTest() {
    output_surface_ = FakeOutputSurface::Create3d().Pass();
    CHECK(output_surface_->BindToClient(&output_surface_client_));

    resource_provider_ = ResourceProvider::Create(
        output_surface_.get(), NULL, 0, false, 1).Pass();
    renderer_.reset(new FakeRendererGL(&renderer_client_,
                                       &settings_,
                                       output_surface_.get(),
                                       resource_provider_.get()));
    renderer_->Initialize();
  }

  void TestRenderPassProgram() {
    EXPECT_PROGRAM_VALID(&renderer_->render_pass_program_);
    EXPECT_EQ(renderer_->render_pass_program_.program(),
              renderer_->program_shadow_);
  }

  void TestRenderPassColorMatrixProgram() {
    EXPECT_PROGRAM_VALID(&renderer_->render_pass_color_matrix_program_);
    EXPECT_EQ(renderer_->render_pass_color_matrix_program_.program(),
              renderer_->program_shadow_);
  }

  void TestRenderPassMaskProgram() {
    EXPECT_PROGRAM_VALID(&renderer_->render_pass_mask_program_);
    EXPECT_EQ(renderer_->render_pass_mask_program_.program(),
              renderer_->program_shadow_);
  }

  void TestRenderPassMaskColorMatrixProgram() {
    EXPECT_PROGRAM_VALID(&renderer_->render_pass_mask_color_matrix_program_);
    EXPECT_EQ(renderer_->render_pass_mask_color_matrix_program_.program(),
              renderer_->program_shadow_);
  }

  void TestRenderPassProgramAA() {
    EXPECT_PROGRAM_VALID(&renderer_->render_pass_program_aa_);
    EXPECT_EQ(renderer_->render_pass_program_aa_.program(),
              renderer_->program_shadow_);
  }

  void TestRenderPassColorMatrixProgramAA() {
    EXPECT_PROGRAM_VALID(&renderer_->render_pass_color_matrix_program_aa_);
    EXPECT_EQ(renderer_->render_pass_color_matrix_program_aa_.program(),
              renderer_->program_shadow_);
  }

  void TestRenderPassMaskProgramAA() {
    EXPECT_PROGRAM_VALID(&renderer_->render_pass_mask_program_aa_);
    EXPECT_EQ(renderer_->render_pass_mask_program_aa_.program(),
              renderer_->program_shadow_);
  }

  void TestRenderPassMaskColorMatrixProgramAA() {
    EXPECT_PROGRAM_VALID(&renderer_->render_pass_mask_color_matrix_program_aa_);
    EXPECT_EQ(renderer_->render_pass_mask_color_matrix_program_aa_.program(),
              renderer_->program_shadow_);
  }

  void TestSolidColorProgramAA() {
    EXPECT_PROGRAM_VALID(&renderer_->solid_color_program_aa_);
    EXPECT_EQ(renderer_->solid_color_program_aa_.program(),
              renderer_->program_shadow_);
  }

  LayerTreeSettings settings_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  FakeRendererClient renderer_client_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<FakeRendererGL> renderer_;
};

namespace {

// Test GLRenderer DiscardBackbuffer functionality:
// Suggest discarding framebuffer when one exists and the renderer is not
// visible.
// Expected: it is discarded and damage tracker is reset.
TEST_F(
    GLRendererTest,
    SuggestBackbufferNoShouldDiscardBackbufferAndDamageRootLayerIfNotVisible) {
  renderer_->SetVisible(false);
  EXPECT_EQ(1, renderer_client_.set_full_root_layer_damage_count());
  EXPECT_TRUE(renderer_->IsBackbufferDiscarded());
}

// Test GLRenderer DiscardBackbuffer functionality:
// Suggest discarding framebuffer when one exists and the renderer is visible.
// Expected: the allocation is ignored.
TEST_F(GLRendererTest, SuggestBackbufferNoDoNothingWhenVisible) {
  renderer_->SetVisible(true);
  EXPECT_EQ(0, renderer_client_.set_full_root_layer_damage_count());
  EXPECT_FALSE(renderer_->IsBackbufferDiscarded());
}

// Test GLRenderer DiscardBackbuffer functionality:
// Suggest discarding framebuffer when one does not exist.
// Expected: it does nothing.
TEST_F(GLRendererTest, SuggestBackbufferNoWhenItDoesntExistShouldDoNothing) {
  renderer_->SetVisible(false);
  EXPECT_EQ(1, renderer_client_.set_full_root_layer_damage_count());
  EXPECT_TRUE(renderer_->IsBackbufferDiscarded());

  EXPECT_EQ(1, renderer_client_.set_full_root_layer_damage_count());
  EXPECT_TRUE(renderer_->IsBackbufferDiscarded());
}

// Test GLRenderer DiscardBackbuffer functionality:
// Begin drawing a frame while a framebuffer is discarded.
// Expected: will recreate framebuffer.
TEST_F(GLRendererTest, DiscardedBackbufferIsRecreatedForScopeDuration) {
  renderer_->SetVisible(false);
  EXPECT_TRUE(renderer_->IsBackbufferDiscarded());
  EXPECT_EQ(1, renderer_client_.set_full_root_layer_damage_count());

  renderer_->SetVisible(true);
  renderer_->DrawFrame(
      renderer_client_.render_passes_in_draw_order(), NULL, 1.f, true, false);
  EXPECT_FALSE(renderer_->IsBackbufferDiscarded());

  SwapBuffers();
  EXPECT_EQ(1, context3d_->frame_count());
}

TEST_F(GLRendererTest, FramebufferDiscardedAfterReadbackWhenNotVisible) {
  renderer_->SetVisible(false);
  EXPECT_TRUE(renderer_->IsBackbufferDiscarded());
  EXPECT_EQ(1, renderer_client_.set_full_root_layer_damage_count());

  char pixels[4];
  renderer_->DrawFrame(
      renderer_client_.render_passes_in_draw_order(), NULL, 1.f, true, false);
  EXPECT_FALSE(renderer_->IsBackbufferDiscarded());

  renderer_->GetFramebufferPixels(pixels, gfx::Rect(0, 0, 1, 1));
  EXPECT_TRUE(renderer_->IsBackbufferDiscarded());
  EXPECT_EQ(2, renderer_client_.set_full_root_layer_damage_count());
}

TEST_F(GLRendererTest, ExternalStencil) {
  EXPECT_FALSE(renderer_->stencil_enabled());

  output_surface_->set_has_external_stencil_test(true);
  renderer_client_.root_render_pass()->has_transparent_background = false;

  renderer_->DrawFrame(
      renderer_client_.render_passes_in_draw_order(), NULL, 1.f, true, false);
  EXPECT_TRUE(renderer_->stencil_enabled());
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
    ADD_FAILURE() << name;
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
  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      scoped_ptr<TestWebGraphicsContext3D>(new ForbidSynchronousCallContext)));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), NULL, 0, false, 1));

  LayerTreeSettings settings;
  FakeRendererClient renderer_client;
  FakeRendererGL renderer(&renderer_client,
                          &settings,
                          output_surface.get(),
                          resource_provider.get());

  EXPECT_TRUE(renderer.Initialize());
}

class LoseContextOnFirstGetContext : public TestWebGraphicsContext3D {
 public:
  LoseContextOnFirstGetContext() {}

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
};

TEST(GLRendererTest2, InitializationWithQuicklyLostContextDoesNotAssert) {
  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      scoped_ptr<TestWebGraphicsContext3D>(new LoseContextOnFirstGetContext)));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), NULL, 0, false, 1));

  LayerTreeSettings settings;
  FakeRendererClient renderer_client;
  FakeRendererGL renderer(&renderer_client,
                          &settings,
                          output_surface.get(),
                          resource_provider.get());

  renderer.Initialize();
}

class ClearCountingContext : public TestWebGraphicsContext3D {
 public:
  ClearCountingContext() {
    test_capabilities_.discard_framebuffer = true;
  }

  MOCK_METHOD3(discardFramebufferEXT,
               void(WGC3Denum target,
                    WGC3Dsizei numAttachments,
                    const WGC3Denum* attachments));
  MOCK_METHOD1(clear, void(WGC3Dbitfield mask));
};

TEST(GLRendererTest2, OpaqueBackground) {
  scoped_ptr<ClearCountingContext> context_owned(new ClearCountingContext);
  ClearCountingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), NULL, 0, false, 1));

  LayerTreeSettings settings;
  FakeRendererClient renderer_client;
  FakeRendererGL renderer(&renderer_client,
                          &settings,
                          output_surface.get(),
                          resource_provider.get());

  renderer_client.root_render_pass()->has_transparent_background = false;

  EXPECT_TRUE(renderer.Initialize());

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
  renderer.DrawFrame(
      renderer_client.render_passes_in_draw_order(), NULL, 1.f, true, false);
  Mock::VerifyAndClearExpectations(context);
}

TEST(GLRendererTest2, TransparentBackground) {
  scoped_ptr<ClearCountingContext> context_owned(new ClearCountingContext);
  ClearCountingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), NULL, 0, false, 1));

  LayerTreeSettings settings;
  FakeRendererClient renderer_client;
  FakeRendererGL renderer(&renderer_client,
                          &settings,
                          output_surface.get(),
                          resource_provider.get());

  renderer_client.root_render_pass()->has_transparent_background = true;

  EXPECT_TRUE(renderer.Initialize());

  EXPECT_CALL(*context, discardFramebufferEXT(GL_FRAMEBUFFER, 1, _))
      .Times(1);
  EXPECT_CALL(*context, clear(_)).Times(1);
  renderer.DrawFrame(
      renderer_client.render_passes_in_draw_order(), NULL, 1.f, true, false);

  Mock::VerifyAndClearExpectations(context);
}

TEST(GLRendererTest2, OffscreenOutputSurface) {
  scoped_ptr<ClearCountingContext> context_owned(new ClearCountingContext);
  ClearCountingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::CreateOffscreen(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), NULL, 0, false, 1));

  LayerTreeSettings settings;
  FakeRendererClient renderer_client;
  FakeRendererGL renderer(&renderer_client,
                          &settings,
                          output_surface.get(),
                          resource_provider.get());

  EXPECT_TRUE(renderer.Initialize());

  EXPECT_CALL(*context, discardFramebufferEXT(GL_FRAMEBUFFER, _, _))
      .With(Args<2, 1>(ElementsAre(GL_COLOR_ATTACHMENT0)))
      .Times(1);
  EXPECT_CALL(*context, clear(_)).Times(AnyNumber());
  renderer.DrawFrame(
      renderer_client.render_passes_in_draw_order(), NULL, 1.f, true, false);
  Mock::VerifyAndClearExpectations(context);
}

class VisibilityChangeIsLastCallTrackingContext
    : public TestWebGraphicsContext3D {
 public:
  VisibilityChangeIsLastCallTrackingContext()
      : last_call_was_set_visibility_(false) {
    test_capabilities_.set_visibility = true;
    test_capabilities_.discard_backbuffer = true;
  }

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

  // Methods added for test.
  bool last_call_was_set_visibility() const {
    return last_call_was_set_visibility_;
  }

 private:
  bool last_call_was_set_visibility_;
};

TEST(GLRendererTest2, VisibilityChangeIsLastCall) {
  scoped_ptr<VisibilityChangeIsLastCallTrackingContext> context_owned(
      new VisibilityChangeIsLastCallTrackingContext);
  VisibilityChangeIsLastCallTrackingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), NULL, 0, false, 1));

  LayerTreeSettings settings;
  FakeRendererClient renderer_client;
  FakeRendererGL renderer(&renderer_client,
                          &settings,
                          output_surface.get(),
                          resource_provider.get());

  EXPECT_TRUE(renderer.Initialize());

  // Ensure that the call to setVisibilityCHROMIUM is the last call issue to the
  // GPU process, after glFlush is called, and after the RendererClient's
  // SetManagedMemoryPolicy is called. Plumb this tracking between both the
  // RenderClient and the Context by giving them both a pointer to a variable on
  // the stack.
  renderer.SetVisible(true);
  renderer.DrawFrame(
      renderer_client.render_passes_in_draw_order(), NULL, 1.f, true, false);
  renderer.SetVisible(false);
  EXPECT_TRUE(context->last_call_was_set_visibility());
}

class TextureStateTrackingContext : public TestWebGraphicsContext3D {
 public:
  TextureStateTrackingContext()
      : active_texture_(GL_INVALID_ENUM) {
    test_capabilities_.egl_image_external = true;
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
  scoped_ptr<TextureStateTrackingContext> context_owned(
      new TextureStateTrackingContext);
  TextureStateTrackingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), NULL, 0, false, 1));

  LayerTreeSettings settings;
  FakeRendererClient renderer_client;
  FakeRendererGL renderer(&renderer_client,
                          &settings,
                          output_surface.get(),
                          resource_provider.get());

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
  scoped_ptr<NoClearRootRenderPassMockContext> mock_context_owned(
      new NoClearRootRenderPassMockContext);
  NoClearRootRenderPassMockContext* mock_context = mock_context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
          mock_context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), NULL, 0, false, 1));

  LayerTreeSettings settings;
  settings.should_clear_root_render_pass = false;

  FakeRendererClient renderer_client;
  FakeRendererGL renderer(&renderer_client,
                          &settings,
                          output_surface.get(),
                          resource_provider.get());
  EXPECT_TRUE(renderer.Initialize());

  gfx::Rect viewport_rect(renderer_client.DeviceViewport());
  ScopedPtrVector<RenderPass>& render_passes =
      *renderer_client.render_passes_in_draw_order();
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
      *renderer_client.render_passes_in_draw_order());
  renderer.DrawFrame(
      renderer_client.render_passes_in_draw_order(), NULL, 1.f, true, false);

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
  scoped_ptr<ScissorTestOnClearCheckingContext> context_owned(
      new ScissorTestOnClearCheckingContext);

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), NULL, 0, false, 1));

  LayerTreeSettings settings;
  FakeRendererClient renderer_client;
  FakeRendererGL renderer(&renderer_client,
                          &settings,
                          output_surface.get(),
                          resource_provider.get());
  EXPECT_TRUE(renderer.Initialize());
  EXPECT_FALSE(renderer.Capabilities().using_partial_swap);

  gfx::Rect viewport_rect(renderer_client.DeviceViewport());
  ScopedPtrVector<RenderPass>& render_passes =
      *renderer_client.render_passes_in_draw_order();
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
      *renderer_client.render_passes_in_draw_order());
  renderer.DrawFrame(
      renderer_client.render_passes_in_draw_order(), NULL, 1.f, true, false);
}

class DiscardCheckingContext : public TestWebGraphicsContext3D {
 public:
  DiscardCheckingContext() : discarded_(0) {
    set_have_post_sub_buffer(true);
    set_have_discard_framebuffer(true);
  }

  virtual void discardFramebufferEXT(WGC3Denum target,
                                     WGC3Dsizei numAttachments,
                                     const WGC3Denum* attachments) {
    ++discarded_;
  }

  int discarded() const { return discarded_; }
  void reset() { discarded_ = 0; }

 private:
  int discarded_;
};

class NonReshapableOutputSurface : public FakeOutputSurface {
 public:
  explicit NonReshapableOutputSurface(
      scoped_ptr<TestWebGraphicsContext3D> context3d)
      : FakeOutputSurface(TestContextProvider::Create(context3d.Pass()),
                          false) {
    surface_size_ = gfx::Size(500, 500);
  }
  virtual void Reshape(gfx::Size size, float scale_factor) OVERRIDE {}
  void set_fixed_size(gfx::Size size) { surface_size_ = size; }
};

TEST(GLRendererTest2, NoDiscardOnPartialUpdates) {
  scoped_ptr<DiscardCheckingContext> context_owned(new DiscardCheckingContext);
  DiscardCheckingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<NonReshapableOutputSurface> output_surface(
      new NonReshapableOutputSurface(
          context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));
  output_surface->set_fixed_size(gfx::Size(100, 100));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), NULL, 0, false, 1));

  LayerTreeSettings settings;
  settings.partial_swap_enabled = true;
  FakeRendererClient renderer_client;
  renderer_client.set_viewport(gfx::Rect(0, 0, 100, 100));
  renderer_client.set_clip(gfx::Rect(0, 0, 100, 100));
  FakeRendererGL renderer(&renderer_client,
                          &settings,
                          output_surface.get(),
                          resource_provider.get());
  EXPECT_TRUE(renderer.Initialize());
  EXPECT_TRUE(renderer.Capabilities().using_partial_swap);

  gfx::Rect viewport_rect(renderer_client.DeviceViewport());
  ScopedPtrVector<RenderPass>& render_passes =
      *renderer_client.render_passes_in_draw_order();
  render_passes.clear();

  {
    // Partial frame, should not discard.
    RenderPass::Id root_pass_id(1, 0);
    TestRenderPass* root_pass = AddRenderPass(
        &render_passes, root_pass_id, viewport_rect, gfx::Transform());
    AddQuad(root_pass, viewport_rect, SK_ColorGREEN);
    root_pass->damage_rect = gfx::RectF(2.f, 2.f, 3.f, 3.f);

    renderer.DecideRenderPassAllocationsForFrame(
        *renderer_client.render_passes_in_draw_order());
    renderer.DrawFrame(
        renderer_client.render_passes_in_draw_order(), NULL, 1.f, true, false);
    EXPECT_EQ(0, context->discarded());
    context->reset();
  }
  {
    // Full frame, should discard.
    RenderPass::Id root_pass_id(1, 0);
    TestRenderPass* root_pass = AddRenderPass(
        &render_passes, root_pass_id, viewport_rect, gfx::Transform());
    AddQuad(root_pass, viewport_rect, SK_ColorGREEN);
    root_pass->damage_rect = gfx::RectF(root_pass->output_rect);

    renderer.DecideRenderPassAllocationsForFrame(
        *renderer_client.render_passes_in_draw_order());
    renderer.DrawFrame(
        renderer_client.render_passes_in_draw_order(), NULL, 1.f, true, false);
    EXPECT_EQ(1, context->discarded());
    context->reset();
  }
  {
    // Partial frame, disallow partial swap, should discard.
    RenderPass::Id root_pass_id(1, 0);
    TestRenderPass* root_pass = AddRenderPass(
        &render_passes, root_pass_id, viewport_rect, gfx::Transform());
    AddQuad(root_pass, viewport_rect, SK_ColorGREEN);
    root_pass->damage_rect = gfx::RectF(2.f, 2.f, 3.f, 3.f);

    renderer.DecideRenderPassAllocationsForFrame(
        *renderer_client.render_passes_in_draw_order());
    renderer.DrawFrame(
        renderer_client.render_passes_in_draw_order(), NULL, 1.f, false, false);
    EXPECT_EQ(1, context->discarded());
    context->reset();
  }
  {
    // Full frame, external scissor is set, should not discard.
    output_surface->set_has_external_stencil_test(true);
    RenderPass::Id root_pass_id(1, 0);
    TestRenderPass* root_pass = AddRenderPass(
        &render_passes, root_pass_id, viewport_rect, gfx::Transform());
    AddQuad(root_pass, viewport_rect, SK_ColorGREEN);
    root_pass->damage_rect = gfx::RectF(root_pass->output_rect);
    root_pass->has_transparent_background = false;

    renderer.DecideRenderPassAllocationsForFrame(
        *renderer_client.render_passes_in_draw_order());
    renderer.DrawFrame(
        renderer_client.render_passes_in_draw_order(), NULL, 1.f, true, false);
    EXPECT_EQ(0, context->discarded());
    context->reset();
    output_surface->set_has_external_stencil_test(false);
  }
  {
    // Full frame, clipped, should not discard.
    renderer_client.set_clip(gfx::Rect(10, 10, 10, 10));
    RenderPass::Id root_pass_id(1, 0);
    TestRenderPass* root_pass = AddRenderPass(
        &render_passes, root_pass_id, viewport_rect, gfx::Transform());
    AddQuad(root_pass, viewport_rect, SK_ColorGREEN);
    root_pass->damage_rect = gfx::RectF(root_pass->output_rect);

    renderer.DecideRenderPassAllocationsForFrame(
        *renderer_client.render_passes_in_draw_order());
    renderer.DrawFrame(
        renderer_client.render_passes_in_draw_order(), NULL, 1.f, true, false);
    EXPECT_EQ(0, context->discarded());
    context->reset();
  }
  {
    // Full frame, doesn't cover the surface, should not discard.
    renderer_client.set_viewport(gfx::Rect(10, 10, 10, 10));
    viewport_rect = renderer_client.DeviceViewport();
    RenderPass::Id root_pass_id(1, 0);
    TestRenderPass* root_pass = AddRenderPass(
        &render_passes, root_pass_id, viewport_rect, gfx::Transform());
    AddQuad(root_pass, viewport_rect, SK_ColorGREEN);
    root_pass->damage_rect = gfx::RectF(root_pass->output_rect);

    renderer.DecideRenderPassAllocationsForFrame(
        *renderer_client.render_passes_in_draw_order());
    renderer.DrawFrame(
        renderer_client.render_passes_in_draw_order(), NULL, 1.f, true, false);
    EXPECT_EQ(0, context->discarded());
    context->reset();
  }
  {
    // Full frame, doesn't cover the surface (no offset), should not discard.
    renderer_client.set_viewport(gfx::Rect(0, 0, 50, 50));
    renderer_client.set_clip(gfx::Rect(0, 0, 100, 100));
    viewport_rect = renderer_client.DeviceViewport();
    RenderPass::Id root_pass_id(1, 0);
    TestRenderPass* root_pass = AddRenderPass(
        &render_passes, root_pass_id, viewport_rect, gfx::Transform());
    AddQuad(root_pass, viewport_rect, SK_ColorGREEN);
    root_pass->damage_rect = gfx::RectF(root_pass->output_rect);

    renderer.DecideRenderPassAllocationsForFrame(
        *renderer_client.render_passes_in_draw_order());
    renderer.DrawFrame(
        renderer_client.render_passes_in_draw_order(), NULL, 1.f, true, false);
    EXPECT_EQ(0, context->discarded());
    context->reset();
  }
}

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
  scoped_ptr<FlippedScissorAndViewportContext> context_owned(
      new FlippedScissorAndViewportContext);

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(new NonReshapableOutputSurface(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(), NULL, 0, false, 1));

  LayerTreeSettings settings;
  FakeRendererClient renderer_client;
  renderer_client.set_viewport(gfx::Rect(10, 10, 100, 100));
  renderer_client.set_clip(gfx::Rect(10, 10, 100, 100));
  FakeRendererGL renderer(&renderer_client,
                          &settings,
                          output_surface.get(),
                          resource_provider.get());
  EXPECT_TRUE(renderer.Initialize());
  EXPECT_FALSE(renderer.Capabilities().using_partial_swap);

  gfx::Rect viewport_rect(renderer_client.DeviceViewport().size());
  gfx::Rect quad_rect = gfx::Rect(20, 20, 20, 20);
  ScopedPtrVector<RenderPass>& render_passes =
      *renderer_client.render_passes_in_draw_order();
  render_passes.clear();

  RenderPass::Id root_pass_id(1, 0);
  TestRenderPass* root_pass = AddRenderPass(
      &render_passes, root_pass_id, viewport_rect, gfx::Transform());
  AddClippedQuad(root_pass, quad_rect, SK_ColorGREEN);

  renderer.DecideRenderPassAllocationsForFrame(
      *renderer_client.render_passes_in_draw_order());
  renderer.DrawFrame(
      renderer_client.render_passes_in_draw_order(), NULL, 1.f, true, false);
}

TEST_F(GLRendererShaderTest, DrawRenderPassQuadShaderPermutations) {
  gfx::Rect viewport_rect(renderer_client_.DeviceViewport());
  ScopedPtrVector<RenderPass>* render_passes =
      renderer_client_.render_passes_in_draw_order();

  gfx::Rect child_rect(50, 50);
  RenderPass::Id child_pass_id(2, 0);
  TestRenderPass* child_pass;

  RenderPass::Id root_pass_id(1, 0);
  TestRenderPass* root_pass;

  cc::ResourceProvider::ResourceId mask =
  resource_provider_->CreateResource(gfx::Size(20, 12),
                                     GL_CLAMP_TO_EDGE,
                                     ResourceProvider::TextureUsageAny,
                                     resource_provider_->best_texture_format());
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
  FilterOperations filters;
  filters.Append(FilterOperation::CreateReferenceFilter(filter));

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
                    FilterOperations(),
                    gfx::Transform());

  renderer_->DecideRenderPassAllocationsForFrame(
      *renderer_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(
      renderer_client_.render_passes_in_draw_order(), NULL, 1.f, true, false);
  TestRenderPassProgram();

  // RenderPassColorMatrixProgram
  render_passes->clear();

  child_pass = AddRenderPass(
      render_passes, child_pass_id, child_rect, transform_causing_aa);

  root_pass = AddRenderPass(
      render_passes, root_pass_id, viewport_rect, gfx::Transform());

  AddRenderPassQuad(root_pass, child_pass, 0, filters, gfx::Transform());

  renderer_->DecideRenderPassAllocationsForFrame(
      *renderer_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(
      renderer_client_.render_passes_in_draw_order(), NULL, 1.f, true, false);
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
                    FilterOperations(),
                    gfx::Transform());

  renderer_->DecideRenderPassAllocationsForFrame(
      *renderer_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(
      renderer_client_.render_passes_in_draw_order(), NULL, 1.f, true, false);
  TestRenderPassMaskProgram();

  // RenderPassMaskColorMatrixProgram
  render_passes->clear();

  child_pass = AddRenderPass(
      render_passes, child_pass_id, child_rect, gfx::Transform());

  root_pass = AddRenderPass(
      render_passes, root_pass_id, viewport_rect, gfx::Transform());

  AddRenderPassQuad(root_pass, child_pass, mask, filters, gfx::Transform());

  renderer_->DecideRenderPassAllocationsForFrame(
      *renderer_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(
      renderer_client_.render_passes_in_draw_order(), NULL, 1.f, true, false);
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
                    FilterOperations(),
                    transform_causing_aa);

  renderer_->DecideRenderPassAllocationsForFrame(
      *renderer_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(
      renderer_client_.render_passes_in_draw_order(), NULL, 1.f, true, false);
  TestRenderPassProgramAA();

  // RenderPassColorMatrixProgramAA
  render_passes->clear();

  child_pass = AddRenderPass(
      render_passes, child_pass_id, child_rect, transform_causing_aa);

  root_pass = AddRenderPass(
      render_passes, root_pass_id, viewport_rect, gfx::Transform());

  AddRenderPassQuad(root_pass, child_pass, 0, filters, transform_causing_aa);

  renderer_->DecideRenderPassAllocationsForFrame(
      *renderer_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(
      renderer_client_.render_passes_in_draw_order(), NULL, 1.f, true, false);
  TestRenderPassColorMatrixProgramAA();

  // RenderPassMaskProgramAA
  render_passes->clear();

  child_pass = AddRenderPass(render_passes, child_pass_id, child_rect,
      transform_causing_aa);

  root_pass = AddRenderPass(render_passes, root_pass_id, viewport_rect,
      gfx::Transform());

  AddRenderPassQuad(root_pass, child_pass, mask, FilterOperations(),
      transform_causing_aa);

  renderer_->DecideRenderPassAllocationsForFrame(
      *renderer_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(
      renderer_client_.render_passes_in_draw_order(), NULL, 1.f, true, false);
  TestRenderPassMaskProgramAA();

  // RenderPassMaskColorMatrixProgramAA
  render_passes->clear();

  child_pass = AddRenderPass(render_passes, child_pass_id, child_rect,
      transform_causing_aa);

  root_pass = AddRenderPass(render_passes, root_pass_id, viewport_rect,
      transform_causing_aa);

  AddRenderPassQuad(root_pass, child_pass, mask, filters, transform_causing_aa);

  renderer_->DecideRenderPassAllocationsForFrame(
      *renderer_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(
      renderer_client_.render_passes_in_draw_order(), NULL, 1.f, true, false);
  TestRenderPassMaskColorMatrixProgramAA();
}

// At this time, the AA code path cannot be taken if the surface's rect would
// project incorrectly by the given transform, because of w<0 clipping.
TEST_F(GLRendererShaderTest, DrawRenderPassQuadSkipsAAForClippingTransform) {
  gfx::Rect child_rect(50, 50);
  RenderPass::Id child_pass_id(2, 0);
  TestRenderPass* child_pass;

  gfx::Rect viewport_rect(renderer_client_.DeviceViewport());
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
      renderer_client_.render_passes_in_draw_order();

  render_passes->clear();

  child_pass = AddRenderPass(
      render_passes, child_pass_id, child_rect, transform_preventing_aa);

  root_pass = AddRenderPass(
      render_passes, root_pass_id, viewport_rect, gfx::Transform());

  AddRenderPassQuad(root_pass,
                    child_pass,
                    0,
                    FilterOperations(),
                    transform_preventing_aa);

  renderer_->DecideRenderPassAllocationsForFrame(
      *renderer_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(
      renderer_client_.render_passes_in_draw_order(), NULL, 1.f, true, false);

  // If use_aa incorrectly ignores clipping, it will use the
  // RenderPassProgramAA shader instead of the RenderPassProgram.
  TestRenderPassProgram();
}

TEST_F(GLRendererShaderTest, DrawSolidColorShader) {
  gfx::Rect viewport_rect(renderer_client_.DeviceViewport());
  ScopedPtrVector<RenderPass>* render_passes =
      renderer_client_.render_passes_in_draw_order();

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
      *renderer_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(
      renderer_client_.render_passes_in_draw_order(), NULL, 1.f, true, false);

  TestSolidColorProgramAA();
}

class OutputSurfaceMockContext : public TestWebGraphicsContext3D {
 public:
  OutputSurfaceMockContext() {
    test_capabilities_.discard_backbuffer = true;
    test_capabilities_.post_sub_buffer = true;
  }

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
};

class MockOutputSurface : public OutputSurface {
 public:
  MockOutputSurface()
      : OutputSurface(TestContextProvider::Create(
          scoped_ptr<TestWebGraphicsContext3D>(
              new StrictMock<OutputSurfaceMockContext>))) {
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
  virtual void SetUp() {
    FakeOutputSurfaceClient output_surface_client_;
    CHECK(output_surface_.BindToClient(&output_surface_client_));

    resource_provider_ =
        ResourceProvider::Create(&output_surface_, NULL, 0, false, 1).Pass();

    renderer_.reset(new FakeRendererGL(
        this, &settings_, &output_surface_, resource_provider_.get()));
    EXPECT_TRUE(renderer_->Initialize());
  }

  void SwapBuffers() { renderer_->SwapBuffers(); }

  void DrawFrame(float device_scale_factor) {
    gfx::Rect viewport_rect(DeviceViewport());
    ScopedPtrVector<RenderPass>* render_passes = render_passes_in_draw_order();
    render_passes->clear();

    RenderPass::Id render_pass_id(1, 0);
    TestRenderPass* render_pass = AddRenderPass(
        render_passes, render_pass_id, viewport_rect, gfx::Transform());
    AddQuad(render_pass, viewport_rect, SK_ColorGREEN);

    EXPECT_CALL(output_surface_, EnsureBackbuffer()).WillRepeatedly(Return());

    EXPECT_CALL(output_surface_,
                Reshape(DeviceViewport().size(), device_scale_factor)).Times(1);

    EXPECT_CALL(output_surface_, BindFramebuffer()).Times(1);

    EXPECT_CALL(*Context(), drawElements(_, _, _, _)).Times(1);

    renderer_->DecideRenderPassAllocationsForFrame(
        *render_passes_in_draw_order());
    renderer_->DrawFrame(
        render_passes_in_draw_order(), NULL, device_scale_factor, true, false);
  }

  OutputSurfaceMockContext* Context() {
    return static_cast<OutputSurfaceMockContext*>(
        output_surface_.context_provider()->Context3d());
  }

  LayerTreeSettings settings_;
  FakeOutputSurfaceClient output_surface_client_;
  StrictMock<MockOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<FakeRendererGL> renderer_;
};

TEST_F(MockOutputSurfaceTest, DrawFrameAndSwap) {
  DrawFrame(1.f);

  EXPECT_CALL(output_surface_, SwapBuffers(_)).Times(1);
  renderer_->SwapBuffers();
}

TEST_F(MockOutputSurfaceTest, DrawFrameAndResizeAndSwap) {
  DrawFrame(1.f);
  EXPECT_CALL(output_surface_, SwapBuffers(_)).Times(1);
  renderer_->SwapBuffers();

  set_viewport(gfx::Rect(0, 0, 2, 2));
  renderer_->ViewportChanged();

  DrawFrame(2.f);
  EXPECT_CALL(output_surface_, SwapBuffers(_)).Times(1);
  renderer_->SwapBuffers();

  DrawFrame(2.f);
  EXPECT_CALL(output_surface_, SwapBuffers(_)).Times(1);
  renderer_->SwapBuffers();

  set_viewport(gfx::Rect(0, 0, 1, 1));
  renderer_->ViewportChanged();

  DrawFrame(1.f);
  EXPECT_CALL(output_surface_, SwapBuffers(_)).Times(1);
  renderer_->SwapBuffers();
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
  blink::WebGraphicsContext3D* context3d =
      output_surface_->context_provider()->Context3d();
  gpu::ContextSupport* context_support =
      output_surface_->context_provider()->ContextSupport();

  uint32 sync_point = context3d->insertSyncPoint();

  context3d->loseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                                 GL_INNOCENT_CONTEXT_RESET_ARB);

  context_support->SignalSyncPoint(
      sync_point, base::Bind(&SyncPointCallback, &sync_point_callback_count));
  EXPECT_EQ(0, sync_point_callback_count);
  EXPECT_EQ(0, other_callback_count);

  // Make the sync point happen.
  context3d->finish();
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

  blink::WebGraphicsContext3D* context3d =
      output_surface_->context_provider()->Context3d();
  gpu::ContextSupport* context_support =
      output_surface_->context_provider()->ContextSupport();

  uint32 sync_point = context3d->insertSyncPoint();

  context_support->SignalSyncPoint(
      sync_point, base::Bind(&SyncPointCallback, &sync_point_callback_count));
  EXPECT_EQ(0, sync_point_callback_count);
  EXPECT_EQ(0, other_callback_count);

  // Make the sync point happen.
  context3d->finish();
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
