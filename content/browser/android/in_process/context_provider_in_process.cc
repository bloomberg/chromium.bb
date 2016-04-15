// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/context_provider_in_process.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "cc/output/managed_memory_policy.h"
#include "gpu/blink/webgraphicscontext3d_in_process_command_buffer_impl.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "third_party/skia/include/gpu/GrContext.h"

using gpu_blink::WebGraphicsContext3DInProcessCommandBufferImpl;

namespace content {

class ContextProviderInProcess::LostContextCallbackProxy
    : public blink::WebGraphicsContext3D::WebGraphicsContextLostCallback {
 public:
  explicit LostContextCallbackProxy(ContextProviderInProcess* provider)
      : provider_(provider) {
    provider_->context3d_->setContextLostCallback(this);
  }

  ~LostContextCallbackProxy() override {
    provider_->context3d_->setContextLostCallback(nullptr);
  }

  void onContextLost() override {
    provider_->OnLostContext();
  }

 private:
  ContextProviderInProcess* provider_;
};

// static
scoped_refptr<ContextProviderInProcess> ContextProviderInProcess::Create(
    std::unique_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d,
    const std::string& debug_name) {
  if (!context3d)
    return nullptr;
  return new ContextProviderInProcess(std::move(context3d), debug_name);
}

ContextProviderInProcess::ContextProviderInProcess(
    std::unique_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d,
    const std::string& debug_name)
    : context3d_(std::move(context3d)), debug_name_(debug_name) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(context3d_);
  context_thread_checker_.DetachFromThread();
}

ContextProviderInProcess::~ContextProviderInProcess() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());
}

blink::WebGraphicsContext3D* ContextProviderInProcess::WebContext3D() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_.get();
}

bool ContextProviderInProcess::BindToCurrentThread() {
  DCHECK(context3d_);

  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (lost_context_callback_proxy_)
    return true;

  if (!context3d_->InitializeOnCurrentThread())
    return false;

  InitializeCapabilities();

  const std::string unique_context_name =
      base::StringPrintf("%s-%p", debug_name_.c_str(), context3d_.get());
  ContextGL()->TraceBeginCHROMIUM("gpu_toplevel", unique_context_name.c_str());

  lost_context_callback_proxy_.reset(new LostContextCallbackProxy(this));
  return true;
}

void ContextProviderInProcess::DetachFromThread() {
  context_thread_checker_.DetachFromThread();
}

void ContextProviderInProcess::InitializeCapabilities() {
  capabilities_.gpu = context3d_->GetImplementation()->capabilities();

  size_t mapped_memory_limit = context3d_->GetMappedMemoryLimit();
  capabilities_.max_transfer_buffer_usage_bytes =
      mapped_memory_limit ==
              WebGraphicsContext3DInProcessCommandBufferImpl::kNoLimit
          ? std::numeric_limits<size_t>::max()
          : mapped_memory_limit;
}

cc::ContextProvider::Capabilities
ContextProviderInProcess::ContextCapabilities() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());
  return capabilities_;
}

::gpu::gles2::GLES2Interface* ContextProviderInProcess::ContextGL() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_->GetGLInterface();
}

::gpu::ContextSupport* ContextProviderInProcess::ContextSupport() {
  DCHECK(context3d_);
  if (!lost_context_callback_proxy_)
    return NULL;  // Not bound to anything.

  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_->GetContextSupport();
}

class GrContext* ContextProviderInProcess::GrContext() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(new skia_bindings::GrContextForGLES2Interface(
      context3d_->GetGLInterface()));
  return gr_context_->get();
}

void ContextProviderInProcess::InvalidateGrContext(uint32_t state) {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    return gr_context_->ResetContext(state);
}

void ContextProviderInProcess::SetupLock() {
  context3d_->SetLock(&context_lock_);
}

base::Lock* ContextProviderInProcess::GetLock() {
  return &context_lock_;
}

void ContextProviderInProcess::DeleteCachedResources() {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    gr_context_->FreeGpuResources();
}

void ContextProviderInProcess::OnLostContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  if (!lost_context_callback_.is_null())
    lost_context_callback_.Run();
  if (gr_context_)
    gr_context_->OnLostContext();
}

void ContextProviderInProcess::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(lost_context_callback_.is_null() ||
         lost_context_callback.is_null());
  lost_context_callback_ = lost_context_callback;
}

}  // namespace content
