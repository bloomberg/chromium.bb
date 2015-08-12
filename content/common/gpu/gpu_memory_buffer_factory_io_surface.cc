// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory_io_surface.h"

#include <CoreFoundation/CoreFoundation.h>

#include <vector>

#include "base/logging.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"
#include "content/common/mac/io_surface_manager.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_image_io_surface.h"

namespace content {
namespace {

void AddIntegerValue(CFMutableDictionaryRef dictionary,
                     const CFStringRef key,
                     int32 value) {
  base::ScopedCFTypeRef<CFNumberRef> number(
      CFNumberCreate(NULL, kCFNumberSInt32Type, &value));
  CFDictionaryAddValue(dictionary, key, number.get());
}

int32 BytesPerElement(gfx::BufferFormat format, int plane) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      DCHECK_EQ(plane, 0);
      return 1;
    case gfx::BufferFormat::BGRA_8888:
      DCHECK_EQ(plane, 0);
      return 4;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      static int32 bytes_per_element[] = {1, 2};
      DCHECK_LT(static_cast<size_t>(plane), arraysize(bytes_per_element));
      return bytes_per_element[plane];
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::YUV_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

int32 PixelFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return 'L008';
    case gfx::BufferFormat::BGRA_8888:
      return 'BGRA';
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return '420v';
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::YUV_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

const GpuMemoryBufferFactory::Configuration kSupportedConfigurations[] = {
    {gfx::BufferFormat::BGRA_8888, gfx::BufferUsage::SCANOUT},
    {gfx::BufferFormat::R_8, gfx::BufferUsage::PERSISTENT_MAP},
    {gfx::BufferFormat::R_8, gfx::BufferUsage::MAP},
    {gfx::BufferFormat::BGRA_8888, gfx::BufferUsage::PERSISTENT_MAP},
    {gfx::BufferFormat::BGRA_8888, gfx::BufferUsage::MAP},
    {gfx::BufferFormat::YUV_420_BIPLANAR, gfx::BufferUsage::MAP},
    {gfx::BufferFormat::YUV_420_BIPLANAR, gfx::BufferUsage::PERSISTENT_MAP},
};

}  // namespace

GpuMemoryBufferFactoryIOSurface::GpuMemoryBufferFactoryIOSurface() {
}

GpuMemoryBufferFactoryIOSurface::~GpuMemoryBufferFactoryIOSurface() {
}

// static
bool GpuMemoryBufferFactoryIOSurface::IsGpuMemoryBufferConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
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
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    gfx::PluginWindowHandle surface_handle) {
  size_t num_planes = gfx::NumberOfPlanesForBufferFormat(format);
  base::ScopedCFTypeRef<CFMutableArrayRef> planes(CFArrayCreateMutable(
      kCFAllocatorDefault, num_planes, &kCFTypeArrayCallBacks));

  for (size_t plane = 0; plane < num_planes; ++plane) {
    size_t factor = GpuMemoryBufferImpl::SubsamplingFactor(format, plane);

    base::ScopedCFTypeRef<CFMutableDictionaryRef> plane_info(
        CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));
    AddIntegerValue(plane_info, kIOSurfacePlaneWidth, size.width() / factor);
    AddIntegerValue(plane_info, kIOSurfacePlaneHeight, size.height() / factor);
    AddIntegerValue(plane_info, kIOSurfacePlaneBytesPerElement,
                    BytesPerElement(format, plane));

    CFArrayAppendValue(planes, plane_info);
  }

  base::ScopedCFTypeRef<CFMutableDictionaryRef> properties(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  AddIntegerValue(properties, kIOSurfaceWidth, size.width());
  AddIntegerValue(properties, kIOSurfaceHeight, size.height());
  AddIntegerValue(properties, kIOSurfacePixelFormat, PixelFormat(format));
  CFDictionaryAddValue(properties, kIOSurfacePlaneInfo, planes);

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(IOSurfaceCreate(properties));
  if (!io_surface)
    return gfx::GpuMemoryBufferHandle();

  if (!IOSurfaceManager::GetInstance()->RegisterIOSurface(id, client_id,
                                                          io_surface)) {
    return gfx::GpuMemoryBufferHandle();
  }

  {
    base::AutoLock lock(io_surfaces_lock_);

    IOSurfaceMapKey key(id, client_id);
    DCHECK(io_surfaces_.find(key) == io_surfaces_.end());
    io_surfaces_[key] = io_surface;
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.id = id;
  return handle;
}

void GpuMemoryBufferFactoryIOSurface::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  {
    base::AutoLock lock(io_surfaces_lock_);

    IOSurfaceMapKey key(id, client_id);
    DCHECK(io_surfaces_.find(key) != io_surfaces_.end());
    io_surfaces_.erase(key);
  }

  IOSurfaceManager::GetInstance()->UnregisterIOSurface(id, client_id);
}

gpu::ImageFactory* GpuMemoryBufferFactoryIOSurface::AsImageFactory() {
  return this;
}

scoped_refptr<gfx::GLImage>
GpuMemoryBufferFactoryIOSurface::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    unsigned internalformat,
    int client_id) {
  base::AutoLock lock(io_surfaces_lock_);

  DCHECK_EQ(handle.type, gfx::IO_SURFACE_BUFFER);
  IOSurfaceMapKey key(handle.id, client_id);
  IOSurfaceMap::iterator it = io_surfaces_.find(key);
  if (it == io_surfaces_.end())
    return scoped_refptr<gfx::GLImage>();

  scoped_refptr<gfx::GLImageIOSurface> image(
      new gfx::GLImageIOSurface(size, internalformat));
  if (!image->Initialize(it->second.get(), format))
    return scoped_refptr<gfx::GLImage>();

  return image;
}

}  // namespace content
