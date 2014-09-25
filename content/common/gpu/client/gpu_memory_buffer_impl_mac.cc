// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

#include "content/common/gpu/client/gpu_memory_buffer_impl_io_surface.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl_shared_memory.h"

namespace content {

// static
void GpuMemoryBufferImpl::Create(const gfx::Size& size,
                                 unsigned internalformat,
                                 unsigned usage,
                                 int client_id,
                                 const CreationCallback& callback) {
  if (GpuMemoryBufferImplIOSurface::IsConfigurationSupported(internalformat,
                                                             usage)) {
    GpuMemoryBufferImplIOSurface::Create(
        size, internalformat, usage, client_id, callback);
    return;
  }

  if (GpuMemoryBufferImplSharedMemory::IsConfigurationSupported(
          size, internalformat, usage)) {
    GpuMemoryBufferImplSharedMemory::Create(
        size, internalformat, usage, callback);
    return;
  }

  callback.Run(scoped_ptr<GpuMemoryBufferImpl>());
}

// static
void GpuMemoryBufferImpl::AllocateForChildProcess(
    const gfx::Size& size,
    unsigned internalformat,
    unsigned usage,
    base::ProcessHandle child_process,
    int child_client_id,
    const AllocationCallback& callback) {
  if (GpuMemoryBufferImplIOSurface::IsConfigurationSupported(internalformat,
                                                             usage)) {
    GpuMemoryBufferImplIOSurface::AllocateForChildProcess(
        size, internalformat, usage, child_client_id, callback);
    return;
  }

  if (GpuMemoryBufferImplSharedMemory::IsConfigurationSupported(
          size, internalformat, usage)) {
    GpuMemoryBufferImplSharedMemory::AllocateForChildProcess(
        size, internalformat, child_process, callback);
    return;
  }

  callback.Run(gfx::GpuMemoryBufferHandle());
}

// static
void GpuMemoryBufferImpl::DeletedByChildProcess(
    gfx::GpuMemoryBufferType type,
    const gfx::GpuMemoryBufferId& id,
    base::ProcessHandle child_process) {
}

// static
scoped_ptr<GpuMemoryBufferImpl> GpuMemoryBufferImpl::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    unsigned internalformat,
    const DestructionCallback& callback) {
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER:
      return GpuMemoryBufferImplSharedMemory::CreateFromHandle(
          handle, size, internalformat, callback);
    case gfx::IO_SURFACE_BUFFER:
      return GpuMemoryBufferImplIOSurface::CreateFromHandle(
          handle, size, internalformat, callback);
    default:
      return scoped_ptr<GpuMemoryBufferImpl>();
  }
}

}  // namespace content
