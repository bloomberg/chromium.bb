// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gpu_display_provider.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/switches.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/gpu/in_process_context_provider.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/texture_mailbox_deleter.h"
#include "components/viz/service/display_embedder/display_output_surface.h"
#include "components/viz/service/display_embedder/in_process_gpu_memory_buffer_manager.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ui/base/ui_base_switches.h"

#if defined(USE_OZONE)
#include "components/viz/service/display_embedder/display_output_surface_ozone.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#endif

namespace {

gpu::ImageFactory* GetImageFactory(gpu::GpuChannelManager* channel_manager) {
  auto* buffer_factory = channel_manager->gpu_memory_buffer_factory();
  return buffer_factory ? buffer_factory->AsImageFactory() : nullptr;
}

}  // namespace

namespace viz {

GpuDisplayProvider::GpuDisplayProvider(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_service,
    gpu::GpuChannelManager* gpu_channel_manager)
    : gpu_service_(std::move(gpu_service)),
      gpu_memory_buffer_manager_(
          base::MakeUnique<InProcessGpuMemoryBufferManager>(
              gpu_channel_manager)),
      image_factory_(GetImageFactory(gpu_channel_manager)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

GpuDisplayProvider::~GpuDisplayProvider() = default;

std::unique_ptr<Display> GpuDisplayProvider::CreateDisplay(
    const FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    const RendererSettings& renderer_settings,
    std::unique_ptr<BeginFrameSource>* begin_frame_source) {
  auto synthetic_begin_frame_source =
      base::MakeUnique<DelayBasedBeginFrameSource>(
          base::MakeUnique<DelayBasedTimeSource>(task_runner_.get()));

  scoped_refptr<InProcessContextProvider> context_provider =
      new InProcessContextProvider(gpu_service_, surface_handle,
                                   gpu_memory_buffer_manager_.get(),
                                   image_factory_, gpu::SharedMemoryLimits(),
                                   nullptr /* shared_context */);

  // TODO(rjkroege): If there is something better to do than CHECK, add it.
  // TODO(danakj): Should retry if the result is kTransientFailure.
  auto result = context_provider->BindToCurrentThread();
  CHECK_EQ(result, gpu::ContextResult::kSuccess);

  std::unique_ptr<OutputSurface> display_output_surface;
  if (context_provider->ContextCapabilities().surfaceless) {
#if defined(USE_OZONE)
    display_output_surface = base::MakeUnique<DisplayOutputSurfaceOzone>(
        std::move(context_provider), surface_handle,
        synthetic_begin_frame_source.get(), gpu_memory_buffer_manager_.get(),
        GL_TEXTURE_2D, GL_RGB);
#else
    NOTREACHED();
#endif
  } else {
    display_output_surface = base::MakeUnique<DisplayOutputSurface>(
        std::move(context_provider), synthetic_begin_frame_source.get());
  }

  int max_frames_pending =
      display_output_surface->capabilities().max_frames_pending;
  DCHECK_GT(max_frames_pending, 0);

  auto scheduler = base::MakeUnique<DisplayScheduler>(
      synthetic_begin_frame_source.get(), task_runner_.get(),
      max_frames_pending);

  // The ownership of the BeginFrameSource is transfered to the caller.
  *begin_frame_source = std::move(synthetic_begin_frame_source);

  return base::MakeUnique<Display>(
      ServerSharedBitmapManager::current(), gpu_memory_buffer_manager_.get(),
      renderer_settings, frame_sink_id, std::move(display_output_surface),
      std::move(scheduler),
      base::MakeUnique<TextureMailboxDeleter>(task_runner_.get()));
}

}  // namespace viz
