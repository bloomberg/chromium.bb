// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_CHILD_THREAD_H_
#define CONTENT_GPU_GPU_CHILD_THREAD_H_

#include <stdint.h>

#include <queue>
#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_config.h"
#include "content/common/gpu/x_util.h"
#include "content/common/process_control.mojom.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/weak_binding_set.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {
class SyncPointManager;
}

namespace sandbox {
class TargetServices;
}

namespace content {
class GpuMemoryBufferFactory;
class GpuProcessControlImpl;
class GpuWatchdogThread;

// The main thread of the GPU child process. There will only ever be one of
// these per process. It does process initialization and shutdown. It forwards
// IPC messages to GpuChannelManager, which is responsible for issuing rendering
// commands to the GPU.
class GpuChildThread : public ChildThreadImpl {
 public:
  typedef std::queue<IPC::Message*> DeferredMessages;

  GpuChildThread(GpuWatchdogThread* gpu_watchdog_thread,
                 bool dead_on_arrival,
                 const gpu::GPUInfo& gpu_info,
                 const DeferredMessages& deferred_messages,
                 GpuMemoryBufferFactory* gpu_memory_buffer_factory,
                 gpu::SyncPointManager* sync_point_manager);

  GpuChildThread(const InProcessChildThreadParams& params,
                 GpuMemoryBufferFactory* gpu_memory_buffer_factory,
                 gpu::SyncPointManager* sync_point_manager);

  ~GpuChildThread() override;

  void Shutdown() override;

  void Init(const base::Time& process_start_time);
  void StopWatchdog();

  // ChildThread overrides.
  bool Send(IPC::Message* msg) override;
  bool OnControlMessageReceived(const IPC::Message& msg) override;
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // Message handlers.
  void OnInitialize();
  void OnFinalize();
  void OnCollectGraphicsInfo();
  void OnGetVideoMemoryUsageStats();
  void OnSetVideoMemoryWindowCount(uint32_t window_count);

  void OnClean();
  void OnCrash();
  void OnHang();
  void OnDisableWatchdog();
  void OnGpuSwitched();

  void BindProcessControlRequest(
      mojo::InterfaceRequest<ProcessControl> request);

  // Set this flag to true if a fatal error occurred before we receive the
  // OnInitialize message, in which case we just declare ourselves DOA.
  bool dead_on_arrival_;
  base::Time process_start_time_;
  scoped_refptr<GpuWatchdogThread> watchdog_thread_;

#if defined(OS_WIN)
  // Windows specific client sandbox interface.
  sandbox::TargetServices* target_services_;
#endif

  // Non-owning.
  gpu::SyncPointManager* sync_point_manager_;

  scoped_ptr<GpuChannelManager> gpu_channel_manager_;

  // Information about the GPU, such as device and vendor ID.
  gpu::GPUInfo gpu_info_;

  // Error messages collected in gpu_main() before the thread is created.
  DeferredMessages deferred_messages_;

  // Whether the GPU thread is running in the browser process.
  bool in_browser_process_;

  // The GpuMemoryBufferFactory instance used to allocate GpuMemoryBuffers.
  GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;

  // Process control for Mojo application hosting.
  scoped_ptr<GpuProcessControlImpl> process_control_;

  // Bindings to the ProcessControl impl.
  mojo::WeakBindingSet<ProcessControl> process_control_bindings_;

  DISALLOW_COPY_AND_ASSIGN(GpuChildThread);
};

}  // namespace content

#endif  // CONTENT_GPU_GPU_CHILD_THREAD_H_
