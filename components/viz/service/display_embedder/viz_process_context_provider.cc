// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/viz_process_context_provider.h"

#include <stdint.h>

#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/context_lost_reason.h"
#include "components/viz/common/resources/platform_color.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/raster_implementation_gles.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/skia_utils.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
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

gpu::ContextCreationAttribs CreateAttributes(bool requires_alpha_channel) {
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = requires_alpha_channel ? 8 : -1;
  attributes.depth_size = 0;
#if defined(OS_CHROMEOS)
  // Chrome OS uses surfaceless when running on a real device and stencil
  // buffers can then be added dynamically so supporting them does not have an
  // impact on normal usage. If we are not running on a real Chrome OS device
  // but instead on a workstation for development, then stencil support is
  // useful as it allows the overdraw feedback debugging feature to be used.
  attributes.stencil_size = 8;
#else
  attributes.stencil_size = 0;
#endif
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.fail_if_major_perf_caveat = false;
  attributes.lose_context_when_out_of_memory = true;

#if defined(OS_ANDROID)
  // TODO(cblume): We should add wide gamut code here, setting
  // attributes.color_space.

  if (requires_alpha_channel) {
    attributes.alpha_size = 8;
  } else if (base::SysInfo::AmountOfPhysicalMemoryMB() <= 512) {
    // See compositor_impl_android.cc for more information about this.
    // It is inside GetCompositorContextAttributes().
    attributes.alpha_size = 0;
    attributes.red_size = 5;
    attributes.green_size = 6;
    attributes.blue_size = 5;
  }

  attributes.enable_swap_timestamps_if_supported = true;
#endif  // defined(OS_ANDROID)

  return attributes;
}

void UmaRecordContextLost(ContextLostReason reason) {
  UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.DisplayCompositor", reason);
}

}  // namespace

VizProcessContextProvider::VizProcessContextProvider(
    scoped_refptr<gpu::CommandBufferTaskExecutor> task_executor,
    gpu::SurfaceHandle surface_handle,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gpu::ImageFactory* image_factory,
    gpu::GpuChannelManagerDelegate* gpu_channel_manager_delegate,
    const gpu::SharedMemoryLimits& limits,
    bool requires_alpha_channel)
    : attributes_(CreateAttributes(requires_alpha_channel)),
      context_(std::make_unique<gpu::GLInProcessContext>()),
      context_result_(
          context_->Initialize(std::move(task_executor),
                               nullptr,
                               (surface_handle == gpu::kNullSurfaceHandle),
                               surface_handle,
                               attributes_,
                               limits,
                               gpu_memory_buffer_manager,
                               image_factory,
                               gpu_channel_manager_delegate,
                               base::ThreadTaskRunnerHandle::Get())) {
  if (context_result_ == gpu::ContextResult::kSuccess) {
    auto* gles2_implementation = context_->GetImplementation();
    cache_controller_ = std::make_unique<ContextCacheController>(
        gles2_implementation, base::ThreadTaskRunnerHandle::Get());
    // |context_| is owned here so bind an unretained pointer or there will be a
    // circular reference preventing destruction.
    gles2_implementation->SetLostContextCallback(base::BindOnce(
        &VizProcessContextProvider::OnContextLost, base::Unretained(this)));
  } else {
    // Context initialization failed. Record UMA and cleanup.
    UmaRecordContextLost(CONTEXT_INIT_FAILED);
    context_.reset();
  }
}

VizProcessContextProvider::~VizProcessContextProvider() = default;

void VizProcessContextProvider::AddRef() const {
  base::RefCountedThreadSafe<VizProcessContextProvider>::AddRef();
}

void VizProcessContextProvider::Release() const {
  base::RefCountedThreadSafe<VizProcessContextProvider>::Release();
}

gpu::ContextResult VizProcessContextProvider::BindToCurrentThread() {
  return context_result_;
}

gpu::gles2::GLES2Interface* VizProcessContextProvider::ContextGL() {
  return context_->GetImplementation();
}

gpu::ContextSupport* VizProcessContextProvider::ContextSupport() {
  return context_->GetImplementation();
}

class GrContext* VizProcessContextProvider::GrContext() {
  if (gr_context_)
    return gr_context_->get();

  size_t max_resource_cache_bytes;
  size_t max_glyph_cache_texture_bytes;
  gpu::raster::DetermineGrCacheLimitsFromAvailableMemory(
      &max_resource_cache_bytes, &max_glyph_cache_texture_bytes);

  gr_context_ = std::make_unique<skia_bindings::GrContextForGLES2Interface>(
      ContextGL(), ContextSupport(), ContextCapabilities(),
      max_resource_cache_bytes, max_glyph_cache_texture_bytes);
  return gr_context_->get();
}

ContextCacheController* VizProcessContextProvider::CacheController() {
  return cache_controller_.get();
}

base::Lock* VizProcessContextProvider::GetLock() {
  return &context_lock_;
}

const gpu::Capabilities& VizProcessContextProvider::ContextCapabilities()
    const {
  return context_->GetCapabilities();
}

const gpu::GpuFeatureInfo& VizProcessContextProvider::GetGpuFeatureInfo()
    const {
  return context_->GetGpuFeatureInfo();
}

void VizProcessContextProvider::AddObserver(ContextLostObserver* obs) {
  observers_.AddObserver(obs);
}

void VizProcessContextProvider::RemoveObserver(ContextLostObserver* obs) {
  observers_.RemoveObserver(obs);
}

void VizProcessContextProvider::SetUpdateVSyncParametersCallback(
    const gpu::InProcessCommandBuffer::UpdateVSyncParametersCallback&
        callback) {
  context_->SetUpdateVSyncParametersCallback(callback);
}

void VizProcessContextProvider::OnContextLost() {
  for (auto& observer : observers_)
    observer.OnContextLost();
  if (gr_context_)
    gr_context_->OnLostContext();

  gpu::CommandBuffer::State state =
      context_->GetCommandBuffer()->GetLastState();
  UmaRecordContextLost(
      GetContextLostReason(state.error, state.context_lost_reason));
}

}  // namespace viz
