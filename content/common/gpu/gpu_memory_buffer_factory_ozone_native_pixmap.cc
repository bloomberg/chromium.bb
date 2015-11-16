// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory_ozone_native_pixmap.h"

#include "ui/gl/gl_image_ozone_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace content {

GpuMemoryBufferFactoryOzoneNativePixmap::
    GpuMemoryBufferFactoryOzoneNativePixmap() {}

GpuMemoryBufferFactoryOzoneNativePixmap::
    ~GpuMemoryBufferFactoryOzoneNativePixmap() {}

// static
bool GpuMemoryBufferFactoryOzoneNativePixmap::
    IsGpuMemoryBufferConfigurationSupported(gfx::BufferFormat format,
                                            gfx::BufferUsage usage) {
  if (!ui::ClientNativePixmapFactory::GetInstance()) {
    // unittests don't have to set ClientNativePixmapFactory.
    return false;
  }
  return ui::ClientNativePixmapFactory::GetInstance()->IsConfigurationSupported(
      format, usage);
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
    DLOG(ERROR) << "Failed to create pixmap " << size.width() << "x"
                << size.height() << " format " << static_cast<int>(format)
                << ", usage " << static_cast<int>(usage);
    return gfx::GpuMemoryBufferHandle();
  }

  gfx::GpuMemoryBufferHandle new_handle;
  new_handle.type = gfx::OZONE_NATIVE_PIXMAP;
  new_handle.id = id;
  new_handle.native_pixmap_handle = pixmap->ExportHandle();

  {
    base::AutoLock lock(native_pixmaps_lock_);
    NativePixmapMapKey key(id.id, client_id);
    DCHECK(native_pixmaps_.find(key) == native_pixmaps_.end());
    native_pixmaps_[key] = pixmap;
  }

  return new_handle;
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryOzoneNativePixmap::CreateGpuMemoryBufferFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    int client_id) {
  scoped_refptr<ui::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmapFromHandle(handle.native_pixmap_handle);
  if (!pixmap.get()) {
    DLOG(ERROR) << "Failed to create pixmap from handle";
    return gfx::GpuMemoryBufferHandle();
  }

  gfx::GpuMemoryBufferHandle new_handle;
  new_handle.type = gfx::OZONE_NATIVE_PIXMAP;
  new_handle.id = id;
  new_handle.native_pixmap_handle = pixmap->ExportHandle();

  {
    base::AutoLock lock(native_pixmaps_lock_);
    NativePixmapMapKey key(id.id, client_id);
    DCHECK(native_pixmaps_.find(key) == native_pixmaps_.end());
    native_pixmaps_[key] = pixmap;
  }

  return new_handle;
}

void GpuMemoryBufferFactoryOzoneNativePixmap::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  base::AutoLock lock(native_pixmaps_lock_);
  auto it = native_pixmaps_.find(NativePixmapMapKey(id.id, client_id));
  DCHECK(it != native_pixmaps_.end());
  native_pixmaps_.erase(it);
}

gpu::ImageFactory* GpuMemoryBufferFactoryOzoneNativePixmap::AsImageFactory() {
  return this;
}

scoped_refptr<gl::GLImage>
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
        native_pixmaps_.find(NativePixmapMapKey(handle.id.id, client_id));
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
