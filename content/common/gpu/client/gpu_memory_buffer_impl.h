// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/size.h"

namespace content {

// Provides common implementation of a GPU memory buffer based
// on a shared memory handle.
class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(scoped_ptr<base::SharedMemory> shared_memory,
                      size_t width,
                      size_t height,
                      unsigned internalformat);
  virtual ~GpuMemoryBufferImpl();

  // Overridden from gfx::GpuMemoryBuffer:
  virtual void Map(AccessMode mode, void** vaddr) OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual bool IsMapped() const OVERRIDE;
  virtual uint32 GetStride() const OVERRIDE;
  virtual gfx::GpuMemoryBufferHandle GetHandle() const OVERRIDE;

  static bool IsFormatValid(unsigned internalformat);
  static size_t BytesPerPixel(unsigned internalformat);

 private:
  scoped_ptr<base::SharedMemory> shared_memory_;
  const gfx::Size size_;
  unsigned internalformat_;
  bool mapped_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_
