// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory_io_surface.h"

#include <CoreFoundation/CoreFoundation.h>

#include "base/logging.h"
#include "ui/gl/gl_image_io_surface.h"

namespace content {
namespace {

void AddBooleanValue(CFMutableDictionaryRef dictionary,
                     const CFStringRef key,
                     bool value) {
  CFDictionaryAddValue(
      dictionary, key, value ? kCFBooleanTrue : kCFBooleanFalse);
}

void AddIntegerValue(CFMutableDictionaryRef dictionary,
                     const CFStringRef key,
                     int32 value) {
  base::ScopedCFTypeRef<CFNumberRef> number(
      CFNumberCreate(NULL, kCFNumberSInt32Type, &value));
  CFDictionaryAddValue(dictionary, key, number.get());
}

int32 BytesPerPixel(gfx::GpuMemoryBuffer::Format format) {
  switch (format) {
    case gfx::GpuMemoryBuffer::BGRA_8888:
      return 4;
    case gfx::GpuMemoryBuffer::ATC:
    case gfx::GpuMemoryBuffer::ATCIA:
    case gfx::GpuMemoryBuffer::DXT1:
    case gfx::GpuMemoryBuffer::DXT5:
    case gfx::GpuMemoryBuffer::ETC1:
    case gfx::GpuMemoryBuffer::RGBA_8888:
    case gfx::GpuMemoryBuffer::RGBX_8888:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

int32 PixelFormat(gfx::GpuMemoryBuffer::Format format) {
  switch (format) {
    case gfx::GpuMemoryBuffer::BGRA_8888:
      return 'BGRA';
    case gfx::GpuMemoryBuffer::ATC:
    case gfx::GpuMemoryBuffer::ATCIA:
    case gfx::GpuMemoryBuffer::DXT1:
    case gfx::GpuMemoryBuffer::DXT5:
    case gfx::GpuMemoryBuffer::ETC1:
    case gfx::GpuMemoryBuffer::RGBA_8888:
    case gfx::GpuMemoryBuffer::RGBX_8888:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

const GpuMemoryBufferFactory::Configuration kSupportedConfigurations[] = {
  { gfx::GpuMemoryBuffer::BGRA_8888, gfx::GpuMemoryBuffer::MAP }
};

}  // namespace

GpuMemoryBufferFactoryIOSurface::GpuMemoryBufferFactoryIOSurface() {
}

GpuMemoryBufferFactoryIOSurface::~GpuMemoryBufferFactoryIOSurface() {
}

// static
bool GpuMemoryBufferFactoryIOSurface::IsGpuMemoryBufferConfigurationSupported(
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage) {
  for (auto& configuration : kSupportedConfigurations) {
    if (configuration.format == format && configuration.usage == usage)
      return true;
  }

  return false;
}

void GpuMemoryBufferFactoryIOSurface::GetSupportedGpuMemoryBufferConfigurations(
    std::vector<Configuration>* configurations) {
  configurations->assign(
      kSupportedConfigurations,
      kSupportedConfigurations + arraysize(kSupportedConfigurations));
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryIOSurface::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage,
    int client_id,
    gfx::PluginWindowHandle surface_handle) {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> properties;
  properties.reset(CFDictionaryCreateMutable(kCFAllocatorDefault,
                                             0,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks));
  AddIntegerValue(properties, kIOSurfaceWidth, size.width());
  AddIntegerValue(properties, kIOSurfaceHeight, size.height());
  AddIntegerValue(properties, kIOSurfaceBytesPerElement, BytesPerPixel(format));
  AddIntegerValue(properties, kIOSurfacePixelFormat, PixelFormat(format));
  // TODO(reveman): Remove this when using a mach_port_t to transfer
  // IOSurface to browser and renderer process. crbug.com/323304
  AddBooleanValue(properties, kIOSurfaceIsGlobal, true);

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(IOSurfaceCreate(properties));
  if (!io_surface)
    return gfx::GpuMemoryBufferHandle();

  {
    base::AutoLock lock(io_surfaces_lock_);

    IOSurfaceMapKey key(id, client_id);
    DCHECK(io_surfaces_.find(key) == io_surfaces_.end());
    io_surfaces_[key] = io_surface;
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.id = id;
  handle.io_surface_id = IOSurfaceGetID(io_surface);
  return handle;
}

void GpuMemoryBufferFactoryIOSurface::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  base::AutoLock lock(io_surfaces_lock_);

  IOSurfaceMapKey key(id, client_id);
  IOSurfaceMap::iterator it = io_surfaces_.find(key);
  if (it != io_surfaces_.end())
    io_surfaces_.erase(it);
}

gpu::ImageFactory* GpuMemoryBufferFactoryIOSurface::AsImageFactory() {
  return this;
}

scoped_refptr<gfx::GLImage>
GpuMemoryBufferFactoryIOSurface::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    unsigned internalformat,
    int client_id) {
  base::AutoLock lock(io_surfaces_lock_);

  DCHECK_EQ(handle.type, gfx::IO_SURFACE_BUFFER);
  IOSurfaceMapKey key(handle.id, client_id);
  IOSurfaceMap::iterator it = io_surfaces_.find(key);
  if (it == io_surfaces_.end())
    return scoped_refptr<gfx::GLImage>();

  scoped_refptr<gfx::GLImageIOSurface> image(new gfx::GLImageIOSurface(size));
  if (!image->Initialize(it->second.get()))
    return scoped_refptr<gfx::GLImage>();

  return image;
}

}  // namespace content
