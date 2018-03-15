// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gpu_display_provider.h"

#include <utility>

#include "base/command_line.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/switches.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/gpu/in_process_context_provider.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display_embedder/external_begin_frame_controller_impl.h"
#include "components/viz/service/display_embedder/gl_output_surface.h"
#include "components/viz/service/display_embedder/in_process_gpu_memory_buffer_manager.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/display_embedder/software_output_surface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include "components/viz/service/display_embedder/gl_output_surface_win.h"
#include "components/viz/service/display_embedder/software_output_device_win.h"
#endif

#if defined(OS_MACOSX)
#include "components/viz/service/display_embedder/gl_output_surface_mac.h"
#include "components/viz/service/display_embedder/software_output_device_mac.h"
#include "ui/base/cocoa/remote_layer_api.h"
#endif

#if defined(USE_X11)
#include "components/viz/service/display_embedder/software_output_device_x11.h"
#endif

#if defined(USE_OZONE)
#include "components/viz/service/display_embedder/gl_output_surface_ozone.h"
#include "components/viz/service/display_embedder/software_output_device_ozone.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#endif

namespace {

gpu::ImageFactory* GetImageFactory(gpu::GpuChannelManager* channel_manager) {
  auto* buffer_factory = channel_manager->gpu_memory_buffer_factory();
  return buffer_factory ? buffer_factory->AsImageFactory() : nullptr;
}

}  // namespace

namespace viz {

GpuDisplayProvider::GpuDisplayProvider(
    uint32_t restart_id,
    scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_service,
    gpu::GpuChannelManager* gpu_channel_manager,
    bool headless,
    bool wait_for_all_pipeline_stages_before_draw)
    : restart_id_(restart_id),
      gpu_service_(std::move(gpu_service)),
      gpu_channel_manager_delegate_(gpu_channel_manager->delegate()),
      gpu_memory_buffer_manager_(
          std::make_unique<InProcessGpuMemoryBufferManager>(
              gpu_channel_manager)),
      image_factory_(GetImageFactory(gpu_channel_manager)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      headless_(headless),
      wait_for_all_pipeline_stages_before_draw_(
          wait_for_all_pipeline_stages_before_draw) {
  DCHECK_NE(restart_id_, BeginFrameSource::kNotRestartableId);
}

GpuDisplayProvider::~GpuDisplayProvider() = default;

std::unique_ptr<Display> GpuDisplayProvider::CreateDisplay(
    const FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    bool gpu_compositing,
    ExternalBeginFrameControllerImpl* external_begin_frame_controller,
    const RendererSettings& renderer_settings,
    std::unique_ptr<SyntheticBeginFrameSource>* out_begin_frame_source) {
  BeginFrameSource* display_begin_frame_source = nullptr;
  std::unique_ptr<DelayBasedBeginFrameSource> synthetic_begin_frame_source;
  if (external_begin_frame_controller) {
    display_begin_frame_source =
        external_begin_frame_controller->begin_frame_source();
  } else {
    synthetic_begin_frame_source = std::make_unique<DelayBasedBeginFrameSource>(
        std::make_unique<DelayBasedTimeSource>(task_runner_.get()),
        restart_id_);
    display_begin_frame_source = synthetic_begin_frame_source.get();
  }

  std::unique_ptr<OutputSurface> output_surface;

  if (!gpu_compositing) {
    output_surface = std::make_unique<SoftwareOutputSurface>(
        CreateSoftwareOutputDeviceForPlatform(surface_handle), task_runner_);
  } else {
    scoped_refptr<InProcessContextProvider> context_provider;

    // Retry creating and binding |context_provider| on transient failures.
    gpu::ContextResult context_result = gpu::ContextResult::kTransientFailure;
    while (context_result != gpu::ContextResult::kSuccess) {
      context_provider = base::MakeRefCounted<InProcessContextProvider>(
          gpu_service_, surface_handle, gpu_memory_buffer_manager_.get(),
          image_factory_, gpu_channel_manager_delegate_,
          gpu::SharedMemoryLimits(), nullptr /* shared_context */);
      context_result = context_provider->BindToCurrentThread();

      // TODO(crbug.com/819474): Don't crash here, instead fallback to software
      // compositing for fatal failures.
      CHECK_NE(context_result, gpu::ContextResult::kFatalFailure);
    }

    if (context_provider->ContextCapabilities().surfaceless) {
#if defined(USE_OZONE)
      output_surface = std::make_unique<GLOutputSurfaceOzone>(
          std::move(context_provider), surface_handle,
          synthetic_begin_frame_source.get(), gpu_memory_buffer_manager_.get(),
          GL_TEXTURE_2D, GL_RGB);
#elif defined(OS_MACOSX)
      output_surface = std::make_unique<GLOutputSurfaceMac>(
          std::move(context_provider), surface_handle,
          synthetic_begin_frame_source.get(), gpu_memory_buffer_manager_.get(),
          renderer_settings.allow_overlays);
#else
      NOTREACHED();
#endif
    } else {
#if defined(OS_WIN)
      const auto& capabilities = context_provider->ContextCapabilities();
      const bool use_overlays =
          capabilities.dc_layers && capabilities.use_dc_overlays_for_video;
      output_surface = std::make_unique<GLOutputSurfaceWin>(
          std::move(context_provider), synthetic_begin_frame_source.get(),
          use_overlays);
#else
      output_surface = std::make_unique<GLOutputSurface>(
          std::move(context_provider), synthetic_begin_frame_source.get());
#endif
    }
  }

  int max_frames_pending = output_surface->capabilities().max_frames_pending;
  DCHECK_GT(max_frames_pending, 0);

  auto scheduler = std::make_unique<DisplayScheduler>(
      display_begin_frame_source, task_runner_.get(), max_frames_pending,
      wait_for_all_pipeline_stages_before_draw_);

  // The ownership of the BeginFrameSource is transferred to the caller.
  *out_begin_frame_source = std::move(synthetic_begin_frame_source);

  return std::make_unique<Display>(
      ServerSharedBitmapManager::current(), renderer_settings, frame_sink_id,
      std::move(output_surface), std::move(scheduler), task_runner_);
}

std::unique_ptr<SoftwareOutputDevice>
GpuDisplayProvider::CreateSoftwareOutputDeviceForPlatform(
    gpu::SurfaceHandle surface_handle) {
  if (headless_)
    return std::make_unique<SoftwareOutputDevice>();

#if defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
  gfx::AcceleratedWidget widget = surface_handle;
#endif

#if defined(OS_WIN)
  if (!output_device_backing_)
    output_device_backing_ = std::make_unique<OutputDeviceBacking>();
  return std::make_unique<SoftwareOutputDeviceWin>(output_device_backing_.get(),
                                                   widget);
#elif defined(OS_MACOSX)
  // TODO(crbug.com/730660): What do we do to get something we can draw to? Can
  // we use an IO surface? Can we use CA layers and overlays like we do for gpu
  // compositing? See https://crrev.com/c/792295 to no longer have
  // GpuSurfaceTracker. Part of the SoftwareOutputDeviceMac::EndPaint probably
  // needs to move to the browser process, and we need to set up transport of an
  // IO surface to here?
  NOTIMPLEMENTED();
  (void)widget;
  return nullptr;
#elif defined(OS_ANDROID)
  // Android does not do software compositing, so we can't get here.
  NOTREACHED();
  return nullptr;
#elif defined(USE_OZONE)
  ui::SurfaceFactoryOzone* factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  std::unique_ptr<ui::SurfaceOzoneCanvas> surface_ozone =
      factory->CreateCanvasForWidget(widget);
  CHECK(surface_ozone);
  return std::make_unique<SoftwareOutputDeviceOzone>(std::move(surface_ozone));
#elif defined(USE_X11)
  return std::make_unique<SoftwareOutputDeviceX11>(widget);
#endif
}

}  // namespace viz
