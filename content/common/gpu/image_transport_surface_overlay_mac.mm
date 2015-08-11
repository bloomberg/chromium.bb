// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface_overlay_mac.h"

#include <IOSurface/IOSurface.h>
#include <OpenGL/GL.h>

// This type consistently causes problem on Mac, and needs to be dealt with
// in a systemic way.
// http://crbug.com/517208
#ifndef GL_OES_EGL_image
typedef void* GLeglImageOES;
#endif

#include "base/mac/scoped_cftyperef.h"
#include "content/common/gpu/gpu_messages.h"
#include "ui/accelerated_widget_mac/surface_handle_types.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_image_io_surface.h"
#include "ui/gl/scoped_api.h"
#include "ui/gl/scoped_cgl.h"

namespace {

// Don't let a frame draw until 5% of the way through the next vsync interval
// after the call to SwapBuffers. This slight offset is to ensure that skew
// doesn't result in the frame being presented to the previous vsync interval.

const double kVSyncIntervalFractionForEarliestDisplay = 0.05;
// Target 50% of the way through the next vsync interval. Empirically, it has
// been determined to be a good target for smooth animation.
const double kVSyncIntervalFractionForDisplayCallback = 0.5;

// If a frame takes more than 1/4th of a second for its fence to finish, just
// pretend that the frame is ready to draw.
const double kMaximumDelayWaitingForFenceInSeconds = 0.25;

void CheckGLErrors(const char* msg) {
  GLenum gl_error;
  while ((gl_error = glGetError()) != GL_NO_ERROR) {
    LOG(ERROR) << "OpenGL error hit " << msg << ": " << gl_error;
  }
}

}  // namespace

@interface CALayer(Private)
-(void)setContentsChanged;
@end

namespace content {

class ImageTransportSurfaceOverlayMac::PendingSwap {
 public:
  PendingSwap() : scale_factor(1) {}
  ~PendingSwap() { DCHECK(!gl_fence); }

  gfx::Size pixel_size;
  float scale_factor;
  std::vector<ui::LatencyInfo> latency_info;

  // If true, the partial damage rect for the frame.
  bool use_partial_damage;
  gfx::Rect pixel_partial_damage_rect;

  // The IOSurface with new content for this swap.
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface;

  // A fence object, and the CGL context it was issued in.
  base::ScopedTypeRef<CGLContextObj> cgl_context;
  scoped_ptr<gfx::GLFence> gl_fence;

  // The earliest time that this frame may be drawn. A frame is not allowed
  // to draw until a fraction of the way through the vsync interval after its
  // This extra latency is to allow wiggle-room for smoothness.
  base::TimeTicks earliest_allowed_draw_time;
  // After this time, always draw the frame, no matter what its fence says. This
  // is to prevent GL bugs from locking the compositor forever.
  base::TimeTicks latest_allowed_draw_time;
};

ImageTransportSurfaceOverlayMac::ImageTransportSurfaceOverlayMac(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    gfx::PluginWindowHandle handle)
    : scale_factor_(1), pending_overlay_image_(nullptr),
      has_pending_callback_(false), weak_factory_(this) {
  helper_.reset(new ImageTransportHelper(this, manager, stub, handle));
}

ImageTransportSurfaceOverlayMac::~ImageTransportSurfaceOverlayMac() {
  Destroy();
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
  [layer_ setGeometryFlipped:YES];
  [layer_ setOpaque:YES];
  [ca_context_ setLayer:layer_];

  partial_damage_layer_.reset([[CALayer alloc] init]);
  [partial_damage_layer_ setOpaque:YES];
  return true;
}

void ImageTransportSurfaceOverlayMac::Destroy() {
  FinishAllPendingSwaps();
}

bool ImageTransportSurfaceOverlayMac::IsOffscreen() {
  return false;
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::SwapBuffersInternal(
    const gfx::Rect& pixel_damage_rect) {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::SwapBuffersInternal");

  // Use the same concept of 'now' for the entire function. The duration of
  // this function only affect the result if this function lasts across a vsync
  // boundary, in which case smooth animation is out the window anyway.
  const base::TimeTicks now = base::TimeTicks::Now();

  // If the previous swap is ready to display, do it before flushing the
  // new swap. It is desirable to always be hitting this path when trying to
  // animate smoothly with vsync.
  if (!pending_swaps_.empty()) {
    if (IsFirstPendingSwapReadyToDisplay(now))
      DisplayFirstPendingSwapImmediately();
  }

  // The remainder of the function will populate the PendingSwap structure and
  // then enqueue it.
  linked_ptr<PendingSwap> new_swap(new PendingSwap);
  new_swap->pixel_size = pixel_size_;
  new_swap->scale_factor = scale_factor_;
  new_swap->latency_info.swap(latency_info_);

  // There should exist only one overlay image, and it should cover the whole
  // surface.
  DCHECK(pending_overlay_image_);
  if (pending_overlay_image_) {
    gfx::GLImageIOSurface* pending_overlay_image_io_surface =
        static_cast<gfx::GLImageIOSurface*>(pending_overlay_image_);
    new_swap->io_surface = pending_overlay_image_io_surface->io_surface();
    pending_overlay_image_ = nullptr;
  }

  // A flush is required to ensure that all content appears in the layer.
  {
    gfx::ScopedSetGLToRealGLApi scoped_set_gl_api;
    TRACE_EVENT1("gpu", "ImageTransportSurfaceOverlayMac::glFlush", "surface",
                 new_swap->io_surface.get());
    CheckGLErrors("before flushing frame");
    new_swap->cgl_context.reset(CGLGetCurrentContext(),
                                base::scoped_policy::RETAIN);
    if (gfx::GLFence::IsSupported())
      new_swap->gl_fence.reset(gfx::GLFence::Create());
    else
      glFlush();
    CheckGLErrors("while flushing frame");
  }

  // Compute the deadlines for drawing this frame.
  if (display_link_mac_) {
    new_swap->earliest_allowed_draw_time =
        display_link_mac_->GetNextVSyncTimeAfter(
            now, kVSyncIntervalFractionForEarliestDisplay);
    new_swap->latest_allowed_draw_time = now +
        base::TimeDelta::FromSecondsD(kMaximumDelayWaitingForFenceInSeconds);
  } else {
    // If we have no display link (because vsync is disabled or because we have
    // not received display parameters yet), immediately attempt to display the
    // surface.
    new_swap->earliest_allowed_draw_time = now;
    new_swap->latest_allowed_draw_time = now;
  }

  // Determine if this will be a full or partial damage, and compute the rects
  // for the damage.
  {
    // Grow the partial damage rect to include the new damage.
    accumulated_partial_damage_pixel_rect_.Union(pixel_damage_rect);
    // Compute the fraction of the full layer that has been damaged. If this
    // fraction is very large (>85%), just damage the full layer, and don't
    // bother with the partial layer.
    const double kMaximumFractionOfFullDamage = 0.85;
    double fraction_of_full_damage =
        accumulated_partial_damage_pixel_rect_.size().GetArea() /
            static_cast<double>(pixel_size_.GetArea());
    // Compute the fraction of the accumulated partial damage rect that has been
    // damaged. If this gets too small (<75%), just re-damage the full window,
    // so we can re-create a smaller partial damage layer next frame.
    const double kMinimumFractionOfPartialDamage = 0.75;
    double fraction_of_partial_damage =
        pixel_damage_rect.size().GetArea() / static_cast<double>(
            accumulated_partial_damage_pixel_rect_.size().GetArea());
    if (fraction_of_full_damage < kMaximumFractionOfFullDamage &&
        fraction_of_partial_damage > kMinimumFractionOfPartialDamage) {
      new_swap->use_partial_damage = true;
      new_swap->pixel_partial_damage_rect =
          accumulated_partial_damage_pixel_rect_;
    } else {
      new_swap->use_partial_damage = false;
      accumulated_partial_damage_pixel_rect_ = gfx::Rect();
    }
  }

  pending_swaps_.push_back(new_swap);
  PostCheckPendingSwapsCallbackIfNeeded(now);
  return gfx::SwapResult::SWAP_ACK;
}

bool ImageTransportSurfaceOverlayMac::IsFirstPendingSwapReadyToDisplay(
    const base::TimeTicks& now) {
  DCHECK(!pending_swaps_.empty());
  linked_ptr<PendingSwap> swap = pending_swaps_.front();

  // If more that a certain amount of time has passed since the swap,
  // unconditionally continue.
  if (now > swap->latest_allowed_draw_time)
    return true;

  // Frames are disallowed from drawing until the vsync interval after their
  // swap is issued.
  if (now < swap->earliest_allowed_draw_time)
    return false;

  // If there is no fence then this is either for immediate display, or the
  // fence was aready successfully checked and deleted.
  if (!swap->gl_fence)
    return true;

  // Check if the pending work has finished (and early-out if it is not, and
  // this is not forced).
  bool has_completed = true;
  if (swap->gl_fence) {
    gfx::ScopedSetGLToRealGLApi scoped_set_gl_api;
    gfx::ScopedCGLSetCurrentContext scoped_set_current(swap->cgl_context);

    CheckGLErrors("before testing fence");
    has_completed= swap->gl_fence->HasCompleted();
    CheckGLErrors("after testing fence");
    if (has_completed) {
      swap->gl_fence.reset();
      CheckGLErrors("while deleting fence");
    }
  }
  return has_completed;
}

void ImageTransportSurfaceOverlayMac::DisplayFirstPendingSwapImmediately() {
  TRACE_EVENT0("gpu",
      "ImageTransportSurfaceOverlayMac::DisplayFirstPendingSwapImmediately");
  DCHECK(!pending_swaps_.empty());
  linked_ptr<PendingSwap> swap = pending_swaps_.front();

  // If there is a fence for this object, delete it.
  if (swap->gl_fence) {
    gfx::ScopedSetGLToRealGLApi scoped_set_gl_api;
    gfx::ScopedCGLSetCurrentContext scoped_set_current(swap->cgl_context);

    CheckGLErrors("before deleting active fence");
    swap->gl_fence.reset();
    CheckGLErrors("while deleting active fence");
  }

  // Update the CALayer hierarchy.
  {
    TRACE_EVENT1("gpu", "ImageTransportSurfaceOverlayMac::setContents",
                 "surface", swap->io_surface.get());
    ScopedCAActionDisabler disabler;

    id new_contents = static_cast<id>(swap->io_surface.get());
    if (swap->use_partial_damage) {
      if (![partial_damage_layer_ superlayer])
        [layer_ addSublayer:partial_damage_layer_];
      [partial_damage_layer_ setContents:new_contents];

      CGRect new_frame = gfx::ConvertRectToDIP(
          swap->scale_factor, swap->pixel_partial_damage_rect).ToCGRect();
      if (!CGRectEqualToRect([partial_damage_layer_ frame], new_frame))
        [partial_damage_layer_ setFrame:new_frame];

      gfx::RectF contents_rect =
          gfx::RectF(swap->pixel_partial_damage_rect);
      contents_rect.Scale(
          1. / swap->pixel_size.width(), 1. / swap->pixel_size.height());
      CGRect cg_contents_rect = CGRectMake(
          contents_rect.x(), contents_rect.y(),
          contents_rect.width(), contents_rect.height());
      [partial_damage_layer_ setContentsRect:cg_contents_rect];
    } else {
      // Remove the partial damage layer.
      if ([partial_damage_layer_ superlayer]) {
        [partial_damage_layer_ removeFromSuperlayer];
        [partial_damage_layer_ setContents:nil];
      }

      // Note that calling setContents with the same IOSurface twice will result
      // in the screen not being updated, even if the IOSurface's content has
      // changed. Avoid this by calling setContentsChanged.
      if ([layer_ contents] == new_contents)
        [layer_ setContentsChanged];
      else
        [layer_ setContents:new_contents];

      CGRect new_frame = gfx::ConvertRectToDIP(
          swap->scale_factor, gfx::Rect(swap->pixel_size)).ToCGRect();
      if (!CGRectEqualToRect([layer_ frame], new_frame))
        [layer_ setFrame:new_frame];
    }
  }

  // Send acknowledgement to the browser.
  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_handle =
      ui::SurfaceHandleFromCAContextID([ca_context_ contextId]);
  params.size = pixel_size_;
  params.scale_factor = scale_factor_;
  params.latency_info.swap(swap->latency_info);
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  // Remove this from the queue, and reset any callback timers.
  pending_swaps_.pop_front();
}

void ImageTransportSurfaceOverlayMac::FinishAllPendingSwaps() {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::FinishAllPendingSwaps");
  while (!pending_swaps_.empty())
    DisplayFirstPendingSwapImmediately();
}

void ImageTransportSurfaceOverlayMac::CheckPendingSwapsCallback() {
  TRACE_EVENT0("gpu",
      "ImageTransportSurfaceOverlayMac::CheckPendingSwapsCallback");

  DCHECK(has_pending_callback_);
  has_pending_callback_ = false;

  if (pending_swaps_.empty())
    return;

  const base::TimeTicks now = base::TimeTicks::Now();
  if (IsFirstPendingSwapReadyToDisplay(now))
    DisplayFirstPendingSwapImmediately();
  PostCheckPendingSwapsCallbackIfNeeded(now);
}

void ImageTransportSurfaceOverlayMac::PostCheckPendingSwapsCallbackIfNeeded(
    const base::TimeTicks& now) {
  TRACE_EVENT0("gpu",
      "ImageTransportSurfaceOverlayMac::PostCheckPendingSwapsCallbackIfNeeded");

  if (has_pending_callback_)
    return;
  if (pending_swaps_.empty())
    return;

  base::TimeTicks target;
  if (display_link_mac_) {
    target = display_link_mac_->GetNextVSyncTimeAfter(
          now, kVSyncIntervalFractionForDisplayCallback);
  } else {
    target = now;
  }
  base::TimeDelta delay = target - now;

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ImageTransportSurfaceOverlayMac::CheckPendingSwapsCallback,
                     weak_factory_.GetWeakPtr()), delay);
  has_pending_callback_ = true;
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::SwapBuffers() {
  return SwapBuffersInternal(gfx::Rect(pixel_size_));
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::PostSubBuffer(int x,
                                                               int y,
                                                               int width,
                                                               int height) {
  return SwapBuffersInternal(gfx::Rect(x, y, width, height));
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
    const AcceleratedSurfaceMsg_BufferPresented_Params& params) {
  if (display_link_mac_ &&
      display_link_mac_->display_id() == params.display_id_for_vsync)
    return;
  display_link_mac_ = ui::DisplayLinkMac::GetForDisplay(
      params.display_id_for_vsync);
}

void ImageTransportSurfaceOverlayMac::OnResize(gfx::Size pixel_size,
                                               float scale_factor) {
  // Flush through any pending frames.
  FinishAllPendingSwaps();
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
