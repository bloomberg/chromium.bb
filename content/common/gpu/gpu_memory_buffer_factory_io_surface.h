// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_

#include <IOSurface/IOSurfaceAPI.h>

#include "base/containers/hash_tables.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gfx {
class GLImage;
}

namespace content {

class GpuMemoryBufferFactoryIOSurface {
 public:
  GpuMemoryBufferFactoryIOSurface();
  ~GpuMemoryBufferFactoryIOSurface();

  // Creates a IOSurface backed GPU memory buffer with |size| and
  // |internalformat|. A valid handle is returned on success.
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      const gfx::GpuMemoryBufferId& id,
      const gfx::Size& size,
      unsigned internalformat);

  // Destroy a previously created GPU memory buffer.
  void DestroyGpuMemoryBuffer(const gfx::GpuMemoryBufferId& id);

  // Creates a GLImage instance for a GPU memory buffer.
  scoped_refptr<gfx::GLImage> CreateImageForGpuMemoryBuffer(
      const gfx::GpuMemoryBufferId& id,
      const gfx::Size& size,
      unsigned internalformat);

 private:
  typedef std::pair<int, int> IOSurfaceMapKey;
  typedef base::hash_map<IOSurfaceMapKey, base::ScopedCFTypeRef<IOSurfaceRef>>
      IOSurfaceMap;
  IOSurfaceMap io_surfaces_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_IO_SURFACE_H_
