// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory_ozone_native_pixmap.h"

#include "ui/gl/gl_image_ozone_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace content {
namespace {

void GetSupportedConfigurations(
    std::vector<GpuMemoryBufferFactory::Configuration>* configurations) {
  if (!ui::ClientNativePixmapFactory::GetInstance()) {
    // unittests don't have to set ClientNativePixmapFactory.
    return;
  }
  std::vector<ui::ClientNativePixmapFactory::Configuration>
      native_pixmap_configurations =
          ui::ClientNativePixmapFactory::GetInstance()
              ->GetSupportedConfigurations();
  for (auto& native_pixmap_configuration : native_pixmap_configurations) {
    configurations->push_back({native_pixmap_configuration.format,
                               native_pixmap_configuration.usage});
  }
}

}  // namespace

GpuMemoryBufferFactoryOzoneNativePixmap::
    GpuMemoryBufferFactoryOzoneNativePixmap() {}

GpuMemoryBufferFactoryOzoneNativePixmap::
    ~GpuMemoryBufferFactoryOzoneNativePixmap() {}

// static
bool GpuMemoryBufferFactoryOzoneNativePixmap::
    IsGpuMemoryBufferConfigurationSupported(gfx::BufferFormat format,
                                            gfx::BufferUsage usage) {
  std::vector<Configuration> configurations;
  GetSupportedConfigurations(&configurations);
  for (auto& configuration : configurations) {
    if (configuration.format == format && configuration.usage == usage)
      return true;
  }

  return false;
}

void GpuMemoryBufferFactoryOzoneNativePixmap::
    GetSupportedGpuMemoryBufferConfigurations(
        std::vector<Configuration>* configurations) {
  GetSupportedConfigurations(configurations);
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryOzoneNativePixmap::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    gfx::PluginWindowHandle surface_handle) {
  scoped_refptr<ui::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(surface_handle, size, format, usage);
  if (!pixmap.get()) {
    LOG(ERROR) << "Failed to create pixmap " << size.width() << "x"
               << size.height() << " format " << static_cast<int>(format)
               << ", usage " << static_cast<int>(usage);
    return gfx::GpuMemoryBufferHandle();
  }
  base::AutoLock lock(native_pixmaps_lock_);
  NativePixmapMapKey key(id, client_id);
  DCHECK(native_pixmaps_.find(key) == native_pixmaps_.end())
      << "pixmap with this key must not exist";
  native_pixmaps_[key] = pixmap;

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::OZONE_NATIVE_PIXMAP;
  handle.id = id;
  return handle;
}

void GpuMemoryBufferFactoryOzoneNativePixmap::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  base::AutoLock lock(native_pixmaps_lock_);
  auto it = native_pixmaps_.find(NativePixmapMapKey(id, client_id));
  DCHECK(it != native_pixmaps_.end()) << "pixmap with this key must exist";
  native_pixmaps_.erase(it);
}

gpu::ImageFactory* GpuMemoryBufferFactoryOzoneNativePixmap::AsImageFactory() {
  return this;
}

scoped_refptr<gfx::GLImage>
GpuMemoryBufferFactoryOzoneNativePixmap::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    unsigned internalformat,
    int client_id) {
  DCHECK_EQ(handle.type, gfx::OZONE_NATIVE_PIXMAP);
  scoped_refptr<ui::NativePixmap> pixmap;
  {
    base::AutoLock lock(native_pixmaps_lock_);
    NativePixmapMap::iterator it =
        native_pixmaps_.find(NativePixmapMapKey(handle.id, client_id));
    if (it == native_pixmaps_.end()) {
      return nullptr;
    }
    pixmap = it->second;
  }

  scoped_refptr<gfx::GLImageOzoneNativePixmap> image(
      new gfx::GLImageOzoneNativePixmap(size, internalformat));
  if (!image->Initialize(pixmap.get(), format)) {
    LOG(ERROR) << "Failed to create GLImage";
    return nullptr;
  }
  return image;
}

}  // namespace content
