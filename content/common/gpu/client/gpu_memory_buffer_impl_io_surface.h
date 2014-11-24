// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_IO_SURFACE_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_IO_SURFACE_H_

#include <IOSurface/IOSurfaceAPI.h>

#include "base/mac/scoped_cftyperef.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

namespace content {

// Implementation of GPU memory buffer based on IO surfaces.
class GpuMemoryBufferImplIOSurface : public GpuMemoryBufferImpl {
 public:
  static scoped_ptr<GpuMemoryBufferImpl> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      Format format,
      const DestructionCallback& callback);

  // Overridden from gfx::GpuMemoryBuffer:
  void* Map() override;
  void Unmap() override;
  uint32 GetStride() const override;
  gfx::GpuMemoryBufferHandle GetHandle() const override;

 private:
  GpuMemoryBufferImplIOSurface(gfx::GpuMemoryBufferId id,
                               const gfx::Size& size,
                               Format format,
                               const DestructionCallback& callback,
                               IOSurfaceRef io_surface);
  ~GpuMemoryBufferImplIOSurface() override;

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplIOSurface);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_IO_SURFACE_H_
