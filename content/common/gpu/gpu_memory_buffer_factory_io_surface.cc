// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory_io_surface.h"

#include <CoreFoundation/CoreFoundation.h>

#include "content/common/gpu/client/gpu_memory_buffer_impl_io_surface.h"
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

}  // namespace

GpuMemoryBufferFactoryIOSurface::GpuMemoryBufferFactoryIOSurface() {
}

GpuMemoryBufferFactoryIOSurface::~GpuMemoryBufferFactoryIOSurface() {
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryIOSurface::CreateGpuMemoryBuffer(
    const gfx::GpuMemoryBufferId& id,
    const gfx::Size& size,
    unsigned internalformat) {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> properties;
  properties.reset(CFDictionaryCreateMutable(kCFAllocatorDefault,
                                             0,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks));
  AddIntegerValue(properties, kIOSurfaceWidth, size.width());
  AddIntegerValue(properties, kIOSurfaceHeight, size.height());
  AddIntegerValue(properties,
                  kIOSurfaceBytesPerElement,
                  GpuMemoryBufferImpl::BytesPerPixel(internalformat));
  AddIntegerValue(properties,
                  kIOSurfacePixelFormat,
                  GpuMemoryBufferImplIOSurface::PixelFormat(internalformat));
  // TODO(reveman): Remove this when using a mach_port_t to transfer
  // IOSurface to browser and renderer process. crbug.com/323304
  AddBooleanValue(properties, kIOSurfaceIsGlobal, true);

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(IOSurfaceCreate(properties));
  if (!io_surface)
    return gfx::GpuMemoryBufferHandle();

  IOSurfaceMapKey key(id.primary_id, id.secondary_id);
  DCHECK(io_surfaces_.find(key) == io_surfaces_.end());
  io_surfaces_[key] = io_surface;

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.global_id = id;
  handle.io_surface_id = IOSurfaceGetID(io_surface);
  return handle;
}

void GpuMemoryBufferFactoryIOSurface::DestroyGpuMemoryBuffer(
    const gfx::GpuMemoryBufferId& id) {
  IOSurfaceMapKey key(id.primary_id, id.secondary_id);
  IOSurfaceMap::iterator it = io_surfaces_.find(key);
  if (it != io_surfaces_.end())
    io_surfaces_.erase(it);
}

scoped_refptr<gfx::GLImage>
GpuMemoryBufferFactoryIOSurface::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferId& id,
    const gfx::Size& size,
    unsigned internalformat) {
  IOSurfaceMapKey key(id.primary_id, id.secondary_id);
  IOSurfaceMap::iterator it = io_surfaces_.find(key);
  if (it == io_surfaces_.end())
    return scoped_refptr<gfx::GLImage>();

  scoped_refptr<gfx::GLImageIOSurface> image(new gfx::GLImageIOSurface(size));
  if (!image->Initialize(it->second.get()))
    return scoped_refptr<gfx::GLImage>();

  return image;
}

}  // namespace content
