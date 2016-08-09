// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CONTEXT_PROVIDER_FACTORY_IMPL_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_CONTEXT_PROVIDER_FACTORY_IMPL_ANDROID_H_

#include <list>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/command_buffer_metrics.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/android/context_provider_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace cc {
class VulkanInProcessContextProvider;
}

namespace gpu {
class GpuChannelHost;
}

namespace content {
class ContextProviderCommandBuffer;

class CONTENT_EXPORT ContextProviderFactoryImpl
    : public ui::ContextProviderFactory {
 public:
  static ContextProviderFactoryImpl* GetInstance();

  ~ContextProviderFactoryImpl() override;

  // The callback may be triggered synchronously, if the Gpu Channel is already
  // initialized. In case the surface_handle is invalidated before the context
  // can be created, the request is dropped and the callback will *not* run.
  void CreateDisplayContextProvider(
      gpu::SurfaceHandle surface_handle,
      gpu::SharedMemoryLimits shared_memory_limits,
      gpu::gles2::ContextCreationAttribHelper attributes,
      bool support_locking,
      bool automatic_flushes,
      ContextProviderCallback result_callback);

  // ContextProviderFactory implementation.
  scoped_refptr<cc::VulkanContextProvider> GetSharedVulkanContextProvider()
      override;
  void CreateOffscreenContextProvider(
      ContextType context_type,
      gpu::SharedMemoryLimits shared_memory_limits,
      gpu::gles2::ContextCreationAttribHelper attributes,
      bool support_locking,
      bool automatic_flushes,
      cc::ContextProvider* shared_context_provider,
      ContextProviderCallback result_callback) override;
  cc::SurfaceManager* GetSurfaceManager() override;
  uint32_t AllocateSurfaceClientId() override;
  cc::SharedBitmapManager* GetSharedBitmapManager() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;

 private:
  friend struct base::DefaultSingletonTraits<ContextProviderFactoryImpl>;

  void CreateContextProviderInternal(
      command_buffer_metrics::ContextType context_type,
      gpu::SurfaceHandle surface_handle,
      gpu::SharedMemoryLimits shared_memory_limits,
      gpu::gles2::ContextCreationAttribHelper attributes,
      bool support_locking,
      bool automatic_flushes,
      cc::ContextProvider* shared_context_provider,
      ContextProviderCallback result_callback);

  struct ContextProvidersRequest {
    ContextProvidersRequest();
    ContextProvidersRequest(const ContextProvidersRequest& other);
    ~ContextProvidersRequest();

    command_buffer_metrics::ContextType context_type;
    gpu::SurfaceHandle surface_handle;
    gpu::SharedMemoryLimits shared_memory_limits;
    gpu::gles2::ContextCreationAttribHelper attributes;
    bool support_locking;
    bool automatic_flushes;
    cc::ContextProvider* shared_context_provider;
    ContextProviderCallback result_callback;
  };

  ContextProviderFactoryImpl();

  // Will return nullptr if the Gpu channel has not been established.
  gpu::GpuChannelHost* EnsureGpuChannelEstablished();
  void OnGpuChannelEstablished();
  void OnGpuChannelTimeout();

  void HandlePendingRequests();

  std::list<ContextProvidersRequest> context_provider_requests_;

  scoped_refptr<ContextProviderCommandBuffer> shared_worker_context_provider_;

  scoped_refptr<cc::VulkanContextProvider> shared_vulkan_context_provider_;

  bool in_handle_pending_requests_;

  base::OneShotTimer establish_gpu_channel_timeout_;

  std::unique_ptr<cc::SurfaceManager> surface_manager_;
  int surface_client_id_;

  base::WeakPtrFactory<ContextProviderFactoryImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContextProviderFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CONTEXT_PROVIDER_FACTORY_IMPL_ANDROID_H_
