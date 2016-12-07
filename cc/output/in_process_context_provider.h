// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_IN_PROCESS_CONTEXT_PROVIDER_H_
#define CC_OUTPUT_IN_PROCESS_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "cc/base/cc_export.h"
#include "cc/output/context_cache_controller.h"
#include "cc/output/context_provider.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "ui/gfx/native_widget_types.h"

class GrContext;

namespace gpu {
class GLInProcessContext;
class GpuMemoryBufferManager;
class ImageFactory;
struct SharedMemoryLimits;
}

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace cc {

class CC_EXPORT InProcessContextProvider
    : public NON_EXPORTED_BASE(ContextProvider) {
 public:
  InProcessContextProvider(
      scoped_refptr<gpu::InProcessCommandBuffer::Service> service,
      gpu::SurfaceHandle widget,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gpu::ImageFactory* image_factory,
      const gpu::SharedMemoryLimits& limits,
      InProcessContextProvider* shared_context);

  bool BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  ContextCacheController* CacheController() override;
  void InvalidateGrContext(uint32_t state) override;
  base::Lock* GetLock() override;
  gpu::Capabilities ContextCapabilities() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;

  uint32_t GetCopyTextureInternalFormat();

  void SetSwapBuffersCompletionCallback(
      const gpu::InProcessCommandBuffer::SwapBuffersCompletionCallback&
          callback);

  void SetUpdateVSyncParametersCallback(
      const gpu::InProcessCommandBuffer::UpdateVSyncParametersCallback&
          callback);

 protected:
  friend class base::RefCountedThreadSafe<InProcessContextProvider>;
  ~InProcessContextProvider() override;

 private:
  const gpu::gles2::ContextCreationAttribHelper attributes_;

  base::Lock context_lock_;
  std::unique_ptr<gpu::GLInProcessContext> context_;
  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;
  std::unique_ptr<ContextCacheController> cache_controller_;
};

}  // namespace cc

#endif  // CC_OUTPUT_IN_PROCESS_CONTEXT_PROVIDER_H_
