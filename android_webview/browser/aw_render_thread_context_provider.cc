// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_render_thread_context_provider.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/managed_memory_policy.h"
#include "gpu/blink/webgraphicscontext3d_impl.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace android_webview {

namespace {

// Singleton used to initialize and terminate the gles2 library.
class GLES2Initializer {
 public:
  GLES2Initializer() { gles2::Initialize(); }

  ~GLES2Initializer() { gles2::Terminate(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(GLES2Initializer);
};

base::LazyInstance<GLES2Initializer> g_gles2_initializer =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
scoped_refptr<AwRenderThreadContextProvider>
AwRenderThreadContextProvider::Create(
    scoped_refptr<gfx::GLSurface> surface,
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service) {
  return new AwRenderThreadContextProvider(surface, service);
}

AwRenderThreadContextProvider::AwRenderThreadContextProvider(
    scoped_refptr<gfx::GLSurface> surface,
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  blink::WebGraphicsContext3D::Attributes attributes;
  attributes.antialias = false;
  attributes.depth = false;
  attributes.stencil = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;
  gpu::gles2::ContextCreationAttribHelper attribs_for_gles2;
  gpu_blink::WebGraphicsContext3DImpl::ConvertAttributes(attributes,
                                                         &attribs_for_gles2);
  attribs_for_gles2.lose_context_when_out_of_memory = true;

  context_.reset(gpu::GLInProcessContext::Create(
      service,
      surface,
      surface->IsOffscreen(),
      gfx::kNullAcceleratedWidget,
      surface->GetSize(),
      NULL /* share_context */,
      false /* share_resources */,
      attribs_for_gles2,
      gfx::PreferDiscreteGpu,
      gpu::GLInProcessContextSharedMemoryLimits(),
      nullptr,
      nullptr));

  context_->SetContextLostCallback(base::Bind(
      &AwRenderThreadContextProvider::OnLostContext, base::Unretained(this)));

  capabilities_.gpu = context_->GetImplementation()->capabilities();
}

AwRenderThreadContextProvider::~AwRenderThreadContextProvider() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
}

bool AwRenderThreadContextProvider::BindToCurrentThread() {
  // This is called on the thread the context will be used.
  DCHECK(main_thread_checker_.CalledOnValidThread());

  return true;
}

cc::ContextProvider::Capabilities
AwRenderThreadContextProvider::ContextCapabilities() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  return capabilities_;
}

gpu::gles2::GLES2Interface* AwRenderThreadContextProvider::ContextGL() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  return context_->GetImplementation();
}

gpu::ContextSupport* AwRenderThreadContextProvider::ContextSupport() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  return context_->GetImplementation();
}

static void BindGrContextCallback(const GrGLInterface* interface) {
  cc::ContextProvider* context_provider =
      reinterpret_cast<AwRenderThreadContextProvider*>(
          interface->fCallbackData);

  gles2::SetGLContext(context_provider->ContextGL());
}

class GrContext* AwRenderThreadContextProvider::GrContext() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    return gr_context_.get();

  // The GrGLInterface factory will make GL calls using the C GLES2 interface.
  // Make sure the gles2 library is initialized first on exactly one thread.
  g_gles2_initializer.Get();
  gles2::SetGLContext(ContextGL());

  skia::RefPtr<GrGLInterface> interface =
      skia::AdoptRef(skia_bindings::CreateCommandBufferSkiaGLBinding());
  interface->fCallback = BindGrContextCallback;
  interface->fCallbackData = reinterpret_cast<GrGLInterfaceCallbackData>(this);

  gr_context_ = skia::AdoptRef(GrContext::Create(
      kOpenGL_GrBackend, reinterpret_cast<GrBackendContext>(interface.get())));

  return gr_context_.get();
}

void AwRenderThreadContextProvider::InvalidateGrContext(uint32_t state) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    gr_context_.get()->resetContext(state);
}

void AwRenderThreadContextProvider::SetupLock() {
  context_->SetLock(&context_lock_);
}

base::Lock* AwRenderThreadContextProvider::GetLock() {
  return &context_lock_;
}

void AwRenderThreadContextProvider::VerifyContexts() {
}

void AwRenderThreadContextProvider::DeleteCachedResources() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  if (gr_context_) {
    TRACE_EVENT_INSTANT0("gpu", "GrContext::freeGpuResources",
                         TRACE_EVENT_SCOPE_THREAD);
    gr_context_->freeGpuResources();
  }
}

void AwRenderThreadContextProvider::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  lost_context_callback_ = lost_context_callback;
}

void AwRenderThreadContextProvider::OnLostContext() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  if (!lost_context_callback_.is_null())
    base::ResetAndReturn(&lost_context_callback_).Run();
  if (gr_context_)
    gr_context_->abandonContext();
}

}  // namespace android_webview
