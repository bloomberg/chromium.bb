// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_surface.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/software_output_device.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/latency/latency_info.h"

namespace viz {

SoftwareOutputSurface::SoftwareOutputSurface(
    std::unique_ptr<SoftwareOutputDevice> software_device,
    SyntheticBeginFrameSource* synthetic_begin_frame_source)
    : OutputSurface(std::move(software_device)),
      synthetic_begin_frame_source_(synthetic_begin_frame_source),
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
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, swap_time, 1);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT, swap_time, 1);
  }

  DCHECK(stored_latency_info_.empty())
      << "A second frame is not expected to "
      << "arrive before the previous latency info is processed.";
  stored_latency_info_ = std::move(frame.latency_info);

  software_device()->OnSwapBuffers(base::BindOnce(
      &SoftwareOutputSurface::SwapBuffersCallback, weak_factory_.GetWeakPtr()));

  gfx::VSyncProvider* vsync_provider = software_device()->GetVSyncProvider();
  if (vsync_provider && synthetic_begin_frame_source_) {
    vsync_provider->GetVSyncParameters(
        base::BindOnce(&SoftwareOutputSurface::UpdateVSyncParametersCallback,
                       weak_factory_.GetWeakPtr()));
  }
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

uint32_t SoftwareOutputSurface::GetFramebufferCopyTextureFormat() {
  // Not used for software surfaces.
  NOTREACHED();
  return 0;
}

void SoftwareOutputSurface::SwapBuffersCallback() {
  latency_tracker_.OnGpuSwapBuffersCompleted(stored_latency_info_);
  client_->DidFinishLatencyInfo(stored_latency_info_);
  std::vector<ui::LatencyInfo>().swap(stored_latency_info_);
  client_->DidReceiveSwapBuffersAck();

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta interval_to_next_refresh =
      now.SnappedToNextTick(refresh_timebase_, refresh_interval_) - now;

  client_->DidReceivePresentationFeedback(
      gfx::PresentationFeedback(now, interval_to_next_refresh, 0u));
}

void SoftwareOutputSurface::UpdateVSyncParametersCallback(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(synthetic_begin_frame_source_);
  refresh_timebase_ = timebase;
  refresh_interval_ = interval;
  synthetic_begin_frame_source_->OnUpdateVSyncParameters(timebase, interval);
}

#if BUILDFLAG(ENABLE_VULKAN)
gpu::VulkanSurface* SoftwareOutputSurface::GetVulkanSurface() {
  NOTIMPLEMENTED();
  return nullptr;
}
#endif

unsigned SoftwareOutputSurface::UpdateGpuFence() {
  return 0;
}

}  // namespace viz
