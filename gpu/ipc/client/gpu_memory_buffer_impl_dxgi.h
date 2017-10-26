// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_GPU_MEMORY_BUFFER_IMPL_DXGI_H_
#define GPU_IPC_CLIENT_GPU_MEMORY_BUFFER_IMPL_DXGI_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/macros.h"
#include "base/win/scoped_handle.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl.h"
#include "ui/gfx/color_space.h"

namespace gpu {

// Implementation of GPU memory buffer based on dxgi textures.
class GPU_EXPORT GpuMemoryBufferImplDXGI : public GpuMemoryBufferImpl {
 public:
  ~GpuMemoryBufferImplDXGI() override;

  static std::unique_ptr<GpuMemoryBufferImplDXGI> CreateFromHandle(
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

  bool Map() override;
  void* memory(size_t plane) override;
  void Unmap() override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferHandle GetHandle() const override;

 private:
  GpuMemoryBufferImplDXGI(gfx::GpuMemoryBufferId id,
                          const gfx::Size& size,
                          gfx::BufferFormat format,
                          const DestructionCallback& callback,
                          const base::SharedMemoryHandle& dxgi_handle);

  base::SharedMemory shared_memory_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplDXGI);
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_GPU_MEMORY_BUFFER_IMPL_DXGI_H_
