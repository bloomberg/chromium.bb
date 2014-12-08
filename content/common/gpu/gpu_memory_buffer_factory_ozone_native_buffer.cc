// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory_ozone_native_buffer.h"

#include "base/logging.h"
#include "ui/gl/gl_image.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace content {
namespace {

const GpuMemoryBufferFactory::Configuration kSupportedConfigurations[] = {
  { gfx::GpuMemoryBuffer::RGBA_8888, gfx::GpuMemoryBuffer::SCANOUT },
  { gfx::GpuMemoryBuffer::RGBX_8888, gfx::GpuMemoryBuffer::SCANOUT }
};

}  // namespace

GpuMemoryBufferFactoryOzoneNativeBuffer::
    GpuMemoryBufferFactoryOzoneNativeBuffer() {
}

GpuMemoryBufferFactoryOzoneNativeBuffer::
    ~GpuMemoryBufferFactoryOzoneNativeBuffer() {
}

// static
bool GpuMemoryBufferFactoryOzoneNativeBuffer::
    IsGpuMemoryBufferConfigurationSupported(gfx::GpuMemoryBuffer::Format format,
                                            gfx::GpuMemoryBuffer::Usage usage) {
  for (auto& configuration : kSupportedConfigurations) {
    if (configuration.format == format && configuration.usage == usage)
      return true;
  }

  return false;
}

void GpuMemoryBufferFactoryOzoneNativeBuffer::
    GetSupportedGpuMemoryBufferConfigurations(
        std::vector<Configuration>* configurations) {
  if (!ui::SurfaceFactoryOzone::GetInstance()->CanCreateNativePixmap(
          ui::SurfaceFactoryOzone::SCANOUT))
    return;

  configurations->assign(
      kSupportedConfigurations,
      kSupportedConfigurations + arraysize(kSupportedConfigurations));
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryOzoneNativeBuffer::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage,
    int client_id,
    gfx::PluginWindowHandle surface_handle) {
  if (!ozone_native_buffer_factory_.CreateGpuMemoryBuffer(
          id, size, format, usage, client_id, surface_handle)) {
    return gfx::GpuMemoryBufferHandle();
  }
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::OZONE_NATIVE_BUFFER;
  handle.id = id;
  return handle;
}

void GpuMemoryBufferFactoryOzoneNativeBuffer::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  ozone_native_buffer_factory_.DestroyGpuMemoryBuffer(id, client_id);
}

gpu::ImageFactory* GpuMemoryBufferFactoryOzoneNativeBuffer::AsImageFactory() {
  return this;
}

scoped_refptr<gfx::GLImage>
GpuMemoryBufferFactoryOzoneNativeBuffer::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    unsigned internalformat,
    int client_id) {
  DCHECK_EQ(handle.type, gfx::OZONE_NATIVE_BUFFER);
  return ozone_native_buffer_factory_.CreateImageForGpuMemoryBuffer(
      handle.id, size, format, internalformat, client_id);
}

}  // namespace content
