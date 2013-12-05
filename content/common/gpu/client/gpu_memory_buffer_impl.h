// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/size.h"

namespace content {

// Provides common implementation of a GPU memory buffer.
class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  static scoped_ptr<GpuMemoryBufferImpl> Create(
      gfx::GpuMemoryBufferHandle handle,
      gfx::Size size,
      unsigned internalformat);

  virtual ~GpuMemoryBufferImpl();

  static bool IsFormatValid(unsigned internalformat);
  static size_t BytesPerPixel(unsigned internalformat);

  // Overridden from gfx::GpuMemoryBuffer:
  virtual bool IsMapped() const OVERRIDE;
  virtual uint32 GetStride() const OVERRIDE;

 protected:
  GpuMemoryBufferImpl(gfx::Size size, unsigned internalformat);

  const gfx::Size size_;
  unsigned internalformat_;
  bool mapped_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_
