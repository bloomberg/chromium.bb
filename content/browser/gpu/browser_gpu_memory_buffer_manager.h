// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_
#define CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"

namespace content {
class GpuMemoryBufferFactoryHost;
class GpuMemoryBufferImpl;

class CONTENT_EXPORT BrowserGpuMemoryBufferManager
    : public gpu::GpuMemoryBufferManager {
 public:
  typedef base::Callback<void(const gfx::GpuMemoryBufferHandle& handle)>
      AllocationCallback;

  BrowserGpuMemoryBufferManager(
      GpuMemoryBufferFactoryHost* gpu_memory_buffer_factory_host,
      int gpu_client_id);
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

  // Virtual for testing.
  virtual scoped_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBufferForScanout(
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      int32 surface_id);
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

  scoped_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBufferCommon(
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage,
      int32 surface_id);
  void AllocateGpuMemoryBufferOnIO(AllocateGpuMemoryBufferRequest* request);
  void GpuMemoryBufferAllocatedOnIO(AllocateGpuMemoryBufferRequest* request,
                                    const gfx::GpuMemoryBufferHandle& handle);
  void GpuMemoryBufferDeleted(gfx::GpuMemoryBufferId id,
                              int client_id,
                              uint32 sync_point);
  void GpuMemoryBufferAllocatedForChildProcess(
      int child_client_id,
      const AllocationCallback& callback,
      const gfx::GpuMemoryBufferHandle& handle);

  GpuMemoryBufferFactoryHost* gpu_memory_buffer_factory_host_;
  int gpu_client_id_;

  typedef base::hash_map<gfx::GpuMemoryBufferId, gfx::GpuMemoryBufferType>
      BufferMap;
  typedef base::hash_map<int, BufferMap> ClientMap;
  ClientMap clients_;

  base::WeakPtrFactory<BrowserGpuMemoryBufferManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserGpuMemoryBufferManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_
