// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_io_surface.h"

#include "ui/gl/gl_bindings.h"

namespace content {

GpuMemoryBufferImplIOSurface::GpuMemoryBufferImplIOSurface(
    const gfx::Size& size,
    unsigned internalformat,
    const DestructionCallback& callback,
    IOSurfaceRef io_surface)
    : GpuMemoryBufferImpl(size, internalformat, callback),
      io_surface_(io_surface) {
}

GpuMemoryBufferImplIOSurface::~GpuMemoryBufferImplIOSurface() {
}

// static
scoped_ptr<GpuMemoryBufferImpl> GpuMemoryBufferImplIOSurface::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    unsigned internalformat,
    const DestructionCallback& callback) {
  DCHECK(IsFormatSupported(internalformat));

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      IOSurfaceLookup(handle.io_surface_id));
  if (!io_surface)
    return scoped_ptr<GpuMemoryBufferImpl>();

  return make_scoped_ptr<GpuMemoryBufferImpl>(new GpuMemoryBufferImplIOSurface(
      size, internalformat, callback, io_surface.get()));
}

// static
bool GpuMemoryBufferImplIOSurface::IsFormatSupported(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
      return true;
    default:
      return false;
  }
}

// static
bool GpuMemoryBufferImplIOSurface::IsUsageSupported(unsigned usage) {
  switch (usage) {
    case GL_IMAGE_MAP_CHROMIUM:
      return true;
    default:
      return false;
  }
}

// static
bool GpuMemoryBufferImplIOSurface::IsConfigurationSupported(
    unsigned internalformat,
    unsigned usage) {
  return IsFormatSupported(internalformat) && IsUsageSupported(usage);
}

// static
uint32 GpuMemoryBufferImplIOSurface::PixelFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
      return 'BGRA';
    default:
      NOTREACHED();
      return 0;
  }
}

void* GpuMemoryBufferImplIOSurface::Map() {
  DCHECK(!mapped_);
  IOSurfaceLock(io_surface_, 0, NULL);
  mapped_ = true;
  return IOSurfaceGetBaseAddress(io_surface_);
}

void GpuMemoryBufferImplIOSurface::Unmap() {
  DCHECK(mapped_);
  IOSurfaceUnlock(io_surface_, 0, NULL);
  mapped_ = false;
}

uint32 GpuMemoryBufferImplIOSurface::GetStride() const {
  return IOSurfaceGetBytesPerRow(io_surface_);
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplIOSurface::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.io_surface_id = IOSurfaceGetID(io_surface_);
  return handle;
}

}  // namespace content
