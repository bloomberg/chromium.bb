// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/blimp/chrome_compositor_dependencies.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "cc/output/context_provider.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/ipc/client/gpu_channel_host.h"

namespace {

bool IsValidWorkerContext(cc::ContextProvider* context_provider) {
  cc::ContextProvider::ScopedContextLock lock(context_provider);
  return context_provider->ContextGL()->GetGraphicsResetStatusKHR() ==
         GL_NO_ERROR;
}

gpu::gles2::ContextCreationAttribHelper
GetOffscreenContextCreationAttributes() {
  gpu::gles2::ContextCreationAttribHelper attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  return attributes;
}

bool IsAsyncWorkerContextEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableGpuAsyncWorkerContext))
    return false;
  else if (command_line.HasSwitch(switches::kEnableGpuAsyncWorkerContext))
    return true;

  return false;
}

}  // namespace

ChromeCompositorDependencies::ChromeCompositorDependencies(
    ui::ContextProviderFactory* context_provider_factory)
    : context_provider_factory_(context_provider_factory), weak_factory_(this) {
  DCHECK(context_provider_factory_);
}

ChromeCompositorDependencies::~ChromeCompositorDependencies() = default;

gpu::GpuMemoryBufferManager*
ChromeCompositorDependencies::GetGpuMemoryBufferManager() {
  return context_provider_factory_->GetGpuMemoryBufferManager();
}

cc::SurfaceManager* ChromeCompositorDependencies::GetSurfaceManager() {
  return context_provider_factory_->GetSurfaceManager();
}

cc::FrameSinkId ChromeCompositorDependencies::AllocateFrameSinkId() {
  return context_provider_factory_->AllocateFrameSinkId();
}

void ChromeCompositorDependencies::GetContextProviders(
    const ContextProviderCallback& callback) {
  context_provider_factory_->RequestGpuChannelHost(
      base::Bind(&ChromeCompositorDependencies::OnGpuChannelEstablished,
                 weak_factory_.GetWeakPtr(), callback));
}

void ChromeCompositorDependencies::OnGpuChannelEstablished(
    const ContextProviderCallback& callback,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    ui::ContextProviderFactory::GpuChannelHostResult result) {
  using GpuRequestResult = ui::ContextProviderFactory::GpuChannelHostResult;

  switch (result) {
    case GpuRequestResult::FAILURE_GPU_PROCESS_INITIALIZATION_FAILED:
      // Pass the null result to the caller and let them try again.
      break;
    case GpuRequestResult::FAILURE_FACTORY_SHUTDOWN:
      // If the factory is shutting down, early out and drop the requests.
      return;
    case GpuRequestResult::SUCCESS:
      // Create the worker context first if necessary.
      if (!shared_main_thread_worker_context_ ||
          !IsValidWorkerContext(shared_main_thread_worker_context_.get())) {
        shared_main_thread_worker_context_ =
            context_provider_factory_->CreateOffscreenContextProvider(
                ui::ContextProviderFactory::ContextType::
                    BLIMP_RENDER_WORKER_CONTEXT,
                gpu::SharedMemoryLimits(),
                GetOffscreenContextCreationAttributes(),
                true /* support_locking */, false /* automatic_flushes */,
                nullptr, gpu_channel_host);
        if (!shared_main_thread_worker_context_->BindToCurrentThread()) {
          shared_main_thread_worker_context_ = nullptr;
        }
      }
      break;
  }

  scoped_refptr<cc::ContextProvider> compositor_context_provider;
  if (gpu_channel_host && shared_main_thread_worker_context_) {
    // The compositor context shares resources with the worker context unless
    // the worker is async.
    cc::ContextProvider* shared_context =
        IsAsyncWorkerContextEnabled()
            ? nullptr
            : shared_main_thread_worker_context_.get();

    compositor_context_provider =
        context_provider_factory_->CreateOffscreenContextProvider(
            ui::ContextProviderFactory::ContextType::
                BLIMP_RENDER_COMPOSITOR_CONTEXT,
            gpu::SharedMemoryLimits::ForMailboxContext(),
            GetOffscreenContextCreationAttributes(),
            false /* support_locking */, false /* automatic_flushes */,
            shared_context, std::move(gpu_channel_host));
  }

  callback.Run(compositor_context_provider, shared_main_thread_worker_context_);
}
