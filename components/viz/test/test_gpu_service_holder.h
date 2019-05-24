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
struct GpuPreferences;
}  // namespace gpu

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace viz {
class GpuServiceImpl;

// Starts GPU Main and IO threads, and creates a GpuServiceImpl that can be used
// to create a SkiaOutputSurfaceImpl. This isn't a full GPU service
// implementation and should only be used in tests.
class TestGpuServiceHolder {
 public:
  // Exposes a singleton to allow easy sharing of the GpuServiceImpl by
  // different clients (e.g. to share SharedImages via a common
  // SharedImageManager).
  //
  // The instance will parse GpuPreferences from the command line when it is
  // first created (e.g. to allow entire test suite with --enable-vulkan).
  //
  // If specific feature flags or GpuPreferences are needed for a specific test,
  // a separate instance of this class can be created.
  static TestGpuServiceHolder* GetInstance();

  // Calling this method ensures that GetInstance() is destroyed after each
  // gtest completes -- it only applies to gtest because it uses gtest hooks. A
  // subsequent call to GetInstance() will create a new instance. Safe to call
  // more than once.
  static void DestroyInstanceAfterEachTest();

  TestGpuServiceHolder(const gpu::GpuPreferences& preferences,
                       bool use_swiftshader_for_vulkan);
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
  friend struct base::DefaultSingletonTraits<TestGpuServiceHolder>;

  void InitializeOnGpuThread(const gpu::GpuPreferences& preferences,
                             bool use_swiftshader_for_vulkan,
                             base::WaitableEvent* completion);
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
