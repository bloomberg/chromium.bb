// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_output_surface_mac.h"

#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/accelerated_widget_mac/ca_layer_frame_sink.h"
#include "ui/compositor/compositor.h"

namespace content {

GpuOutputSurfaceMac::GpuOutputSurfaceMac(
    gfx::AcceleratedWidget widget,
    scoped_refptr<ui::ContextProviderCommandBuffer> context,
    gpu::SurfaceHandle surface_handle,
    const UpdateVSyncParametersCallback& update_vsync_parameters_callback,
    std::unique_ptr<viz::CompositorOverlayCandidateValidator>
        overlay_candidate_validator,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager)
    : GpuSurfacelessBrowserCompositorOutputSurface(
          std::move(context),
          surface_handle,
          update_vsync_parameters_callback,
          std::move(overlay_candidate_validator),
          GL_TEXTURE_RECTANGLE_ARB,
          GL_RGBA,
          gfx::BufferFormat::RGBA_8888,
          gpu_memory_buffer_manager),
      widget_(widget) {}

GpuOutputSurfaceMac::~GpuOutputSurfaceMac() {}

void GpuOutputSurfaceMac::SwapBuffers(viz::OutputSurfaceFrame frame) {
  GpuSurfacelessBrowserCompositorOutputSurface::SwapBuffers(std::move(frame));
  if (should_show_frames_state_ ==
      SHOULD_NOT_SHOW_FRAMES_NO_SWAP_AFTER_SUSPENDED) {
    should_show_frames_state_ = SHOULD_SHOW_FRAMES;
    ui::CALayerFrameSink* ca_layer_frame_sink =
        ui::CALayerFrameSink::FromAcceleratedWidget(widget_);
    if (ca_layer_frame_sink)
      ca_layer_frame_sink->SetSuspended(false);
  }
}

void GpuOutputSurfaceMac::SetSurfaceSuspendedForRecycle(bool suspended) {
  if (suspended) {
    // It may be that there are frames in-flight from the GPU process back to
    // the browser. Make sure that these frames are not displayed by ignoring
    // them in GpuProcessHostUIShim, until the browser issues a SwapBuffers for
    // the new content.
    should_show_frames_state_ = SHOULD_NOT_SHOW_FRAMES_SUSPENDED;
    ui::CALayerFrameSink* ca_layer_frame_sink =
        ui::CALayerFrameSink::FromAcceleratedWidget(widget_);
    if (ca_layer_frame_sink)
      ca_layer_frame_sink->SetSuspended(true);
  } else {
    // Discard the backbuffer before drawing the new frame. This is necessary
    // only when using a ImageTransportSurfaceFBO with a
    // CALayerStorageProvider. Discarding the backbuffer results in the next
    // frame using a new CALayer and CAContext, which guarantees that the
    // browser will not flash stale content when adding the remote CALayer to
    // the NSView hierarchy (it could flash stale content because the system
    // window server is not synchronized with any signals we control or
    // observe).
    if (should_show_frames_state_ == SHOULD_NOT_SHOW_FRAMES_SUSPENDED) {
      DiscardBackbuffer();
      should_show_frames_state_ =
          SHOULD_NOT_SHOW_FRAMES_NO_SWAP_AFTER_SUSPENDED;
    }
  }
}

bool GpuOutputSurfaceMac::SurfaceIsSuspendForRecycle() const {
  return should_show_frames_state_ == SHOULD_NOT_SHOW_FRAMES_SUSPENDED;
}

}  // namespace content
