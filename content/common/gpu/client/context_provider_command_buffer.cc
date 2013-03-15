// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/context_provider_command_buffer.h"

#include "webkit/gpu/grcontext_for_webgraphicscontext3d.h"

namespace content {

class ContextProviderCommandBuffer::LostContextCallbackProxy
    : public WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback {
 public:
  explicit LostContextCallbackProxy(ContextProviderCommandBuffer* provider)
      : provider_(provider) {
    provider_->context3d_->setContextLostCallback(this);
  }

  virtual void onContextLost() {
    provider_->OnLostContextInternal();
  }

 private:
  ContextProviderCommandBuffer* provider_;
};

class ContextProviderCommandBuffer::MemoryAllocationCallbackProxy
    : public WebKit::WebGraphicsContext3D::
          WebGraphicsMemoryAllocationChangedCallbackCHROMIUM {
 public:
  explicit MemoryAllocationCallbackProxy(ContextProviderCommandBuffer* provider)
      : provider_(provider) {
    provider_->context3d_->setMemoryAllocationChangedCallbackCHROMIUM(this);
  }

  virtual void onMemoryAllocationChanged(
      WebKit::WebGraphicsMemoryAllocation alloc) {
    provider_->OnMemoryAllocationChanged(!!alloc.gpuResourceSizeInBytes);
  }

 private:
  ContextProviderCommandBuffer* provider_;
};

ContextProviderCommandBuffer::ContextProviderCommandBuffer()
    : leak_on_destroy_(false),
      destroyed_(false) {
}

ContextProviderCommandBuffer::~ContextProviderCommandBuffer() {
  base::AutoLock lock(main_thread_lock_);
  if (leak_on_destroy_) {
    WebGraphicsContext3DCommandBufferImpl* context3d ALLOW_UNUSED =
        context3d_.release();
    webkit::gpu::GrContextForWebGraphicsContext3D* gr_context ALLOW_UNUSED =
        gr_context_.release();
  }
}

bool ContextProviderCommandBuffer::InitializeOnMainThread() {
  DCHECK(!context3d_);
  context3d_ = CreateOffscreenContext3d().Pass();
  return !!context3d_;
}

bool ContextProviderCommandBuffer::BindToCurrentThread() {
  DCHECK(context3d_);

  if (lost_context_callback_proxy_)
    return true;

  if (!context3d_->makeContextCurrent())
    return false;

  lost_context_callback_proxy_.reset(new LostContextCallbackProxy(this));
  return true;
}

WebGraphicsContext3DCommandBufferImpl*
ContextProviderCommandBuffer::Context3d() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.

  return context3d_.get();
}

class GrContext* ContextProviderCommandBuffer::GrContext() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.

  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(
      new webkit::gpu::GrContextForWebGraphicsContext3D(context3d_.get()));
  memory_allocation_callback_proxy_.reset(
      new MemoryAllocationCallbackProxy(this));
  return gr_context_->get();
}

void ContextProviderCommandBuffer::VerifyContexts() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.

  if (context3d_->isContextLost())
    OnLostContextInternal();
}

void ContextProviderCommandBuffer::OnLostContextInternal() {
  {
    base::AutoLock lock(main_thread_lock_);
    if (destroyed_)
      return;
    destroyed_ = true;
  }
  OnLostContext();
}

bool ContextProviderCommandBuffer::DestroyedOnMainThread() {
  base::AutoLock lock(main_thread_lock_);
  return destroyed_;
}

void ContextProviderCommandBuffer::OnMemoryAllocationChanged(
    bool nonzero_allocation) {
  if (gr_context_)
    gr_context_->SetMemoryLimit(nonzero_allocation);
}

}  // namespace content
