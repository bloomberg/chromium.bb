// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_context_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/test/test_gles2_interface.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace cc {

namespace {

// Various tests rely on functionality (capabilities) enabled by these extension
// strings.
const char* const kExtensions[] = {"GL_EXT_stencil_wrap",
                                   "GL_EXT_texture_format_BGRA8888",
                                   "GL_OES_rgb8_rgba8",
                                   "GL_EXT_texture_norm16",
                                   "GL_CHROMIUM_framebuffer_multisample",
                                   "GL_CHROMIUM_renderbuffer_format_BGRA8888"};

class TestGLES2InterfaceForContextProvider : public TestGLES2Interface {
 public:
  TestGLES2InterfaceForContextProvider()
      : extension_string_(BuildExtensionString()) {}
  ~TestGLES2InterfaceForContextProvider() override = default;

  // TestGLES2Interface:
  const GLubyte* GetString(GLenum name) override {
    switch (name) {
      case GL_EXTENSIONS:
        return reinterpret_cast<const GLubyte*>(extension_string_.c_str());
      case GL_VERSION:
        return reinterpret_cast<const GrGLubyte*>("4.0 Null GL");
      case GL_SHADING_LANGUAGE_VERSION:
        return reinterpret_cast<const GrGLubyte*>("4.20.8 Null GLSL");
      case GL_VENDOR:
        return reinterpret_cast<const GrGLubyte*>("Null Vendor");
      case GL_RENDERER:
        return reinterpret_cast<const GrGLubyte*>("The Null (Non-)Renderer");
    }
    return nullptr;
  }
  const GrGLubyte* GetStringi(GrGLenum name, GrGLuint i) override {
    if (name == GL_EXTENSIONS && i < arraysize(kExtensions))
      return reinterpret_cast<const GLubyte*>(kExtensions[i]);
    return nullptr;
  }
  void GetIntegerv(GLenum name, GLint* params) override {
    switch (name) {
      case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
        *params = 8;
        return;
      default:
        break;
    }
    TestGLES2Interface::GetIntegerv(name, params);
  }

 private:
  static std::string BuildExtensionString() {
    std::string extension_string = kExtensions[0];
    for (size_t i = 1; i < arraysize(kExtensions); ++i) {
      extension_string += " ";
      extension_string += kExtensions[i];
    }
    return extension_string;
  }

  const std::string extension_string_;

  DISALLOW_COPY_AND_ASSIGN(TestGLES2InterfaceForContextProvider);
};

}  // namespace

// static
scoped_refptr<TestContextProvider> TestContextProvider::Create() {
  return new TestContextProvider(
      std::make_unique<TestContextSupport>(),
      std::make_unique<TestGLES2InterfaceForContextProvider>(),
      TestWebGraphicsContext3D::Create());
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::CreateWorker() {
  scoped_refptr<TestContextProvider> worker_context_provider(
      new TestContextProvider(
          std::make_unique<TestContextSupport>(),
          std::make_unique<TestGLES2InterfaceForContextProvider>(),
          TestWebGraphicsContext3D::Create()));
  // Worker contexts are bound to the thread they are created on.
  if (!worker_context_provider->BindToCurrentThread())
    return nullptr;
  return worker_context_provider;
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::Create(
    std::unique_ptr<TestWebGraphicsContext3D> context) {
  DCHECK(context);
  return new TestContextProvider(
      std::make_unique<TestContextSupport>(),
      std::make_unique<TestGLES2InterfaceForContextProvider>(),
      std::move(context));
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::Create(
    std::unique_ptr<TestGLES2Interface> gl) {
  DCHECK(gl);
  return new TestContextProvider(std::make_unique<TestContextSupport>(),
                                 std::move(gl),
                                 TestWebGraphicsContext3D::Create());
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::Create(
    std::unique_ptr<TestWebGraphicsContext3D> context,
    std::unique_ptr<TestContextSupport> support) {
  DCHECK(context);
  DCHECK(support);
  return new TestContextProvider(
      std::move(support),
      std::make_unique<TestGLES2InterfaceForContextProvider>(),
      std::move(context));
}

TestContextProvider::TestContextProvider(
    std::unique_ptr<TestContextSupport> support,
    std::unique_ptr<TestGLES2Interface> gl,
    std::unique_ptr<TestWebGraphicsContext3D> context)
    : support_(std::move(support)),
      context3d_(std::move(context)),
      context_gl_(std::move(gl)),
      weak_ptr_factory_(this) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(context3d_);
  DCHECK(context_gl_);
  context_thread_checker_.DetachFromThread();
  context_gl_->set_test_context(context3d_.get());
  context3d_->set_test_support(support_.get());
  // Just pass nullptr to the viz::ContextCacheController for its task runner.
  // Idle handling is tested directly in viz::ContextCacheController's
  // unittests, and isn't needed here.
  cache_controller_.reset(
      new viz::ContextCacheController(support_.get(), nullptr));
}

TestContextProvider::~TestContextProvider() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());
}

bool TestContextProvider::BindToCurrentThread() {
  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (bound_)
    return true;

  if (context_gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    return false;
  }
  bound_ = true;

  context3d_->set_context_lost_callback(
      base::Bind(&TestContextProvider::OnLostContext,
                 base::Unretained(this)));

  return true;
}

void TestContextProvider::DetachFromThread() {
  context_thread_checker_.DetachFromThread();
}

const gpu::Capabilities& TestContextProvider::ContextCapabilities() const {
  DCHECK(bound_);
  DCHECK(context_thread_checker_.CalledOnValidThread());
  return context3d_->test_capabilities();
}

gpu::gles2::GLES2Interface* TestContextProvider::ContextGL() {
  DCHECK(context3d_);
  DCHECK(bound_);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context_gl_.get();
}

gpu::ContextSupport* TestContextProvider::ContextSupport() {
  return support();
}

class GrContext* TestContextProvider::GrContext() {
  DCHECK(bound_);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    return gr_context_->get();

  gr_context_ = std::make_unique<skia_bindings::GrContextForGLES2Interface>(
      context_gl_.get(), context3d_->test_capabilities());
  cache_controller_->SetGrContext(gr_context_->get());

  // If GlContext is already lost, also abandon the new GrContext.
  if (ContextGL()->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    gr_context_->get()->abandonContext();

  return gr_context_->get();
}

viz::ContextCacheController* TestContextProvider::CacheController() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  return cache_controller_.get();
}

void TestContextProvider::InvalidateGrContext(uint32_t state) {
  DCHECK(bound_);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    gr_context_->get()->resetContext(state);
}

base::Lock* TestContextProvider::GetLock() {
  return &context_lock_;
}

void TestContextProvider::OnLostContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  if (!lost_context_callback_.is_null())
    lost_context_callback_.Run();
  if (gr_context_)
    gr_context_->get()->abandonContext();
}

TestWebGraphicsContext3D* TestContextProvider::TestContext3d() {
  DCHECK(bound_);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_.get();
}

TestWebGraphicsContext3D* TestContextProvider::UnboundTestContext3d() {
  return context3d_.get();
}

void TestContextProvider::SetLostContextCallback(
    const LostContextCallback& cb) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(lost_context_callback_.is_null() || cb.is_null());
  lost_context_callback_ = cb;
}

}  // namespace cc
