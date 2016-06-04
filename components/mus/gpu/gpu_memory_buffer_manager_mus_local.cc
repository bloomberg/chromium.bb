// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gpu/gpu_memory_buffer_manager_mus_local.h"

namespace mus {

GpuMemoryBufferManagerMusLocal::GpuMemoryBufferManagerMusLocal(
    int gpu_client_id,
    uint64_t gpu_client_tracing_id) {}

GpuMemoryBufferManagerMusLocal::~GpuMemoryBufferManagerMusLocal() {}

std::unique_ptr<gfx::GpuMemoryBuffer>
GpuMemoryBufferManagerMusLocal::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  NOTIMPLEMENTED();
  return std::unique_ptr<gfx::GpuMemoryBuffer>();
}

std::unique_ptr<gfx::GpuMemoryBuffer>
GpuMemoryBufferManagerMusLocal::CreateGpuMemoryBufferFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format) {
  NOTIMPLEMENTED();
  return std::unique_ptr<gfx::GpuMemoryBuffer>();
}

std::unique_ptr<gfx::GpuMemoryBuffer>
GpuMemoryBufferManagerMusLocal::CreateGpuMemoryBufferFromClientId(
    int client_id,
    const gfx::GpuMemoryBufferId& gpu_memory_buffer_id) {
  NOTIMPLEMENTED();
  return std::unique_ptr<gfx::GpuMemoryBuffer>();
}

gfx::GpuMemoryBuffer*
GpuMemoryBufferManagerMusLocal::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  NOTIMPLEMENTED();
  return nullptr;
}

void GpuMemoryBufferManagerMusLocal::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  NOTIMPLEMENTED();
}

}  // namespace mus
