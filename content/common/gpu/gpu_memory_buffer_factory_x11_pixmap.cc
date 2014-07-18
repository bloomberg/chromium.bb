// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory_x11_pixmap.h"

#include "ui/gl/gl_image_glx.h"

namespace content {

GpuMemoryBufferFactoryX11Pixmap::GpuMemoryBufferFactoryX11Pixmap() {
}

GpuMemoryBufferFactoryX11Pixmap::~GpuMemoryBufferFactoryX11Pixmap() {
}

void GpuMemoryBufferFactoryX11Pixmap::CreateGpuMemoryBuffer(
    const gfx::GpuMemoryBufferId& id,
    XID pixmap) {
  X11PixmapMapKey key(id.primary_id, id.secondary_id);
  DCHECK(pixmaps_.find(key) == pixmaps_.end());
  pixmaps_[key] = pixmap;
}

void GpuMemoryBufferFactoryX11Pixmap::DestroyGpuMemoryBuffer(
    const gfx::GpuMemoryBufferId& id) {
  X11PixmapMapKey key(id.primary_id, id.secondary_id);
  X11PixmapMap::iterator it = pixmaps_.find(key);
  if (it != pixmaps_.end())
    pixmaps_.erase(it);
}

scoped_refptr<gfx::GLImage>
GpuMemoryBufferFactoryX11Pixmap::AcquireImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferId& id,
    const gfx::Size& size,
    unsigned internalformat) {
  X11PixmapMapKey key(id.primary_id, id.secondary_id);
  X11PixmapMap::iterator it = pixmaps_.find(key);
  if (it == pixmaps_.end())
    return scoped_refptr<gfx::GLImage>();
  XID pixmap = it->second;
  pixmaps_.erase(it);

  scoped_refptr<gfx::GLImageGLX> image(
      new gfx::GLImageGLX(size, internalformat));
  if (!image->Initialize(pixmap))
    return scoped_refptr<gfx::GLImage>();

  return image;
}

}  // namespace content
