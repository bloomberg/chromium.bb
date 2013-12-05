// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_io_surface.h"

#include "base/logging.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/io_surface_support_mac.h"

namespace content {

GpuMemoryBufferImplIOSurface::GpuMemoryBufferImplIOSurface(
    gfx::Size size, unsigned internalformat)
    : GpuMemoryBufferImpl(size, internalformat),
      io_surface_support_(IOSurfaceSupport::Initialize()) {
  CHECK(io_surface_support_);
}

GpuMemoryBufferImplIOSurface::~GpuMemoryBufferImplIOSurface() {
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
uint32 GpuMemoryBufferImplIOSurface::PixelFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_BGRA8_EXT:
      return 'BGRA';
    default:
      NOTREACHED();
      return 0;
  }
}

bool GpuMemoryBufferImplIOSurface::Initialize(
    gfx::GpuMemoryBufferHandle handle) {
  io_surface_.reset(io_surface_support_->IOSurfaceLookup(handle.io_surface_id));
  if (!io_surface_) {
    VLOG(1) << "IOSurface lookup failed";
    return false;
  }

  return true;
}

void GpuMemoryBufferImplIOSurface::Map(AccessMode mode, void** vaddr) {
  DCHECK(!mapped_);
  io_surface_support_->IOSurfaceLock(io_surface_, 0, NULL);
  *vaddr = io_surface_support_->IOSurfaceGetBaseAddress(io_surface_);
  mapped_ = true;
}

void GpuMemoryBufferImplIOSurface::Unmap() {
  DCHECK(mapped_);
  io_surface_support_->IOSurfaceUnlock(io_surface_, 0, NULL);
  mapped_ = false;
}

uint32 GpuMemoryBufferImplIOSurface::GetStride() const {
  return io_surface_support_->IOSurfaceGetBytesPerRow(io_surface_);
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplIOSurface::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.io_surface_id = io_surface_support_->IOSurfaceGetID(io_surface_);
  return handle;
}

}  // namespace content
