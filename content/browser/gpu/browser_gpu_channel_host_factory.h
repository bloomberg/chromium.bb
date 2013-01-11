// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_BROWSER_GPU_CHANNEL_HOST_FACTORY_H_
#define CONTENT_BROWSER_GPU_BROWSER_GPU_CHANNEL_HOST_FACTORY_H_

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
      size_t size) OVERRIDE;
  virtual int32 CreateViewCommandBuffer(
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params) OVERRIDE;
  virtual void CreateImage(
      gfx::PluginWindowHandle window,
      int32 image_id,
      const CreateImageCallback& callback) OVERRIDE;
  virtual void DeleteImage(int32 image_idu, int32 sync_point) OVERRIDE;
  virtual GpuChannelHost* EstablishGpuChannelSync(
      CauseForGpuLaunch cause_for_gpu_launch) OVERRIDE;

  // Specify a task runner and callback to be used for a set of messages.
  virtual void SetHandlerForControlMessages(
      const uint32* message_ids,
      size_t num_messages,
      const base::Callback<void(const IPC::Message&)>& handler,
      base::TaskRunner* target_task_runner);

 private:
  struct CreateRequest {
    CreateRequest();
    ~CreateRequest();
    base::WaitableEvent event;
    int gpu_host_id;
    int32 route_id;
  };

  struct EstablishRequest {
    explicit EstablishRequest(CauseForGpuLaunch);
    ~EstablishRequest();
    base::WaitableEvent event;
    CauseForGpuLaunch cause_for_gpu_launch;
    int gpu_host_id;
    bool reused_gpu_process;
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
  void CreateImageOnIO(
      gfx::PluginWindowHandle window,
      int32 image_id,
      const CreateImageCallback& callback);
  static void ImageCreatedOnIO(
      const CreateImageCallback& callback, const gfx::Size size);
  static void OnImageCreated(
      const CreateImageCallback& callback, const gfx::Size size);
  void DeleteImageOnIO(int32 image_id, int32 sync_point);
  void EstablishGpuChannelOnIO(EstablishRequest* request);
  void GpuChannelEstablishedOnIO(
      EstablishRequest* request,
      const IPC::ChannelHandle& channel_handle,
      const GPUInfo& gpu_info);
  static void AddFilterOnIO(
      int gpu_host_id,
      scoped_refptr<IPC::ChannelProxy::MessageFilter> filter);

  int gpu_client_id_;
  scoped_ptr<base::WaitableEvent> shutdown_event_;
  scoped_refptr<GpuChannelHost> gpu_channel_;
  int gpu_host_id_;

  static BrowserGpuChannelHostFactory* instance_;

  DISALLOW_COPY_AND_ASSIGN(BrowserGpuChannelHostFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_BROWSER_GPU_CHANNEL_HOST_FACTORY_H_
