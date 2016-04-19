// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/software_browser_compositor_output_surface.h"

#include <utility>

#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/vsync_provider.h"

namespace content {

SoftwareBrowserCompositorOutputSurface::SoftwareBrowserCompositorOutputSurface(
    std::unique_ptr<cc::SoftwareOutputDevice> software_device,
    const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
    base::SingleThreadTaskRunner* task_runner)
    : BrowserCompositorOutputSurface(std::move(software_device),
                                     vsync_manager,
                                     task_runner),
      weak_factory_(this) {}

SoftwareBrowserCompositorOutputSurface::
    ~SoftwareBrowserCompositorOutputSurface() {
}

void SoftwareBrowserCompositorOutputSurface::SwapBuffers(
    cc::CompositorFrame* frame) {
  base::TimeTicks swap_time = base::TimeTicks::Now();
  for (auto& latency : frame->metadata.latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0, 0, swap_time, 1);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0,
        swap_time, 1);
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&RenderWidgetHostImpl::CompositorFrameDrawn,
                            frame->metadata.latency_info));

  gfx::VSyncProvider* vsync_provider = software_device()->GetVSyncProvider();
  if (vsync_provider) {
    vsync_provider->GetVSyncParameters(base::Bind(
        &BrowserCompositorOutputSurface::OnUpdateVSyncParametersFromGpu,
        weak_factory_.GetWeakPtr()));
  }
  PostSwapBuffersComplete();
  client_->DidSwapBuffers();
}

void SoftwareBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted(
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::SwapResult result) {
  NOTREACHED();
}

#if defined(OS_MACOSX)
void SoftwareBrowserCompositorOutputSurface::SetSurfaceSuspendedForRecycle(
    bool suspended) {
}

bool SoftwareBrowserCompositorOutputSurface::
    SurfaceShouldNotShowFramesAfterSuspendForRecycle() const {
  return false;
}
#endif

}  // namespace content
