// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory_io_surface.h"

#include <vector>

#include "base/logging.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/gl_image_io_surface.h"

namespace content {

GpuMemoryBufferFactoryIOSurface::GpuMemoryBufferFactoryIOSurface() {
}

GpuMemoryBufferFactoryIOSurface::~GpuMemoryBufferFactoryIOSurface() {
}

// static
bool GpuMemoryBufferFactoryIOSurface::IsGpuMemoryBufferConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
      return format == gfx::BufferFormat::BGRA_8888 ||
             format == gfx::BufferFormat::RGBA_8888;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
      return format == gfx::BufferFormat::R_8 ||
             format == gfx::BufferFormat::BGRA_8888 ||
             format == gfx::BufferFormat::UYVY_422 ||
             format == gfx::BufferFormat::YUV_420_BIPLANAR;
  }
  NOTREACHED();
  return false;
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryIOSurface::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    gfx::PluginWindowHandle surface_handle) {
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      gfx::CreateIOSurface(size, format));
  if (!io_surface)
    return gfx::GpuMemoryBufferHandle();

  {
    base::AutoLock lock(io_surfaces_lock_);

    IOSurfaceMapKey key(id, client_id);
    DCHECK(io_surfaces_.find(key) == io_surfaces_.end());
    io_surfaces_[key] = io_surface;
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.id = id;
  handle.mach_port.reset(IOSurfaceCreateMachPort(io_surface));
  return handle;
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryIOSurface::CreateGpuMemoryBufferFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    int client_id) {
  NOTIMPLEMENTED();
  return gfx::GpuMemoryBufferHandle();
}

void GpuMemoryBufferFactoryIOSurface::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  {
    base::AutoLock lock(io_surfaces_lock_);

    IOSurfaceMapKey key(id, client_id);
    DCHECK(io_surfaces_.find(key) != io_surfaces_.end());
    io_surfaces_.erase(key);
  }
}

gpu::ImageFactory* GpuMemoryBufferFactoryIOSurface::AsImageFactory() {
  return this;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryIOSurface::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    unsigned internalformat,
    int client_id) {
  base::AutoLock lock(io_surfaces_lock_);

  DCHECK_EQ(handle.type, gfx::IO_SURFACE_BUFFER);
  IOSurfaceMapKey key(handle.id, client_id);
  IOSurfaceMap::iterator it = io_surfaces_.find(key);
  if (it == io_surfaces_.end())
    return scoped_refptr<gl::GLImage>();

  scoped_refptr<gl::GLImageIOSurface> image(
      new gl::GLImageIOSurface(size, internalformat));
  if (!image->Initialize(it->second.get(), handle.id, format))
    return scoped_refptr<gl::GLImage>();

  return image;
}

}  // namespace content
