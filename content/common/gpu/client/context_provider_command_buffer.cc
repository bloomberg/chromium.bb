// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/context_provider_command_buffer.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "cc/output/managed_memory_policy.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_trace_implementation.h"
#include "gpu/command_buffer/client/gpu_switches.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace content {

ContextProviderCommandBuffer::SharedProviders::SharedProviders() = default;
ContextProviderCommandBuffer::SharedProviders::~SharedProviders() = default;

class ContextProviderCommandBuffer::LostContextCallbackProxy
    : public WebGraphicsContext3DCommandBufferImpl::
          WebGraphicsContextLostCallback {
 public:
  explicit LostContextCallbackProxy(ContextProviderCommandBuffer* provider)
      : provider_(provider) {
    provider_->context3d_->SetContextLostCallback(this);
  }

  ~LostContextCallbackProxy() override {
    provider_->context3d_->SetContextLostCallback(nullptr);
  }

  void onContextLost() override { provider_->OnLostContext(); }

 private:
  ContextProviderCommandBuffer* provider_;
};

ContextProviderCommandBuffer::ContextProviderCommandBuffer(
    std::unique_ptr<WebGraphicsContext3DCommandBufferImpl> context3d,
    const gpu::SharedMemoryLimits& memory_limits,
    ContextProviderCommandBuffer* shared_context_provider,
    CommandBufferContextType type)
    : shared_providers_(shared_context_provider
                            ? shared_context_provider->shared_providers_
                            : new SharedProviders),
      context3d_(std::move(context3d)),
      memory_limits_(memory_limits),
      context_type_(type),
      debug_name_(CommandBufferContextTypeToString(type)) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(context3d_);
  context_thread_checker_.DetachFromThread();
}

ContextProviderCommandBuffer::~ContextProviderCommandBuffer() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());

  {
    base::AutoLock hold(shared_providers_->lock);
    auto it = std::find(shared_providers_->list.begin(),
                        shared_providers_->list.end(), this);
    if (it != shared_providers_->list.end())
      shared_providers_->list.erase(it);
  }

  // Destroy references to the context3d_ before leaking it.
  // TODO(danakj): Delete this.
  if (context3d_->GetCommandBufferProxy())
    context3d_->GetCommandBufferProxy()->SetLock(nullptr);

  if (lost_context_callback_proxy_) {
    // Disconnect lost callbacks during destruction.
    lost_context_callback_proxy_.reset();
  }
}

gpu::CommandBufferProxyImpl*
ContextProviderCommandBuffer::GetCommandBufferProxy() {
  return context3d_->GetCommandBufferProxy();
}

WebGraphicsContext3DCommandBufferImpl*
ContextProviderCommandBuffer::WebContext3D() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());
  return context3d_.get();
}

bool ContextProviderCommandBuffer::BindToCurrentThread() {
  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (lost_context_callback_proxy_)
    return true;

  context3d_->SetContextType(context_type_);

  // It's possible to be running BindToCurrentThread on two contexts
  // on different threads at the same time, but which will be in the same share
  // group. To ensure they end up in the same group, hold the lock on the
  // shared_providers_ (which they will share) after querying the group, until
  // this context has been added to the list.
  {
    ContextProviderCommandBuffer* shared_context_provider = nullptr;
    gpu::CommandBufferProxyImpl* shared_command_buffer = nullptr;
    scoped_refptr<gpu::gles2::ShareGroup> share_group;

    base::AutoLock hold(shared_providers_->lock);

    if (!shared_providers_->list.empty()) {
      shared_context_provider = shared_providers_->list.front();
      shared_command_buffer =
          shared_context_provider->context3d_->GetCommandBufferProxy();
      share_group = shared_context_provider->context3d_->GetImplementation()
                        ->share_group();
    }

    if (!context3d_->InitializeOnCurrentThread(
            memory_limits_, shared_command_buffer, std::move(share_group)))
      return false;

    // If any context in the share group has been lost, then abort and don't
    // continue since we need to go back to the caller of the constructor to
    // find the correct share group.
    // This may happen in between the share group being chosen at the
    // constructor, and getting to run this BindToCurrentThread method which
    // can be on some other thread.
    // We intentionally call this *after* creating the command buffer via the
    // GpuChannelHost. Once that has happened, the service knows we are in the
    // share group and if a shared context is lost, our context will be informed
    // also, and the lost context callback will occur for the owner of the
    // context provider. If we check sooner, the shared context may be lost in
    // between these two states and our context here would be left in an orphan
    // share group.
    if (share_group && share_group->IsLost())
      return false;

    shared_providers_->list.push_back(this);
  }

  lost_context_callback_proxy_.reset(new LostContextCallbackProxy(this));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableGpuClientTracing)) {
    // This wraps the real GLES2Implementation and we should always use this
    // instead when it's present.
    trace_impl_.reset(new gpu::gles2::GLES2TraceImplementation(
        context3d_->GetImplementation()));
  }

  // Do this last once the context is set up.
  std::string unique_context_name =
      base::StringPrintf("%s-%p", debug_name_.c_str(), context3d_.get());
  ContextGL()->TraceBeginCHROMIUM("gpu_toplevel", unique_context_name.c_str());
  return true;
}

void ContextProviderCommandBuffer::DetachFromThread() {
  context_thread_checker_.DetachFromThread();
}

gpu::gles2::GLES2Interface* ContextProviderCommandBuffer::ContextGL() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (trace_impl_)
    return trace_impl_.get();
  return context3d_->GetImplementation();
}

gpu::ContextSupport* ContextProviderCommandBuffer::ContextSupport() {
  return context3d_->GetContextSupport();
}

class GrContext* ContextProviderCommandBuffer::GrContext() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(new skia_bindings::GrContextForGLES2Interface(ContextGL()));

  // If GlContext is already lost, also abandon the new GrContext.
  if (gr_context_->get() &&
      ContextGL()->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    gr_context_->get()->abandonContext();

  return gr_context_->get();
}

void ContextProviderCommandBuffer::InvalidateGrContext(uint32_t state) {
  if (gr_context_) {
    DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
    DCHECK(context_thread_checker_.CalledOnValidThread());
    gr_context_->ResetContext(state);
  }
}

void ContextProviderCommandBuffer::SetupLock() {
  DCHECK(context3d_);
  context3d_->GetCommandBufferProxy()->SetLock(&context_lock_);
}

base::Lock* ContextProviderCommandBuffer::GetLock() {
  return &context_lock_;
}

gpu::Capabilities ContextProviderCommandBuffer::ContextCapabilities() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());
  // Skips past the trace_impl_ as it doesn't have capabilities.
  return context3d_->GetImplementation()->capabilities();
}

void ContextProviderCommandBuffer::DeleteCachedResources() {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    gr_context_->FreeGpuResources();
}

void ContextProviderCommandBuffer::OnLostContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (!lost_context_callback_.is_null())
    lost_context_callback_.Run();
  if (gr_context_)
    gr_context_->OnLostContext();
}

void ContextProviderCommandBuffer::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(lost_context_callback_.is_null() ||
         lost_context_callback.is_null());
  lost_context_callback_ = lost_context_callback;
}

}  // namespace content
