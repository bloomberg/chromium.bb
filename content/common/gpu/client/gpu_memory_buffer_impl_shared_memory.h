// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SHARED_MEMORY_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SHARED_MEMORY_H_

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

namespace content {

// Implementation of GPU memory buffer based on shared memory.
class GpuMemoryBufferImplSharedMemory : public GpuMemoryBufferImpl {
 public:
  ~GpuMemoryBufferImplSharedMemory() override;

  static scoped_ptr<GpuMemoryBufferImpl> Create(gfx::GpuMemoryBufferId id,
                                                const gfx::Size& size,
                                                Format format);

  static gfx::GpuMemoryBufferHandle AllocateForChildProcess(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      Format format,
      base::ProcessHandle child_process);

  static scoped_ptr<GpuMemoryBufferImpl> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      Format format,
      const DestructionCallback& callback);

  static bool IsFormatSupported(Format format);
  static bool IsSizeValidForFormat(const gfx::Size& size, Format format);

  // Overridden from gfx::GpuMemoryBuffer:
  void* Map() override;
  void Unmap() override;
  uint32 GetStride() const override;
  gfx::GpuMemoryBufferHandle GetHandle() const override;

 private:
  GpuMemoryBufferImplSharedMemory(gfx::GpuMemoryBufferId id,
                                  const gfx::Size& size,
                                  Format format,
                                  const DestructionCallback& callback,
                                  scoped_ptr<base::SharedMemory> shared_memory);

  scoped_ptr<base::SharedMemory> shared_memory_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplSharedMemory);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SHARED_MEMORY_H_
