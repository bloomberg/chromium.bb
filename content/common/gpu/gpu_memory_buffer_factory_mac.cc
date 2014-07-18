// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory.h"

#include "base/logging.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_io_surface.h"
#include "ui/gl/gl_image_shared_memory.h"

namespace content {
namespace {

class GpuMemoryBufferFactoryImpl : public GpuMemoryBufferFactory {
 public:
  // Overridden from GpuMemoryBufferFactory:
  virtual gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      unsigned internalformat,
      unsigned usage) OVERRIDE {
    NOTREACHED();
    return gfx::GpuMemoryBufferHandle();
  }
  virtual void DestroyGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle) OVERRIDE {
    NOTREACHED();
  }
  virtual scoped_refptr<gfx::GLImage> CreateImageForGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      unsigned internalformat,
      int client_id) OVERRIDE {
    switch (handle.type) {
      case gfx::SHARED_MEMORY_BUFFER: {
        scoped_refptr<gfx::GLImageSharedMemory> image(
            new gfx::GLImageSharedMemory(size, internalformat));
        if (!image->Initialize(handle))
          return NULL;

        return image;
      }
      case gfx::IO_SURFACE_BUFFER: {
        scoped_refptr<gfx::GLImageIOSurface> image(
            new gfx::GLImageIOSurface(size));
        if (!image->Initialize(handle))
          return NULL;

        return image;
      }
      default:
        NOTREACHED();
        return scoped_refptr<gfx::GLImage>();
    }
  }
};

}  // namespace

// static
scoped_ptr<GpuMemoryBufferFactory> GpuMemoryBufferFactory::Create() {
  return make_scoped_ptr<GpuMemoryBufferFactory>(
      new GpuMemoryBufferFactoryImpl);
}

}  // namespace content
