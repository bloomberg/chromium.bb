// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_

#include <utility>

#include <IOSurface/IOSurface.h>

#include "base/containers/hash_tables.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/mac/io_surface.h"

namespace gl {
class GLImage;
}

namespace content {

class CONTENT_EXPORT GpuMemoryBufferFactoryIOSurface
    : public GpuMemoryBufferFactory,
      public gpu::ImageFactory {
 public:
  GpuMemoryBufferFactoryIOSurface();
  ~GpuMemoryBufferFactoryIOSurface() override;

  static bool IsGpuMemoryBufferConfigurationSupported(gfx::BufferFormat format,
                                                      gfx::BufferUsage usage);

  // Overridden from GpuMemoryBufferFactory:
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      gfx::PluginWindowHandle surface_handle) override;
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      int client_id) override;
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id) override;
  gpu::ImageFactory* AsImageFactory() override;

  // Overridden from gpu::ImageFactory:
  scoped_refptr<gl::GLImage> CreateImageForGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      unsigned internalformat,
      int client_id) override;

 private:
  typedef std::pair<gfx::IOSurfaceId, int> IOSurfaceMapKey;
  typedef base::hash_map<IOSurfaceMapKey, base::ScopedCFTypeRef<IOSurfaceRef>>
      IOSurfaceMap;
  // TOOD(reveman): Remove |io_surfaces_| and allow IOSurface backed GMBs to be
  // used with any GPU process by passing a mach_port to CreateImageCHROMIUM.
  IOSurfaceMap io_surfaces_;
  base::Lock io_surfaces_lock_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferFactoryIOSurface);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_
