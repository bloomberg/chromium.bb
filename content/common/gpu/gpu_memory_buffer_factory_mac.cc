// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory.h"

#include "base/logging.h"
#include "content/common/gpu/gpu_memory_buffer_factory_io_surface.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_shared_memory.h"

namespace content {
namespace {

class GpuMemoryBufferFactoryImpl : public GpuMemoryBufferFactory,
                                   public gpu::ImageFactory {
 public:
  // Overridden from GpuMemoryBufferFactory:
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferType type,
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage,
      int client_id) override {
    switch (type) {
      case gfx::IO_SURFACE_BUFFER:
        return io_surface_factory_.CreateGpuMemoryBuffer(
            id, size, format, client_id);
      default:
        NOTREACHED();
        return gfx::GpuMemoryBufferHandle();
    }
  }
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferType type,
                              gfx::GpuMemoryBufferId id,
                              int client_id) override {
    switch (type) {
      case gfx::IO_SURFACE_BUFFER:
        io_surface_factory_.DestroyGpuMemoryBuffer(id, client_id);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  gpu::ImageFactory* AsImageFactory() override { return this; }

  // Overridden from gpu::ImageFactory:
  scoped_refptr<gfx::GLImage> CreateImageForGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      unsigned internalformat,
      int client_id) override {
    switch (handle.type) {
      case gfx::SHARED_MEMORY_BUFFER: {
        scoped_refptr<gfx::GLImageSharedMemory> image(
            new gfx::GLImageSharedMemory(size, internalformat));
        if (!image->Initialize(handle, format))
          return NULL;

        return image;
      }
      case gfx::IO_SURFACE_BUFFER: {
        return io_surface_factory_.CreateImageForGpuMemoryBuffer(
            handle.id, size, format, client_id);
      }
      default:
        NOTREACHED();
        return scoped_refptr<gfx::GLImage>();
    }
  }

 private:
  GpuMemoryBufferFactoryIOSurface io_surface_factory_;
};

}  // namespace

// static
scoped_ptr<GpuMemoryBufferFactory> GpuMemoryBufferFactory::Create() {
  return make_scoped_ptr<GpuMemoryBufferFactory>(
      new GpuMemoryBufferFactoryImpl);
}

}  // namespace content
