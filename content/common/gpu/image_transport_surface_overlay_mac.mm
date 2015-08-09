// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface_overlay_mac.h"

#include <OpenGL/gl.h>

#include "content/common/gpu/gpu_messages.h"
#include "ui/accelerated_widget_mac/surface_handle_types.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gl/gl_image_io_surface.h"

namespace content {

ImageTransportSurfaceOverlayMac::ImageTransportSurfaceOverlayMac(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    gfx::PluginWindowHandle handle)
    : scale_factor_(1), pending_overlay_image_(nullptr) {
  helper_.reset(new ImageTransportHelper(this, manager, stub, handle));
}

ImageTransportSurfaceOverlayMac::~ImageTransportSurfaceOverlayMac() {
  gfx::GLImageIOSurface::SetLayerForWidget(widget_, nil);
}

bool ImageTransportSurfaceOverlayMac::Initialize() {
  if (!helper_->Initialize())
    return false;

  // Create the CAContext to send this to the GPU process, and the layer for
  // the context.
  CGSConnectionID connection_id = CGSMainConnectionID();
  ca_context_.reset(
      [[CAContext contextWithCGSConnection:connection_id options:@{}] retain]);
  layer_.reset([[CALayer alloc] init]);
  [ca_context_ setLayer:layer_];

  // Register the CALayer so that it can be picked up in GLImageIOSurface.
  static intptr_t previous_widget = 0;
  previous_widget += 1;
  widget_ = reinterpret_cast<gfx::AcceleratedWidget>(previous_widget);
  gfx::GLImageIOSurface::SetLayerForWidget(widget_, layer_);

  return true;
}

void ImageTransportSurfaceOverlayMac::Destroy() {}

bool ImageTransportSurfaceOverlayMac::IsOffscreen() {
  return false;
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::SwapBuffers() {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::SwapBuffers");

  // A flush is required to ensure that all content appears in the layer.
  {
    TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::glFlush");
    glFlush();
  }

  // There should exist only one overlay image, and it should cover the whole
  // surface.
  DCHECK(pending_overlay_image_);
  if (pending_overlay_image_) {
    TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::setContents");
    ScopedCAActionDisabler disabler;
    gfx::Rect dip_bounds = gfx::ConvertRectToDIP(
        scale_factor_, gfx::Rect(pixel_size_));
    gfx::RectF crop_rect(0, 0, 1, 1);
    pending_overlay_image_->ScheduleOverlayPlane(
        widget_, 0, gfx::OVERLAY_TRANSFORM_NONE, dip_bounds, crop_rect);
    pending_overlay_image_ = nullptr;
  }

  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_handle =
      ui::SurfaceHandleFromCAContextID([ca_context_ contextId]);
  params.size = pixel_size_;
  params.scale_factor = scale_factor_;
  params.latency_info.swap(latency_info_);
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);
  return gfx::SwapResult::SWAP_ACK;
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::PostSubBuffer(int x,
                                                               int y,
                                                               int width,
                                                               int height) {
  return SwapBuffers();
}

bool ImageTransportSurfaceOverlayMac::SupportsPostSubBuffer() {
  return true;
}

gfx::Size ImageTransportSurfaceOverlayMac::GetSize() {
  return gfx::Size();
}

void* ImageTransportSurfaceOverlayMac::GetHandle() {
  return nullptr;
}

bool ImageTransportSurfaceOverlayMac::ScheduleOverlayPlane(
    int z_order,
    gfx::OverlayTransform transform,
    gfx::GLImage* image,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect) {
  // For now we allow only the one full-surface overlay plane.
  // TODO(ccameron): This will need to be updated when support for multiple
  // planes is enabled.
  DCHECK_EQ(z_order, 0);
  DCHECK_EQ(bounds_rect.ToString(), gfx::Rect(pixel_size_).ToString());
  DCHECK_EQ(crop_rect.ToString(), gfx::RectF(0, 0, 1, 1).ToString());
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);
  DCHECK(!pending_overlay_image_);
  pending_overlay_image_ = image;
  return true;
}

bool ImageTransportSurfaceOverlayMac::IsSurfaceless() const {
  return true;
}

void ImageTransportSurfaceOverlayMac::OnBufferPresented(
    const AcceleratedSurfaceMsg_BufferPresented_Params& params) {}

void ImageTransportSurfaceOverlayMac::OnResize(gfx::Size pixel_size,
                                               float scale_factor) {
  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;
}

void ImageTransportSurfaceOverlayMac::SetLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  latency_info_.insert(
      latency_info_.end(), latency_info.begin(), latency_info.end());
}

void ImageTransportSurfaceOverlayMac::WakeUpGpu() {}

}  // namespace content
