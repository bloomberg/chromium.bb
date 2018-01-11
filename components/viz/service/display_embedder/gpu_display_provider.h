// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GPU_DISPLAY_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GPU_DISPLAY_PROVIDER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/display_embedder/display_provider.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/in_process_command_buffer.h"

namespace gpu {
class GpuChannelManager;
class GpuChannelManagerDelegate;
class ImageFactory;
}  // namespace gpu

namespace viz {
class CompositingModeReporterImpl;
class Display;
class ExternalBeginFrameControllerImpl;
class OutputDeviceBacking;
class SoftwareOutputDevice;

// In-process implementation of DisplayProvider.
class VIZ_SERVICE_EXPORT GpuDisplayProvider : public DisplayProvider {
 public:
  GpuDisplayProvider(
      uint32_t restart_id,
      scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_service,
      gpu::GpuChannelManager* gpu_channel_manager,
      CompositingModeReporterImpl* compositing_mode_reporter);
  ~GpuDisplayProvider() override;

  // DisplayProvider implementation.
  std::unique_ptr<Display> CreateDisplay(
      const FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      bool force_software_compositing,
      ExternalBeginFrameControllerImpl* external_begin_frame_controller,
      const RendererSettings& renderer_settings,
      std::unique_ptr<SyntheticBeginFrameSource>* out_begin_frame_source)
      override;

 private:
  std::unique_ptr<SoftwareOutputDevice> CreateSoftwareOutputDeviceForPlatform(
      gfx::AcceleratedWidget widget);

  const uint32_t restart_id_;
  scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_service_;
  gpu::GpuChannelManagerDelegate* const gpu_channel_manager_delegate_;
  std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_;
  gpu::ImageFactory* const image_factory_;
  CompositingModeReporterImpl* const compositing_mode_reporter_;

#if defined(OS_WIN)
  // Used for software compositing output on Windows.
  std::unique_ptr<OutputDeviceBacking> output_device_backing_;
#endif

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(GpuDisplayProvider);
};

}  // namespace viz

#endif  //  COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GPU_DISPLAY_PROVIDER_H_
