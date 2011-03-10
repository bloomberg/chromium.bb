// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_THREAD_H_
#define CHROME_GPU_GPU_THREAD_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "build/build_config.h"
#include "chrome/gpu/gpu_channel.h"
#include "chrome/gpu/gpu_config.h"
#include "chrome/gpu/x_util.h"
#include "content/common/child_thread.h"
#include "content/common/gpu_info.h"
#include "ui/gfx/native_widget_types.h"

namespace IPC {
struct ChannelHandle;
}

namespace sandbox {
class TargetServices;
}

class GpuWatchdogThread;

class GpuThread : public ChildThread {
 public:
#if defined(OS_WIN)
  explicit GpuThread(sandbox::TargetServices* target_services);
#else
  GpuThread();
#endif

  // For single-process mode.
  explicit GpuThread(const std::string& channel_id);

  ~GpuThread();

  void Init(const base::Time& process_start_time);
  void StopWatchdog();

  // Remove the channel for a particular renderer.
  void RemoveChannel(int renderer_id);

 private:
  // ChildThread overrides.
  virtual bool OnControlMessageReceived(const IPC::Message& msg);

  // Message handlers.
  void OnInitialize();
  void OnEstablishChannel(int renderer_id);
  void OnCloseChannel(const IPC::ChannelHandle& channel_handle);
  void OnSynchronize();
  void OnCollectGraphicsInfo(GPUInfo::Level level);
  void OnCreateViewCommandBuffer(
      gfx::PluginWindowHandle window,
      int32 render_view_id,
      int32 renderer_id,
      const GPUCreateCommandBufferConfig& init_params);
#if defined(OS_MACOSX)
  void OnAcceleratedSurfaceBuffersSwappedACK(
      int renderer_id, int32 route_id, uint64 swap_buffers_count);
  void OnDidDestroyAcceleratedSurface(int renderer_id, int32 renderer_route_id);
#endif
  void OnCrash();
  void OnHang();

#if defined(OS_WIN)
  static void CollectDxDiagnostics(GpuThread* thread);
  static void SetDxDiagnostics(GpuThread* thread, const DxDiagNode& node);
#endif

  base::Time process_start_time_;
  scoped_refptr<GpuWatchdogThread> watchdog_thread_;

  typedef base::hash_map<int, scoped_refptr<GpuChannel> > GpuChannelMap;
  GpuChannelMap gpu_channels_;

  // Information about the GPU, such as device and vendor ID.
  GPUInfo gpu_info_;

#if defined(OS_WIN)
  // Windows specific client sandbox interface.
  sandbox::TargetServices* target_services_;
#endif

  DISALLOW_COPY_AND_ASSIGN(GpuThread);
};

#endif  // CHROME_GPU_GPU_THREAD_H_
