// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_GPU_MEMORY_BUFFER_MANAGER_H_
#define BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_GPU_MEMORY_BUFFER_MANAGER_H_

#include "base/macros.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"

namespace blimp {
namespace client {

class BlimpGpuMemoryBufferManager : public gpu::GpuMemoryBufferManager {
 public:
  BlimpGpuMemoryBufferManager();
  ~BlimpGpuMemoryBufferManager() override;

  // Overridden from gpu::GpuMemoryBufferManager:
  scoped_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;
  scoped_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBufferFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format) override;
  gfx::GpuMemoryBuffer* GpuMemoryBufferFromClientBuffer(
      ClientBuffer buffer) override;
  void SetDestructionSyncToken(gfx::GpuMemoryBuffer* buffer,
                               const gpu::SyncToken& sync_token) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpGpuMemoryBufferManager);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_GPU_MEMORY_BUFFER_MANAGER_H_
