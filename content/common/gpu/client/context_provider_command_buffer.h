// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_CONTEXT_PROVIDER_COMMAND_BUFFER
#define CONTENT_COMMON_GPU_CLIENT_CONTEXT_PROVIDER_COMMAND_BUFFER

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "cc/output/context_provider.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"

namespace webkit {
namespace gpu {
class GrContextForWebGraphicsContext3D;
}
}

namespace content {

// Implementation of cc::ContextProvider that provides a
// WebGraphicsContext3DCommandBufferImpl context and a GrContext.
class ContextProviderCommandBuffer : public cc::ContextProvider {
 public:
  virtual bool BindToCurrentThread() OVERRIDE;
  virtual WebGraphicsContext3DCommandBufferImpl* Context3d() OVERRIDE;
  virtual class GrContext* GrContext() OVERRIDE;
  virtual void VerifyContexts() OVERRIDE;
  virtual bool DestroyedOnMainThread() OVERRIDE;

  void set_leak_on_destroy() {
    base::AutoLock lock(main_thread_lock_);
    leak_on_destroy_ = true;
  }

 protected:
  ContextProviderCommandBuffer();
  virtual ~ContextProviderCommandBuffer();

  // This must be called immedately after creating this object, and it should
  // be thrown away if this returns false.
  bool InitializeOnMainThread();

  // Subclass must provide an implementation to create an offscreen context.
  virtual scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
      CreateOffscreenContext3d() = 0;

  void OnLostContextInternal();
  virtual void OnLostContext() {}
  virtual void OnMemoryAllocationChanged(bool nonzero_allocation);

 private:
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d_;
  scoped_ptr<webkit::gpu::GrContextForWebGraphicsContext3D> gr_context_;

  base::Lock main_thread_lock_;
  bool leak_on_destroy_;
  bool destroyed_;

  class LostContextCallbackProxy;
  scoped_ptr<LostContextCallbackProxy> lost_context_callback_proxy_;

  class MemoryAllocationCallbackProxy;
  scoped_ptr<MemoryAllocationCallbackProxy> memory_allocation_callback_proxy_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_CONTEXT_PROVIDER_COMMAND_BUFFER
