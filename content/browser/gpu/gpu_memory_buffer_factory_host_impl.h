// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_MEMORY_BUFFER_FACTORY_HOST_IMPL_H_
#define CONTENT_BROWSER_GPU_GPU_MEMORY_BUFFER_FACTORY_HOST_IMPL_H_

#include <map>

#include "content/common/gpu/client/gpu_memory_buffer_factory_host.h"

namespace content {
class GpuMemoryBufferImpl;

class CONTENT_EXPORT GpuMemoryBufferFactoryHostImpl
    : public GpuMemoryBufferFactoryHost {
 public:
  GpuMemoryBufferFactoryHostImpl();
  virtual ~GpuMemoryBufferFactoryHostImpl();

  // Overridden from GpuMemoryBufferFactoryHost:
  virtual void CreateGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      unsigned internalformat,
      unsigned usage,
      const CreateGpuMemoryBufferCallback& callback) OVERRIDE;
  virtual void DestroyGpuMemoryBuffer(const gfx::GpuMemoryBufferHandle& handle,
                                      int32 sync_point) OVERRIDE;

  void set_gpu_host_id(int gpu_host_id) { gpu_host_id_ = gpu_host_id; }

 private:
  void OnGpuMemoryBufferCreated(uint32 request_id,
                                const gfx::GpuMemoryBufferHandle& handle);

  int gpu_host_id_;
  uint32 next_create_gpu_memory_buffer_request_id_;
  typedef std::map<uint32, CreateGpuMemoryBufferCallback>
      CreateGpuMemoryBufferCallbackMap;
  CreateGpuMemoryBufferCallbackMap create_gpu_memory_buffer_requests_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferFactoryHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_MEMORY_BUFFER_FACTORY_HOST_IMPL_H_
