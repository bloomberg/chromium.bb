// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/gpu_memory_buffer_impl.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl_shared_memory.h"

#if defined(OS_MACOSX)
#include "gpu/ipc/client/gpu_memory_buffer_impl_io_surface.h"
#endif

#if defined(OS_LINUX)
#include "gpu/ipc/client/gpu_memory_buffer_impl_native_pixmap.h"
#endif

#if defined(OS_WIN)
#include "gpu/ipc/client/gpu_memory_buffer_impl_dxgi.h"
#endif

#if defined(OS_ANDROID)
#include "gpu/ipc/client/gpu_memory_buffer_impl_android_hardware_buffer.h"
#endif

namespace gpu {

GpuMemoryBufferImpl::GpuMemoryBufferImpl(gfx::GpuMemoryBufferId id,
                                         const gfx::Size& size,
                                         gfx::BufferFormat format,
                                         const DestructionCallback& callback)
    : id_(id),
      size_(size),
      format_(format),
      callback_(callback),
      mapped_(false) {}

GpuMemoryBufferImpl::~GpuMemoryBufferImpl() {
  DCHECK(!mapped_);
  if (!callback_.is_null())
    callback_.Run(destruction_sync_token_);
}

// static
std::unique_ptr<GpuMemoryBufferImpl> GpuMemoryBufferImpl::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const DestructionCallback& callback) {
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER:
      return GpuMemoryBufferImplSharedMemory::CreateFromHandle(
          handle, size, format, usage, callback);
#if defined(OS_MACOSX)
    case gfx::IO_SURFACE_BUFFER:
      return GpuMemoryBufferImplIOSurface::CreateFromHandle(
          handle, size, format, usage, callback);
#endif
#if defined(OS_LINUX)
    case gfx::NATIVE_PIXMAP:
      return GpuMemoryBufferImplNativePixmap::CreateFromHandle(
          handle, size, format, usage, callback);
#endif
#if defined(OS_WIN)
    case gfx::DXGI_SHARED_HANDLE:
      return GpuMemoryBufferImplDXGI::CreateFromHandle(handle, size, format,
                                                       usage, callback);
#endif
#if defined(OS_ANDROID)
    case gfx::ANDROID_HARDWARE_BUFFER:
      return GpuMemoryBufferImplAndroidHardwareBuffer::CreateFromHandle(
          handle, size, format, usage, callback);
#endif
    default:
      NOTREACHED();
      return nullptr;
  }
}

gfx::Size GpuMemoryBufferImpl::GetSize() const {
  return size_;
}

gfx::BufferFormat GpuMemoryBufferImpl::GetFormat() const {
  return format_;
}

gfx::GpuMemoryBufferId GpuMemoryBufferImpl::GetId() const {
  return id_;
}

ClientBuffer GpuMemoryBufferImpl::AsClientBuffer() {
  return reinterpret_cast<ClientBuffer>(this);
}

}  // namespace gpu
