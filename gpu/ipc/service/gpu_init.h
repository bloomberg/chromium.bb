// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_INIT_H_
#define GPU_IPC_SERVICE_GPU_INIT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"

namespace base {
class CommandLine;
}

namespace gpu {

class GPU_EXPORT GpuSandboxHelper {
 public:
  virtual ~GpuSandboxHelper() {}

  virtual void PreSandboxStartup() = 0;

  virtual bool EnsureSandboxInitialized(GpuWatchdogThread* watchdog_thread,
                                        const GPUInfo* gpu_info) = 0;
};

class GPU_EXPORT GpuInit {
 public:
  GpuInit();
  ~GpuInit();

  void set_sandbox_helper(GpuSandboxHelper* helper) {
    sandbox_helper_ = helper;
  }

  // TODO(zmo): Get rid of |command_line| in the following two functions.
  // Pass all bits through GpuPreferences.
  bool InitializeAndStartSandbox(base::CommandLine* command_line,
                                 const GpuPreferences& gpu_preferences);
  void InitializeInProcess(base::CommandLine* command_line,
                           const GpuPreferences& gpu_preferences,
                           const GPUInfo* gpu_info = nullptr,
                           const GpuFeatureInfo* gpu_feature_info = nullptr);

  const GPUInfo& gpu_info() const { return gpu_info_; }
  const GpuFeatureInfo& gpu_feature_info() const { return gpu_feature_info_; }
  const GpuPreferences& gpu_preferences() const { return gpu_preferences_; }
  std::unique_ptr<GpuWatchdogThread> TakeWatchdogThread() {
    return std::move(watchdog_thread_);
  }
  bool init_successful() const { return init_successful_; }

 private:
  GpuSandboxHelper* sandbox_helper_ = nullptr;
  std::unique_ptr<GpuWatchdogThread> watchdog_thread_;
  GPUInfo gpu_info_;
  GpuFeatureInfo gpu_feature_info_;
  GpuPreferences gpu_preferences_;
  bool init_successful_ = false;

  DISALLOW_COPY_AND_ASSIGN(GpuInit);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_INIT_H_
