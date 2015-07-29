// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory_ozone_native_pixmap.h"

#include "ui/gl/gl_image_ozone_native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace content {
namespace {

const GpuMemoryBufferFactory::Configuration kSupportedConfigurations[] = {
    {gfx::GpuMemoryBuffer::BGRA_8888, gfx::GpuMemoryBuffer::SCANOUT},
    {gfx::GpuMemoryBuffer::RGBX_8888, gfx::GpuMemoryBuffer::SCANOUT}};

ui::SurfaceFactoryOzone::BufferFormat GetOzoneFormatFor(
    gfx::GpuMemoryBuffer::Format format) {
  switch (format) {
    case gfx::GpuMemoryBuffer::BGRA_8888:
      return ui::SurfaceFactoryOzone::BGRA_8888;
    case gfx::GpuMemoryBuffer::RGBX_8888:
      return ui::SurfaceFactoryOzone::RGBX_8888;
    case gfx::GpuMemoryBuffer::ATC:
    case gfx::GpuMemoryBuffer::ATCIA:
    case gfx::GpuMemoryBuffer::DXT1:
    case gfx::GpuMemoryBuffer::DXT5:
    case gfx::GpuMemoryBuffer::ETC1:
    case gfx::GpuMemoryBuffer::R_8:
    case gfx::GpuMemoryBuffer::RGBA_4444:
    case gfx::GpuMemoryBuffer::RGBA_8888:
    case gfx::GpuMemoryBuffer::YUV_420:
      NOTREACHED();
      return ui::SurfaceFactoryOzone::BGRA_8888;
  }

  NOTREACHED();
  return ui::SurfaceFactoryOzone::BGRA_8888;
}

ui::SurfaceFactoryOzone::BufferUsage GetOzoneUsageFor(
    gfx::GpuMemoryBuffer::Usage usage) {
  switch (usage) {
    case gfx::GpuMemoryBuffer::MAP:
      return ui::SurfaceFactoryOzone::MAP;
    case gfx::GpuMemoryBuffer::PERSISTENT_MAP:
      return ui::SurfaceFactoryOzone::PERSISTENT_MAP;
    case gfx::GpuMemoryBuffer::SCANOUT:
      return ui::SurfaceFactoryOzone::SCANOUT;
  }

  NOTREACHED();
  return ui::SurfaceFactoryOzone::MAP;
}

}  // namespace

GpuMemoryBufferFactoryOzoneNativePixmap::
    GpuMemoryBufferFactoryOzoneNativePixmap() {}

GpuMemoryBufferFactoryOzoneNativePixmap::
    ~GpuMemoryBufferFactoryOzoneNativePixmap() {}

// static
bool GpuMemoryBufferFactoryOzoneNativePixmap::
    IsGpuMemoryBufferConfigurationSupported(gfx::GpuMemoryBuffer::Format format,
                                            gfx::GpuMemoryBuffer::Usage usage) {
  for (auto& configuration : kSupportedConfigurations) {
    if (configuration.format == format && configuration.usage == usage)
      return true;
  }

  return false;
}

void GpuMemoryBufferFactoryOzoneNativePixmap::
    GetSupportedGpuMemoryBufferConfigurations(
        std::vector<Configuration>* configurations) {
  if (!ui::OzonePlatform::GetInstance()
           ->GetSurfaceFactoryOzone()
           ->CanCreateNativePixmap(ui::SurfaceFactoryOzone::SCANOUT))
    return;

  configurations->assign(
      kSupportedConfigurations,
      kSupportedConfigurations + arraysize(kSupportedConfigurations));
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryOzoneNativePixmap::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage,
    int client_id,
    gfx::PluginWindowHandle surface_handle) {
  scoped_refptr<ui::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(surface_handle, size, GetOzoneFormatFor(format),
                               GetOzoneUsageFor(usage));
  if (!pixmap.get()) {
    LOG(ERROR) << "Failed to create pixmap " << size.width() << "x"
               << size.height() << " format " << format << ", usage " << usage;
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
    gfx::GpuMemoryBuffer::Format format,
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
