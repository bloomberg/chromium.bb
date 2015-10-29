// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_SURFACE_TEXTURE_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_SURFACE_TEXTURE_H_

#include <utility>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gfx {
class SurfaceTexture;
}

namespace gl {
class GLImage;
}

namespace content {

class CONTENT_EXPORT GpuMemoryBufferFactorySurfaceTexture
    : public GpuMemoryBufferFactory,
      public gpu::ImageFactory {
 public:
  GpuMemoryBufferFactorySurfaceTexture();
  ~GpuMemoryBufferFactorySurfaceTexture() override;

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
  typedef std::pair<int, int> SurfaceTextureMapKey;
  typedef base::hash_map<SurfaceTextureMapKey,
                         scoped_refptr<gfx::SurfaceTexture>> SurfaceTextureMap;
  SurfaceTextureMap surface_textures_;
  base::Lock surface_textures_lock_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_SURFACE_TEXTURE_H_
