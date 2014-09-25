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
  static void Create(const gfx::Size& size,
                     unsigned internalformat,
                     unsigned usage,
                     int client_id,
                     const CreationCallback& callback);

  static void AllocateForChildProcess(const gfx::Size& size,
                                      unsigned internalformat,
                                      unsigned usage,
                                      int child_client_id,
                                      const AllocationCallback& callback);

  static scoped_ptr<GpuMemoryBufferImpl> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      unsigned internalformat,
      const DestructionCallback& callback);

  static bool IsFormatSupported(unsigned internalformat);
  static bool IsUsageSupported(unsigned usage);
  static bool IsConfigurationSupported(unsigned internalformat, unsigned usage);
  static uint32 PixelFormat(unsigned internalformat);

  // Overridden from gfx::GpuMemoryBuffer:
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual uint32 GetStride() const OVERRIDE;
  virtual gfx::GpuMemoryBufferHandle GetHandle() const OVERRIDE;

 private:
  GpuMemoryBufferImplIOSurface(const gfx::Size& size,
                               unsigned internalformat,
                               const DestructionCallback& callback,
                               IOSurfaceRef io_surface);
  virtual ~GpuMemoryBufferImplIOSurface();

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplIOSurface);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_IO_SURFACE_H_
