// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_io_surface.h"

#include <vector>

#include "base/logging.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/gl_image_io_surface.h"

namespace gpu {

GpuMemoryBufferFactoryIOSurface::GpuMemoryBufferFactoryIOSurface() {
}

GpuMemoryBufferFactoryIOSurface::~GpuMemoryBufferFactoryIOSurface() {
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryIOSurface::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    SurfaceHandle surface_handle) {
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      gfx::CreateIOSurface(size, format));
  if (!io_surface)
    return gfx::GpuMemoryBufferHandle();

  // A GpuMemoryBuffer with client_id = 0 behaves like anonymous shared memory.
  if (client_id != 0) {
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

ImageFactory* GpuMemoryBufferFactoryIOSurface::AsImageFactory() {
  return this;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryIOSurface::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    unsigned internalformat,
    int client_id,
    SurfaceHandle surface_handle) {
  base::AutoLock lock(io_surfaces_lock_);

  DCHECK_EQ(handle.type, gfx::IO_SURFACE_BUFFER);
  IOSurfaceMapKey key(handle.id, client_id);
  IOSurfaceMap::iterator it = io_surfaces_.find(key);
  if (it == io_surfaces_.end())
    return scoped_refptr<gl::GLImage>();

  scoped_refptr<gl::GLImageIOSurface> image(
      gl::GLImageIOSurface::Create(size, internalformat));
  if (!image->Initialize(it->second.get(), handle.id, format))
    return scoped_refptr<gl::GLImage>();

  return image;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryIOSurface::CreateAnonymousImage(const gfx::Size& size,
                                                      gfx::BufferFormat format,
                                                      gfx::BufferUsage usage,
                                                      unsigned internalformat) {
  // Note that the child id doesn't matter since the texture will never be
  // directly exposed to other processes, only via a mailbox.
  gfx::GpuMemoryBufferHandle handle = CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId(next_service_gmb_id_++), size, format, usage,
      0 /* client_id */, gpu::kNullSurfaceHandle);

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  io_surface.reset(IOSurfaceLookupFromMachPort(handle.mach_port.get()));
  DCHECK_NE(nullptr, io_surface.get());

  scoped_refptr<gl::GLImageIOSurface> image(
      gl::GLImageIOSurface::Create(size, internalformat));
  if (!image->Initialize(io_surface.get(), handle.id, format))
    return scoped_refptr<gl::GLImage>();

  return image;
}

unsigned GpuMemoryBufferFactoryIOSurface::RequiredTextureType() {
  return GL_TEXTURE_RECTANGLE_ARB;
}

bool GpuMemoryBufferFactoryIOSurface::SupportsFormatRGB() {
  return false;
}

}  // namespace gpu
