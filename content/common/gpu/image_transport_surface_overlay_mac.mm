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

#include "base/mac/scoped_cftyperef.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/ca_layer_partial_damage_tree_mac.h"
#include "content/common/gpu/ca_layer_tree_mac.h"
#include "content/common/gpu/gpu_messages.h"
#include "ui/accelerated_widget_mac/io_surface_context.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/cocoa/remote_layer_api.h"
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
      TRACE_EVENT0("gpu", "Blank frame: No overlays or CALayers");
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

}  // namespace content
