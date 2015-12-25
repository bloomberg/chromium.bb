// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_BROWSER_GPU_CHANNEL_HOST_FACTORY_H_
#define CONTENT_BROWSER_GPU_BROWSER_GPU_CHANNEL_HOST_FACTORY_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "ipc/message_filter.h"

namespace content {
class BrowserGpuMemoryBufferManager;

class CONTENT_EXPORT BrowserGpuChannelHostFactory
    : public GpuChannelHostFactory {
 public:
  static void Initialize(bool establish_gpu_channel);
  static void Terminate();
  static BrowserGpuChannelHostFactory* instance() { return instance_; }

  // Overridden from GpuChannelHostFactory:
  bool IsMainThread() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner() override;
  scoped_ptr<base::SharedMemory> AllocateSharedMemory(size_t size) override;
  CreateCommandBufferResult CreateViewCommandBuffer(
      int32_t surface_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32_t route_id) override;

  int GpuProcessHostId() { return gpu_host_id_; }
#if !defined(OS_ANDROID)
  GpuChannelHost* EstablishGpuChannelSync(
      CauseForGpuLaunch cause_for_gpu_launch);
#endif
  void EstablishGpuChannel(CauseForGpuLaunch cause_for_gpu_launch,
                           const base::Closure& callback);
  GpuChannelHost* GetGpuChannel();
  int GetGpuChannelId() { return gpu_client_id_; }

  // Used to skip GpuChannelHost tests when there can be no GPU process.
  static bool CanUseForTesting();

 private:
  struct CreateRequest;
  class EstablishRequest;

  BrowserGpuChannelHostFactory();
  ~BrowserGpuChannelHostFactory() override;

  void GpuChannelEstablished();
  void CreateViewCommandBufferOnIO(
      CreateRequest* request,
      int32_t surface_id,
      const GPUCreateCommandBufferConfig& init_params);
  static void CommandBufferCreatedOnIO(CreateRequest* request,
                                       CreateCommandBufferResult result);
  static void AddFilterOnIO(int gpu_host_id,
                            scoped_refptr<IPC::MessageFilter> filter);
  static void InitializeShaderDiskCacheOnIO(int gpu_client_id,
                                            const base::FilePath& cache_dir);

  const int gpu_client_id_;
  const uint64_t gpu_client_tracing_id_;
  scoped_ptr<base::WaitableEvent> shutdown_event_;
  scoped_refptr<GpuChannelHost> gpu_channel_;
  scoped_ptr<BrowserGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  int gpu_host_id_;
  scoped_refptr<EstablishRequest> pending_request_;
  std::vector<base::Closure> established_callbacks_;

  static BrowserGpuChannelHostFactory* instance_;

  DISALLOW_COPY_AND_ASSIGN(BrowserGpuChannelHostFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_BROWSER_GPU_CHANNEL_HOST_FACTORY_H_
