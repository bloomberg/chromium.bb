// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SURFACE_TEXTURE_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SURFACE_TEXTURE_H_

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

struct ANativeWindow;

namespace content {

// Implementation of GPU memory buffer based on SurfaceTextures.
class GpuMemoryBufferImplSurfaceTexture : public GpuMemoryBufferImpl {
 public:
  static scoped_ptr<GpuMemoryBufferImpl> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      Format format,
      const DestructionCallback& callback);

  // Overridden from gfx::GpuMemoryBuffer:
  void* Map() override;
  void Unmap() override;
  gfx::GpuMemoryBufferHandle GetHandle() const override;
  uint32 GetStride() const override;

 private:
  GpuMemoryBufferImplSurfaceTexture(gfx::GpuMemoryBufferId id,
                                    const gfx::Size& size,
                                    Format format,
                                    const DestructionCallback& callback,
                                    ANativeWindow* native_window);
  ~GpuMemoryBufferImplSurfaceTexture() override;

  ANativeWindow* native_window_;
  size_t stride_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplSurfaceTexture);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SURFACE_TEXTURE_H_
