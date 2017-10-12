// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_
#define CC_TEST_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "cc/test/test_image_factory.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "gpu/config/gpu_feature_info.h"

class GrContext;

namespace gpu {
class GLInProcessContext;
}

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace cc {

std::unique_ptr<gpu::GLInProcessContext> CreateTestInProcessContext();
std::unique_ptr<gpu::GLInProcessContext> CreateTestInProcessContext(
    viz::TestGpuMemoryBufferManager* gpu_memory_buffer_manager,
    TestImageFactory* image_factory,
    gpu::GLInProcessContext* shared_context,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner);

class TestInProcessContextProvider : public viz::ContextProvider {
 public:
  explicit TestInProcessContextProvider(
      TestInProcessContextProvider* shared_context);

  bool BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  viz::ContextCacheController* CacheController() override;
  void InvalidateGrContext(uint32_t state) override;
  base::Lock* GetLock() override;
  const gpu::Capabilities& ContextCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;
  void SetSupportTextureNorm16(bool support) {
    capabilities_texture_norm16_ = support;
  }

 protected:
  friend class base::RefCountedThreadSafe<TestInProcessContextProvider>;
  ~TestInProcessContextProvider() override;

 private:
  viz::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  TestImageFactory image_factory_;
  std::unique_ptr<gpu::GLInProcessContext> context_;
  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;
  std::unique_ptr<viz::ContextCacheController> cache_controller_;
  base::Lock context_lock_;
  bool capabilities_texture_norm16_ = false;
  gpu::Capabilities capabilities_;
  gpu::GpuFeatureInfo gpu_feature_info_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_
