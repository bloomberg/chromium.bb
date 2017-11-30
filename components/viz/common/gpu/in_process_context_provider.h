// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_IN_PROCESS_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_COMMON_GPU_IN_PROCESS_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "ui/gfx/native_widget_types.h"

class GrContext;

namespace gpu {
class GLInProcessContext;
class GpuMemoryBufferManager;
class ImageFactory;
struct SharedMemoryLimits;
}  // namespace gpu

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace viz {

class VIZ_COMMON_EXPORT InProcessContextProvider : public ContextProvider {
 public:
  InProcessContextProvider(
      scoped_refptr<gpu::InProcessCommandBuffer::Service> service,
      gpu::SurfaceHandle widget,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gpu::ImageFactory* image_factory,
      const gpu::SharedMemoryLimits& limits,
      InProcessContextProvider* shared_context);

  gpu::ContextResult BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  ContextCacheController* CacheController() override;
  void InvalidateGrContext(uint32_t state) override;
  base::Lock* GetLock() override;
  const gpu::Capabilities& ContextCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  void AddObserver(ContextLostObserver* obs) override;
  void RemoveObserver(ContextLostObserver* obs) override;

  uint32_t GetCopyTextureInternalFormat();

  void SetSwapBuffersCompletionCallback(
      const gpu::InProcessCommandBuffer::SwapBuffersCompletionCallback&
          callback);

  void SetUpdateVSyncParametersCallback(
      const gpu::InProcessCommandBuffer::UpdateVSyncParametersCallback&
          callback);

  void SetPresentationCallback(
      const gpu::InProcessCommandBuffer::PresentationCallback& callback);

 protected:
  friend class base::RefCountedThreadSafe<InProcessContextProvider>;
  ~InProcessContextProvider() override;

 private:
  const gpu::gles2::ContextCreationAttribHelper attributes_;

  base::Lock context_lock_;
  std::unique_ptr<gpu::GLInProcessContext> context_;
  gpu::ContextResult context_result_;
  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;
  std::unique_ptr<ContextCacheController> cache_controller_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_IN_PROCESS_CONTEXT_PROVIDER_H_
