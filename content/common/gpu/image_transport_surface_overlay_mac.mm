// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface_overlay_mac.h"

#include <algorithm>
#include <IOSurface/IOSurface.h>
#include <OpenGL/CGLRenderers.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/gl.h>

// This type consistently causes problem on Mac, and needs to be dealt with
// in a systemic way.
// http://crbug.com/517208
#ifndef GL_OES_EGL_image
typedef void* GLeglImageOES;
#endif

#include "base/command_line.h"
#include "base/mac/scoped_cftyperef.h"
#include "content/common/gpu/gpu_messages.h"
#include "ui/accelerated_widget_mac/io_surface_context.h"
#include "ui/accelerated_widget_mac/surface_handle_types.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_image_io_surface.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/scoped_api.h"
#include "ui/gl/scoped_cgl.h"

namespace {

// Don't let a frame draw until 5% of the way through the next vsync interval
// after the call to SwapBuffers. This slight offset is to ensure that skew
// doesn't result in the frame being presented to the previous vsync interval.
const double kVSyncIntervalFractionForEarliestDisplay = 0.05;

// After doing a glFlush and putting in a fence in SwapBuffers, post a task to
// query the fence 50% of the way through the next vsync interval. If we are
// trying to animate smoothly, then want to query the fence at the next
// SwapBuffers. For this reason we schedule the callback for a long way into
// the next frame.
const double kVSyncIntervalFractionForDisplayCallback = 0.5;

// If swaps arrive regularly and nearly at the vsync rate, then attempt to
// make animation smooth (each frame is shown for one vsync interval) by sending
// them to the window server only when their GL work completes. If frames are
// not coming in with each vsync, then just throw them at the window server as
// they come.
const double kMaximumVSyncsBetweenSwapsForSmoothAnimation = 1.5;

void CheckGLErrors(const char* msg) {
  GLenum gl_error;
  while ((gl_error = glGetError()) != GL_NO_ERROR) {
    LOG(ERROR) << "OpenGL error hit " << msg << ": " << gl_error;
  }
}

void IOSurfaceContextNoOp(scoped_refptr<ui::IOSurfaceContext>) {
}

}  // namespace

@interface CALayer(Private)
-(void)setContentsChanged;
@end

namespace content {

class ImageTransportSurfaceOverlayMac::OverlayPlane {
 public:
  enum Type {
    ROOT = 0,
    ROOT_PARTIAL_DAMAGE = 1,
    OVERLAY = 2,
  };

  OverlayPlane(
      Type type,
      int z_order,
      base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
      gfx::Rect dip_frame_rect,
      gfx::RectF contents_rect)
      : type(type), z_order(z_order), io_surface(io_surface),
        dip_frame_rect(dip_frame_rect), contents_rect(contents_rect) {}
  ~OverlayPlane() {}

  static bool Compare(const linked_ptr<OverlayPlane>& a,
                      const linked_ptr<OverlayPlane>& b) {
    // Sort by z_order first.
    if (a->z_order < b->z_order)
      return true;
    if (a->z_order > b->z_order)
      return false;
    // Then ensure that the root partial damage is after the root.
    if (a->type < b->type)
      return true;
    if (a->type > b->type)
      return false;
    // Then sort by x.
    if (a->dip_frame_rect.x() < b->dip_frame_rect.x())
      return true;
    if (a->dip_frame_rect.x() > b->dip_frame_rect.x())
      return false;
    // Then sort by y.
    if (a->dip_frame_rect.y() < b->dip_frame_rect.y())
      return true;
    if (a->dip_frame_rect.y() > b->dip_frame_rect.y())
      return false;

    return false;
  }

  const Type type;
  const int z_order;
  // The IOSurface to set the CALayer's contents to. This can be nil for the
  // root layer, indicating that the layer's content has not been damaged since
  // the last time it was set.
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  const gfx::Rect dip_frame_rect;
  const gfx::RectF contents_rect;
};

class ImageTransportSurfaceOverlayMac::PendingSwap {
 public:
  PendingSwap() {}
  ~PendingSwap() { DCHECK(!gl_fence); }

  gfx::Size pixel_size;
  float scale_factor;

  std::vector<linked_ptr<OverlayPlane>> overlay_planes;
  std::vector<ui::LatencyInfo> latency_info;

  // A fence object, and the CGL context it was issued in.
  base::ScopedTypeRef<CGLContextObj> cgl_context;
  scoped_ptr<gfx::GLFence> gl_fence;

  // The earliest time that this frame may be drawn. A frame is not allowed
  // to draw until a fraction of the way through the vsync interval after its
  // This extra latency is to allow wiggle-room for smoothness.
  base::TimeTicks earliest_display_time_allowed;

  // The time that this will wake up and draw, if a following swap does not
  // cause it to draw earlier.
  base::TimeTicks target_display_time;
};

ImageTransportSurfaceOverlayMac::ImageTransportSurfaceOverlayMac(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    gfx::PluginWindowHandle handle)
    : scale_factor_(1), gl_renderer_id_(0), vsync_parameters_valid_(false),
      display_pending_swap_timer_(true, false), weak_factory_(this) {
  helper_.reset(new ImageTransportHelper(this, manager, stub, handle));
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
}

ImageTransportSurfaceOverlayMac::~ImageTransportSurfaceOverlayMac() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
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
  return true;
}

void ImageTransportSurfaceOverlayMac::Destroy() {
  DisplayAndClearAllPendingSwaps();
}

bool ImageTransportSurfaceOverlayMac::IsOffscreen() {
  return false;
}

void ImageTransportSurfaceOverlayMac::ScheduleOverlayPlaneForPartialDamage(
    const gfx::Rect& this_damage_pixel_rect) {
  // Find the root plane. If none is present, then we're hosed.
  linked_ptr<OverlayPlane> root_plane;
  for (linked_ptr<OverlayPlane>& plane : pending_overlay_planes_) {
    if (plane->type == OverlayPlane::ROOT) {
      root_plane = plane;
      break;
    }
  }
  if (!root_plane.get())
    return;
  bool damage_full_window = false;

  // Grow the partial damage rect to include the new damage.
  gfx::Rect this_damage_dip_rect = gfx::ConvertRectToDIP(
      scale_factor_, this_damage_pixel_rect);
  accumulated_damage_dip_rect_.Union(this_damage_dip_rect);

  // If the accumulated partial damage is covered by opaque layers, then tell
  // the root layer not to draw anything, and don't add a partial damage
  // overlay.
  bool overlays_cover_accumulated_damage = false;
  bool overlays_cover_this_damage = false;
  for (const linked_ptr<OverlayPlane>& plane : pending_overlay_planes_) {
    if (plane->type == OverlayPlane::OVERLAY) {
      overlays_cover_accumulated_damage |=
          plane->dip_frame_rect.Contains(accumulated_damage_dip_rect_);
      overlays_cover_this_damage |=
          plane->dip_frame_rect.Contains(this_damage_dip_rect);
    }
  }
  if (overlays_cover_accumulated_damage) {
    root_plane->io_surface.reset();
    return;
  }
  // If the current damage is covered, but not the accumulated damage, then
  // damage the full window, so that we might hit this path next frame.
  if (overlays_cover_this_damage)
    damage_full_window = true;

  // Compute the fraction of the accumulated partial damage rect that has been
  // damaged. If this gets too small (<75%), just re-damage the full window,
  // so we can re-create a smaller partial damage layer next frame.
  const double kMinimumFractionOfPartialDamage = 0.75;
  double fraction_of_damage =
      this_damage_dip_rect.size().GetArea() / static_cast<double>(
          accumulated_damage_dip_rect_.size().GetArea());
  if (fraction_of_damage <= kMinimumFractionOfPartialDamage)
    damage_full_window = true;

  // Early-out if we decided to damage the full window.
  if (damage_full_window) {
    accumulated_damage_dip_rect_ = gfx::Rect();
    return;
  }

  // Create a new overlay plane for the partial damage, and un-set the root
  // plane's damage by un-setting its IOSurface.
  gfx::RectF contents_rect = gfx::RectF(accumulated_damage_dip_rect_);
  contents_rect.Scale(1. / root_plane->dip_frame_rect.width(),
                      1. / root_plane->dip_frame_rect.height());
  pending_overlay_planes_.push_back(linked_ptr<OverlayPlane>(new OverlayPlane(
      OverlayPlane::ROOT_PARTIAL_DAMAGE, 0, root_plane->io_surface,
      accumulated_damage_dip_rect_, contents_rect)));

  root_plane->io_surface.reset();
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::SwapBuffersInternal() {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::SwapBuffersInternal");

  // Use the same concept of 'now' for the entire function. The duration of
  // this function only affect the result if this function lasts across a vsync
  // boundary, in which case smooth animation is out the window anyway.
  const base::TimeTicks now = base::TimeTicks::Now();

  // Decide if the frame should be drawn immediately, or if we should wait until
  // its work finishes before drawing immediately.
  bool display_immediately = false;
  if (vsync_parameters_valid_ &&
      now - last_swap_time_ >
          kMaximumVSyncsBetweenSwapsForSmoothAnimation * vsync_interval_) {
    display_immediately = true;
  }
  last_swap_time_ = now;

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
  new_swap->overlay_planes.swap(pending_overlay_planes_);
  new_swap->latency_info.swap(latency_info_);

  // A flush is required to ensure that all content appears in the layer.
  {
    gfx::ScopedSetGLToRealGLApi scoped_set_gl_api;
    TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::glFlush");
    CheckGLErrors("before flushing frame");
    new_swap->cgl_context.reset(CGLGetCurrentContext(),
                                base::scoped_policy::RETAIN);
    if (gfx::GLFence::IsSupported() && !display_immediately)
      new_swap->gl_fence.reset(gfx::GLFence::Create());
    else
      glFlush();
    CheckGLErrors("while flushing frame");
  }

  // Compute the deadlines for drawing this frame.
  if (display_immediately) {
    new_swap->earliest_display_time_allowed = now;
    new_swap->target_display_time = now;
  } else {
    new_swap->earliest_display_time_allowed =
        GetNextVSyncTimeAfter(now, kVSyncIntervalFractionForEarliestDisplay);
    new_swap->target_display_time =
        GetNextVSyncTimeAfter(now, kVSyncIntervalFractionForDisplayCallback);
  }

  pending_swaps_.push_back(new_swap);
  if (display_immediately)
    DisplayFirstPendingSwapImmediately();
  else
    PostCheckPendingSwapsCallbackIfNeeded(now);
  return gfx::SwapResult::SWAP_ACK;
}

bool ImageTransportSurfaceOverlayMac::IsFirstPendingSwapReadyToDisplay(
    const base::TimeTicks& now) {
  DCHECK(!pending_swaps_.empty());
  linked_ptr<PendingSwap> swap = pending_swaps_.front();

  // Frames are disallowed from drawing until the vsync interval after their
  // swap is issued.
  if (now < swap->earliest_display_time_allowed)
    return false;

  // If we've passed that marker, then wait for the work behind the fence to
  // complete.
  if (swap->gl_fence) {
    gfx::ScopedSetGLToRealGLApi scoped_set_gl_api;
    gfx::ScopedCGLSetCurrentContext scoped_set_current(swap->cgl_context);

    CheckGLErrors("before waiting on fence");
    if (!swap->gl_fence->HasCompleted()) {
      TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::ClientWait");
      swap->gl_fence->ClientWait();
    }
    swap->gl_fence.reset();
    CheckGLErrors("after waiting on fence");
  }
  return true;
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
  UpdateCALayerTree(layer_.get(), swap.get());

  // Send acknowledgement to the browser.
  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_handle =
      ui::SurfaceHandleFromCAContextID([ca_context_ contextId]);
  params.size = swap->pixel_size;
  params.scale_factor = swap->scale_factor;
  params.latency_info.swap(swap->latency_info);
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  // Remove this from the queue, and reset any callback timers.
  pending_swaps_.pop_front();
}

// static
void ImageTransportSurfaceOverlayMac::UpdateCALayerTree(
    CALayer* root_layer, PendingSwap* swap) {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::UpdateCALayerTree");
  ScopedCAActionDisabler disabler;

  // Sort the planes by z-index.
  std::sort(swap->overlay_planes.begin(), swap->overlay_planes.end(),
            OverlayPlane::Compare);

  NSUInteger child_index = 0;
  for (linked_ptr<OverlayPlane>& plane : swap->overlay_planes) {
    // Look up or create the CALayer for this plane.
    CALayer* plane_layer = nil;
    if (plane->type == OverlayPlane::ROOT) {
      plane_layer = root_layer;
    } else {
      if (child_index >= [[root_layer sublayers] count]) {
        base::scoped_nsobject<CALayer> new_layer([[CALayer alloc] init]);
        [new_layer setOpaque:YES];
        [root_layer addSublayer:new_layer];
      }
      plane_layer = [[root_layer sublayers] objectAtIndex:child_index];
      child_index += 1;
    }

    // Update layer contents if needed.
    if (plane->io_surface) {
      // Note that calling setContents with the same IOSurface twice will result
      // in the screen not being updated, even if the IOSurface's content has
      // changed. This can be avoided by calling setContentsChanged. Only call
      // this on the root layer, because it is the only layer that will ignore
      // damage.
      id new_contents = static_cast<id>(plane->io_surface.get());
      if ([plane_layer contents] == new_contents) {
        if (plane->type == OverlayPlane::ROOT)
          [plane_layer setContentsChanged];
      } else {
        [plane_layer setContents:new_contents];
      }
    } else {
      // A nil IOSurface indicates that partial damage is being tracked by a
      // sub-layer.
      DCHECK(plane->type == OverlayPlane::ROOT);
    }

    static bool show_borders =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kShowMacOverlayBorders);
    if (show_borders) {
      base::ScopedCFTypeRef<CGColorRef> color;
      if (!plane->io_surface) {
        // Green represents contents that are unchanged across frames.
        color.reset(CGColorCreateGenericRGB(0, 1, 0, 1));
      } else if (plane->type == OverlayPlane::OVERLAY) {
        // Pink represents overlay planes
        color.reset(CGColorCreateGenericRGB(1, 0, 1, 1));
      } else {
        // Red represents damaged contents.
        color.reset(CGColorCreateGenericRGB(1, 0, 0, 1));
      }
      [plane_layer setBorderWidth:2];
      [plane_layer setBorderColor:color];
    }

    [plane_layer setFrame:plane->dip_frame_rect.ToCGRect()];
    [plane_layer setContentsRect:plane->contents_rect.ToCGRect()];
  }

  // Remove any now-obsolete children.
  while ([[root_layer sublayers] count] > child_index) {
    CALayer* layer = [[root_layer sublayers] objectAtIndex:child_index];
    [layer setContents:nil];
    [layer removeFromSuperlayer];
  }
}

void ImageTransportSurfaceOverlayMac::DisplayAndClearAllPendingSwaps() {
  TRACE_EVENT0("gpu",
      "ImageTransportSurfaceOverlayMac::DisplayAndClearAllPendingSwaps");
  while (!pending_swaps_.empty())
    DisplayFirstPendingSwapImmediately();
}

void ImageTransportSurfaceOverlayMac::CheckPendingSwapsCallback() {
  TRACE_EVENT0("gpu",
      "ImageTransportSurfaceOverlayMac::CheckPendingSwapsCallback");

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

  if (pending_swaps_.empty()) {
    display_pending_swap_timer_.Stop();
  } else {
    display_pending_swap_timer_.Start(
        FROM_HERE,
        pending_swaps_.front()->target_display_time - now,
        base::Bind(&ImageTransportSurfaceOverlayMac::CheckPendingSwapsCallback,
                       weak_factory_.GetWeakPtr()));
  }
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::SwapBuffers() {
  // Clear the accumulated damage rect, since the partial damage overlay will be
  // removed.
  accumulated_damage_dip_rect_ = gfx::Rect();
  return SwapBuffersInternal();
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::PostSubBuffer(int x,
                                                               int y,
                                                               int width,
                                                               int height) {
  ScheduleOverlayPlaneForPartialDamage(gfx::Rect(x, y, width, height));
  return SwapBuffersInternal();
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

bool ImageTransportSurfaceOverlayMac::OnMakeCurrent(gfx::GLContext* context) {
  // Ensure that the context is on the appropriate GL renderer. The GL renderer
  // will generally only change when the GPU changes.
  if (gl_renderer_id_ && context)
    context->share_group()->SetRendererID(gl_renderer_id_);
  return true;
}

bool ImageTransportSurfaceOverlayMac::SetBackbufferAllocation(bool allocated) {
  if (!allocated) {
    DisplayAndClearAllPendingSwaps();
    last_swap_time_ = base::TimeTicks();
  }
  return true;
}

bool ImageTransportSurfaceOverlayMac::ScheduleOverlayPlane(
    int z_order,
    gfx::OverlayTransform transform,
    gfx::GLImage* image,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect) {
  DCHECK_GE(z_order, 0);
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);
  if (z_order < 0 || transform != gfx::OVERLAY_TRANSFORM_NONE)
    return false;

  OverlayPlane::Type type = z_order == 0 ?
      OverlayPlane::ROOT : OverlayPlane::OVERLAY;
  gfx::Rect dip_frame_rect = gfx::ConvertRectToDIP(
      scale_factor_, bounds_rect);
  gfx::RectF contents_rect = crop_rect;

  gfx::GLImageIOSurface* image_io_surface =
      static_cast<gfx::GLImageIOSurface*>(image);

  pending_overlay_planes_.push_back(linked_ptr<OverlayPlane>(
      new OverlayPlane(
          type, z_order, image_io_surface->io_surface(), dip_frame_rect,
          contents_rect)));
  return true;
}

bool ImageTransportSurfaceOverlayMac::IsSurfaceless() const {
  return true;
}

void ImageTransportSurfaceOverlayMac::OnBufferPresented(
    const AcceleratedSurfaceMsg_BufferPresented_Params& params) {
  vsync_timebase_ = params.vsync_timebase;
  vsync_interval_ = params.vsync_interval;
  vsync_parameters_valid_ = (vsync_interval_ != base::TimeDelta());

  // Compute |vsync_timebase_| to be the first vsync after time zero.
  if (vsync_parameters_valid_) {
    vsync_timebase_ -=
        vsync_interval_ *
        ((vsync_timebase_ - base::TimeTicks()) / vsync_interval_);
  }
}

void ImageTransportSurfaceOverlayMac::OnResize(gfx::Size pixel_size,
                                               float scale_factor) {
  // Flush through any pending frames.
  DisplayAndClearAllPendingSwaps();
  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;
}

void ImageTransportSurfaceOverlayMac::SetLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  latency_info_.insert(
      latency_info_.end(), latency_info.begin(), latency_info.end());
}

void ImageTransportSurfaceOverlayMac::WakeUpGpu() {}

void ImageTransportSurfaceOverlayMac::OnGpuSwitched() {
  // Create a new context, and use the GL renderer ID that the new context gets.
  scoped_refptr<ui::IOSurfaceContext> context_on_new_gpu =
      ui::IOSurfaceContext::Get(ui::IOSurfaceContext::kCALayerContext);
  if (!context_on_new_gpu)
    return;
  GLint context_renderer_id = -1;
  if (CGLGetParameter(context_on_new_gpu->cgl_context(),
                      kCGLCPCurrentRendererID,
                      &context_renderer_id) != kCGLNoError) {
    LOG(ERROR) << "Failed to create test context after GPU switch";
    return;
  }
  gl_renderer_id_ = context_renderer_id & kCGLRendererIDMatchingMask;

  // Post a task holding a reference to the new GL context. The reason for
  // this is to avoid creating-then-destroying the context for every image
  // transport surface that is observing the GPU switch.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&IOSurfaceContextNoOp, context_on_new_gpu));
}

base::TimeTicks ImageTransportSurfaceOverlayMac::GetNextVSyncTimeAfter(
    const base::TimeTicks& from, double interval_fraction) {
  if (!vsync_parameters_valid_)
    return from;

  // Compute the previous vsync time.
  base::TimeTicks previous_vsync =
      vsync_interval_ * ((from - vsync_timebase_) / vsync_interval_) +
      vsync_timebase_;

  // Return |interval_fraction| through the next vsync.
  return previous_vsync + (1 + interval_fraction) * vsync_interval_;
}

}  // namespace content
