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
#include "components/viz/service/display_embedder/compositing_mode_reporter_impl.h"
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
#include "components/viz/service/display_embedder/software_output_device_win.h"
#endif

#if defined(OS_MACOSX)
#include "components/viz/service/display_embedder/gl_output_surface_mac.h"
#include "components/viz/service/display_embedder/software_output_device_mac.h"
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
    CompositingModeReporterImpl* compositing_mode_reporter)
    : restart_id_(restart_id),
      gpu_service_(std::move(gpu_service)),
      gpu_channel_manager_delegate_(gpu_channel_manager->delegate()),
      gpu_memory_buffer_manager_(
          std::make_unique<InProcessGpuMemoryBufferManager>(
              gpu_channel_manager)),
      image_factory_(GetImageFactory(gpu_channel_manager)),
      compositing_mode_reporter_(compositing_mode_reporter),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK_NE(restart_id_, BeginFrameSource::kNotRestartableId);
}

GpuDisplayProvider::~GpuDisplayProvider() = default;

std::unique_ptr<Display> GpuDisplayProvider::CreateDisplay(
    const FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    bool force_software_compositing,
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

  // TODO(crbug.com/730660): Fallback to software if gpu doesn't work with
  // compositing_mode_reporter_->SetUsingSoftwareCompositing(), and once that
  // is done, always make software-based Displays only.
  bool gpu_compositing = !force_software_compositing;
  (void)compositing_mode_reporter_;

#if !defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
  // TODO(crbug.com/730660): On Mac/Android the handle is not an
  // AcceleratedWidget, and the widget is only available in the browser process
  // via GpuSurfaceTracker (and maybe can't be used in the viz process??)
  NOTIMPLEMENTED();
  gfx::AcceleratedWidget widget = 0;
  (void)widget;
#else
  gfx::AcceleratedWidget widget = surface_handle;
#endif

  std::unique_ptr<OutputSurface> output_surface;

  if (!gpu_compositing) {
    output_surface = std::make_unique<SoftwareOutputSurface>(
        CreateSoftwareOutputDeviceForPlatform(widget), task_runner_);
  } else {
    auto context_provider = base::MakeRefCounted<InProcessContextProvider>(
        gpu_service_, surface_handle, gpu_memory_buffer_manager_.get(),
        image_factory_, gpu_channel_manager_delegate_,
        gpu::SharedMemoryLimits(), nullptr /* shared_context */);

    // TODO(rjkroege): If there is something better to do than CHECK, add it.
    // TODO(danakj): Should retry if the result is kTransientFailure.
    auto result = context_provider->BindToCurrentThread();
    CHECK_EQ(result, gpu::ContextResult::kSuccess);

    if (context_provider->ContextCapabilities().surfaceless) {
#if defined(USE_OZONE)
      output_surface = std::make_unique<GLOutputSurfaceOzone>(
          std::move(context_provider), surface_handle,
          synthetic_begin_frame_source.get(), gpu_memory_buffer_manager_.get(),
          GL_TEXTURE_2D, GL_RGB);
#elif defined(OS_MACOSX)
      output_surface = std::make_unique<GLOutputSurfaceMac>(
          std::move(context_provider), surface_handle,
          synthetic_begin_frame_source.get(), gpu_memory_buffer_manager_.get());
#else
      NOTREACHED();
#endif
    } else {
      output_surface = std::make_unique<GLOutputSurface>(
          std::move(context_provider), synthetic_begin_frame_source.get());
    }
  }

  int max_frames_pending = output_surface->capabilities().max_frames_pending;
  DCHECK_GT(max_frames_pending, 0);

  auto scheduler = std::make_unique<DisplayScheduler>(
      display_begin_frame_source, task_runner_.get(), max_frames_pending);

  // The ownership of the BeginFrameSource is transfered to the caller.
  *out_begin_frame_source = std::move(synthetic_begin_frame_source);

  return std::make_unique<Display>(
      ServerSharedBitmapManager::current(), renderer_settings, frame_sink_id,
      std::move(output_surface), std::move(scheduler), task_runner_);
}

std::unique_ptr<SoftwareOutputDevice>
GpuDisplayProvider::CreateSoftwareOutputDeviceForPlatform(
    gfx::AcceleratedWidget widget) {
#if defined(OS_WIN)
  if (!output_device_backing_)
    output_device_backing_ = std::make_unique<OutputDeviceBacking>();
  auto device = std::make_unique<SoftwareOutputDeviceWin>(
      output_device_backing_.get(), widget);
#elif defined(OS_MACOSX)
  // TODO(crbug.com/730660): We don't have a widget here, so what do we do to
  // get something we can draw to? Can we use an IO surface? Can we use CA
  // layers and overlays like we do for gpu compositing?
  // See https://chromium-review.googlesource.com/c/chromium/src/+/792295 to
  // no longer have GpuSurfaceTracker.
  // Part of the SoftwareOutputDeviceMac::EndPaint probably needs to move to
  // the browser process, and we need to set up transport of an IO surface to
  // here?
  // auto device = std::make_unique<SoftwareOutputDeviceMac>(widget);
  std::unique_ptr<SoftwareOutputDevice> device;
  NOTIMPLEMENTED();
#elif defined(OS_ANDROID)
  // Android does not do software compositing, so we can't get here.
  std::unique_ptr<SoftwareOutputDevice> device;
  NOTREACHED();
#elif defined(USE_OZONE)
  ui::SurfaceFactoryOzone* factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  std::unique_ptr<ui::SurfaceOzoneCanvas> surface_ozone =
      factory->CreateCanvasForWidget(widget);
  CHECK(surface_ozone);
  auto device =
      std::make_unique<SoftwareOutputDeviceOzone>(std::move(surface_ozone));
#elif defined(USE_X11)
  auto device = std::make_unique<SoftwareOutputDeviceX11>(widget);
#endif
  return device;
}

}  // namespace viz
