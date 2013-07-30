// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/context_provider_command_buffer.h"

#include "base/callback_helpers.h"
#include "webkit/common/gpu/grcontext_for_webgraphicscontext3d.h"

namespace content {

class ContextProviderCommandBuffer::LostContextCallbackProxy
    : public WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback {
 public:
  explicit LostContextCallbackProxy(ContextProviderCommandBuffer* provider)
      : provider_(provider) {
    provider_->context3d_->setContextLostCallback(this);
  }

  virtual ~LostContextCallbackProxy() {
    provider_->context3d_->setContextLostCallback(NULL);
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

  virtual ~MemoryAllocationCallbackProxy() {
    provider_->context3d_->setMemoryAllocationChangedCallbackCHROMIUM(NULL);
  }

  virtual void onMemoryAllocationChanged(
      WebKit::WebGraphicsMemoryAllocation alloc) {
    provider_->OnMemoryAllocationChanged(!!alloc.gpuResourceSizeInBytes);
  }

 private:
  ContextProviderCommandBuffer* provider_;
};

scoped_refptr<ContextProviderCommandBuffer>
ContextProviderCommandBuffer::Create(const CreateCallback& create_callback) {
  scoped_refptr<ContextProviderCommandBuffer> provider =
      new ContextProviderCommandBuffer;
  if (!provider->InitializeOnMainThread(create_callback))
    return NULL;
  return provider;
}

ContextProviderCommandBuffer::ContextProviderCommandBuffer()
    : leak_on_destroy_(false),
      destroyed_(false) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  context_thread_checker_.DetachFromThread();
}

ContextProviderCommandBuffer::~ContextProviderCommandBuffer() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());

  base::AutoLock lock(main_thread_lock_);
  if (leak_on_destroy_) {
    WebGraphicsContext3DCommandBufferImpl* context3d ALLOW_UNUSED =
        context3d_.release();
    webkit::gpu::GrContextForWebGraphicsContext3D* gr_context ALLOW_UNUSED =
        gr_context_.release();
  }
}

bool ContextProviderCommandBuffer::InitializeOnMainThread(
    const CreateCallback& create_callback) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  DCHECK(!context3d_);
  DCHECK(!create_callback.is_null());
  context3d_ = create_callback.Run();
  return !!context3d_;
}

bool ContextProviderCommandBuffer::BindToCurrentThread() {
  DCHECK(context3d_);

  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

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
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_.get();
}

class GrContext* ContextProviderCommandBuffer::GrContext() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

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
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (context3d_->isContextLost())
    OnLostContext();
}

void ContextProviderCommandBuffer::OnLostContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  {
    base::AutoLock lock(main_thread_lock_);
    if (destroyed_)
      return;
    destroyed_ = true;
  }
  if (!lost_context_callback_.is_null())
    base::ResetAndReturn(&lost_context_callback_).Run();
}

bool ContextProviderCommandBuffer::DestroyedOnMainThread() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  base::AutoLock lock(main_thread_lock_);
  return destroyed_;
}

void ContextProviderCommandBuffer::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(lost_context_callback_.is_null());
  lost_context_callback_ = lost_context_callback;
}

void ContextProviderCommandBuffer::OnMemoryAllocationChanged(
    bool nonzero_allocation) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  if (gr_context_)
    gr_context_->SetMemoryLimit(nonzero_allocation);
}

}  // namespace content
