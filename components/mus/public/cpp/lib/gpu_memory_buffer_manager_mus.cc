// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/lib/gpu_memory_buffer_manager_mus.h"

namespace mus {

GpuMemoryBufferManagerMus::GpuMemoryBufferManagerMus() {}

GpuMemoryBufferManagerMus::~GpuMemoryBufferManagerMus() {}

std::unique_ptr<gfx::GpuMemoryBuffer>
GpuMemoryBufferManagerMus::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  NOTIMPLEMENTED();
  return std::unique_ptr<gfx::GpuMemoryBuffer>();
}

std::unique_ptr<gfx::GpuMemoryBuffer>
GpuMemoryBufferManagerMus::CreateGpuMemoryBufferFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format) {
  NOTIMPLEMENTED();
  return std::unique_ptr<gfx::GpuMemoryBuffer>();
}

std::unique_ptr<gfx::GpuMemoryBuffer>
GpuMemoryBufferManagerMus::CreateGpuMemoryBufferFromClientId(
    int client_id,
    const gfx::GpuMemoryBufferId& gpu_memory_buffer_id) {
  NOTIMPLEMENTED();
  return std::unique_ptr<gfx::GpuMemoryBuffer>();
}

gfx::GpuMemoryBuffer*
GpuMemoryBufferManagerMus::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  NOTIMPLEMENTED();
  return nullptr;
}

void GpuMemoryBufferManagerMus::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  NOTIMPLEMENTED();
}

}  // namespace mus
