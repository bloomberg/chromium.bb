// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface_overlay_mac.h"

#include <algorithm>
#include <CoreGraphics/CoreGraphics.h>
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
#include "ui/gfx/transform.h"
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

// When selecting a CALayer to re-use for partial damage, this is the maximum
// fraction of the merged layer's pixels that may be not-updated by the swap
// before we consider the CALayer to not be a good enough match, and create a
// new one.
const float kMaximumPartialDamageWasteFraction = 1.2f;

// The maximum number of partial damage layers that may be created before we
// give up and remove them all (doing full damage in the process).
const size_t kMaximumPartialDamageLayers = 8;

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

scoped_refptr<gfx::GLSurface> ImageTransportSurfaceCreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    gfx::PluginWindowHandle handle) {
  return new ImageTransportSurfaceOverlayMac(manager, stub, handle);
}

class ImageTransportSurfaceOverlayMac::OverlayPlane {
 public:
  static linked_ptr<OverlayPlane> CreateWithFrameRect(
      int z_order,
      int io_surface_id,
      base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
      const gfx::RectF& pixel_frame_rect,
      const gfx::RectF& contents_rect) {
    gfx::Transform transform;
    transform.Translate(pixel_frame_rect.x(), pixel_frame_rect.y());
    return linked_ptr<OverlayPlane>(
        new OverlayPlane(z_order, io_surface_id, io_surface, contents_rect, 1.f,
                         base::ScopedCFTypeRef<CGColorRef>(),
                         pixel_frame_rect.size(), transform, pixel_frame_rect));
  }

  static linked_ptr<OverlayPlane> CreateWithTransform(
      int z_order,
      int io_surface_id,
      base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
      const gfx::RectF& contents_rect,
      float opacity,
      base::ScopedCFTypeRef<CGColorRef> background_color,
      const gfx::SizeF& bounds_size,
      const gfx::Transform& transform) {
    gfx::RectF pixel_frame_rect = gfx::RectF(bounds_size);
    transform.TransformRect(&pixel_frame_rect);
    return linked_ptr<OverlayPlane>(new OverlayPlane(
        z_order, io_surface_id, io_surface, contents_rect, opacity,
        background_color, bounds_size, transform, pixel_frame_rect));
  }

  ~OverlayPlane() { DCHECK(!ca_layer); }

  const int z_order;
  base::scoped_nsobject<CALayer> ca_layer;

  // The IOSurface to set the CALayer's contents to.
  const int io_surface_id;
  const base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  const gfx::RectF contents_rect;
  float opacity;
  const base::ScopedCFTypeRef<CGColorRef> background_color;
  const gfx::SizeF bounds_size;
  const gfx::Transform transform;

  const gfx::RectF pixel_frame_rect;

  bool layer_needs_update;

  static bool Compare(const linked_ptr<OverlayPlane>& a,
                      const linked_ptr<OverlayPlane>& b) {
    return (a->z_order < b->z_order);
  }

  void TakeCALayerFrom(OverlayPlane* other_plane) {
    ca_layer.swap(other_plane->ca_layer);
  }

  void UpdateProperties() {
    if (layer_needs_update) {
      [ca_layer setOpaque:YES];

      id new_contents = static_cast<id>(io_surface.get());
      if ([ca_layer contents] == new_contents && z_order == 0) {
        [ca_layer setContentsChanged];
      } else {
        [ca_layer setContents:new_contents];
      }
      [ca_layer setContentsRect:contents_rect.ToCGRect()];

      [ca_layer setOpacity:opacity];
      if (background_color) {
        [ca_layer setBackgroundColor:background_color];
      } else {
        [ca_layer setBackgroundColor:CGColorGetConstantColor(kCGColorClear)];
      }

      [ca_layer setAnchorPoint:CGPointZero];
      [ca_layer setBounds:gfx::RectF(bounds_size).ToCGRect()];
      CATransform3D ca_transform;
      transform.matrix().asColMajord(&ca_transform.m11);
      [ca_layer setTransform:ca_transform];
    }
    static bool show_borders =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kShowMacOverlayBorders);
    if (show_borders) {
      base::ScopedCFTypeRef<CGColorRef> color;
      if (!layer_needs_update) {
        // Green represents contents that are unchanged across frames.
        color.reset(CGColorCreateGenericRGB(0, 1, 0, 1));
      } else if (z_order != 0) {
        // Pink represents overlay planes
        color.reset(CGColorCreateGenericRGB(1, 0, 1, 1));
      } else {
        // Red represents damaged contents.
        color.reset(CGColorCreateGenericRGB(1, 0, 0, 1));
      }
      [ca_layer setBorderWidth:1];
      [ca_layer setBorderColor:color];
    }
    layer_needs_update = false;
  }

  void Destroy() {
    if (!ca_layer)
      return;
    [ca_layer setContents:nil];
    [ca_layer removeFromSuperlayer];
    [ca_layer setBorderWidth:0];
    [ca_layer setBorderColor:CGColorGetConstantColor(kCGColorClear)];
    [ca_layer setBackgroundColor:CGColorGetConstantColor(kCGColorClear)];
    ca_layer.reset();
  }

 private:
  OverlayPlane(int z_order,
               int io_surface_id,
               base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
               const gfx::RectF& contents_rect,
               float opacity,
               base::ScopedCFTypeRef<CGColorRef> background_color,
               const gfx::SizeF& bounds_size,
               const gfx::Transform& transform,
               const gfx::RectF& pixel_frame_rect)
      : z_order(z_order),
        io_surface_id(io_surface_id),
        io_surface(io_surface),
        contents_rect(contents_rect),
        opacity(opacity),
        background_color(background_color),
        bounds_size(bounds_size),
        transform(transform),
        pixel_frame_rect(pixel_frame_rect),
        layer_needs_update(true) {}
};

class ImageTransportSurfaceOverlayMac::PendingSwap {
 public:
  PendingSwap() {}
  ~PendingSwap() { DCHECK(!gl_fence); }

  gfx::Size pixel_size;
  float scale_factor;
  gfx::Rect pixel_damage_rect;

  linked_ptr<OverlayPlane> root_plane;
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
    : use_remote_layer_api_(ui::RemoteLayerAPISupported()),
      scale_factor_(1),
      gl_renderer_id_(0),
      vsync_parameters_valid_(false),
      next_ca_layer_z_order_(1),
      display_pending_swap_timer_(true, false),
      weak_factory_(this) {
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
  if (use_remote_layer_api_) {
    CGSConnectionID connection_id = CGSMainConnectionID();
    ca_context_.reset([
        [CAContext contextWithCGSConnection:connection_id options:@{}] retain]);
    ca_root_layer_.reset([[CALayer alloc] init]);
    [ca_root_layer_ setGeometryFlipped:YES];
    [ca_root_layer_ setOpaque:YES];
    [ca_context_ setLayer:ca_root_layer_];
  }
  return true;
}

void ImageTransportSurfaceOverlayMac::Destroy() {
  DisplayAndClearAllPendingSwaps();

  if (current_root_plane_.get())
    current_root_plane_->Destroy();
  current_root_plane_.reset();
  for (auto& plane : current_partial_damage_planes_)
    plane->Destroy();
  current_partial_damage_planes_.clear();
  for (auto& plane : current_overlay_planes_)
    plane->Destroy();
  current_overlay_planes_.clear();
}

bool ImageTransportSurfaceOverlayMac::IsOffscreen() {
  return false;
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::SwapBuffersInternal(
    const gfx::Rect& pixel_damage_rect) {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::SwapBuffersInternal");
  next_ca_layer_z_order_ = 1;

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
  new_swap->pixel_damage_rect = pixel_damage_rect;
  new_swap->root_plane = pending_root_plane_;
  pending_root_plane_ = linked_ptr<OverlayPlane>();
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

  // Update the plane lists.
  {
    // Sort the input planes by z-index, and remove any overlays from the
    // damage rect.
    gfx::RectF pixel_damage_rect = gfx::RectF(swap->pixel_damage_rect);
    std::sort(swap->overlay_planes.begin(), swap->overlay_planes.end(),
              OverlayPlane::Compare);
    for (auto& plane : swap->overlay_planes)
      pixel_damage_rect.Subtract(plane->pixel_frame_rect);

    ScopedCAActionDisabler disabler;
    UpdateRootAndPartialDamagePlanes(swap->root_plane, pixel_damage_rect);
    UpdateOverlayPlanes(swap->overlay_planes);
    UpdateCALayerTree();
    swap->overlay_planes.clear();
  }

  // Update the latency info to reflect the swap time.
  base::TimeTicks swap_time = base::TimeTicks::Now();
  for (auto latency_info : swap->latency_info) {
    latency_info.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0, 0, swap_time, 1);
    latency_info.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0,
        swap_time, 1);
  }

  // Send acknowledgement to the browser.
  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  if (use_remote_layer_api_) {
    params.surface_handle =
        ui::SurfaceHandleFromCAContextID([ca_context_ contextId]);
  } else {
    params.surface_handle =
        ui::SurfaceHandleFromIOSurfaceID(current_root_plane_->io_surface_id);
  }
  params.size = swap->pixel_size;
  params.scale_factor = swap->scale_factor;
  params.latency_info.swap(swap->latency_info);
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  // Remove this from the queue, and reset any callback timers.
  pending_swaps_.pop_front();
}

void ImageTransportSurfaceOverlayMac::UpdateOverlayPlanes(
    const std::vector<linked_ptr<OverlayPlane>>& new_overlay_planes) {
  std::list<linked_ptr<OverlayPlane>> old_overlay_planes;
  old_overlay_planes.swap(current_overlay_planes_);

  // Move the new overlay planes into the |current_overlay_planes_| list,
  // cannibalizing from the old |current_overlay_planes_| as much as possible.
  for (auto& new_plane : new_overlay_planes) {
    if (!old_overlay_planes.empty()) {
      new_plane->TakeCALayerFrom(old_overlay_planes.front().get());
      old_overlay_planes.pop_front();
    }
    current_overlay_planes_.push_back(new_plane);
  }

  // Destroy any of the previous |current_overlay_planes_| that we couldn't
  // cannibalize.
  for (auto& old_plane : old_overlay_planes)
    old_plane->Destroy();
}

void ImageTransportSurfaceOverlayMac::UpdateRootAndPartialDamagePlanes(
    const linked_ptr<OverlayPlane>& new_root_plane,
    const gfx::RectF& pixel_damage_rect) {
  std::list<linked_ptr<OverlayPlane>> old_partial_damage_planes;
  old_partial_damage_planes.swap(current_partial_damage_planes_);
  linked_ptr<OverlayPlane> plane_for_swap;

  // If there is no new root plane, destroy the old one.
  if (!new_root_plane.get()) {
    for (auto& old_plane : old_partial_damage_planes)
      old_plane->Destroy();
    if (current_root_plane_.get())
      current_root_plane_->Destroy();
    current_root_plane_.reset();
    return;
  }

  // If the frame's size changed, if we haven't updated the root layer, if
  // we have full damage, or if we don't support remote layers, then use the
  // root layer directly.
  if (!use_remote_layer_api_ || !current_root_plane_.get() ||
      current_root_plane_->pixel_frame_rect !=
          new_root_plane->pixel_frame_rect ||
      pixel_damage_rect == new_root_plane->pixel_frame_rect) {
    plane_for_swap = new_root_plane;
  }

  // Walk though the existing partial damage layers and see if there is one that
  // is appropriate to re-use.
  if (!plane_for_swap.get() && !pixel_damage_rect.IsEmpty()) {
    gfx::RectF plane_to_reuse_dip_enlarged_rect;

    // Find the last partial damage plane to re-use the CALayer from. Grow the
    // new rect for this layer to include this damage, and all nearby partial
    // damage layers.
    linked_ptr<OverlayPlane> plane_to_reuse;
    for (auto& old_plane : old_partial_damage_planes) {
      gfx::RectF dip_enlarged_rect = old_plane->pixel_frame_rect;
      dip_enlarged_rect.Union(pixel_damage_rect);

      // Compute the fraction of the pixels that would not be updated by this
      // swap. If it is too big, try another layer.
      float waste_fraction = dip_enlarged_rect.size().GetArea() * 1.f /
                             pixel_damage_rect.size().GetArea();
      if (waste_fraction > kMaximumPartialDamageWasteFraction)
        continue;

      plane_to_reuse = old_plane;
      plane_to_reuse_dip_enlarged_rect.Union(dip_enlarged_rect);
    }

    if (plane_to_reuse.get()) {
      gfx::RectF enlarged_contents_rect = plane_to_reuse_dip_enlarged_rect;
      enlarged_contents_rect.Scale(
          1. / new_root_plane->pixel_frame_rect.width(),
          1. / new_root_plane->pixel_frame_rect.height());

      plane_for_swap = OverlayPlane::CreateWithFrameRect(
          0, new_root_plane->io_surface_id, new_root_plane->io_surface,
          plane_to_reuse_dip_enlarged_rect, enlarged_contents_rect);

      plane_for_swap->TakeCALayerFrom(plane_to_reuse.get());
      if (plane_to_reuse != old_partial_damage_planes.back())
        [plane_for_swap->ca_layer removeFromSuperlayer];
    }
  }

  // If we haven't found an appropriate layer to re-use, create a new one, if
  // we haven't already created too many.
  if (!plane_for_swap.get() && !pixel_damage_rect.IsEmpty() &&
      old_partial_damage_planes.size() < kMaximumPartialDamageLayers) {
    gfx::RectF contents_rect = gfx::RectF(pixel_damage_rect);
    contents_rect.Scale(1. / new_root_plane->pixel_frame_rect.width(),
                        1. / new_root_plane->pixel_frame_rect.height());
    plane_for_swap = OverlayPlane::CreateWithFrameRect(
        0, new_root_plane->io_surface_id, new_root_plane->io_surface,
        pixel_damage_rect, contents_rect);
  }

  // And if we still don't have a layer, use the root layer.
  if (!plane_for_swap.get() && !pixel_damage_rect.IsEmpty())
    plane_for_swap = new_root_plane;

  // Walk all old partial damage planes. Remove anything that is now completely
  // covered, and move everything else into the new
  // |current_partial_damage_planes_|.
  for (auto& old_plane : old_partial_damage_planes) {
    // Intersect the planes' frames with the new root plane to ensure that
    // they don't get kept alive inappropriately.
    gfx::RectF old_plane_frame_rect = old_plane->pixel_frame_rect;
    old_plane_frame_rect.Intersect(new_root_plane->pixel_frame_rect);

    if (plane_for_swap.get() &&
        plane_for_swap->pixel_frame_rect.Contains(old_plane_frame_rect)) {
      old_plane->Destroy();
    } else {
      DCHECK(old_plane->ca_layer);
      current_partial_damage_planes_.push_back(old_plane);
    }
  }

  // Finally, add the new swap's plane at the back of the list, if it exists.
  if (plane_for_swap == new_root_plane) {
    if (current_root_plane_.get()) {
      plane_for_swap->TakeCALayerFrom(current_root_plane_.get());
    } else {
      plane_for_swap->ca_layer = ca_root_layer_;
    }
    current_root_plane_ = new_root_plane;
  } else if (plane_for_swap.get()) {
    current_partial_damage_planes_.push_back(plane_for_swap);
  }
}

void ImageTransportSurfaceOverlayMac::UpdateCALayerTree() {
  if (!use_remote_layer_api_) {
    DCHECK(current_overlay_planes_.empty());
    DCHECK(current_partial_damage_planes_.empty());
    return;
  }

  // Allocate new CALayers as needed. Overlay layers are always added to the
  // back of the list.
  CALayer* first_overlay_ca_layer = nil;
  for (auto& plane : current_overlay_planes_) {
    if (!plane->ca_layer) {
      plane->ca_layer.reset([[CALayer alloc] init]);
      [ca_root_layer_ addSublayer:plane->ca_layer];
    }
    if (!first_overlay_ca_layer)
      first_overlay_ca_layer = plane->ca_layer;
  }
  // Partial damage layers are inserted below the overlay layers.
  for (auto& plane : current_partial_damage_planes_) {
    if (!plane->ca_layer) {
      DCHECK(plane == current_partial_damage_planes_.back());
      plane->ca_layer.reset([[CALayer alloc] init]);
    }
    if (![plane->ca_layer superlayer]) {
      DCHECK(plane == current_partial_damage_planes_.back());
      if (first_overlay_ca_layer) {
        [ca_root_layer_ insertSublayer:plane->ca_layer
                                 below:first_overlay_ca_layer];
      } else {
        [ca_root_layer_ addSublayer:plane->ca_layer];
      }
    }
  }

  // Update CALayer contents, frames, and borders.
  if (current_root_plane_.get())
    current_root_plane_->UpdateProperties();
  for (auto& plane : current_partial_damage_planes_)
    plane->UpdateProperties();
  for (auto& plane : current_overlay_planes_)
    plane->UpdateProperties();
  [ca_root_layer_ setTransform:CATransform3DMakeScale(1 / scale_factor_,
                                                      1 / scale_factor_, 1)];

  DCHECK_EQ(
      static_cast<size_t>([[ca_root_layer_ sublayers] count]),
      current_partial_damage_planes_.size() + current_overlay_planes_.size());
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
  return SwapBuffersInternal(
      gfx::Rect(0, 0, pixel_size_.width(), pixel_size_.height()));
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
    gl::GLImage* image,
    const gfx::Rect& pixel_frame_rect,
    const gfx::RectF& crop_rect) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);
  if (transform != gfx::OVERLAY_TRANSFORM_NONE)
    return false;

  linked_ptr<OverlayPlane> plane = OverlayPlane::CreateWithFrameRect(
      z_order, static_cast<gl::GLImageIOSurface*>(image)->io_surface_id().id,
      static_cast<gl::GLImageIOSurface*>(image)->io_surface(),
      gfx::RectF(pixel_frame_rect), crop_rect);
  if (z_order == 0)
    pending_root_plane_ = plane;
  else
    pending_overlay_planes_.push_back(plane);

  return true;
}

bool ImageTransportSurfaceOverlayMac::ScheduleCALayer(
    gl::GLImage* contents_image,
    const gfx::RectF& contents_rect,
    float opacity,
    unsigned background_color,
    const gfx::SizeF& bounds_size,
    const gfx::Transform& transform) {
  // Extract the IOSurface, if this layer is not just a solid color.
  int io_surface_id = 0;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  if (contents_image) {
    io_surface_id =
        static_cast<gl::GLImageIOSurface*>(contents_image)->io_surface_id().id;
    io_surface =
        static_cast<gl::GLImageIOSurface*>(contents_image)->io_surface();
  }

  // Convert the RGBA SkColor to an sRGB CGColorRef.
  CGFloat rgba_color_components[4] = {
      SkColorGetR(background_color) / 255.,
      SkColorGetG(background_color) / 255.,
      SkColorGetB(background_color) / 255.,
      SkColorGetA(background_color) / 255.,
  };
  base::ScopedCFTypeRef<CGColorRef> srgb_background_color(CGColorCreate(
      CGColorSpaceCreateWithName(kCGColorSpaceSRGB), rgba_color_components));

  pending_overlay_planes_.push_back(OverlayPlane::CreateWithTransform(
      next_ca_layer_z_order_++, io_surface_id, io_surface, contents_rect,
      opacity, srgb_background_color, bounds_size, transform));
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

bool ImageTransportSurfaceOverlayMac::Resize(const gfx::Size& pixel_size,
                                             float scale_factor) {
  // Flush through any pending frames.
  DisplayAndClearAllPendingSwaps();
  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;
  return true;
}

void ImageTransportSurfaceOverlayMac::SetLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  latency_info_.insert(
      latency_info_.end(), latency_info.begin(), latency_info.end());
}

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
