// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_MOJO_GPU_MEMORY_BUFFER_H_
#define COMPONENTS_MUS_GLES2_MOJO_GPU_MEMORY_BUFFER_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace mus {

class MojoGpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  MojoGpuMemoryBufferImpl(const gfx::Size& size,
                          gfx::BufferFormat format,
                          scoped_ptr<base::SharedMemory> shared_memory);
  ~MojoGpuMemoryBufferImpl() override;

  static scoped_ptr<gfx::GpuMemoryBuffer> Create(const gfx::Size& size,
                                                 gfx::BufferFormat format,
                                                 gfx::BufferUsage usage);

  static MojoGpuMemoryBufferImpl* FromClientBuffer(ClientBuffer buffer);

  const unsigned char* GetMemory() const;

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override;
  void* memory(size_t plane) override;
  void Unmap() override;
  gfx::Size GetSize() const override;
  gfx::BufferFormat GetFormat() const override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferId GetId() const override;
  gfx::GpuMemoryBufferHandle GetHandle() const override;
  ClientBuffer AsClientBuffer() override;

 private:
  const gfx::Size size_;
  gfx::BufferFormat format_;
  scoped_ptr<base::SharedMemory> shared_memory_;
  bool mapped_;

  DISALLOW_COPY_AND_ASSIGN(MojoGpuMemoryBufferImpl);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_GLES2_MOJO_GPU_MEMORY_BUFFER_H_
