// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_X11_PIXMAP_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_X11_PIXMAP_H_

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/x/x11_types.h"

namespace gfx {
class GLImage;
}

namespace content {

class GpuMemoryBufferFactoryX11Pixmap {
 public:
  GpuMemoryBufferFactoryX11Pixmap();
  ~GpuMemoryBufferFactoryX11Pixmap();

  // Create a GPU memory buffer for an existing X11 pixmap.
  void CreateGpuMemoryBuffer(const gfx::GpuMemoryBufferId& id, XID pixmap);

  // Destroy a previously created GPU memory buffer.
  void DestroyGpuMemoryBuffer(const gfx::GpuMemoryBufferId& id);

  // Creates a GLImage instance for a GPU memory buffer.
  scoped_refptr<gfx::GLImage> CreateImageForGpuMemoryBuffer(
      const gfx::GpuMemoryBufferId& id,
      const gfx::Size& size,
      unsigned internalformat);

 private:
  typedef std::pair<int, int> X11PixmapMapKey;
  typedef base::hash_map<X11PixmapMapKey, XID> X11PixmapMap;
  X11PixmapMap pixmaps_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_X11_PIXMAP_H_
