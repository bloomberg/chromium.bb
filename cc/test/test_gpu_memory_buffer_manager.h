// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_GPU_MEMORY_BUFFER_MANAGER_H_
#define CC_TEST_TEST_GPU_MEMORY_BUFFER_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"

namespace cc {

class TestGpuMemoryBufferManager : public gpu::GpuMemoryBufferManager {
 public:
  TestGpuMemoryBufferManager();
  ~TestGpuMemoryBufferManager() override;

  void SetGpuMemoryBufferIsInUseByMacOSWindowServer(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      bool in_use);

  // Overridden from gpu::GpuMemoryBufferManager:
  std::unique_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int32_t surface_id) override;
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBufferFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format) override;
  gfx::GpuMemoryBuffer* GpuMemoryBufferFromClientBuffer(
      ClientBuffer buffer) override;
  void SetDestructionSyncToken(gfx::GpuMemoryBuffer* buffer,
                               const gpu::SyncToken& sync_token) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestGpuMemoryBufferManager);
};

}  // namespace cc

#endif  // CC_TEST_TEST_GPU_MEMORY_BUFFER_MANAGER_H_
