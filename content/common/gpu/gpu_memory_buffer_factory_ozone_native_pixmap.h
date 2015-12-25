// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_OZONE_NATIVE_PIXMAP_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_OZONE_NATIVE_PIXMAP_H_

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "ui/ozone/public/native_pixmap.h"

namespace gl {
class GLImage;
}

namespace content {

class CONTENT_EXPORT GpuMemoryBufferFactoryOzoneNativePixmap
    : public GpuMemoryBufferFactory,
      public gpu::ImageFactory {
 public:
  GpuMemoryBufferFactoryOzoneNativePixmap();
  ~GpuMemoryBufferFactoryOzoneNativePixmap() override;

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
  using NativePixmapMapKey = std::pair<int, int>;
  using NativePixmapMap =
      base::hash_map<NativePixmapMapKey, scoped_refptr<ui::NativePixmap>>;
  NativePixmapMap native_pixmaps_;
  base::Lock native_pixmaps_lock_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferFactoryOzoneNativePixmap);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_OZONE_NATIVE_PIXMAP_H_
