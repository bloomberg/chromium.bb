// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_surface.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/software_output_device.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/latency/latency_info.h"

namespace viz {

SoftwareOutputSurface::SoftwareOutputSurface(
    std::unique_ptr<SoftwareOutputDevice> software_device,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : OutputSurface(std::move(software_device)),
      task_runner_(std::move(task_runner)),
      weak_factory_(this) {}

SoftwareOutputSurface::~SoftwareOutputSurface() = default;

void SoftwareOutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void SoftwareOutputSurface::EnsureBackbuffer() {
  software_device()->EnsureBackbuffer();
}

void SoftwareOutputSurface::DiscardBackbuffer() {
  software_device()->DiscardBackbuffer();
}

void SoftwareOutputSurface::BindFramebuffer() {
  // Not used for software surfaces.
  NOTREACHED();
}

void SoftwareOutputSurface::SetDrawRectangle(const gfx::Rect& draw_rectangle) {
  NOTREACHED();
}

void SoftwareOutputSurface::Reshape(const gfx::Size& size,
                                    float device_scale_factor,
                                    const gfx::ColorSpace& color_space,
                                    bool has_alpha,
                                    bool use_stencil) {
  software_device()->Resize(size, device_scale_factor);
}

void SoftwareOutputSurface::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK(client_);
  base::TimeTicks swap_time = base::TimeTicks::Now();
  for (auto& latency : frame.latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0, 0, swap_time, 1);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0,
        swap_time, 1);
  }
  // TODO(danakj): Send frame.latency_info somewhere like
  // RenderWidgetHostImpl::OnGpuSwapBuffersCompleted. It should go to the
  // ui::LatencyTracker in the viz process.

  // TODO(danakj): Update vsync params.
  // gfx::VSyncProvider* vsync_provider = software_device()->GetVSyncProvider();
  // if (vsync_provider)
  //  vsync_provider->GetVSyncParameters(update_vsync_parameters_callback_);
  // Update refresh_interval_ as well.

  ++swap_id_;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SoftwareOutputSurface::SwapBuffersCallback,
                                weak_factory_.GetWeakPtr(), swap_id_));
}

bool SoftwareOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

OverlayCandidateValidator* SoftwareOutputSurface::GetOverlayCandidateValidator()
    const {
  // No overlay support in software compositing.
  return nullptr;
}

unsigned SoftwareOutputSurface::GetOverlayTextureId() const {
  return 0;
}

gfx::BufferFormat SoftwareOutputSurface::GetOverlayBufferFormat() const {
  return gfx::BufferFormat::RGBX_8888;
}

bool SoftwareOutputSurface::HasExternalStencilTest() const {
  return false;
}

void SoftwareOutputSurface::ApplyExternalStencil() {}

bool SoftwareOutputSurface::SurfaceIsSuspendForRecycle() const {
  return false;
}

uint32_t SoftwareOutputSurface::GetFramebufferCopyTextureFormat() {
  // Not used for software surfaces.
  NOTREACHED();
  return 0;
}

void SoftwareOutputSurface::SwapBuffersCallback(uint64_t swap_id) {
  client_->DidReceiveSwapBuffersAck(swap_id);
  client_->DidReceivePresentationFeedback(
      swap_id,
      gfx::PresentationFeedback(base::TimeTicks::Now(), refresh_interval_, 0u));
}

#if BUILDFLAG(ENABLE_VULKAN)
gpu::VulkanSurface* SoftwareOutputSurface::GetVulkanSurface() {
  NOTIMPLEMENTED();
  return nullptr;
}
#endif

}  // namespace viz
