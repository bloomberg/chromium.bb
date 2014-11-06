// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_
#define CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_

#include "base/callback.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"

namespace content {
class GpuMemoryBufferImpl;

class CONTENT_EXPORT BrowserGpuMemoryBufferManager
    : public gpu::GpuMemoryBufferManager {
 public:
  typedef base::Callback<void(const gfx::GpuMemoryBufferHandle& handle)>
      AllocationCallback;

  explicit BrowserGpuMemoryBufferManager(int gpu_client_id);
  ~BrowserGpuMemoryBufferManager() override;

  static BrowserGpuMemoryBufferManager* current();

  // Overridden from gpu::GpuMemoryBufferManager:
  scoped_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage) override;
  gfx::GpuMemoryBuffer* GpuMemoryBufferFromClientBuffer(
      ClientBuffer buffer) override;
  void SetDestructionSyncPoint(gfx::GpuMemoryBuffer* buffer,
                               uint32 sync_point) override;

  void AllocateGpuMemoryBufferForChildProcess(
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage,
      base::ProcessHandle child_process_handle,
      int child_client_id,
      const AllocationCallback& callback);
  void ChildProcessDeletedGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      base::ProcessHandle child_process_handle,
      int child_client_id,
      uint32 sync_point);
  void ProcessRemoved(base::ProcessHandle process_handle, int client_id);

 private:
  struct AllocateGpuMemoryBufferRequest;

  void GpuMemoryBufferAllocatedForChildProcess(
      base::ProcessHandle child_process_handle,
      int child_client_id,
      const AllocationCallback& callback,
      const gfx::GpuMemoryBufferHandle& handle);

  static void AllocateGpuMemoryBufferOnIO(
      AllocateGpuMemoryBufferRequest* request);
  static void GpuMemoryBufferCreatedOnIO(
      AllocateGpuMemoryBufferRequest* request,
      scoped_ptr<GpuMemoryBufferImpl> buffer);

  int gpu_client_id_;

  typedef base::hash_map<gfx::GpuMemoryBufferId, gfx::GpuMemoryBufferType>
      BufferMap;
  typedef base::hash_map<int, BufferMap> ClientMap;
  ClientMap clients_;

  DISALLOW_COPY_AND_ASSIGN(BrowserGpuMemoryBufferManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_
