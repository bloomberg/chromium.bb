// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_COMPOSITOR_GPU_DISPLAY_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_COMPOSITOR_GPU_DISPLAY_PROVIDER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/surfaces/frame_sink_id.h"
#include "components/viz/service/display_compositor/display_provider.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/in_process_command_buffer.h"

namespace gpu {
class GpuChannelManager;
class ImageFactory;
}

namespace viz {

// In-process implementation of DisplayProvider.
class VIZ_SERVICE_EXPORT GpuDisplayProvider
    : public NON_EXPORTED_BASE(DisplayProvider) {
 public:
  GpuDisplayProvider(
      scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_service,
      gpu::GpuChannelManager* gpu_channel_manager);
  ~GpuDisplayProvider() override;

  // DisplayProvider:
  std::unique_ptr<cc::Display> CreateDisplay(
      const cc::FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      std::unique_ptr<cc::BeginFrameSource>* begin_frame_source) override;

 private:
  scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_service_;
  std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_;
  gpu::ImageFactory* image_factory_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(GpuDisplayProvider);
};

}  // namespace viz

#endif  //  COMPONENTS_VIZ_SERVICE_DISPLAY_COMPOSITOR_GPU_DISPLAY_PROVIDER_H_
