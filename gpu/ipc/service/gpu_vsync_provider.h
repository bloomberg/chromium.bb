// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_VSYNC_PROVIDER_H_
#define GPU_IPC_SERVICE_GPU_VSYNC_PROVIDER_H_

#include <memory>

#include "base/callback.h"
#include "base/time/time.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"

namespace gpu {

class GpuVSyncWorker;

// Implements waiting for VSync signal on background thread.
class GPU_EXPORT GpuVSyncProvider {
 public:
  // Once VSync is enabled, this callback is repeatedly invoked on every VSync.
  // The call is made on background thread to avoid increased latency due to
  // serializing callback invocation with other GPU tasks. The code that
  // implements the callback function is expected to handle that.
  using VSyncCallback = base::Callback<void(base::TimeTicks timestamp)>;

  ~GpuVSyncProvider();

  static std::unique_ptr<GpuVSyncProvider> Create(const VSyncCallback& callback,
                                                  SurfaceHandle surface_handle);

  // Enable or disable VSync production.
  void EnableVSync(bool enabled);

 private:
#if defined(OS_WIN)
  GpuVSyncProvider(const VSyncCallback& callback, SurfaceHandle surface_handle);

  std::unique_ptr<GpuVSyncWorker> vsync_worker_;
#endif  // defined(OS_WIN)

  DISALLOW_COPY_AND_ASSIGN(GpuVSyncProvider);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_VSYNC_PROVIDER_H_
