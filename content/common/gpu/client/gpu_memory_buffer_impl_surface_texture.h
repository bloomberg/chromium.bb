// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SURFACE_TEXTURE_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SURFACE_TEXTURE_H_

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

struct ANativeWindow;

namespace content {

// Provides implementation of a GPU memory buffer based on a surface texture id.
class GpuMemoryBufferImplSurfaceTexture : public GpuMemoryBufferImpl {
 public:
  GpuMemoryBufferImplSurfaceTexture(gfx::Size size, unsigned internalformat);
  virtual ~GpuMemoryBufferImplSurfaceTexture();

  bool Initialize(gfx::GpuMemoryBufferHandle handle);

  static bool IsFormatSupported(unsigned internalformat);
  static bool IsUsageSupported(unsigned usage);
  static int WindowFormat(unsigned internalformat);

  // Overridden from gfx::GpuMemoryBuffer:
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual gfx::GpuMemoryBufferHandle GetHandle() const OVERRIDE;
  virtual uint32 GetStride() const OVERRIDE;

 private:
  gfx::SurfaceTextureId surface_texture_id_;
  ANativeWindow* native_window_;
  size_t stride_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplSurfaceTexture);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_SURFACE_TEXTURE_H_
