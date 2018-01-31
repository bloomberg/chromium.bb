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

namespace {
// A GpuMemoryBuffer with client_id = 0 behaves like anonymous shared memory.
const int kAnonymousClientId = 0;
}  // namespace

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
  // Don't clear anonymous io surfaces.
  bool should_clear = (client_id != kAnonymousClientId);
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      gfx::CreateIOSurface(size, format, should_clear));
  if (!io_surface) {
    DLOG(ERROR) << "Failed to allocate IOSurface.";
    return gfx::GpuMemoryBufferHandle();
  }

  if (client_id != kAnonymousClientId) {
    base::AutoLock lock(io_surfaces_lock_);

    IOSurfaceMapKey key(id, client_id);
    DCHECK(io_surfaces_.find(key) == io_surfaces_.end());
    io_surfaces_[key] = io_surface;
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.id = id;
  handle.mach_port.reset(IOSurfaceCreateMachPort(io_surface));

  // TODO(ccameron): This should never happen, but a similar call to
  // IOSurfaceLookupFromMachPort is failing below. This should determine if
  // the lifetime of the underlying IOSurface determines the failure.
  // https://crbug.com/795649
  CHECK(handle.mach_port);
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_recreated(
      IOSurfaceLookupFromMachPort(handle.mach_port.get()));
  CHECK_NE(nullptr, io_surface_recreated.get())
      << "Failed to reconstitute still-existing IOSurface from mach port.";

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
  if (it == io_surfaces_.end()) {
    DLOG(ERROR) << "Failed to find IOSurface based on key.";
    return scoped_refptr<gl::GLImage>();
  }

  scoped_refptr<gl::GLImageIOSurface> image(
      gl::GLImageIOSurface::Create(size, internalformat));
  if (!image->Initialize(it->second.get(), handle.id, format)) {
    DLOG(ERROR) << "Failed to initialize GLImage for IOSurface.";
    return scoped_refptr<gl::GLImage>();
  }

  return image;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryIOSurface::CreateAnonymousImage(const gfx::Size& size,
                                                      gfx::BufferFormat format,
                                                      gfx::BufferUsage usage,
                                                      unsigned internalformat,
                                                      bool* is_cleared) {
  // Note that the child id doesn't matter since the texture will never be
  // directly exposed to other processes, only via a mailbox.
  gfx::GpuMemoryBufferHandle handle = CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId(next_anonymous_image_id_++), size, format, usage,
      kAnonymousClientId, gpu::kNullSurfaceHandle);
  if (handle.is_null())
    return scoped_refptr<gl::GLImage>();

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      IOSurfaceLookupFromMachPort(handle.mach_port.get()));
  // TODO(ccameron): This should never happen, but has been seen in the wild. If
  // this happens frequently, it can be replaced by directly using the allocated
  // IOSurface, rather than going through the handle creation.
  // https://crbug.com/795649
  CHECK_NE(nullptr, io_surface.get())
      << "Failed to reconstitute just-created IOSurface from mach port.";

  scoped_refptr<gl::GLImageIOSurface> image(
      gl::GLImageIOSurface::Create(size, internalformat));
  if (!image->Initialize(io_surface.get(), handle.id, format)) {
    DLOG(ERROR) << "Failed to initialize anonymous GLImage.";
    return scoped_refptr<gl::GLImage>();
  }

  *is_cleared = false;
  return image;
}

unsigned GpuMemoryBufferFactoryIOSurface::RequiredTextureType() {
  return GL_TEXTURE_RECTANGLE_ARB;
}

bool GpuMemoryBufferFactoryIOSurface::SupportsFormatRGB() {
  return false;
}

}  // namespace gpu
