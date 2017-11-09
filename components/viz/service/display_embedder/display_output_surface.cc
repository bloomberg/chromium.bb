// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/display_output_surface.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gl/gl_utils.h"

namespace viz {

DisplayOutputSurface::DisplayOutputSurface(
    scoped_refptr<InProcessContextProvider> context_provider,
    SyntheticBeginFrameSource* synthetic_begin_frame_source)
    : OutputSurface(context_provider),
      synthetic_begin_frame_source_(synthetic_begin_frame_source),
      latency_tracker_(true),
      weak_ptr_factory_(this) {
  capabilities_.flipped_output_surface =
      context_provider->ContextCapabilities().flips_vertically;
  capabilities_.supports_stencil =
      context_provider->ContextCapabilities().num_stencil_bits > 0;
  context_provider->SetSwapBuffersCompletionCallback(
      base::Bind(&DisplayOutputSurface::OnGpuSwapBuffersCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
  context_provider->SetUpdateVSyncParametersCallback(
      base::Bind(&DisplayOutputSurface::OnVSyncParametersUpdated,
                 weak_ptr_factory_.GetWeakPtr()));
}

DisplayOutputSurface::~DisplayOutputSurface() {}

void DisplayOutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void DisplayOutputSurface::EnsureBackbuffer() {}

void DisplayOutputSurface::DiscardBackbuffer() {
  context_provider()->ContextGL()->DiscardBackbufferCHROMIUM();
}

void DisplayOutputSurface::BindFramebuffer() {
  context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DisplayOutputSurface::SetDrawRectangle(const gfx::Rect& rect) {
  if (set_draw_rectangle_for_frame_)
    return;
  DCHECK(gfx::Rect(size_).Contains(rect));
  DCHECK(has_set_draw_rectangle_since_last_resize_ ||
         (gfx::Rect(size_) == rect));
  set_draw_rectangle_for_frame_ = true;
  has_set_draw_rectangle_since_last_resize_ = true;
  context_provider()->ContextGL()->SetDrawRectangleCHROMIUM(
      rect.x(), rect.y(), rect.width(), rect.height());
}

void DisplayOutputSurface::Reshape(const gfx::Size& size,
                                   float device_scale_factor,
                                   const gfx::ColorSpace& color_space,
                                   bool has_alpha,
                                   bool use_stencil) {
  size_ = size;
  has_set_draw_rectangle_since_last_resize_ = false;
  context_provider()->ContextGL()->ResizeCHROMIUM(
      size.width(), size.height(), device_scale_factor,
      gl::GetGLColorSpace(color_space), has_alpha);
}

void DisplayOutputSurface::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK(context_provider_);

  if (frame.latency_info.size() > 0)
    context_provider_->ContextSupport()->AddLatencyInfo(frame.latency_info);

  set_draw_rectangle_for_frame_ = false;
  if (frame.sub_buffer_rect) {
    context_provider_->ContextSupport()->PartialSwapBuffers(
        *frame.sub_buffer_rect);
  } else {
    context_provider_->ContextSupport()->Swap();
  }
}

uint32_t DisplayOutputSurface::GetFramebufferCopyTextureFormat() {
  // TODO(danakj): What attributes are used for the default framebuffer here?
  // Can it have alpha? InProcessContextProvider doesn't take any
  // attributes.
  return GL_RGB;
}

OverlayCandidateValidator* DisplayOutputSurface::GetOverlayCandidateValidator()
    const {
  return nullptr;
}

bool DisplayOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned DisplayOutputSurface::GetOverlayTextureId() const {
  return 0;
}

gfx::BufferFormat DisplayOutputSurface::GetOverlayBufferFormat() const {
  return gfx::BufferFormat::RGBX_8888;
}

bool DisplayOutputSurface::SurfaceIsSuspendForRecycle() const {
  return false;
}

bool DisplayOutputSurface::HasExternalStencilTest() const {
  return false;
}

void DisplayOutputSurface::ApplyExternalStencil() {}

void DisplayOutputSurface::DidReceiveSwapBuffersAck(gfx::SwapResult result) {
  client_->DidReceiveSwapBuffersAck();
}

void DisplayOutputSurface::OnGpuSwapBuffersCompleted(
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::SwapResult result,
    const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) {
  for (const auto& latency : latency_info) {
    if (latency.latency_components().size() > 0)
      latency_tracker_.OnGpuSwapBuffersCompleted(latency);
  }
  DidReceiveSwapBuffersAck(result);
}

void DisplayOutputSurface::OnVSyncParametersUpdated(base::TimeTicks timebase,
                                                    base::TimeDelta interval) {
  client_->DidUpdateVSyncParameters(timebase, interval);
  // TODO(brianderson): We should not be receiving 0 intervals.
  synthetic_begin_frame_source_->OnUpdateVSyncParameters(
      timebase,
      interval.is_zero() ? BeginFrameArgs::DefaultInterval() : interval);
}

}  // namespace viz
