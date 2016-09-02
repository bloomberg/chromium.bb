// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/context_provider_factory_impl_android.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "cc/output/context_provider.h"
#include "cc/output/vulkan_in_process_context_provider.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/client/gpu_channel_host.h"

namespace content {

namespace {

command_buffer_metrics::ContextType ToCommandBufferContextType(
    ui::ContextProviderFactory::ContextType context_type) {
  switch (context_type) {
    case ui::ContextProviderFactory::ContextType::
        BLIMP_RENDER_COMPOSITOR_CONTEXT:
      return command_buffer_metrics::BLIMP_RENDER_COMPOSITOR_CONTEXT;
    case ui::ContextProviderFactory::ContextType::BLIMP_RENDER_WORKER_CONTEXT:
      return command_buffer_metrics::BLIMP_RENDER_WORKER_CONTEXT;
  }
  NOTREACHED();
  return command_buffer_metrics::CONTEXT_TYPE_UNKNOWN;
}

}  // namespace

// static
ContextProviderFactoryImpl* ContextProviderFactoryImpl::GetInstance() {
  return base::Singleton<ContextProviderFactoryImpl>::get();
}

ContextProviderFactoryImpl::ContextProviderFactoryImpl()
    : in_handle_pending_requests_(false),
      surface_client_id_(0),
      weak_factory_(this) {}

ContextProviderFactoryImpl::~ContextProviderFactoryImpl() {}

ContextProviderFactoryImpl::ContextProvidersRequest::ContextProvidersRequest()
    : context_type(command_buffer_metrics::CONTEXT_TYPE_UNKNOWN),
      surface_handle(gpu::kNullSurfaceHandle),
      support_locking(false),
      automatic_flushes(false),
      shared_context_provider(nullptr) {}

ContextProviderFactoryImpl::ContextProvidersRequest::ContextProvidersRequest(
    const ContextProvidersRequest& other) = default;

ContextProviderFactoryImpl::ContextProvidersRequest::
    ~ContextProvidersRequest() = default;

scoped_refptr<cc::VulkanContextProvider>
ContextProviderFactoryImpl::GetSharedVulkanContextProvider() {
  if (!shared_vulkan_context_provider_)
    shared_vulkan_context_provider_ =
        cc::VulkanInProcessContextProvider::Create();

  return shared_vulkan_context_provider_.get();
}

void ContextProviderFactoryImpl::CreateDisplayContextProvider(
    gpu::SurfaceHandle surface_handle,
    gpu::SharedMemoryLimits shared_memory_limits,
    gpu::gles2::ContextCreationAttribHelper attributes,
    bool support_locking,
    bool automatic_flushes,
    ContextProviderCallback result_callback) {
  DCHECK(surface_handle != gpu::kNullSurfaceHandle);
  CreateContextProviderInternal(
      command_buffer_metrics::DISPLAY_COMPOSITOR_ONSCREEN_CONTEXT,
      surface_handle, shared_memory_limits, attributes, support_locking,
      automatic_flushes, nullptr, result_callback);
}

void ContextProviderFactoryImpl::CreateOffscreenContextProvider(
    ContextType context_type,
    gpu::SharedMemoryLimits shared_memory_limits,
    gpu::gles2::ContextCreationAttribHelper attributes,
    bool support_locking,
    bool automatic_flushes,
    cc::ContextProvider* shared_context_provider,
    ContextProviderCallback result_callback) {
  CreateContextProviderInternal(ToCommandBufferContextType(context_type),
                                gpu::kNullSurfaceHandle, shared_memory_limits,
                                attributes, support_locking, automatic_flushes,
                                shared_context_provider, result_callback);
}

cc::SurfaceManager* ContextProviderFactoryImpl::GetSurfaceManager() {
  if (!surface_manager_)
    surface_manager_ = base::WrapUnique(new cc::SurfaceManager);

  return surface_manager_.get();
}

uint32_t ContextProviderFactoryImpl::AllocateSurfaceClientId() {
  return ++surface_client_id_;
}

cc::SharedBitmapManager* ContextProviderFactoryImpl::GetSharedBitmapManager() {
  return HostSharedBitmapManager::current();
}

gpu::GpuMemoryBufferManager*
ContextProviderFactoryImpl::GetGpuMemoryBufferManager() {
  return BrowserGpuMemoryBufferManager::current();
}

void ContextProviderFactoryImpl::CreateContextProviderInternal(
    command_buffer_metrics::ContextType context_type,
    gpu::SurfaceHandle surface_handle,
    gpu::SharedMemoryLimits shared_memory_limits,
    gpu::gles2::ContextCreationAttribHelper attributes,
    bool support_locking,
    bool automatic_flushes,
    cc::ContextProvider* shared_context_provider,
    ContextProviderCallback result_callback) {
  DCHECK(!result_callback.is_null());

  ContextProvidersRequest context_request;
  context_request.context_type = context_type;
  context_request.surface_handle = surface_handle;
  context_request.shared_memory_limits = shared_memory_limits;
  context_request.attributes = attributes;
  context_request.support_locking = support_locking;
  context_request.automatic_flushes = automatic_flushes;
  context_request.shared_context_provider = shared_context_provider;
  context_request.result_callback = result_callback;

  context_provider_requests_.push_back(context_request);
  HandlePendingRequests();
}

void ContextProviderFactoryImpl::HandlePendingRequests() {
  DCHECK(!context_provider_requests_.empty())
      << "We don't have any pending requests?";

  // Failure to initialize the context could result in new requests. Handle
  // them after going through the current list.
  if (in_handle_pending_requests_)
    return;

  {
    base::AutoReset<bool> auto_reset_in_handle_requests(
        &in_handle_pending_requests_, true);

    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
        EnsureGpuChannelEstablished());

    // If we don't have a Gpu Channel Host, we will come back here when the Gpu
    // channel is established, since OnGpuChannelEstablished triggers handling
    // of the requests we couldn't process right now.
    if (!gpu_channel_host)
      return;

    std::list<ContextProvidersRequest> context_requests =
        context_provider_requests_;
    context_provider_requests_.clear();

    for (ContextProvidersRequest& context_request : context_requests) {
      scoped_refptr<cc::ContextProvider> context_provider;

      const bool create_onscreen_context =
          context_request.surface_handle != gpu::kNullSurfaceHandle;

      // Is the request for an onscreen context? Make sure the surface is
      // still valid in that case. DO NOT run the callback if we don't have a
      // valid surface.
      if (create_onscreen_context &&
          !GpuSurfaceTracker::GetInstance()->IsValidSurfaceHandle(
              context_request.surface_handle)) {
        continue;
      }

      context_provider = new ContextProviderCommandBuffer(
          gpu_channel_host, gpu::GPU_STREAM_DEFAULT,
          gpu::GpuStreamPriority::NORMAL, context_request.surface_handle,
          GURL(std::string("chrome://gpu/ContextProviderFactoryImpl::") +
               std::string("CompositorContextProvider")),
          context_request.automatic_flushes, context_request.support_locking,
          context_request.shared_memory_limits, context_request.attributes,
          static_cast<ContextProviderCommandBuffer*>(
              context_request.shared_context_provider),
          context_request.context_type);
      context_request.result_callback.Run(context_provider);
    }
  }

  if (!context_provider_requests_.empty())
    HandlePendingRequests();
}

gpu::GpuChannelHost* ContextProviderFactoryImpl::EnsureGpuChannelEstablished() {
#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(SYZYASAN) || defined(CYGPROFILE_INSTRUMENTATION)
  const int64_t kGpuChannelTimeoutInSeconds = 40;
#else
  const int64_t kGpuChannelTimeoutInSeconds = 10;
#endif

  BrowserGpuChannelHostFactory* factory =
      BrowserGpuChannelHostFactory::instance();

  if (factory->GetGpuChannel())
    return factory->GetGpuChannel();

  factory->EstablishGpuChannel(
      base::Bind(&ContextProviderFactoryImpl::OnGpuChannelEstablished,
                 weak_factory_.GetWeakPtr()));
  establish_gpu_channel_timeout_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kGpuChannelTimeoutInSeconds),
      this, &ContextProviderFactoryImpl::OnGpuChannelTimeout);

  return nullptr;
}

void ContextProviderFactoryImpl::OnGpuChannelEstablished(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel) {
  establish_gpu_channel_timeout_.Stop();

  // This should happen only during shutdown. So early out instead of queuing
  // more requests with the factory.
  if (!gpu_channel)
    return;

  // We can queue the Gpu Channel initialization requests multiple times as
  // we get context requests. So we might have already handled any pending
  // requests when this callback runs.
  if (!context_provider_requests_.empty())
    HandlePendingRequests();
}

void ContextProviderFactoryImpl::OnGpuChannelTimeout() {
  LOG(FATAL) << "Timed out waiting for GPU channel.";
}

}  // namespace content
