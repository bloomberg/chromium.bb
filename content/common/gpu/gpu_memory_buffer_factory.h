// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class GLImage;
}

namespace gpu {
class ImageFactory;
}

namespace content {

class CONTENT_EXPORT GpuMemoryBufferFactory {
 public:
  struct Configuration {
    gfx::GpuMemoryBuffer::Format format;
    gfx::GpuMemoryBuffer::Usage usage;
  };

  GpuMemoryBufferFactory() {}
  virtual ~GpuMemoryBufferFactory() {}

  // Gets system supported GPU memory buffer factory types. Preferred type at
  // the front of vector.
  static void GetSupportedTypes(std::vector<gfx::GpuMemoryBufferType>* types);

  // Creates a new factory instance for |type|.
  static scoped_ptr<GpuMemoryBufferFactory> Create(
      gfx::GpuMemoryBufferType type);

  // Gets supported format/usage configurations.
  virtual void GetSupportedGpuMemoryBufferConfigurations(
      std::vector<Configuration>* configurations) = 0;

  // Creates a new GPU memory buffer instance. A valid handle is returned on
  // success.
  virtual gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage,
      int client_id,
      gfx::PluginWindowHandle surface_handle) = 0;

  // Destroys GPU memory buffer identified by |id|.
  virtual void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                      int client_id) = 0;

  // Type-checking downcast routine.
  virtual gpu::ImageFactory* AsImageFactory() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferFactory);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_MEMORY_BUFFER_FACTORY_H_
