// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GPU_GPU_MEMORY_BUFFER_MANAGER_MUS_LOCAL_H_
#define COMPONENTS_MUS_GPU_GPU_MEMORY_BUFFER_MANAGER_MUS_LOCAL_H_

#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"

namespace mus {

// A GpuMemoryBufferManager used locally.
class GpuMemoryBufferManagerMusLocal : public gpu::GpuMemoryBufferManager {
 public:
  GpuMemoryBufferManagerMusLocal(int gpu_client_id,
                                 uint64_t gpu_client_tracing_id);
  ~GpuMemoryBufferManagerMusLocal() override;

  // Overridden from gpu::GpuMemoryBufferManager:
  std::unique_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle) override;
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBufferFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format) override;
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBufferFromClientId(
      int client_id,
      const gfx::GpuMemoryBufferId& gpu_memory_buffer_id) override;
  gfx::GpuMemoryBuffer* GpuMemoryBufferFromClientBuffer(
      ClientBuffer buffer) override;
  void SetDestructionSyncToken(gfx::GpuMemoryBuffer* buffer,
                               const gpu::SyncToken& sync_token) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferManagerMusLocal);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_GPU_GPU_MEMORY_BUFFER_MANAGER_MUS_LOCAL_H_
