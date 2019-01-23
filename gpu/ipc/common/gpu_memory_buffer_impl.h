// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_H_
#define GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

// Provides common implementation of a GPU memory buffer.
//
// TODO(reveman): Rename to GpuMemoryBufferBase.
class GPU_EXPORT GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  using DestructionCallback = base::OnceCallback<void(const gpu::SyncToken&)>;

  ~GpuMemoryBufferImpl() override;

  // Overridden from gfx::GpuMemoryBuffer:
  gfx::Size GetSize() const override;
  gfx::BufferFormat GetFormat() const override;
  gfx::GpuMemoryBufferId GetId() const override;
  ClientBuffer AsClientBuffer() override;
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override;

  void set_destruction_sync_token(const gpu::SyncToken& sync_token) {
    destruction_sync_token_ = sync_token;
  }

 protected:
  GpuMemoryBufferImpl(gfx::GpuMemoryBufferId id,
                      const gfx::Size& size,
                      gfx::BufferFormat format,
                      DestructionCallback callback);

  const gfx::GpuMemoryBufferId id_;
  const gfx::Size size_;
  const gfx::BufferFormat format_;
  DestructionCallback callback_;
  bool mapped_;
  gpu::SyncToken destruction_sync_token_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImpl);
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_H_
