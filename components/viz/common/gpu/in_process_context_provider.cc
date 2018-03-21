// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/in_process_context_provider.h"

#include <stdint.h>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/resources/platform_color.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/raster_implementation_gles.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace viz {

namespace {

gpu::ContextCreationAttribs CreateAttributes() {
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 8;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.fail_if_major_perf_caveat = false;
  attributes.bind_generates_resource = false;
  return attributes;
}

}  // namespace

InProcessContextProvider::InProcessContextProvider(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service,
    gpu::SurfaceHandle surface_handle,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gpu::ImageFactory* image_factory,
    gpu::GpuChannelManagerDelegate* gpu_channel_manager_delegate,
    const gpu::SharedMemoryLimits& limits,
    InProcessContextProvider* shared_context)
    : attributes_(CreateAttributes()),
      context_(gpu::GLInProcessContext::CreateWithoutInit()),
      context_result_(context_->Initialize(
          std::move(service),
          nullptr,
          (surface_handle == gpu::kNullSurfaceHandle),
          surface_handle,
          (shared_context ? shared_context->context_.get() : nullptr),
          attributes_,
          limits,
          gpu_memory_buffer_manager,
          image_factory,
          gpu_channel_manager_delegate,
          base::ThreadTaskRunnerHandle::Get())),
      cache_controller_(std::make_unique<ContextCacheController>(
          context_->GetImplementation(),
          base::ThreadTaskRunnerHandle::Get())) {}

InProcessContextProvider::~InProcessContextProvider() = default;

void InProcessContextProvider::AddRef() const {
  base::RefCountedThreadSafe<InProcessContextProvider>::AddRef();
}

void InProcessContextProvider::Release() const {
  base::RefCountedThreadSafe<InProcessContextProvider>::Release();
}

gpu::ContextResult InProcessContextProvider::BindToCurrentThread() {
  return context_result_;
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

  size_t max_resource_cache_bytes;
  size_t max_glyph_cache_texture_bytes;
  skia_bindings::GrContextForGLES2Interface::
      DetermineCacheLimitsFromAvailableMemory(&max_resource_cache_bytes,
                                              &max_glyph_cache_texture_bytes);

  gr_context_.reset(new skia_bindings::GrContextForGLES2Interface(
      ContextGL(), ContextCapabilities(), max_resource_cache_bytes,
      max_glyph_cache_texture_bytes));
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

const gpu::Capabilities& InProcessContextProvider::ContextCapabilities() const {
  return context_->GetCapabilities();
}

const gpu::GpuFeatureInfo& InProcessContextProvider::GetGpuFeatureInfo() const {
  return context_->GetGpuFeatureInfo();
}

void InProcessContextProvider::AddObserver(ContextLostObserver* obs) {}

void InProcessContextProvider::RemoveObserver(ContextLostObserver* obs) {}

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

void InProcessContextProvider::SetPresentationCallback(
    const gpu::InProcessCommandBuffer::PresentationCallback& callback) {
  context_->SetPresentationCallback(callback);
}
}  // namespace viz
