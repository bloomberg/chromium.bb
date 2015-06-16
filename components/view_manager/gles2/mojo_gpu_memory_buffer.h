// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_GLES2_MOJO_GPU_MEMORY_BUFFER_H_
#define COMPONENTS_VIEW_MANAGER_GLES2_MOJO_GPU_MEMORY_BUFFER_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gles2 {

class MojoGpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  MojoGpuMemoryBufferImpl(const gfx::Size& size,
                          Format format,
                          scoped_ptr<base::SharedMemory> shared_memory);
  ~MojoGpuMemoryBufferImpl() override;


  static scoped_ptr<gfx::GpuMemoryBuffer> Create(const gfx::Size& size,
                                                 Format format,
                                                 Usage usage);

  static MojoGpuMemoryBufferImpl* FromClientBuffer(ClientBuffer buffer);

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map(void** data) override;
  void Unmap() override;
  bool IsMapped() const override;
  Format GetFormat() const override;
  void GetStride(int* stride) const override;
  gfx::GpuMemoryBufferHandle GetHandle() const override;
  ClientBuffer AsClientBuffer() override;

 private:
  const gfx::Size size_;
  gfx::GpuMemoryBuffer::Format format_;
  scoped_ptr<base::SharedMemory> shared_memory_;
  bool mapped_;

  DISALLOW_COPY_AND_ASSIGN(MojoGpuMemoryBufferImpl);
};

}  // gles2

#endif  // COMPONENTS_VIEW_MANAGER_GLES2_MOJO_GPU_MEMORY_BUFFER_H_
