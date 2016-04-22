// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GPU_GPU_SERVICE_MUS_H_
#define COMPONENTS_MUS_GPU_GPU_SERVICE_MUS_H_

#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"

namespace mus {

// TODO(fsamuel): GpuServiceMus is intended to be the Gpu thread within Mus.
// Similar to GpuChildThread, it is a GpuChannelManagerDelegate and will have a
// GpuChannelManager.
class GpuServiceMus : public gpu::GpuChannelManagerDelegate {
 public:
  GpuServiceMus();
  ~GpuServiceMus() override;

  // GpuChannelManagerDelegate overrides:
  void AddSubscription(int32_t client_id, unsigned int target) override;
  void DidCreateOffscreenContext(const GURL& active_url) override;
  void DidDestroyChannel(int client_id) override;
  void DidDestroyOffscreenContext(const GURL& active_url) override;
  void DidLoseContext(bool offscreen,
                      gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;
  void GpuMemoryUmaStats(const gpu::GPUMemoryUmaStats& params) override;
  void RemoveSubscription(int32_t client_id, unsigned int target) override;
  void StoreShaderToDisk(int32_t client_id,
                         const std::string& key,
                         const std::string& shader) override;
#if defined(OS_MACOSX)
  void SendAcceleratedSurfaceBuffersSwapped(
      int32_t surface_id,
      CAContextID ca_context_id,
      const gfx::ScopedRefCountedIOSurfaceMachPort& io_surface,
      const gfx::Size& size,
      float scale_factor,
      std::vector<ui::LatencyInfo> latency_info) override;
#endif
#if defined(OS_WIN)
  void SendAcceleratedSurfaceCreatedChildWindow(
      gpu::SurfaceHandle parent_window,
      gpu::SurfaceHandle child_window) override;
#endif
  void SetActiveURL(const GURL& url) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuServiceMus);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_GPU_GPU_SERVICE_MUS_H_
