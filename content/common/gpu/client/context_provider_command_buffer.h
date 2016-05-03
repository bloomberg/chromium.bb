// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_CONTEXT_PROVIDER_COMMAND_BUFFER_H_
#define CONTENT_COMMON_GPU_CLIENT_CONTEXT_PROVIDER_COMMAND_BUFFER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/output/context_provider.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/command_buffer_metrics.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gl/gpu_preference.h"
#include "url/gurl.h"

namespace gpu {
class CommandBufferProxyImpl;
class GpuChannelHost;
class TransferBuffer;
namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
class GLES2TraceImplementation;
}
}

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace content {
class WebGraphicsContext3DCommandBufferImpl;

// Implementation of cc::ContextProvider that provides a GL implementation over
// command buffer to the GPU process.
class CONTENT_EXPORT ContextProviderCommandBuffer
    : NON_EXPORTED_BASE(public cc::ContextProvider) {
 public:
  ContextProviderCommandBuffer(
      scoped_refptr<gpu::GpuChannelHost> channel,
      gpu::SurfaceHandle surface_handle,
      const GURL& active_url,
      gfx::GpuPreference gpu_preference,
      bool automatic_flushes,
      const gpu::SharedMemoryLimits& memory_limits,
      const gpu::gles2::ContextCreationAttribHelper& attributes,
      ContextProviderCommandBuffer* shared_context_provider,
      command_buffer_metrics::ContextType type);

  gpu::CommandBufferProxyImpl* GetCommandBufferProxy();

  // cc::ContextProvider implementation.
  bool BindToCurrentThread() override;
  void DetachFromThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  void InvalidateGrContext(uint32_t state) override;
  base::Lock* GetLock() override;
  gpu::Capabilities ContextCapabilities() override;
  void DeleteCachedResources() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;

  // Sets up a lock so this context can be used from multiple threads. After
  // calling this, all functions without explicit thread usage constraints can
  // be used on any thread while the lock returned by GetLock() is acquired.
  void SetupLock();

 protected:
  ~ContextProviderCommandBuffer() override;

  void OnLostContext();

 private:
  struct SharedProviders : public base::RefCountedThreadSafe<SharedProviders> {
    base::Lock lock;
    std::vector<ContextProviderCommandBuffer*> list;

    SharedProviders();

   private:
    friend class base::RefCountedThreadSafe<SharedProviders>;
    ~SharedProviders();
  };

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  bool bind_succeeded_ = false;
  bool bind_failed_ = false;

  gpu::SurfaceHandle surface_handle_;
  GURL active_url_;
  gfx::GpuPreference gpu_preference_;
  bool automatic_flushes_;
  gpu::SharedMemoryLimits memory_limits_;
  gpu::gles2::ContextCreationAttribHelper attributes_;
  command_buffer_metrics::ContextType context_type_;

  scoped_refptr<SharedProviders> shared_providers_;
  scoped_refptr<gpu::GpuChannelHost> channel_;

  base::Lock context_lock_;  // Referenced by command_buffer_.
  std::unique_ptr<gpu::CommandBufferProxyImpl> command_buffer_;
  std::unique_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;
  std::unique_ptr<gpu::gles2::GLES2Implementation> gles2_impl_;
  std::unique_ptr<gpu::gles2::GLES2TraceImplementation> trace_impl_;
  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;

  LostContextCallback lost_context_callback_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_CONTEXT_PROVIDER_COMMAND_BUFFER_H_
