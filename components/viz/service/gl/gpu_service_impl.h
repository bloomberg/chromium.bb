// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_GL_GPU_SERVICE_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_GL_GPU_SERVICE_IMPL_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_config.h"
#include "gpu/ipc/service/x_util.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/viz/privileged/interfaces/gl/gpu_host.mojom.h"
#include "services/viz/privileged/interfaces/gl/gpu_service.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {
class GpuMemoryBufferFactory;
class GpuWatchdogThread;
class Scheduler;
class SyncPointManager;
}  // namespace gpu

namespace media {
class MediaGpuChannelManager;
}

namespace viz {

// This runs in the GPU process, and communicates with the gpu host (which is
// the window server) over the mojom APIs. This is responsible for setting up
// the connection to clients, allocating/free'ing gpu memory etc.
class VIZ_SERVICE_EXPORT GpuServiceImpl : public gpu::GpuChannelManagerDelegate,
                                          public mojom::GpuService {
 public:
  GpuServiceImpl(const gpu::GPUInfo& gpu_info,
                 std::unique_ptr<gpu::GpuWatchdogThread> watchdog,
                 scoped_refptr<base::SingleThreadTaskRunner> io_runner,
                 const gpu::GpuFeatureInfo& gpu_feature_info);

  ~GpuServiceImpl() override;

  void UpdateGPUInfoFromPreferences(const gpu::GpuPreferences& preferences);

  void InitializeWithHost(mojom::GpuHostPtr gpu_host,
                          gpu::GpuProcessActivityFlags activity_flags,
                          gpu::SyncPointManager* sync_point_manager = nullptr,
                          base::WaitableEvent* shutdown_event = nullptr);
  void Bind(mojom::GpuServiceRequest request);

  bool is_initialized() const { return !!gpu_host_; }

  media::MediaGpuChannelManager* media_gpu_channel_manager() {
    return media_gpu_channel_manager_.get();
  }

  gpu::GpuChannelManager* gpu_channel_manager() {
    return gpu_channel_manager_.get();
  }

  gpu::ImageFactory* gpu_image_factory();
  gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory() {
    return gpu_memory_buffer_factory_.get();
  }

  gpu::gles2::MailboxManager* mailbox_manager() {
    return gpu_channel_manager_->mailbox_manager();
  }

  gl::GLShareGroup* share_group() {
    return gpu_channel_manager_->share_group();
  }

  gpu::SyncPointManager* sync_point_manager() { return sync_point_manager_; }

  gpu::GpuWatchdogThread* watchdog_thread() { return watchdog_thread_.get(); }

  const gpu::GpuFeatureInfo& gpu_feature_info() const {
    return gpu_feature_info_;
  }

  bool in_host_process() const { return gpu_info_.in_process_gpu; }

  void set_start_time(base::Time start_time) { start_time_ = start_time; }

  const gpu::GPUInfo& gpu_info() const { return gpu_info_; }
  const gpu::GpuPreferences& gpu_preferences() const {
    return gpu_preferences_;
  }

 private:
  void RecordLogMessage(int severity,
                        size_t message_start,
                        const std::string& message);

  void UpdateGpuInfoPlatform(base::OnceClosure on_gpu_info_updated);

  // gpu::GpuChannelManagerDelegate:
  void DidCreateOffscreenContext(const GURL& active_url) override;
  void DidDestroyChannel(int client_id) override;
  void DidDestroyOffscreenContext(const GURL& active_url) override;
  void DidLoseContext(bool offscreen,
                      gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;
  void StoreShaderToDisk(int client_id,
                         const std::string& key,
                         const std::string& shader) override;
#if defined(OS_WIN)
  void SendAcceleratedSurfaceCreatedChildWindow(
      gpu::SurfaceHandle parent_window,
      gpu::SurfaceHandle child_window) override;
#endif
  void SetActiveURL(const GURL& url) override;

  // mojom::GpuService:
  void EstablishGpuChannel(int32_t client_id,
                           uint64_t client_tracing_id,
                           bool is_gpu_host,
                           EstablishGpuChannelCallback callback) override;
  void CloseChannel(int32_t client_id) override;
  void CreateJpegDecodeAccelerator(
      media::mojom::GpuJpegDecodeAcceleratorRequest jda_request) override;
  void CreateVideoEncodeAcceleratorProvider(
      media::mojom::VideoEncodeAcceleratorProviderRequest vea_provider_request)
      override;
  void CreateGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                             const gfx::Size& size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage,
                             int client_id,
                             gpu::SurfaceHandle surface_handle,
                             CreateGpuMemoryBufferCallback callback) override;
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id,
                              const gpu::SyncToken& sync_token) override;
  void GetVideoMemoryUsageStats(
      GetVideoMemoryUsageStatsCallback callback) override;
  void RequestCompleteGpuInfo(RequestCompleteGpuInfoCallback callback) override;
  void LoadedShader(const std::string& key, const std::string& data) override;
  void DestroyingVideoSurface(int32_t surface_id,
                              DestroyingVideoSurfaceCallback callback) override;
  void WakeUpGpu() override;
  void GpuSwitched() override;
  void DestroyAllChannels() override;
  void Crash() override;
  void Hang() override;
  void ThrowJavaException() override;
  void Stop(StopCallback callback) override;

  scoped_refptr<base::SingleThreadTaskRunner> main_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

  std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread_;

  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;

  gpu::GpuPreferences gpu_preferences_;

  // Information about the GPU, such as device and vendor ID.
  gpu::GPUInfo gpu_info_;

  // Information about general chrome feature support for the GPU.
  gpu::GpuFeatureInfo gpu_feature_info_;

  scoped_refptr<mojom::ThreadSafeGpuHostPtr> gpu_host_;
  std::unique_ptr<gpu::GpuChannelManager> gpu_channel_manager_;
  std::unique_ptr<media::MediaGpuChannelManager> media_gpu_channel_manager_;

  // On some platforms (e.g. android webview), the SyncPointManager comes from
  // external sources.
  std::unique_ptr<gpu::SyncPointManager> owned_sync_point_manager_;
  gpu::SyncPointManager* sync_point_manager_ = nullptr;

  std::unique_ptr<gpu::Scheduler> scheduler_;

  // An event that will be signalled when we shutdown. On some platforms it
  // comes from external sources.
  std::unique_ptr<base::WaitableEvent> owned_shutdown_event_;
  base::WaitableEvent* shutdown_event_ = nullptr;

  base::Time start_time_;

  // Used to track the task to bind a GpuServiceRequest on the io thread.
  base::CancelableTaskTracker bind_task_tracker_;
  std::unique_ptr<mojo::BindingSet<mojom::GpuService>> bindings_;

  base::WeakPtr<GpuServiceImpl> weak_ptr_;
  base::WeakPtrFactory<GpuServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuServiceImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_GL_GPU_SERVICE_IMPL_H_
