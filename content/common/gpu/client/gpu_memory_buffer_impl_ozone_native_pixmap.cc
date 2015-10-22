// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_ozone_native_pixmap.h"

#include "content/common/gpu/gpu_memory_buffer_factory_ozone_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace content {
namespace {

void FreeNativePixmapForTesting(scoped_refptr<ui::NativePixmap> native_pixmap) {
  // Nothing to do here. |native_pixmap| will be freed when this function
  // returns and reference count drops to 0.
}

}  // namespace

GpuMemoryBufferImplOzoneNativePixmap::GpuMemoryBufferImplOzoneNativePixmap(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    const DestructionCallback& callback,
    scoped_ptr<ui::ClientNativePixmap> pixmap)
    : GpuMemoryBufferImpl(id, size, format, callback), pixmap_(pixmap.Pass()) {}

GpuMemoryBufferImplOzoneNativePixmap::~GpuMemoryBufferImplOzoneNativePixmap() {}

// static
scoped_ptr<GpuMemoryBufferImplOzoneNativePixmap>
GpuMemoryBufferImplOzoneNativePixmap::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const DestructionCallback& callback) {
  scoped_ptr<ui::ClientNativePixmap> native_pixmap =
      ui::ClientNativePixmapFactory::GetInstance()->ImportFromHandle(
          handle.native_pixmap_handle, size, usage);
  DCHECK(native_pixmap);
  return make_scoped_ptr(new GpuMemoryBufferImplOzoneNativePixmap(
      handle.id, size, format, callback, native_pixmap.Pass()));
}

// static
bool GpuMemoryBufferImplOzoneNativePixmap::IsConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return GpuMemoryBufferFactoryOzoneNativePixmap::
      IsGpuMemoryBufferConfigurationSupported(format, usage);
}

// static
base::Closure GpuMemoryBufferImplOzoneNativePixmap::AllocateForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  scoped_refptr<ui::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(gfx::kNullPluginWindow, size, format, usage);
  handle->type = gfx::OZONE_NATIVE_PIXMAP;
  handle->native_pixmap_handle = pixmap->ExportHandle();
  return base::Bind(&FreeNativePixmapForTesting, pixmap);
}

bool GpuMemoryBufferImplOzoneNativePixmap::Map(void** data) {
  *data = pixmap_->Map();
  mapped_ = true;
  return mapped_;
}

void GpuMemoryBufferImplOzoneNativePixmap::Unmap() {
  pixmap_->Unmap();
  mapped_ = false;
}

void GpuMemoryBufferImplOzoneNativePixmap::GetStride(int* stride) const {
  pixmap_->GetStride(stride);
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplOzoneNativePixmap::GetHandle()
    const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::OZONE_NATIVE_PIXMAP;
  handle.id = id_;
  return handle;
}

}  // namespace content
