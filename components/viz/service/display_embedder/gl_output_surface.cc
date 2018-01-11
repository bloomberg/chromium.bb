// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface.h"

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
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "ui/gl/gl_utils.h"

namespace viz {

GLOutputSurface::GLOutputSurface(
    scoped_refptr<InProcessContextProvider> context_provider,
    SyntheticBeginFrameSource* synthetic_begin_frame_source)
    : OutputSurface(context_provider),
      synthetic_begin_frame_source_(synthetic_begin_frame_source),
      latency_tracker_(true),
      latency_info_cache_(this),
      weak_ptr_factory_(this) {
  capabilities_.flipped_output_surface =
      context_provider->ContextCapabilities().flips_vertically;
  capabilities_.supports_stencil =
      context_provider->ContextCapabilities().num_stencil_bits > 0;
  context_provider->SetSwapBuffersCompletionCallback(
      base::BindRepeating(&GLOutputSurface::OnGpuSwapBuffersCompleted,
                          weak_ptr_factory_.GetWeakPtr()));
  context_provider->SetUpdateVSyncParametersCallback(
      base::BindRepeating(&GLOutputSurface::OnVSyncParametersUpdated,
                          weak_ptr_factory_.GetWeakPtr()));
  context_provider->SetPresentationCallback(base::BindRepeating(
      &GLOutputSurface::OnPresentation, weak_ptr_factory_.GetWeakPtr()));
}

GLOutputSurface::~GLOutputSurface() {}

void GLOutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void GLOutputSurface::EnsureBackbuffer() {}

void GLOutputSurface::DiscardBackbuffer() {
  context_provider()->ContextGL()->DiscardBackbufferCHROMIUM();
}

void GLOutputSurface::BindFramebuffer() {
  context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLOutputSurface::SetDrawRectangle(const gfx::Rect& rect) {
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

void GLOutputSurface::Reshape(const gfx::Size& size,
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

void GLOutputSurface::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK(context_provider_);

  if (latency_info_cache_.WillSwap(std::move(frame.latency_info)))
    context_provider_->ContextSupport()->SetSnapshotRequested();

  set_draw_rectangle_for_frame_ = false;
  if (frame.sub_buffer_rect) {
    context_provider_->ContextSupport()->PartialSwapBuffers(
        *frame.sub_buffer_rect);
  } else {
    context_provider_->ContextSupport()->Swap();
  }
}

uint32_t GLOutputSurface::GetFramebufferCopyTextureFormat() {
  // TODO(danakj): What attributes are used for the default framebuffer here?
  // Can it have alpha? InProcessContextProvider doesn't take any
  // attributes.
  return GL_RGB;
}

OverlayCandidateValidator* GLOutputSurface::GetOverlayCandidateValidator()
    const {
  return nullptr;
}

bool GLOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned GLOutputSurface::GetOverlayTextureId() const {
  return 0;
}

gfx::BufferFormat GLOutputSurface::GetOverlayBufferFormat() const {
  return gfx::BufferFormat::RGBX_8888;
}

bool GLOutputSurface::SurfaceIsSuspendForRecycle() const {
  return false;
}

bool GLOutputSurface::HasExternalStencilTest() const {
  return false;
}

void GLOutputSurface::ApplyExternalStencil() {}

void GLOutputSurface::DidReceiveSwapBuffersAck(gfx::SwapResult result,
                                               uint64_t swap_id) {
  client_->DidReceiveSwapBuffersAck(swap_id);
}

void GLOutputSurface::OnGpuSwapBuffersCompleted(
    const gpu::SwapBuffersCompleteParams& params) {
  if (!params.texture_in_use_responses.empty())
    client_->DidReceiveTextureInUseResponses(params.texture_in_use_responses);
  if (!params.ca_layer_params.is_empty)
    client_->DidReceiveCALayerParams(params.ca_layer_params);
  DidReceiveSwapBuffersAck(params.swap_response.result,
                           params.swap_response.swap_id);
  latency_info_cache_.OnSwapBuffersCompleted(params.swap_response);
}

void GLOutputSurface::LatencyInfoCompleted(
    const std::vector<ui::LatencyInfo>& latency_info) {
  for (const auto& latency : latency_info) {
    latency_tracker_.OnGpuSwapBuffersCompleted(latency);
  }
}

void GLOutputSurface::OnVSyncParametersUpdated(base::TimeTicks timebase,
                                               base::TimeDelta interval) {
  if (synthetic_begin_frame_source_) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    synthetic_begin_frame_source_->OnUpdateVSyncParameters(
        timebase,
        interval.is_zero() ? BeginFrameArgs::DefaultInterval() : interval);
  }
}

void GLOutputSurface::OnPresentation(
    uint64_t swap_id,
    const gfx::PresentationFeedback& feedback) {
  client_->DidReceivePresentationFeedback(swap_id, feedback);
}

#if BUILDFLAG(ENABLE_VULKAN)
gpu::VulkanSurface* GLOutputSurface::GetVulkanSurface() {
  NOTIMPLEMENTED();
  return nullptr;
}
#endif

}  // namespace viz
