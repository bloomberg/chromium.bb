// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SHARED_MEMORY_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SHARED_MEMORY_H_

#include <stddef.h>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

namespace content {

// Implementation of GPU memory buffer based on shared memory.
class CONTENT_EXPORT GpuMemoryBufferImplSharedMemory
    : public GpuMemoryBufferImpl {
 public:
  ~GpuMemoryBufferImplSharedMemory() override;

  static scoped_ptr<GpuMemoryBufferImplSharedMemory> Create(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      const DestructionCallback& callback);

  static gfx::GpuMemoryBufferHandle AllocateForChildProcess(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      base::ProcessHandle child_process);

  static scoped_ptr<GpuMemoryBufferImplSharedMemory> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      const DestructionCallback& callback);

  static bool IsUsageSupported(gfx::BufferUsage usage);
  static bool IsConfigurationSupported(gfx::BufferFormat format,
                                       gfx::BufferUsage usage);
  static bool IsSizeValidForFormat(const gfx::Size& size,
                                   gfx::BufferFormat format);

  static base::Closure AllocateForTesting(const gfx::Size& size,
                                          gfx::BufferFormat format,
                                          gfx::BufferUsage usage,
                                          gfx::GpuMemoryBufferHandle* handle);

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override;
  void* memory(size_t plane) override;
  void Unmap() override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferHandle GetHandle() const override;

 private:
  GpuMemoryBufferImplSharedMemory(gfx::GpuMemoryBufferId id,
                                  const gfx::Size& size,
                                  gfx::BufferFormat format,
                                  const DestructionCallback& callback,
                                  scoped_ptr<base::SharedMemory> shared_memory,
                                  size_t offset,
                                  int stride);

  scoped_ptr<base::SharedMemory> shared_memory_;
  size_t offset_;
  int stride_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplSharedMemory);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SHARED_MEMORY_H_
