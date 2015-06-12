// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_io_surface.h"

#include "base/logging.h"
#include "content/common/mac/io_surface_manager.h"

namespace content {
namespace {

uint32_t LockFlags(gfx::GpuMemoryBuffer::Usage usage) {
  switch (usage) {
    case gfx::GpuMemoryBuffer::MAP:
      return kIOSurfaceLockAvoidSync;
    case gfx::GpuMemoryBuffer::PERSISTENT_MAP:
      return 0;
    case gfx::GpuMemoryBuffer::SCANOUT:
      NOTREACHED();
      return 0;
  }
  NOTREACHED();
  return 0;
}

}  // namespace

GpuMemoryBufferImplIOSurface::GpuMemoryBufferImplIOSurface(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    Format format,
    const DestructionCallback& callback,
    IOSurfaceRef io_surface,
    uint32_t lock_flags)
    : GpuMemoryBufferImpl(id, size, format, callback),
      io_surface_(io_surface),
      lock_flags_(lock_flags) {
}

GpuMemoryBufferImplIOSurface::~GpuMemoryBufferImplIOSurface() {
}

// static
scoped_ptr<GpuMemoryBufferImpl> GpuMemoryBufferImplIOSurface::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    Format format,
    Usage usage,
    const DestructionCallback& callback) {
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      IOSurfaceManager::GetInstance()->AcquireIOSurface(handle.id));
  if (!io_surface)
    return nullptr;

  return make_scoped_ptr<GpuMemoryBufferImpl>(
      new GpuMemoryBufferImplIOSurface(handle.id, size, format, callback,
                                       io_surface.release(), LockFlags(usage)));
}

bool GpuMemoryBufferImplIOSurface::Map(void** data) {
  DCHECK(!mapped_);
  IOReturn status = IOSurfaceLock(io_surface_, lock_flags_, NULL);
  DCHECK_NE(status, kIOReturnCannotLock);
  mapped_ = true;
  *data = IOSurfaceGetBaseAddress(io_surface_);
  return true;
}

void GpuMemoryBufferImplIOSurface::Unmap() {
  DCHECK(mapped_);
  IOSurfaceUnlock(io_surface_, lock_flags_, NULL);
  mapped_ = false;
}

void GpuMemoryBufferImplIOSurface::GetStride(int* stride) const {
  *stride = IOSurfaceGetBytesPerRow(io_surface_);
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplIOSurface::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.id = id_;
  return handle;
}

}  // namespace content
