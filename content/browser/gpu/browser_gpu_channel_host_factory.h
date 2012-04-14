// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_BROWSER_GPU_CHANNEL_HOST_FACTORY_H_
#define CONTENT_BROWSER_GPU_BROWSER_GPU_CHANNEL_HOST_FACTORY_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "ipc/ipc_channel_handle.h"

namespace content {

class BrowserGpuChannelHostFactory : public GpuChannelHostFactory {
 public:
  static void Initialize();
  static void Terminate();
  static BrowserGpuChannelHostFactory* instance() { return instance_; }

  // GpuChannelHostFactory implementation.
  virtual bool IsMainThread() OVERRIDE;
  virtual bool IsIOThread() OVERRIDE;
  virtual MessageLoop* GetMainLoop() OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy> GetIOLoopProxy() OVERRIDE;
  virtual base::WaitableEvent* GetShutDownEvent() OVERRIDE;
  virtual scoped_ptr<base::SharedMemory> AllocateSharedMemory(
      uint32 size) OVERRIDE;
  virtual int32 CreateViewCommandBuffer(
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params) OVERRIDE;
  virtual GpuChannelHost* EstablishGpuChannelSync(
      CauseForGpuLaunch cause_for_gpu_launch) OVERRIDE;

 private:
  struct CreateRequest {
    CreateRequest();
    ~CreateRequest();
    base::WaitableEvent event;
    int32 route_id;
  };

  struct EstablishRequest {
    EstablishRequest();
    ~EstablishRequest();
    base::WaitableEvent event;
    IPC::ChannelHandle channel_handle;
    GPUInfo gpu_info;
  };

  BrowserGpuChannelHostFactory();
  virtual ~BrowserGpuChannelHostFactory();

  void CreateViewCommandBufferOnIO(
      CreateRequest* request,
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params);
  static void CommandBufferCreatedOnIO(CreateRequest* request, int32 route_id);
  void EstablishGpuChannelOnIO(EstablishRequest* request,
                               CauseForGpuLaunch cause_for_gpu_launch);
  static void GpuChannelEstablishedOnIO(
      EstablishRequest* request,
      const IPC::ChannelHandle& channel_handle,
      const GPUInfo& gpu_info);

  int gpu_client_id_;
  scoped_ptr<base::WaitableEvent> shutdown_event_;
  scoped_refptr<GpuChannelHost> gpu_channel_;
  int gpu_host_id_;

  static BrowserGpuChannelHostFactory* instance_;

  DISALLOW_COPY_AND_ASSIGN(BrowserGpuChannelHostFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_BROWSER_GPU_CHANNEL_HOST_FACTORY_H_
