// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface_overlay_mac.h"

#include <CoreGraphics/CoreGraphics.h>
#include <IOSurface/IOSurface.h>
#include <OpenGL/CGLRenderers.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/gl.h>
#include <stddef.h>

#include <algorithm>

// This type consistently causes problem on Mac, and needs to be dealt with
// in a systemic way.
// http://crbug.com/517208
#ifndef GL_OES_EGL_image
typedef void* GLeglImageOES;
#endif

#include "base/command_line.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "content/common/gpu/ca_layer_tree_mac.h"
#include "content/common/gpu/gpu_messages.h"
#include "ui/accelerated_widget_mac/io_surface_context.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/rect_conversions.h"
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

class CALayerPartialDamageTree {
 public:
  CALayerPartialDamageTree(bool allow_partial_swap,
                           base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
                           const gfx::Rect& pixel_frame_rect);
  ~CALayerPartialDamageTree();

  base::ScopedCFTypeRef<IOSurfaceRef> RootLayerIOSurface();
  void CommitCALayers(CALayer* superlayer,
                      scoped_ptr<CALayerPartialDamageTree> old_tree,
                      float scale_factor,
                      const gfx::Rect& pixel_damage_rect);

 private:
  class OverlayPlane;

  void UpdateRootAndPartialDamagePlanes(CALayerPartialDamageTree* old_tree,
                                        const gfx::RectF& pixel_damage_rect);
  void UpdateRootAndPartialDamageCALayers(CALayer* superlayer,
                                          float scale_factor);

  const bool allow_partial_swap_;
  linked_ptr<OverlayPlane> root_plane_;
  std::list<linked_ptr<OverlayPlane>> partial_damage_planes_;
};

class CALayerPartialDamageTree::OverlayPlane {
 public:
  static linked_ptr<OverlayPlane> CreateWithFrameRect(
      int z_order,
      base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
      const gfx::RectF& pixel_frame_rect,
      const gfx::RectF& contents_rect) {
    gfx::Transform transform;
    transform.Translate(pixel_frame_rect.x(), pixel_frame_rect.y());
    return linked_ptr<OverlayPlane>(
        new OverlayPlane(z_order, io_surface, contents_rect, pixel_frame_rect));
  }

  ~OverlayPlane() {
    [ca_layer setContents:nil];
    [ca_layer removeFromSuperlayer];
    ca_layer.reset();
  }

  const int z_order;
  const base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  const gfx::RectF contents_rect;
  const gfx::RectF pixel_frame_rect;
  bool layer_needs_update;
  base::scoped_nsobject<CALayer> ca_layer;

  void TakeCALayerFrom(OverlayPlane* other_plane) {
    ca_layer.swap(other_plane->ca_layer);
  }

  void UpdateProperties(float scale_factor) {
    if (layer_needs_update) {
      [ca_layer setOpaque:YES];

      id new_contents = static_cast<id>(io_surface.get());
      if ([ca_layer contents] == new_contents && z_order == 0)
        [ca_layer setContentsChanged];
      else
        [ca_layer setContents:new_contents];
      [ca_layer setContentsRect:contents_rect.ToCGRect()];

      [ca_layer setAnchorPoint:CGPointZero];

      if ([ca_layer respondsToSelector:(@selector(setContentsScale:))])
        [ca_layer setContentsScale:scale_factor];
      gfx::RectF dip_frame_rect = gfx::RectF(pixel_frame_rect);
      dip_frame_rect.Scale(1 / scale_factor);
      [ca_layer setBounds:CGRectMake(0, 0, dip_frame_rect.width(),
                                     dip_frame_rect.height())];
      [ca_layer
          setPosition:CGPointMake(dip_frame_rect.x(), dip_frame_rect.y())];
    }
    static bool show_borders =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kShowMacOverlayBorders);
    if (show_borders) {
      base::ScopedCFTypeRef<CGColorRef> color;
      if (!layer_needs_update) {
        // Green represents contents that are unchanged across frames.
        color.reset(CGColorCreateGenericRGB(0, 1, 0, 1));
      } else {
        // Red represents damaged contents.
        color.reset(CGColorCreateGenericRGB(1, 0, 0, 1));
      }
      [ca_layer setBorderWidth:1];
      [ca_layer setBorderColor:color];
    }
    layer_needs_update = false;
  }

 private:
  OverlayPlane(int z_order,
               base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
               const gfx::RectF& contents_rect,
               const gfx::RectF& pixel_frame_rect)
      : z_order(z_order),
        io_surface(io_surface),
        contents_rect(contents_rect),
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

  scoped_ptr<CALayerPartialDamageTree> partial_damage_tree;
  scoped_ptr<CALayerTree> ca_layer_tree;
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
      display_pending_swap_timer_(true, false),
      weak_factory_(this) {
  helper_.reset(new ImageTransportHelper(this, manager, stub, handle));
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
}

ImageTransportSurfaceOverlayMac::~ImageTransportSurfaceOverlayMac() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  Destroy();
}

bool ImageTransportSurfaceOverlayMac::Initialize(
    gfx::GLSurface::Format format) {
  if (!helper_->Initialize(format))
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

  current_partial_damage_tree_.reset();
  current_ca_layer_tree_.reset();
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
  new_swap->partial_damage_tree.swap(pending_partial_damage_tree_);
  new_swap->ca_layer_tree.swap(pending_ca_layer_tree_);
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
  {
    gfx::RectF pixel_damage_rect = gfx::RectF(swap->pixel_damage_rect);
    ScopedCAActionDisabler disabler;
    if (swap->ca_layer_tree) {
      swap->ca_layer_tree->CommitScheduledCALayers(
          ca_root_layer_.get(), std::move(current_ca_layer_tree_),
          swap->scale_factor);
      current_ca_layer_tree_.swap(swap->ca_layer_tree);
      current_partial_damage_tree_.reset();
    } else if (swap->partial_damage_tree) {
      swap->partial_damage_tree->CommitCALayers(
          ca_root_layer_.get(), std::move(current_partial_damage_tree_),
          swap->scale_factor, swap->pixel_damage_rect);
      current_partial_damage_tree_.swap(swap->partial_damage_tree);
      current_ca_layer_tree_.reset();
    } else {
      [ca_root_layer_ setSublayers:nil];
    }
    swap->ca_layer_tree.reset();
    swap->partial_damage_tree.reset();
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
    params.ca_context_id = [ca_context_ contextId];
  } else if (current_partial_damage_tree_) {
    params.io_surface.reset(IOSurfaceCreateMachPort(
        current_partial_damage_tree_->RootLayerIOSurface()));
  }
  params.size = swap->pixel_size;
  params.scale_factor = swap->scale_factor;
  params.latency_info.swap(swap->latency_info);
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  // Remove this from the queue, and reset any callback timers.
  pending_swaps_.pop_front();
}

void CALayerPartialDamageTree::UpdateRootAndPartialDamagePlanes(
    CALayerPartialDamageTree* old_tree,
    const gfx::RectF& pixel_damage_rect) {
  // This is the plane that will be updated this frame. It may be the root plane
  // or a child plane.
  linked_ptr<OverlayPlane> plane_for_swap;

  // If the frame's size changed, if we haven't updated the root layer, if
  // we have full damage, or if we don't support remote layers, then use the
  // root layer directly.
  if (!allow_partial_swap_ || !old_tree ||
      old_tree->root_plane_->pixel_frame_rect !=
          root_plane_->pixel_frame_rect ||
      pixel_damage_rect == root_plane_->pixel_frame_rect) {
    plane_for_swap = root_plane_;
  }

  // Walk though the old tree's partial damage layers and see if there is one
  // that is appropriate to re-use.
  if (!plane_for_swap.get() && !pixel_damage_rect.IsEmpty()) {
    gfx::RectF plane_to_reuse_dip_enlarged_rect;

    // Find the last partial damage plane to re-use the CALayer from. Grow the
    // new rect for this layer to include this damage, and all nearby partial
    // damage layers.
    linked_ptr<OverlayPlane> plane_to_reuse;
    for (auto& old_plane : old_tree->partial_damage_planes_) {
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
      enlarged_contents_rect.Scale(1. / root_plane_->pixel_frame_rect.width(),
                                   1. / root_plane_->pixel_frame_rect.height());

      plane_for_swap = OverlayPlane::CreateWithFrameRect(
          0, root_plane_->io_surface, plane_to_reuse_dip_enlarged_rect,
          enlarged_contents_rect);

      plane_for_swap->TakeCALayerFrom(plane_to_reuse.get());
      if (plane_to_reuse != old_tree->partial_damage_planes_.back())
        [plane_for_swap->ca_layer removeFromSuperlayer];
    }
  }

  // If we haven't found an appropriate layer to re-use, create a new one, if
  // we haven't already created too many.
  if (!plane_for_swap.get() && !pixel_damage_rect.IsEmpty() &&
      old_tree->partial_damage_planes_.size() < kMaximumPartialDamageLayers) {
    gfx::RectF contents_rect = gfx::RectF(pixel_damage_rect);
    contents_rect.Scale(1. / root_plane_->pixel_frame_rect.width(),
                        1. / root_plane_->pixel_frame_rect.height());
    plane_for_swap = OverlayPlane::CreateWithFrameRect(
        0, root_plane_->io_surface, pixel_damage_rect, contents_rect);
  }

  // And if we still don't have a layer, use the root layer.
  if (!plane_for_swap.get() && !pixel_damage_rect.IsEmpty())
    plane_for_swap = root_plane_;

  // Walk all old partial damage planes. Remove anything that is now completely
  // covered, and move everything else into the new |partial_damage_planes_|.
  if (old_tree) {
    for (auto& old_plane : old_tree->partial_damage_planes_) {
      // Intersect the planes' frames with the new root plane to ensure that
      // they don't get kept alive inappropriately.
      gfx::RectF old_plane_frame_rect = old_plane->pixel_frame_rect;
      old_plane_frame_rect.Intersect(root_plane_->pixel_frame_rect);

      bool old_plane_covered_by_swap = false;
      if (plane_for_swap.get() &&
          plane_for_swap->pixel_frame_rect.Contains(old_plane_frame_rect)) {
        old_plane_covered_by_swap = true;
      }
      if (!old_plane_covered_by_swap) {
        DCHECK(old_plane->ca_layer);
        partial_damage_planes_.push_back(old_plane);
      }
    }
    if (plane_for_swap != root_plane_)
      root_plane_ = old_tree->root_plane_;
  }

  // Finally, add the new swap's plane at the back of the list, if it exists.
  if (plane_for_swap.get() && plane_for_swap != root_plane_) {
    partial_damage_planes_.push_back(plane_for_swap);
  }
}

void CALayerPartialDamageTree::UpdateRootAndPartialDamageCALayers(
    CALayer* superlayer,
    float scale_factor) {
  if (!allow_partial_swap_) {
    DCHECK(partial_damage_planes_.empty());
    return;
  }

  // Allocate and update CALayers for the backbuffer and partial damage layers.
  if (!root_plane_->ca_layer) {
    root_plane_->ca_layer.reset([[CALayer alloc] init]);
    [superlayer setSublayers:nil];
    [superlayer addSublayer:root_plane_->ca_layer];
  }
  for (auto& plane : partial_damage_planes_) {
    if (!plane->ca_layer) {
      DCHECK(plane == partial_damage_planes_.back());
      plane->ca_layer.reset([[CALayer alloc] init]);
    }
    if (![plane->ca_layer superlayer]) {
      DCHECK(plane == partial_damage_planes_.back());
      [superlayer addSublayer:plane->ca_layer];
    }
  }
  root_plane_->UpdateProperties(scale_factor);
  for (auto& plane : partial_damage_planes_)
    plane->UpdateProperties(scale_factor);
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
  if (transform != gfx::OVERLAY_TRANSFORM_NONE) {
    DLOG(ERROR) << "Invalid overlay plane transform.";
    return false;
  }
  if (z_order) {
    DLOG(ERROR) << "Invalid non-zero Z order.";
    return false;
  }
  if (pending_partial_damage_tree_) {
    DLOG(ERROR) << "Only one overlay per swap is allowed.";
    return false;
  }
  pending_partial_damage_tree_.reset(new CALayerPartialDamageTree(
      use_remote_layer_api_,
      static_cast<gl::GLImageIOSurface*>(image)->io_surface(),
      pixel_frame_rect));
  return true;
}

bool ImageTransportSurfaceOverlayMac::ScheduleCALayer(
    gl::GLImage* contents_image,
    const gfx::RectF& contents_rect,
    float opacity,
    unsigned background_color,
    unsigned edge_aa_mask,
    const gfx::RectF& rect,
    bool is_clipped,
    const gfx::RectF& clip_rect,
    const gfx::Transform& transform,
    int sorting_context_id) {
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  if (contents_image) {
    io_surface =
        static_cast<gl::GLImageIOSurface*>(contents_image)->io_surface();
  }
  if (!pending_ca_layer_tree_)
    pending_ca_layer_tree_.reset(new CALayerTree);
  return pending_ca_layer_tree_->ScheduleCALayer(
      is_clipped, gfx::ToEnclosingRect(clip_rect), sorting_context_id,
      transform, io_surface, contents_rect, gfx::ToEnclosingRect(rect),
      background_color, edge_aa_mask, opacity);
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
                                             float scale_factor,
                                             bool has_alpha) {
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

CALayerPartialDamageTree::CALayerPartialDamageTree(
    bool allow_partial_swap,
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const gfx::Rect& pixel_frame_rect)
    : allow_partial_swap_(allow_partial_swap) {
  root_plane_ = OverlayPlane::CreateWithFrameRect(
      0, io_surface, gfx::RectF(pixel_frame_rect), gfx::RectF(0, 0, 1, 1));
}

CALayerPartialDamageTree::~CALayerPartialDamageTree() {}

base::ScopedCFTypeRef<IOSurfaceRef>
CALayerPartialDamageTree::RootLayerIOSurface() {
  return root_plane_->io_surface;
}

void CALayerPartialDamageTree::CommitCALayers(
    CALayer* superlayer,
    scoped_ptr<CALayerPartialDamageTree> old_tree,
    float scale_factor,
    const gfx::Rect& pixel_damage_rect) {
  UpdateRootAndPartialDamagePlanes(old_tree.get(),
                                   gfx::RectF(pixel_damage_rect));
  UpdateRootAndPartialDamageCALayers(superlayer, scale_factor);
}

}  // namespace content
