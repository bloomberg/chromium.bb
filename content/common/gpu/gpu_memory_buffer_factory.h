// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gfx {
class GLImage;
}

namespace gpu {
class ImageFactory;
}

namespace content {

class GpuMemoryBufferFactory {
 public:
  GpuMemoryBufferFactory() {}
  virtual ~GpuMemoryBufferFactory() {}

  // Creates a new platform specific factory instance.
  static scoped_ptr<GpuMemoryBufferFactory> Create();

  // Creates a GPU memory buffer instance from |handle|. Whether the storage for
  // the buffer is passed with the handle or allocated as part of buffer
  // creation depends on the type. A valid handle is returned on success.
  virtual gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage) = 0;

  // Destroys GPU memory buffer identified by |handle|.
  virtual void DestroyGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle) = 0;

  // Type-checking downcast routine.
  virtual gpu::ImageFactory* AsImageFactory() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferFactory);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_H_
