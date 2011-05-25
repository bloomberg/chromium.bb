// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_CHILD_THREAD_H_
#define CONTENT_GPU_GPU_CHILD_THREAD_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "build/build_config.h"
#include "content/common/child_thread.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_config.h"
#include "content/common/gpu/gpu_info.h"
#include "content/common/gpu/x_util.h"
#include "ui/gfx/native_widget_types.h"

namespace IPC {
struct ChannelHandle;
}

namespace sandbox {
class TargetServices;
}

class GpuWatchdogThread;

// The main thread of the GPU child process. There will only ever be one of
// these per process. It does process initialization and shutdown. It forwards
// IPC messages to GpuChannelManager, which is responsible for issuing rendering
// commands to the GPU.
class GpuChildThread : public ChildThread {
 public:
#if defined(OS_WIN)
  explicit GpuChildThread(sandbox::TargetServices* target_services);
#else
  GpuChildThread();
#endif

  // For single-process mode.
  explicit GpuChildThread(const std::string& channel_id);

  virtual ~GpuChildThread();

  void Init(const base::Time& process_start_time);
  void StopWatchdog();

  // ChildThread overrides.
  virtual bool Send(IPC::Message* msg);
  virtual bool OnControlMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnInitialize();
  void OnCollectGraphicsInfo();
  void OnClean();
  void OnCrash();
  void OnHang();

#if defined(OS_WIN)
  static void CollectDxDiagnostics(GpuChildThread* thread);
  static void SetDxDiagnostics(GpuChildThread* thread, const DxDiagNode& node);
#endif

  base::Time process_start_time_;
  scoped_refptr<GpuWatchdogThread> watchdog_thread_;

#if defined(OS_WIN)
  // Windows specific client sandbox interface.
  sandbox::TargetServices* target_services_;

  // Indicates whether DirectX Diagnostics collection is ongoing.
  bool collecting_dx_diagnostics_;
#endif

  scoped_ptr<GpuChannelManager> gpu_channel_manager_;

  // Information about the GPU, such as device and vendor ID.
  GPUInfo gpu_info_;

  DISALLOW_COPY_AND_ASSIGN(GpuChildThread);
};

#endif  // CONTENT_GPU_GPU_CHILD_THREAD_H_
