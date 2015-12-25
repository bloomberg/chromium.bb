// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SURFACE_TEXTURE_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SURFACE_TEXTURE_H_

#include <android/native_window.h>
#include <stddef.h>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

namespace content {

// Implementation of GPU memory buffer based on SurfaceTextures.
class CONTENT_EXPORT GpuMemoryBufferImplSurfaceTexture
    : public GpuMemoryBufferImpl {
 public:
  ~GpuMemoryBufferImplSurfaceTexture() override;

  static scoped_ptr<GpuMemoryBufferImplSurfaceTexture> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      const DestructionCallback& callback);

  static bool IsConfigurationSupported(gfx::BufferFormat format,
                                       gfx::BufferUsage usage);

  static base::Closure AllocateForTesting(const gfx::Size& size,
                                          gfx::BufferFormat format,
                                          gfx::BufferUsage usage,
                                          gfx::GpuMemoryBufferHandle* handle);

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override;
  void* memory(size_t plane) override;
  void Unmap() override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferHandle GetHandle() const override;

 private:
  GpuMemoryBufferImplSurfaceTexture(gfx::GpuMemoryBufferId id,
                                    const gfx::Size& size,
                                    gfx::BufferFormat format,
                                    const DestructionCallback& callback,
                                    ANativeWindow* native_window);

  ANativeWindow* native_window_;
  ANativeWindow_Buffer buffer_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplSurfaceTexture);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SURFACE_TEXTURE_H_
