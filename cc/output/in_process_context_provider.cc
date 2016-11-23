// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/in_process_context_provider.h"

#include <stdint.h>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/resources/platform_color.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/framebuffer_completeness_cache.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_share_group.h"

namespace cc {

namespace {

gpu::gles2::ContextCreationAttribHelper CreateAttributes() {
  gpu::gles2::ContextCreationAttribHelper attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.fail_if_major_perf_caveat = false;
  attributes.bind_generates_resource = false;
  return attributes;
}

std::unique_ptr<gpu::GLInProcessContext> CreateInProcessContext(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service,
    const gpu::gles2::ContextCreationAttribHelper& attributes,
    gpu::SurfaceHandle widget,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gpu::ImageFactory* image_factory,
    const gpu::SharedMemoryLimits& limits,
    gpu::GLInProcessContext* shared_context) {
  const bool is_offscreen = widget == gpu::kNullSurfaceHandle;
  return base::WrapUnique(gpu::GLInProcessContext::Create(
      service, nullptr, is_offscreen, widget, shared_context, attributes,
      limits, gpu_memory_buffer_manager, image_factory,
      base::ThreadTaskRunnerHandle::Get()));
}

}  // namespace

InProcessContextProvider::InProcessContextProvider(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service,
    gpu::SurfaceHandle widget,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gpu::ImageFactory* image_factory,
    const gpu::SharedMemoryLimits& limits,
    InProcessContextProvider* shared_context)
    : attributes_(CreateAttributes()),
      context_(CreateInProcessContext(
          service,
          attributes_,
          widget,
          gpu_memory_buffer_manager,
          image_factory,
          limits,
          (shared_context ? shared_context->context_.get() : nullptr))) {
  cache_controller_.reset(new ContextCacheController(
      context_->GetImplementation(), base::ThreadTaskRunnerHandle::Get()));
}

InProcessContextProvider::~InProcessContextProvider() = default;

bool InProcessContextProvider::BindToCurrentThread() {
  return !!context_;
}

gpu::gles2::GLES2Interface* InProcessContextProvider::ContextGL() {
  return context_->GetImplementation();
}

gpu::ContextSupport* InProcessContextProvider::ContextSupport() {
  return context_->GetImplementation();
}

class GrContext* InProcessContextProvider::GrContext() {
  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(new skia_bindings::GrContextForGLES2Interface(ContextGL()));
  return gr_context_->get();
}

ContextCacheController* InProcessContextProvider::CacheController() {
  return cache_controller_.get();
}

void InProcessContextProvider::InvalidateGrContext(uint32_t state) {
  if (gr_context_)
    gr_context_->ResetContext(state);
}

base::Lock* InProcessContextProvider::GetLock() {
  return &context_lock_;
}

gpu::Capabilities InProcessContextProvider::ContextCapabilities() {
  return context_->GetCapabilities();
}

void InProcessContextProvider::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  // This code lives in the GPU process and so this would go away
  // if the context is lost?
}

uint32_t InProcessContextProvider::GetCopyTextureInternalFormat() {
  if (attributes_.alpha_size > 0)
    return GL_RGBA;
  DCHECK_NE(attributes_.red_size, 0);
  DCHECK_NE(attributes_.green_size, 0);
  DCHECK_NE(attributes_.blue_size, 0);
  return GL_RGB;
}

void InProcessContextProvider::SetSwapBuffersCompletionCallback(
    const gpu::InProcessCommandBuffer::SwapBuffersCompletionCallback&
        callback) {
  context_->SetSwapBuffersCompletionCallback(callback);
}

void InProcessContextProvider::SetUpdateVSyncParametersCallback(
    const gpu::InProcessCommandBuffer::UpdateVSyncParametersCallback&
        callback) {
  context_->SetUpdateVSyncParametersCallback(callback);
}

}  // namespace cc
