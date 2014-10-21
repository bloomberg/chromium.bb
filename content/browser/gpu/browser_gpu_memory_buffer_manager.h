// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_
#define CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_

#include "base/callback.h"
#include "cc/resources/gpu_memory_buffer_manager.h"
#include "content/common/content_export.h"

namespace content {
class GpuMemoryBufferImpl;

class CONTENT_EXPORT BrowserGpuMemoryBufferManager
    : public cc::GpuMemoryBufferManager {
 public:
  typedef base::Callback<void(const gfx::GpuMemoryBufferHandle& handle)>
      AllocationCallback;

  explicit BrowserGpuMemoryBufferManager(int gpu_client_id);
  ~BrowserGpuMemoryBufferManager() override;

  static BrowserGpuMemoryBufferManager* current();

  // Overridden from cc::GpuMemoryBufferManager:
  scoped_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage) override;
  gfx::GpuMemoryBuffer* GpuMemoryBufferFromClientBuffer(
      ClientBuffer buffer) override;

  void AllocateGpuMemoryBufferForChildProcess(
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage,
      base::ProcessHandle child_process_handle,
      int child_client_id,
      const AllocationCallback& callback);
  void ChildProcessDeletedGpuMemoryBuffer(
      gfx::GpuMemoryBufferType type,
      const gfx::GpuMemoryBufferId& id,
      base::ProcessHandle child_process_handle);
  void ProcessRemoved(base::ProcessHandle process_handle);

 private:
  struct AllocateGpuMemoryBufferRequest;

  static void AllocateGpuMemoryBufferOnIO(
      AllocateGpuMemoryBufferRequest* request);
  static void GpuMemoryBufferCreatedOnIO(
      AllocateGpuMemoryBufferRequest* request,
      scoped_ptr<GpuMemoryBufferImpl> buffer);

  int gpu_client_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_
