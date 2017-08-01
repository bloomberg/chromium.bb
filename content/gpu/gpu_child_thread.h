// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_CHILD_THREAD_H_
#define CONTENT_GPU_GPU_CHILD_THREAD_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "content/child/child_thread_impl.h"
#include "content/common/associated_interface_registry_impl.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_config.h"
#include "gpu/ipc/service/x_util.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"
#include "services/ui/gpu/interfaces/gpu_main.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {
class GpuWatchdogThread;
}

namespace content {
class GpuServiceFactory;

// The main thread of the GPU child process. There will only ever be one of
// these per process. It does process initialization and shutdown. It forwards
// IPC messages to gpu::GpuChannelManager, which is responsible for issuing
// rendering commands to the GPU.
class GpuChildThread : public ChildThreadImpl, public ui::mojom::GpuMain {
 public:
  struct LogMessage {
    int severity;
    std::string header;
    std::string message;
  };
  typedef std::vector<LogMessage> DeferredMessages;

  GpuChildThread(std::unique_ptr<gpu::GpuWatchdogThread> gpu_watchdog_thread,
                 bool dead_on_arrival,
                 const gpu::GPUInfo& gpu_info,
                 const gpu::GpuFeatureInfo& gpu_feature_info,
                 DeferredMessages deferred_messages);

  GpuChildThread(const InProcessChildThreadParams& params,
                 const gpu::GPUInfo& gpu_info,
                 const gpu::GpuFeatureInfo& gpu_feature_info);

  ~GpuChildThread() override;

  void Init(const base::Time& process_start_time);

 private:
  GpuChildThread(const ChildThreadImpl::Options& options,
                 std::unique_ptr<gpu::GpuWatchdogThread> gpu_watchdog_thread,
                 bool dead_on_arrival,
                 bool in_browser_process,
                 const gpu::GPUInfo& gpu_info,
                 const gpu::GpuFeatureInfo& gpu_feature_info);

  void CreateGpuMainService(ui::mojom::GpuMainAssociatedRequest request);

  // ChildThreadImpl:.
  bool Send(IPC::Message* msg) override;

  // IPC::Listener implementation via ChildThreadImpl:
  void OnAssociatedInterfaceRequest(
      const std::string& name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // ui::mojom::GpuMain:
  void CreateGpuService(viz::mojom::GpuServiceRequest request,
                        ui::mojom::GpuHostPtr gpu_host,
                        const gpu::GpuPreferences& preferences,
                        mojo::ScopedSharedBufferHandle activity_flags) override;
  void CreateFrameSinkManager(
      viz::mojom::FrameSinkManagerRequest request,
      viz::mojom::FrameSinkManagerClientPtr client) override;

  void BindServiceFactoryRequest(
      service_manager::mojom::ServiceFactoryRequest request);

  gpu::GpuChannelManager* gpu_channel_manager() {
    return gpu_service_->gpu_channel_manager();
  }

#if defined(OS_ANDROID)
  static std::unique_ptr<media::AndroidOverlay> CreateAndroidOverlay(
      const base::UnguessableToken& routing_token,
      media::AndroidOverlayConfig);
#endif

  // Set this flag to true if a fatal error occurred before we receive the
  // OnInitialize message, in which case we just declare ourselves DOA.
  const bool dead_on_arrival_;

  // Error messages collected in gpu_main() before the thread is created.
  DeferredMessages deferred_messages_;

  // Whether the GPU thread is running in the browser process.
  const bool in_browser_process_;

  // ServiceFactory for service_manager::Service hosting.
  std::unique_ptr<GpuServiceFactory> service_factory_;

  // Bindings to the service_manager::mojom::ServiceFactory impl.
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;

  AssociatedInterfaceRegistryImpl associated_interfaces_;
  std::unique_ptr<viz::GpuServiceImpl> gpu_service_;
  mojo::AssociatedBinding<ui::mojom::GpuMain> gpu_main_binding_;

  // Holds a closure that releases pending interface requests on the IO thread.
  base::Closure release_pending_requests_closure_;

  base::WeakPtrFactory<GpuChildThread> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuChildThread);
};

}  // namespace content

#endif  // CONTENT_GPU_GPU_CHILD_THREAD_H_
