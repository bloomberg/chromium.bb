// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_ozone_native_pixmap.h"

#include "ui/gl/gl_image_ozone_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gpu {

GpuMemoryBufferFactoryOzoneNativePixmap::
    GpuMemoryBufferFactoryOzoneNativePixmap() {}

GpuMemoryBufferFactoryOzoneNativePixmap::
    ~GpuMemoryBufferFactoryOzoneNativePixmap() {}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryOzoneNativePixmap::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    SurfaceHandle surface_handle) {
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
  return new_handle;
}

ImageFactory* GpuMemoryBufferFactoryOzoneNativePixmap::AsImageFactory() {
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
  scoped_refptr<ui::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmapFromHandle(size, format,
                                         handle.native_pixmap_handle);
  if (!pixmap.get()) {
    DLOG(ERROR) << "Failed to create pixmap from handle";
    return nullptr;
  }

  scoped_refptr<gl::GLImageOzoneNativePixmap> image(
      new gl::GLImageOzoneNativePixmap(size, internalformat));
  if (!image->Initialize(pixmap.get(), format)) {
    LOG(ERROR) << "Failed to create GLImage";
    return nullptr;
  }
  return image;
}

}  // namespace gpu
