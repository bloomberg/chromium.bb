// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_context_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "cc/test/test_gles2_interface.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace cc {

// static
scoped_refptr<TestContextProvider> TestContextProvider::Create() {
  return Create(TestWebGraphicsContext3D::Create());
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::CreateWorker() {
  scoped_refptr<TestContextProvider> worker_context_provider =
      Create(TestWebGraphicsContext3D::Create());
  if (!worker_context_provider)
    return nullptr;
  // Worker contexts are bound to the thread they are created on.
  if (!worker_context_provider->BindToCurrentThread())
    return nullptr;
  worker_context_provider->SetupLock();
  return worker_context_provider;
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::Create(
    std::unique_ptr<TestWebGraphicsContext3D> context) {
  if (!context)
    return NULL;
  return new TestContextProvider(std::move(context));
}

TestContextProvider::TestContextProvider(
    std::unique_ptr<TestWebGraphicsContext3D> context)
    : context3d_(std::move(context)),
      context_gl_(new TestGLES2Interface(context3d_.get())),
      bound_(false),
      weak_ptr_factory_(this) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(context3d_);
  context_thread_checker_.DetachFromThread();
  context3d_->set_test_support(&support_);
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

gpu::Capabilities TestContextProvider::ContextCapabilities() {
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
  return &support_;
}

class GrContext* TestContextProvider::GrContext() {
  DCHECK(bound_);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    return gr_context_.get();

  skia::RefPtr<const GrGLInterface> gl_interface =
      skia::AdoptRef(GrGLCreateNullInterface());
  gr_context_ = skia::AdoptRef(GrContext::Create(
      kOpenGL_GrBackend,
      reinterpret_cast<GrBackendContext>(gl_interface.get())));

  // If GlContext is already lost, also abandon the new GrContext.
  if (ContextGL()->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    gr_context_->abandonContext();

  return gr_context_.get();
}

void TestContextProvider::InvalidateGrContext(uint32_t state) {
  DCHECK(bound_);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    gr_context_.get()->resetContext(state);
}

void TestContextProvider::SetupLock() {
}

base::Lock* TestContextProvider::GetLock() {
  return &context_lock_;
}

void TestContextProvider::DeleteCachedResources() {
}

void TestContextProvider::OnLostContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  if (!lost_context_callback_.is_null())
    lost_context_callback_.Run();
  if (gr_context_)
    gr_context_->abandonContext();
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
