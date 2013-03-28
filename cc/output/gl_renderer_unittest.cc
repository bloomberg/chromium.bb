// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/gl_renderer.h"

#include "cc/output/compositor_frame_metadata.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource_provider.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/pixel_test.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "ui/gfx/transform.h"

using namespace WebKit;

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Expectation;
using testing::InSequence;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

namespace cc {

#define EXPECT_PROGRAM_VALID(program_binding)                                  \
  do {                                                                         \
    EXPECT_TRUE(program_binding->program());                                   \
    EXPECT_TRUE(program_binding->initialized());                               \
  } while (false)

// Explicitly named to be a friend in GLRenderer for shader access.
class GLRendererShaderPixelTest : public PixelTest {
 public:
  void TestShaders() {
    ASSERT_FALSE(renderer_->IsContextLost());
    EXPECT_PROGRAM_VALID(renderer_->GetTileProgram());
    EXPECT_PROGRAM_VALID(renderer_->GetTileProgramOpaque());
    EXPECT_PROGRAM_VALID(renderer_->GetTileProgramAA());
    EXPECT_PROGRAM_VALID(renderer_->GetTileProgramSwizzle());
    EXPECT_PROGRAM_VALID(renderer_->GetTileProgramSwizzleOpaque());
    EXPECT_PROGRAM_VALID(renderer_->GetTileProgramSwizzleAA());
    EXPECT_PROGRAM_VALID(renderer_->GetTileCheckerboardProgram());
    EXPECT_PROGRAM_VALID(renderer_->GetRenderPassProgram());
    EXPECT_PROGRAM_VALID(renderer_->GetRenderPassProgramAA());
    EXPECT_PROGRAM_VALID(renderer_->GetRenderPassMaskProgram());
    EXPECT_PROGRAM_VALID(renderer_->GetRenderPassMaskProgramAA());
    EXPECT_PROGRAM_VALID(renderer_->GetRenderPassColorMatrixProgram());
    EXPECT_PROGRAM_VALID(renderer_->GetRenderPassMaskColorMatrixProgramAA());
    EXPECT_PROGRAM_VALID(renderer_->GetRenderPassColorMatrixProgramAA());
    EXPECT_PROGRAM_VALID(renderer_->GetRenderPassMaskColorMatrixProgram());
    EXPECT_PROGRAM_VALID(renderer_->GetTextureProgram());
    EXPECT_PROGRAM_VALID(renderer_->GetTextureProgramFlip());
    EXPECT_PROGRAM_VALID(renderer_->GetTextureIOSurfaceProgram());
    EXPECT_PROGRAM_VALID(renderer_->GetVideoYUVProgram());
    // This is unlikely to be ever true in tests due to usage of osmesa.
    if (renderer_->Capabilities().using_egl_image)
      EXPECT_PROGRAM_VALID(renderer_->GetVideoStreamTextureProgram());
    else
      EXPECT_FALSE(renderer_->GetVideoStreamTextureProgram());
    EXPECT_PROGRAM_VALID(renderer_->GetDebugBorderProgram());
    EXPECT_PROGRAM_VALID(renderer_->GetSolidColorProgram());
    EXPECT_PROGRAM_VALID(renderer_->GetSolidColorProgramAA());
    ASSERT_FALSE(renderer_->IsContextLost());
  }
};

namespace {

#if !defined(OS_ANDROID)
TEST_F(GLRendererShaderPixelTest, AllShadersCompile) { TestShaders(); }
#endif

class FrameCountingMemoryAllocationSettingContext :
    public TestWebGraphicsContext3D {
 public:
  FrameCountingMemoryAllocationSettingContext() : frame_(0) {}

  // WebGraphicsContext3D methods.

  // This method would normally do a glSwapBuffers under the hood.
  virtual void prepareTexture() { frame_++; }
  virtual void setMemoryAllocationChangedCallbackCHROMIUM(
      WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* callback) {
    memory_allocation_changed_callback_ = callback;
  }
  virtual WebString getString(WebKit::WGC3Denum name) {
    if (name == GL_EXTENSIONS)
      return WebString(
          "GL_CHROMIUM_set_visibility GL_CHROMIUM_gpu_memory_manager "
          "GL_CHROMIUM_discard_backbuffer");
    return WebString();
  }

  // Methods added for test.
  int frame_count() { return frame_; }
  void SetMemoryAllocation(WebGraphicsMemoryAllocation allocation) {
    memory_allocation_changed_callback_->onMemoryAllocationChanged(allocation);
  }

 private:
  int frame_;
  WebGraphicsMemoryAllocationChangedCallbackCHROMIUM*
    memory_allocation_changed_callback_;
};

class FakeRendererClient : public RendererClient {
 public:
  FakeRendererClient()
      : host_impl_(&proxy_),
        set_full_root_layer_damage_count_(0),
        last_call_was_set_visibility_(0),
        root_layer_(LayerImpl::Create(host_impl_.active_tree(), 1)),
        memory_allocation_limit_bytes_(
            PrioritizedResourceManager::DefaultMemoryAllocationLimit()) {
    root_layer_->CreateRenderSurface();
    RenderPass::Id render_pass_id =
        root_layer_->render_surface()->RenderPassId();
    scoped_ptr<RenderPass> root_render_pass = RenderPass::Create();
    root_render_pass->SetNew(
        render_pass_id, gfx::Rect(), gfx::Rect(), gfx::Transform());
    render_passes_in_draw_order_.push_back(root_render_pass.Pass());
  }

  // RendererClient methods.
  virtual gfx::Size DeviceViewportSize() const OVERRIDE {
    static gfx::Size fake_size(1, 1);
    return fake_size;
  }
  virtual const LayerTreeSettings& Settings() const OVERRIDE {
    static LayerTreeSettings fake_settings;
    return fake_settings;
  }
  virtual void DidLoseOutputSurface() OVERRIDE {}
  virtual void OnSwapBuffersComplete() OVERRIDE {}
  virtual void SetFullRootLayerDamage() OVERRIDE {
    set_full_root_layer_damage_count_++;
  }
  virtual void SetManagedMemoryPolicy(const ManagedMemoryPolicy& policy)
      OVERRIDE {
    memory_allocation_limit_bytes_ = policy.bytes_limit_when_visible;
  }
  virtual void EnforceManagedMemoryPolicy(const ManagedMemoryPolicy& policy)
      OVERRIDE {
    if (last_call_was_set_visibility_)
      *last_call_was_set_visibility_ = false;
  }
  virtual bool HasImplThread() const OVERRIDE { return false; }
  virtual bool ShouldClearRootRenderPass() const OVERRIDE { return true; }
  virtual CompositorFrameMetadata MakeCompositorFrameMetadata() const OVERRIDE {
    return CompositorFrameMetadata();
  }

  // Methods added for test.
  int set_full_root_layer_damage_count() const {
    return set_full_root_layer_damage_count_;
  }
  void set_last_call_was_set_visibility_pointer(
      bool* last_call_was_set_visibility) {
    last_call_was_set_visibility_ = last_call_was_set_visibility;
  }

  RenderPass* root_render_pass() { return render_passes_in_draw_order_.back(); }
  RenderPassList* render_passes_in_draw_order() {
    return &render_passes_in_draw_order_;
  }

  size_t memory_allocation_limit_bytes() const {
    return memory_allocation_limit_bytes_;
  }

 private:
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  int set_full_root_layer_damage_count_;
  bool* last_call_was_set_visibility_;
  scoped_ptr<LayerImpl> root_layer_;
  RenderPassList render_passes_in_draw_order_;
  size_t memory_allocation_limit_bytes_;
};

class FakeRendererGL : public GLRenderer {
 public:
  FakeRendererGL(RendererClient* client,
                 OutputSurface* output_surface,
                 ResourceProvider* resource_provider)
      : GLRenderer(client, output_surface, resource_provider) {}

  // GLRenderer methods.

  // Changing visibility to public.
  using GLRenderer::Initialize;
  using GLRenderer::IsBackbufferDiscarded;
  using GLRenderer::DoDrawQuad;
  using GLRenderer::BeginDrawingFrame;
  using GLRenderer::FinishDrawingQuadList;
};

class GLRendererTest : public testing::Test {
 protected:
  GLRendererTest()
      : suggest_have_backbuffer_yes_(1, true),
        suggest_have_backbuffer_no_(1, false),
        output_surface_(FakeOutputSurface::Create3d(
            scoped_ptr<WebKit::WebGraphicsContext3D>(
                new FrameCountingMemoryAllocationSettingContext()))),
        resource_provider_(ResourceProvider::Create(output_surface_.get())),
        renderer_(&mock_client_,
                  output_surface_.get(),
                  resource_provider_.get()) {}

  virtual void SetUp() { renderer_.Initialize(); }

  void SwapBuffers() { renderer_.SwapBuffers(); }

  FrameCountingMemoryAllocationSettingContext* context() {
    return static_cast<FrameCountingMemoryAllocationSettingContext*>(
        output_surface_->context3d());
  }

  WebGraphicsMemoryAllocation suggest_have_backbuffer_yes_;
  WebGraphicsMemoryAllocation suggest_have_backbuffer_no_;

  scoped_ptr<OutputSurface> output_surface_;
  FakeRendererClient mock_client_;
  scoped_ptr<ResourceProvider> resource_provider_;
  FakeRendererGL renderer_;
};

// Closing the namespace here so that GLRendererShaderTest can take advantage
// of the friend relationship with GLRenderer and all of the mock classes
// declared above it.
} // namespace

class GLRendererShaderTest : public testing::Test {
protected:
  GLRendererShaderTest()
      : output_surface_(FakeOutputSurface::Create3d())
      , resource_provider_(ResourceProvider::Create(output_surface_.get()))
      , renderer_(GLRenderer::Create(&mock_client_, output_surface_.get(), resource_provider_.get()))
  {
  }

  void TestRenderPassProgram() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_program_);
    EXPECT_TRUE(renderer_->program_shadow_ ==
        renderer_->render_pass_program_->program());
  }

  void TestRenderPassColorMatrixProgram() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_color_matrix_program_);
    EXPECT_TRUE(renderer_->program_shadow_ ==
        renderer_->render_pass_color_matrix_program_->program());
  }

  void TestRenderPassMaskProgram() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_mask_program_);
    EXPECT_TRUE(renderer_->program_shadow_ ==
        renderer_->render_pass_mask_program_->program());
  }

  void TestRenderPassMaskColorMatrixProgram() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_mask_color_matrix_program_);
    EXPECT_TRUE(renderer_->program_shadow_ ==
        renderer_->render_pass_mask_color_matrix_program_->program());
  }

  void TestRenderPassProgramAA() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_program_aa_);
    EXPECT_TRUE(renderer_->program_shadow_ ==
        renderer_->render_pass_program_aa_->program());
  }

  void TestRenderPassColorMatrixProgramAA() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_color_matrix_program_aa_);
    EXPECT_TRUE(renderer_->program_shadow_ ==
        renderer_->render_pass_color_matrix_program_aa_->program());
  }

  void TestRenderPassMaskProgramAA() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_mask_program_);
    EXPECT_TRUE(renderer_->program_shadow_ ==
        renderer_->render_pass_program_aa_->program());
  }

  void TestRenderPassMaskColorMatrixProgramAA() {
    EXPECT_PROGRAM_VALID(renderer_->render_pass_mask_color_matrix_program_aa_);
    EXPECT_TRUE(renderer_->program_shadow_ ==
        renderer_->render_pass_color_matrix_program_aa_->program());
  }

  scoped_ptr<OutputSurface> output_surface_;
  FakeRendererClient mock_client_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<GLRenderer> renderer_;
};

namespace {

// Test GLRenderer discardBackbuffer functionality:
// Suggest recreating framebuffer when one already exists.
// Expected: it does nothing.
TEST_F(GLRendererTest, SuggestBackbufferYesWhenItAlreadyExistsShouldDoNothing) {
  context()->SetMemoryAllocation(suggest_have_backbuffer_yes_);
  EXPECT_EQ(0, mock_client_.set_full_root_layer_damage_count());
  EXPECT_FALSE(renderer_.IsBackbufferDiscarded());

  SwapBuffers();
  EXPECT_EQ(1, context()->frame_count());
}

// Test GLRenderer DiscardBackbuffer functionality:
// Suggest discarding framebuffer when one exists and the renderer is not
// visible.
// Expected: it is discarded and damage tracker is reset.
TEST_F(GLRendererTest,
       SuggestBackbufferNoShouldDiscardBackbufferAndDamageRootLayerWhileNotVisible) {
  renderer_.SetVisible(false);
  context()->SetMemoryAllocation(suggest_have_backbuffer_no_);
  EXPECT_EQ(1, mock_client_.set_full_root_layer_damage_count());
  EXPECT_TRUE(renderer_.IsBackbufferDiscarded());
}

// Test GLRenderer DiscardBackbuffer functionality:
// Suggest discarding framebuffer when one exists and the renderer is visible.
// Expected: the allocation is ignored.
TEST_F(GLRendererTest, SuggestBackbufferNoDoNothingWhenVisible) {
  renderer_.SetVisible(true);
  context()->SetMemoryAllocation(suggest_have_backbuffer_no_);
  EXPECT_EQ(0, mock_client_.set_full_root_layer_damage_count());
  EXPECT_FALSE(renderer_.IsBackbufferDiscarded());
}

// Test GLRenderer DiscardBackbuffer functionality:
// Suggest discarding framebuffer when one does not exist.
// Expected: it does nothing.
TEST_F(GLRendererTest, SuggestBackbufferNoWhenItDoesntExistShouldDoNothing) {
  renderer_.SetVisible(false);
  context()->SetMemoryAllocation(suggest_have_backbuffer_no_);
  EXPECT_EQ(1, mock_client_.set_full_root_layer_damage_count());
  EXPECT_TRUE(renderer_.IsBackbufferDiscarded());

  context()->SetMemoryAllocation(suggest_have_backbuffer_no_);
  EXPECT_EQ(1, mock_client_.set_full_root_layer_damage_count());
  EXPECT_TRUE(renderer_.IsBackbufferDiscarded());
}

// Test GLRenderer DiscardBackbuffer functionality:
// Begin drawing a frame while a framebuffer is discarded.
// Expected: will recreate framebuffer.
TEST_F(GLRendererTest, DiscardedBackbufferIsRecreatedForScopeDuration) {
  renderer_.SetVisible(false);
  context()->SetMemoryAllocation(suggest_have_backbuffer_no_);
  EXPECT_TRUE(renderer_.IsBackbufferDiscarded());
  EXPECT_EQ(1, mock_client_.set_full_root_layer_damage_count());

  renderer_.SetVisible(true);
  renderer_.DrawFrame(mock_client_.render_passes_in_draw_order());
  EXPECT_FALSE(renderer_.IsBackbufferDiscarded());

  SwapBuffers();
  EXPECT_EQ(1, context()->frame_count());
}

TEST_F(GLRendererTest, FramebufferDiscardedAfterReadbackWhenNotVisible) {
  renderer_.SetVisible(false);
  context()->SetMemoryAllocation(suggest_have_backbuffer_no_);
  EXPECT_TRUE(renderer_.IsBackbufferDiscarded());
  EXPECT_EQ(1, mock_client_.set_full_root_layer_damage_count());

  char pixels[4];
  renderer_.DrawFrame(mock_client_.render_passes_in_draw_order());
  EXPECT_FALSE(renderer_.IsBackbufferDiscarded());

  renderer_.GetFramebufferPixels(pixels, gfx::Rect(0, 0, 1, 1));
  EXPECT_TRUE(renderer_.IsBackbufferDiscarded());
  EXPECT_EQ(2, mock_client_.set_full_root_layer_damage_count());
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
    }
    else {
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
    // FIXME: It'd be better to check that we only do this before starting any
    // other expensive work (like starting a compilation)
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
      ResourceProvider::Create(output_surface.get()));
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
      ResourceProvider::Create(output_surface.get()));
  FakeRendererGL renderer(
      &mock_client, output_surface.get(), resource_provider.get());

  renderer.Initialize();
}

class ContextThatDoesNotSupportMemoryManagmentExtensions :
    public TestWebGraphicsContext3D {
 public:
  ContextThatDoesNotSupportMemoryManagmentExtensions() {}

  // WebGraphicsContext3D methods.

  // This method would normally do a glSwapBuffers under the hood.
  virtual void prepareTexture() {}
  virtual void setMemoryAllocationChangedCallbackCHROMIUM(
      WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* callback) {}
  virtual WebString getString(WebKit::WGC3Denum name) { return WebString(); }
};

TEST(GLRendererTest2,
     InitializationWithoutGpuMemoryManagerExtensionSupportShouldDefaultToNonZeroAllocation) {
  FakeRendererClient mock_client;
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new ContextThatDoesNotSupportMemoryManagmentExtensions)));
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get()));
  FakeRendererGL renderer(
      &mock_client, output_surface.get(), resource_provider.get());

  renderer.Initialize();

  EXPECT_GT(mock_client.memory_allocation_limit_bytes(), 0ul);
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
      ResourceProvider::Create(output_surface.get()));
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
      ResourceProvider::Create(output_surface.get()));
  FakeRendererGL renderer(
      &mock_client, output_surface.get(), resource_provider.get());

  mock_client.root_render_pass()->has_transparent_background = true;

  EXPECT_TRUE(renderer.Initialize());

  renderer.DrawFrame(mock_client.render_passes_in_draw_order());

  EXPECT_EQ(1, context->clear_count());
}

class VisibilityChangeIsLastCallTrackingContext :
    public TestWebGraphicsContext3D {
 public:
  VisibilityChangeIsLastCallTrackingContext()
      : last_call_was_set_visibility_(0) {}

  // WebGraphicsContext3D methods.
  virtual void setVisibilityCHROMIUM(bool visible) {
    if (!last_call_was_set_visibility_)
      return;
    DCHECK(*last_call_was_set_visibility_ == false);
    *last_call_was_set_visibility_ = true;
  }
  virtual void flush() {
    if (last_call_was_set_visibility_)
      *last_call_was_set_visibility_ = false;
  }
  virtual void deleteTexture(WebGLId) {
    if (last_call_was_set_visibility_)
      *last_call_was_set_visibility_ = false;
  }
  virtual void deleteFramebuffer(WebGLId) {
    if (last_call_was_set_visibility_)
      *last_call_was_set_visibility_ = false;
  }
  virtual void deleteRenderbuffer(WebGLId) {
    if (last_call_was_set_visibility_)
      *last_call_was_set_visibility_ = false;
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
  void set_last_call_was_set_visibility_pointer(
      bool* last_call_was_set_visibility) {
    last_call_was_set_visibility_ = last_call_was_set_visibility;
  }

 private:
  bool* last_call_was_set_visibility_;
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
      ResourceProvider::Create(output_surface.get()));
  FakeRendererGL renderer(
      &mock_client, output_surface.get(), resource_provider.get());

  EXPECT_TRUE(renderer.Initialize());

  bool last_call_was_set_visiblity = false;
  // Ensure that the call to setVisibilityCHROMIUM is the last call issue to the
  // GPU process, after glFlush is called, and after the RendererClient's
  // EnforceManagedMemoryPolicy is called. Plumb this tracking between both the
  // RenderClient and the Context by giving them both a pointer to a variable on
  // the stack.
  context->set_last_call_was_set_visibility_pointer(
      &last_call_was_set_visiblity);
  mock_client.set_last_call_was_set_visibility_pointer(
      &last_call_was_set_visiblity);
  renderer.SetVisible(true);
  renderer.DrawFrame(mock_client.render_passes_in_draw_order());
  renderer.SetVisible(false);
  EXPECT_TRUE(last_call_was_set_visiblity);
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
      ResourceProvider::Create(output_surface.get()));
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
  EXPECT_EQ(context->active_texture(), GL_TEXTURE0);

  for (cc::QuadList::BackToFrontIterator
           it = pass->quad_list.BackToFrontBegin();
       it != pass->quad_list.BackToFrontEnd();
       ++it) {
    renderer.DoDrawQuad(&drawing_frame, *it);
  }
  renderer.FinishDrawingQuadList();
  EXPECT_EQ(context->active_texture(), GL_TEXTURE0);
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
      ResourceProvider::Create(output_surface.get()));
  FakeRendererGL renderer(
      &mock_client, output_surface.get(), resource_provider.get());
  EXPECT_TRUE(renderer.Initialize());

  gfx::Rect viewport_rect(mock_client.DeviceViewportSize());
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

  // First render pass is not the root one, clearing should happen.
  EXPECT_CALL(*mock_context, clear(GL_COLOR_BUFFER_BIT)).Times(AtLeast(1));

  Expectation first_render_pass =
      EXPECT_CALL(*mock_context, drawElements(_, _, _, _)).Times(1);

  // The second render pass is the root one, clearing should be prevented.
  EXPECT_CALL(*mock_context, clear(GL_COLOR_BUFFER_BIT)).Times(0)
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
      ResourceProvider::Create(output_surface.get()));
  FakeRendererGL renderer(
      &mock_client, output_surface.get(), resource_provider.get());
  EXPECT_TRUE(renderer.Initialize());
  EXPECT_FALSE(renderer.Capabilities().using_partial_swap);

  gfx::Rect viewport_rect(mock_client.DeviceViewportSize());
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

TEST_F(GLRendererShaderTest, DrawRenderPassQuadShaderPermutations) {
  gfx::Rect viewportRect(mock_client_.DeviceViewportSize());
  ScopedPtrVector<RenderPass>* renderPasses =
        mock_client_.render_passes_in_draw_order();

  gfx::Rect grandChildRect(25, 25);
  RenderPass::Id grandChildPassId(3, 0);
  TestRenderPass* grandChildPass;

  gfx::Rect childRect(50, 50);
  RenderPass::Id childPassId(2, 0);
  TestRenderPass* childPass;

  RenderPass::Id rootPassId(1, 0);
  TestRenderPass* rootPass;

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
  skia::RefPtr<SkColorFilter> colorFilter(skia::AdoptRef(
      new SkColorMatrixFilter(matrix)));
  skia::RefPtr<SkImageFilter> filter =
      skia::AdoptRef(SkColorFilterImageFilter::Create(colorFilter.get(), NULL));

  gfx::Transform transform_causing_aa;
  transform_causing_aa.Rotate(20.0);

  // RenderPassProgram
  renderPasses->clear();

  grandChildPass = AddRenderPass(renderPasses, grandChildPassId, grandChildRect,
      gfx::Transform());
  AddClippedQuad(grandChildPass, grandChildRect, SK_ColorYELLOW);

  childPass = AddRenderPass(renderPasses, childPassId, childRect,
      gfx::Transform());
  AddQuad(childPass, childRect, SK_ColorBLUE);

  rootPass = AddRenderPass(renderPasses, rootPassId, viewportRect,
      gfx::Transform());
  AddQuad(rootPass, viewportRect, SK_ColorGREEN);

  AddRenderPassQuad(rootPass, childPass, 0, skia::RefPtr<SkImageFilter>(),
      gfx::Transform());
  AddRenderPassQuad(childPass, grandChildPass);

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassProgram();

  // RenderPassColorMatrixProgram
  renderPasses->clear();

  grandChildPass = AddRenderPass(renderPasses, grandChildPassId, grandChildRect,
      transform_causing_aa);
  AddClippedQuad(grandChildPass, grandChildRect, SK_ColorYELLOW);

  childPass = AddRenderPass(renderPasses, childPassId, childRect,
      transform_causing_aa);
  AddQuad(childPass, childRect, SK_ColorBLUE);

  rootPass = AddRenderPass(renderPasses, rootPassId, viewportRect,
      gfx::Transform());
  AddQuad(rootPass, viewportRect, SK_ColorGREEN);

  AddRenderPassQuad(rootPass, childPass, 0, filter, gfx::Transform());
  AddRenderPassQuad(childPass, grandChildPass);

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassColorMatrixProgram();

  // RenderPassMaskProgram
  renderPasses->clear();

  grandChildPass = AddRenderPass(renderPasses, grandChildPassId, grandChildRect,
      gfx::Transform());
  AddClippedQuad(grandChildPass, grandChildRect, SK_ColorYELLOW);

  childPass = AddRenderPass(renderPasses, childPassId, childRect,
      gfx::Transform());
  AddQuad(childPass, childRect, SK_ColorBLUE);

  rootPass = AddRenderPass(renderPasses, rootPassId, viewportRect,
      gfx::Transform());
  AddQuad(rootPass, viewportRect, SK_ColorGREEN);

  AddRenderPassQuad(rootPass, childPass, mask, skia::RefPtr<SkImageFilter>(),
      gfx::Transform());
  AddRenderPassQuad(childPass, grandChildPass);

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassMaskProgram();

  // RenderPassMaskColorMatrixProgram
  renderPasses->clear();

  grandChildPass = AddRenderPass(renderPasses, grandChildPassId, grandChildRect,
      gfx::Transform());
  AddClippedQuad(grandChildPass, grandChildRect, SK_ColorYELLOW);

  childPass = AddRenderPass(renderPasses, childPassId, childRect,
      gfx::Transform());
  AddQuad(childPass, childRect, SK_ColorBLUE);

  rootPass = AddRenderPass(renderPasses, rootPassId, viewportRect,
      gfx::Transform());
  AddQuad(rootPass, viewportRect, SK_ColorGREEN);

  AddRenderPassQuad(rootPass, childPass, mask, filter, gfx::Transform());
  AddRenderPassQuad(childPass, grandChildPass);

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassMaskColorMatrixProgram();

  // RenderPassProgramAA
  renderPasses->clear();

  grandChildPass = AddRenderPass(renderPasses, grandChildPassId, grandChildRect,
      transform_causing_aa);
  AddClippedQuad(grandChildPass, grandChildRect, SK_ColorYELLOW);

  childPass = AddRenderPass(renderPasses, childPassId, childRect,
      transform_causing_aa);
  AddQuad(childPass, childRect, SK_ColorBLUE);

  rootPass = AddRenderPass(renderPasses, rootPassId, viewportRect,
      gfx::Transform());
  AddQuad(rootPass, viewportRect, SK_ColorGREEN);

  AddRenderPassQuad(rootPass, childPass, 0, skia::RefPtr<SkImageFilter>(),
      transform_causing_aa);
  AddRenderPassQuad(childPass, grandChildPass);

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassProgramAA();

  // RenderPassColorMatrixProgramAA
  renderPasses->clear();

  grandChildPass = AddRenderPass(renderPasses, grandChildPassId, grandChildRect,
      transform_causing_aa);
  AddClippedQuad(grandChildPass, grandChildRect, SK_ColorYELLOW);

  childPass = AddRenderPass(renderPasses, childPassId, childRect,
      transform_causing_aa);
  AddQuad(childPass, childRect, SK_ColorBLUE);

  rootPass = AddRenderPass(renderPasses, rootPassId, viewportRect,
      gfx::Transform());
  AddQuad(rootPass, viewportRect, SK_ColorGREEN);

  AddRenderPassQuad(rootPass, childPass, 0, filter, transform_causing_aa);
  AddRenderPassQuad(childPass, grandChildPass);

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassColorMatrixProgramAA();

  // RenderPassMaskProgramAA
  renderPasses->clear();

  grandChildPass = AddRenderPass(renderPasses, grandChildPassId, grandChildRect,
      transform_causing_aa);
  AddClippedQuad(grandChildPass, grandChildRect, SK_ColorYELLOW);

  childPass = AddRenderPass(renderPasses, childPassId, childRect,
      transform_causing_aa);
  AddQuad(childPass, childRect, SK_ColorBLUE);

  rootPass = AddRenderPass(renderPasses, rootPassId, viewportRect,
      gfx::Transform());
  AddQuad(rootPass, viewportRect, SK_ColorGREEN);

  AddRenderPassQuad(rootPass, childPass, mask, skia::RefPtr<SkImageFilter>(),
      transform_causing_aa);
  AddRenderPassQuad(childPass, grandChildPass);

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassMaskProgramAA();

  // RenderPassMaskColorMatrixProgramAA
  renderPasses->clear();

  grandChildPass = AddRenderPass(renderPasses, grandChildPassId, grandChildRect,
      transform_causing_aa);
  AddClippedQuad(grandChildPass, grandChildRect, SK_ColorYELLOW);

  childPass = AddRenderPass(renderPasses, childPassId, childRect,
      transform_causing_aa);
  AddQuad(childPass, childRect, SK_ColorBLUE);

  rootPass = AddRenderPass(renderPasses, rootPassId, viewportRect,
      transform_causing_aa);
  AddQuad(rootPass, viewportRect, SK_ColorGREEN);

  AddRenderPassQuad(rootPass, childPass, mask, filter, transform_causing_aa);
  AddRenderPassQuad(childPass, grandChildPass);

  renderer_->DecideRenderPassAllocationsForFrame(
      *mock_client_.render_passes_in_draw_order());
  renderer_->DrawFrame(mock_client_.render_passes_in_draw_order());
  TestRenderPassMaskColorMatrixProgramAA();
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
  MOCK_METHOD2(reshape, void(int width, int height));
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
            new StrictMock<OutputSurfaceMockContext>)) {}
  virtual ~MockOutputSurface() {}

  MOCK_METHOD1(SendFrameToParentCompositor, void(CompositorFrame* frame));
  MOCK_METHOD0(EnsureBackbuffer, void());
  MOCK_METHOD0(DiscardBackbuffer, void());
  MOCK_METHOD1(Reshape, void(gfx::Size size));
  MOCK_METHOD0(BindFramebuffer, void());
  MOCK_METHOD1(PostSubBuffer, void(gfx::Rect rect));
  MOCK_METHOD0(SwapBuffers, void());
};

class MockOutputSurfaceTest : public testing::Test, public FakeRendererClient {
 protected:
  MockOutputSurfaceTest()
      : resource_provider_(ResourceProvider::Create(&output_surface_)),
        renderer_(this, &output_surface_, resource_provider_.get()) {}

  virtual void SetUp() { EXPECT_TRUE(renderer_.Initialize()); }

  void SwapBuffers() { renderer_.SwapBuffers(); }

  void DrawFrame() {
    gfx::Rect viewport_rect(DeviceViewportSize());
    ScopedPtrVector<RenderPass>* render_passes = render_passes_in_draw_order();
    render_passes->clear();

    RenderPass::Id render_pass_id(1, 0);
    TestRenderPass* render_pass = AddRenderPass(
        render_passes, render_pass_id, viewport_rect, gfx::Transform());
    AddQuad(render_pass, viewport_rect, SK_ColorGREEN);

    EXPECT_CALL(output_surface_, EnsureBackbuffer()).WillRepeatedly(Return());

    EXPECT_CALL(output_surface_, Reshape(_)).Times(1);

    EXPECT_CALL(output_surface_, BindFramebuffer()).Times(1);

    EXPECT_CALL(*context(), drawElements(_, _, _, _)).Times(1);

    renderer_.DecideRenderPassAllocationsForFrame(
        *render_passes_in_draw_order());
    renderer_.DrawFrame(render_passes_in_draw_order());
  }

  OutputSurfaceMockContext* context() {
    return static_cast<OutputSurfaceMockContext*>(output_surface_.context3d());
  }

  StrictMock<MockOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  FakeRendererGL renderer_;
};

TEST_F(MockOutputSurfaceTest, DrawFrameAndSwap) {
  DrawFrame();

  EXPECT_CALL(output_surface_, SwapBuffers()).Times(1);
  renderer_.SwapBuffers();
}

class MockOutputSurfaceTestWithPartialSwap : public MockOutputSurfaceTest {
 public:
  virtual const LayerTreeSettings& Settings() const OVERRIDE {
    static LayerTreeSettings fake_settings;
    fake_settings.partial_swap_enabled = true;
    return fake_settings;
  }
};

TEST_F(MockOutputSurfaceTestWithPartialSwap, DrawFrameAndSwap) {
  DrawFrame();

  EXPECT_CALL(output_surface_, PostSubBuffer(_)).Times(1);
  renderer_.SwapBuffers();
}

class MockOutputSurfaceTestWithSendCompositorFrame :
    public MockOutputSurfaceTest {
 public:
  virtual const LayerTreeSettings& Settings() const OVERRIDE {
    static LayerTreeSettings fake_settings;
    fake_settings.compositor_frame_message = true;
    return fake_settings;
  }
};

TEST_F(MockOutputSurfaceTestWithSendCompositorFrame, DrawFrame) {
  EXPECT_CALL(output_surface_, SendFrameToParentCompositor(_)).Times(1);
  DrawFrame();
}

}  // namespace
}  // namespace cc
