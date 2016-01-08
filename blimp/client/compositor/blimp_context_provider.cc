// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/compositor/blimp_context_provider.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace blimp {
namespace client {
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

static void BindGrContextCallback(const GrGLInterface* interface) {
  BlimpContextProvider* context_provider =
      reinterpret_cast<BlimpContextProvider*>(interface->fCallbackData);

  gles2::SetGLContext(context_provider->ContextGL());
}

}  // namespace

// static
scoped_refptr<BlimpContextProvider> BlimpContextProvider::Create(
    gfx::AcceleratedWidget widget) {
  return new BlimpContextProvider(widget);
}

BlimpContextProvider::BlimpContextProvider(gfx::AcceleratedWidget widget) {
  context_thread_checker_.DetachFromThread();

  gpu::gles2::ContextCreationAttribHelper attribs_for_gles2;
  attribs_for_gles2.alpha_size = 8;
  attribs_for_gles2.depth_size = 0;
  attribs_for_gles2.stencil_size = 0;
  attribs_for_gles2.samples = 0;
  attribs_for_gles2.sample_buffers = 0;
  attribs_for_gles2.fail_if_major_perf_caveat = false;
  attribs_for_gles2.bind_generates_resource = false;
  attribs_for_gles2.context_type = gpu::gles2::CONTEXT_TYPE_OPENGLES2;
  attribs_for_gles2.lose_context_when_out_of_memory = true;

  context_.reset(gpu::GLInProcessContext::Create(
      nullptr /* service */, nullptr /* surface */, false /* is_offscreen */,
      widget, gfx::Size(1, 1), nullptr /* share_context */,
      false /* share_resources */, attribs_for_gles2, gfx::PreferDiscreteGpu,
      gpu::GLInProcessContextSharedMemoryLimits(),
      nullptr /* gpu_memory_buffer_manager */, nullptr /* memory_limits */));
  context_->SetContextLostCallback(
      base::Bind(&BlimpContextProvider::OnLostContext, base::Unretained(this)));
}

BlimpContextProvider::~BlimpContextProvider() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());
}

bool BlimpContextProvider::BindToCurrentThread() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  capabilities_.gpu = context_->GetImplementation()->capabilities();
  capabilities_.gpu.image = true;
  return true;
}

void BlimpContextProvider::DetachFromThread() {
  context_thread_checker_.DetachFromThread();
}

cc::ContextProvider::Capabilities BlimpContextProvider::ContextCapabilities() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  return capabilities_;
}

gpu::gles2::GLES2Interface* BlimpContextProvider::ContextGL() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  return context_->GetImplementation();
}

gpu::ContextSupport* BlimpContextProvider::ContextSupport() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  return context_->GetImplementation();
}

class GrContext* BlimpContextProvider::GrContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    return gr_context_.get();

  // The GrGLInterface factory will make GL calls using the C GLES2 interface.
  // Make sure the gles2 library is initialized first on exactly one thread.
  g_gles2_initializer.Get();
  gles2::SetGLContext(ContextGL());

  skia::RefPtr<GrGLInterface> interface = skia::AdoptRef(new GrGLInterface);
  skia_bindings::InitCommandBufferSkiaGLBinding(interface.get());
  interface->fCallback = BindGrContextCallback;
  interface->fCallbackData = reinterpret_cast<GrGLInterfaceCallbackData>(this);

  gr_context_ = skia::AdoptRef(GrContext::Create(
      kOpenGL_GrBackend, reinterpret_cast<GrBackendContext>(interface.get())));

  return gr_context_.get();
}

void BlimpContextProvider::InvalidateGrContext(uint32_t state) {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    gr_context_.get()->resetContext(state);
}

void BlimpContextProvider::SetupLock() {
  context_->SetLock(&context_lock_);
}

base::Lock* BlimpContextProvider::GetLock() {
  return &context_lock_;
}

void BlimpContextProvider::DeleteCachedResources() {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    gr_context_->freeGpuResources();
}

void BlimpContextProvider::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  lost_context_callback_ = lost_context_callback;
}

void BlimpContextProvider::OnLostContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  if (!lost_context_callback_.is_null())
    base::ResetAndReturn(&lost_context_callback_).Run();
  if (gr_context_)
    gr_context_->abandonContext();
}

}  // namespace client
}  // namespace blimp
