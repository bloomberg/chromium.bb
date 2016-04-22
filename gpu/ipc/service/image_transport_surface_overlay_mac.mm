// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface_overlay_mac.h"

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

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/trace_event/trace_event.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "ui/accelerated_widget_mac/ca_layer_partial_damage_tree_mac.h"
#include "ui/accelerated_widget_mac/ca_layer_tree_mac.h"
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

void CheckGLErrors(const char* msg) {
  GLenum gl_error;
  while ((gl_error = glGetError()) != GL_NO_ERROR) {
    LOG(ERROR) << "OpenGL error hit " << msg << ": " << gl_error;
  }
}

void IOSurfaceContextNoOp(scoped_refptr<ui::IOSurfaceContext>) {
}

}  // namespace

namespace gpu {

scoped_refptr<gfx::GLSurface> ImageTransportSurfaceCreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    SurfaceHandle handle) {
  return new ImageTransportSurfaceOverlayMac(manager, stub, handle);
}

ImageTransportSurfaceOverlayMac::ImageTransportSurfaceOverlayMac(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    SurfaceHandle handle)
    : manager_(manager),
      stub_(stub->AsWeakPtr()),
      handle_(handle),
      use_remote_layer_api_(ui::RemoteLayerAPISupported()),
      scale_factor_(1),
      gl_renderer_id_(0),
      vsync_parameters_valid_(false) {
  manager_->AddBufferPresentedCallback(
      handle_, base::Bind(&ImageTransportSurfaceOverlayMac::BufferPresented,
                          base::Unretained(this)));
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
}

ImageTransportSurfaceOverlayMac::~ImageTransportSurfaceOverlayMac() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  if (stub_.get()) {
    stub_->SetLatencyInfoCallback(
        base::Callback<void(const std::vector<ui::LatencyInfo>&)>());
  }
  Destroy();
  manager_->RemoveBufferPresentedCallback(handle_);
}

bool ImageTransportSurfaceOverlayMac::Initialize(
    gfx::GLSurface::Format format) {
  if (!stub_.get() || !stub_->decoder())
    return false;

  stub_->SetLatencyInfoCallback(
      base::Bind(&ImageTransportSurfaceOverlayMac::SetLatencyInfo,
                 base::Unretained(this)));

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
  current_partial_damage_tree_.reset();
  current_ca_layer_tree_.reset();
}

bool ImageTransportSurfaceOverlayMac::IsOffscreen() {
  return false;
}

void ImageTransportSurfaceOverlayMac::SetLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  latency_info_.insert(latency_info_.end(), latency_info.begin(),
                       latency_info.end());
}

void ImageTransportSurfaceOverlayMac::BufferPresented(
    int32_t surface_id,
    const base::TimeTicks& vsync_timebase,
    const base::TimeDelta& vsync_interval) {
  vsync_timebase_ = vsync_timebase;
  vsync_interval_ = vsync_interval;
  vsync_parameters_valid_ = !vsync_interval_.is_zero();

  // Compute |vsync_timebase_| to be the first vsync after time zero.
  if (vsync_parameters_valid_) {
    vsync_timebase_ -=
        vsync_interval_ *
        ((vsync_timebase_ - base::TimeTicks()) / vsync_interval_);
  }
}

void ImageTransportSurfaceOverlayMac::SendAcceleratedSurfaceBuffersSwapped(
    int32_t surface_id,
    CAContextID ca_context_id,
    const gfx::ScopedRefCountedIOSurfaceMachPort& io_surface,
    const gfx::Size& size,
    float scale_factor,
    std::vector<ui::LatencyInfo> latency_info) {
  // TRACE_EVENT for gpu tests:
  TRACE_EVENT_INSTANT2("test_gpu", "SwapBuffers", TRACE_EVENT_SCOPE_THREAD,
                       "GLImpl", static_cast<int>(gfx::GetGLImplementation()),
                       "width", size.width());
  // On mac, handle_ is a surface id. See
  // GpuProcessTransportFactory::CreatePerCompositorData
  manager_->delegate()->SendAcceleratedSurfaceBuffersSwapped(
      surface_id, ca_context_id, io_surface, size, scale_factor,
      std::move(latency_info));
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::SwapBuffersInternal(
    const gfx::Rect& pixel_damage_rect) {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::SwapBuffersInternal");

  // A glFlush is necessary to ensure correct content appears. A glFinish
  // appears empirically to be the best way to get maximum performance when
  // GPU bound.
  {
    gfx::ScopedSetGLToRealGLApi scoped_set_gl_api;
    TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::glFinish");
    CheckGLErrors("Before finish");
    glFinish();
    CheckGLErrors("After finish");
  }

  base::TimeTicks finish_time = base::TimeTicks::Now();

  // Update the CALayer hierarchy.
  {
    ScopedCAActionDisabler disabler;
    if (pending_ca_layer_tree_) {
      pending_ca_layer_tree_->CommitScheduledCALayers(
          ca_root_layer_.get(), std::move(current_ca_layer_tree_),
          scale_factor_);
      current_ca_layer_tree_.swap(pending_ca_layer_tree_);
      current_partial_damage_tree_.reset();
    } else if (pending_partial_damage_tree_) {
      pending_partial_damage_tree_->CommitCALayers(
          ca_root_layer_.get(), std::move(current_partial_damage_tree_),
          scale_factor_, pixel_damage_rect);
      current_partial_damage_tree_.swap(pending_partial_damage_tree_);
      current_ca_layer_tree_.reset();
    } else {
      TRACE_EVENT0("gpu", "Blank frame: No overlays or CALayers");
      [ca_root_layer_ setSublayers:nil];
      current_partial_damage_tree_.reset();
      current_ca_layer_tree_.reset();
    }
  }

  // Update the latency info to reflect the swap time.
  for (auto latency_info : latency_info_) {
    latency_info.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0, 0, finish_time, 1);
    latency_info.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0,
        finish_time, 1);
  }

  // Send acknowledgement to the browser.
  CAContextID ca_context_id = 0;
  gfx::ScopedRefCountedIOSurfaceMachPort io_surface;
  if (use_remote_layer_api_) {
    ca_context_id = [ca_context_ contextId];
  } else if (current_partial_damage_tree_) {
    io_surface.reset(IOSurfaceCreateMachPort(
        current_partial_damage_tree_->RootLayerIOSurface()));
  }
  SendAcceleratedSurfaceBuffersSwapped(handle_, ca_context_id, io_surface,
                                       pixel_size_, scale_factor_,
                                       std::move(latency_info_));

  // Reset all state for the next frame.
  latency_info_.clear();
  pending_ca_layer_tree_.reset();
  pending_partial_damage_tree_.reset();
  return gfx::SwapResult::SWAP_ACK;
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
  pending_partial_damage_tree_.reset(new ui::CALayerPartialDamageTree(
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
    int sorting_context_id,
    unsigned filter) {
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer;
  if (contents_image) {
    gl::GLImageIOSurface* io_surface_image =
        static_cast<gl::GLImageIOSurface*>(contents_image);
    io_surface = io_surface_image->io_surface();
    cv_pixel_buffer = io_surface_image->cv_pixel_buffer();
  }
  if (!pending_ca_layer_tree_)
    pending_ca_layer_tree_.reset(new ui::CALayerTree);
  return pending_ca_layer_tree_->ScheduleCALayer(
      is_clipped, gfx::ToEnclosingRect(clip_rect), sorting_context_id,
      transform, io_surface, cv_pixel_buffer, contents_rect,
      gfx::ToEnclosingRect(rect), background_color, edge_aa_mask, opacity);
}

bool ImageTransportSurfaceOverlayMac::IsSurfaceless() const {
  return true;
}

bool ImageTransportSurfaceOverlayMac::Resize(const gfx::Size& pixel_size,
                                             float scale_factor,
                                             bool has_alpha) {
  // Flush through any pending frames.
  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;
  return true;
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

}  // namespace gpu
