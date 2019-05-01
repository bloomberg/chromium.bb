// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_GPU_SERVICE_HOLDER_H_
#define COMPONENTS_VIZ_TEST_TEST_GPU_SERVICE_HOLDER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "gpu/vulkan/buildflags.h"

namespace gpu {
class CommandBufferTaskExecutor;
#if BUILDFLAG(ENABLE_VULKAN)
class VulkanImplementation;
#endif
}  // namespace gpu

namespace viz {
class GpuServiceImpl;

// Starts GPU Main and IO threads, and creates a GpuServiceImpl that can be used
// to create a SkiaOutputSurfaceImpl. This isn't a full GPU service
// implementation and should only be used in tests. GpuPreferences will be
// constructed from the command line when this class is first created.
class TestGpuServiceHolder {
 public:
  TestGpuServiceHolder();
  ~TestGpuServiceHolder();

  scoped_refptr<base::SingleThreadTaskRunner> gpu_thread_task_runner() {
    return gpu_thread_.task_runner();
  }

  // Most of |gpu_service_| is not safe to use off of the GPU thread, be careful
  // when accessing this.
  GpuServiceImpl* gpu_service() { return gpu_service_.get(); }

  gpu::CommandBufferTaskExecutor* task_executor() {
    return task_executor_.get();
  }

  bool is_vulkan_enabled() {
#if BUILDFLAG(ENABLE_VULKAN)
    return !!vulkan_implementation_;
#else
    return false;
#endif
  }

 private:
  void InitializeOnGpuThread(base::WaitableEvent* completion);
  void DeleteOnGpuThread();

  base::Thread gpu_thread_;
  base::Thread io_thread_;

  // These should only be created and deleted on the gpu thread.
  std::unique_ptr<GpuServiceImpl> gpu_service_;
  std::unique_ptr<gpu::CommandBufferTaskExecutor> task_executor_;
#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestGpuServiceHolder);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_GPU_SERVICE_HOLDER_H_
