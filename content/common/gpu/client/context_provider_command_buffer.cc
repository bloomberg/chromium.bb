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
    provider_->OnLostContext();
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

  void onMemoryAllocationChanged(WebKit::WebGraphicsMemoryAllocation alloc) {
    provider_->OnMemoryAllocationChanged(!!alloc.gpuResourceSizeInBytes);
  }

 private:
  ContextProviderCommandBuffer* provider_;
};

ContextProviderCommandBuffer::ContextProviderCommandBuffer()
    : destroyed_(false) {
}

ContextProviderCommandBuffer::~ContextProviderCommandBuffer() {}

bool ContextProviderCommandBuffer::InitializeOnMainThread() {
  if (destroyed_)
    return false;
  if (context3d_)
    return true;

  context3d_ = CreateOffscreenContext3d().Pass();
  destroyed_ = !context3d_;
  return !!context3d_;
}

bool ContextProviderCommandBuffer::BindToCurrentThread() {
  if (lost_context_callback_proxy_)
    return true;

  bool result = context3d_->makeContextCurrent();
  lost_context_callback_proxy_.reset(new LostContextCallbackProxy(this));
  return result;
}

WebGraphicsContext3DCommandBufferImpl*
ContextProviderCommandBuffer::Context3d() {
  return context3d_.get();
}

class GrContext* ContextProviderCommandBuffer::GrContext() {
  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(
      new webkit::gpu::GrContextForWebGraphicsContext3D(context3d_.get()));
  memory_allocation_callback_proxy_.reset(
      new MemoryAllocationCallbackProxy(this));
  return gr_context_->get();
}

void ContextProviderCommandBuffer::VerifyContexts() {
  if (!destroyed_ && context3d_->isContextLost())
    OnLostContext();
}

void ContextProviderCommandBuffer::OnLostContext() {
  base::AutoLock lock(destroyed_lock_);
  destroyed_ = true;
}

bool ContextProviderCommandBuffer::DestroyedOnMainThread() {
  base::AutoLock lock(destroyed_lock_);
  return destroyed_;
}

void ContextProviderCommandBuffer::OnMemoryAllocationChanged(
    bool nonzero_allocation) {
  if (gr_context_)
    gr_context_->SetMemoryLimit(nonzero_allocation);
}

}  // namespace content
