// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_FACTORY_HOST_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_FACTORY_HOST_H_

#include "base/callback.h"
#include "content/common/content_export.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gfx {
class Size;
}

namespace content {

class CONTENT_EXPORT GpuMemoryBufferFactoryHost {
 public:
  typedef base::Callback<void(const gfx::GpuMemoryBufferHandle& handle)>
      CreateGpuMemoryBufferCallback;

  virtual bool IsGpuMemoryBufferConfigurationSupported(
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage) = 0;
  virtual void CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage,
      int client_id,
      int32 surface_id,
      const CreateGpuMemoryBufferCallback& callback) = 0;
  virtual void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                      int client_id,
                                      int32 sync_point) = 0;

 protected:
  virtual ~GpuMemoryBufferFactoryHost() {}
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_FACTORY_HOST_H_
