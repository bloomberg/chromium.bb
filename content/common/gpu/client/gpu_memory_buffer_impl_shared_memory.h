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
  static void Create(const gfx::Size& size,
                     unsigned internalformat,
                     unsigned usage,
                     const CreationCallback& callback);

  static void AllocateForChildProcess(const gfx::Size& size,
                                      unsigned internalformat,
                                      base::ProcessHandle child_process,
                                      const AllocationCallback& callback);

  static scoped_ptr<GpuMemoryBufferImpl> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      unsigned internalformat,
      const DestructionCallback& callback);

  static bool IsLayoutSupported(const gfx::Size& size, unsigned internalformat);
  static bool IsUsageSupported(unsigned usage);
  static bool IsConfigurationSupported(const gfx::Size& size,
                                       unsigned internalformat,
                                       unsigned usage);

  // Overridden from gfx::GpuMemoryBuffer:
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual uint32 GetStride() const OVERRIDE;
  virtual gfx::GpuMemoryBufferHandle GetHandle() const OVERRIDE;

 private:
  GpuMemoryBufferImplSharedMemory(const gfx::Size& size,
                                  unsigned internalformat,
                                  const DestructionCallback& callback,
                                  scoped_ptr<base::SharedMemory> shared_memory);
  virtual ~GpuMemoryBufferImplSharedMemory();

  scoped_ptr<base::SharedMemory> shared_memory_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplSharedMemory);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SHARED_MEMORY_H_
