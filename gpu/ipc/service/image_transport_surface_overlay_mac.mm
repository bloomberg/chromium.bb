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
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "ui/accelerated_widget_mac/ca_layer_tree_coordinator.h"
#include "ui/accelerated_widget_mac/io_surface_context.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/swap_result.h"
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
  return new ImageTransportSurfaceOverlayMac(stub, handle);
}

ImageTransportSurfaceOverlayMac::ImageTransportSurfaceOverlayMac(
    GpuCommandBufferStub* stub,
    SurfaceHandle handle)
    : stub_(stub->AsWeakPtr()),
      handle_(handle),
      use_remote_layer_api_(ui::RemoteLayerAPISupported()),
      scale_factor_(1),
      gl_renderer_id_(0) {
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
  ca_layer_tree_coordinator_.reset(
      new ui::CALayerTreeCoordinator(use_remote_layer_api_));
}

ImageTransportSurfaceOverlayMac::~ImageTransportSurfaceOverlayMac() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  if (stub_.get()) {
    stub_->SetLatencyInfoCallback(
        base::Callback<void(const std::vector<ui::LatencyInfo>&)>());
  }
  Destroy();
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
    [ca_context_ setLayer:ca_layer_tree_coordinator_->GetCALayerForDisplay()];

    fullscreen_low_power_ca_context_.reset([
        [CAContext contextWithCGSConnection:connection_id options:@{}] retain]);
    [fullscreen_low_power_ca_context_ setLayer:
        ca_layer_tree_coordinator_->GetFullscreenLowPowerLayerForDisplay()];
  }
  return true;
}

void ImageTransportSurfaceOverlayMac::Destroy() {
  ca_layer_tree_coordinator_.reset();
}

bool ImageTransportSurfaceOverlayMac::IsOffscreen() {
  return false;
}

void ImageTransportSurfaceOverlayMac::SetLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  latency_info_.insert(latency_info_.end(), latency_info.begin(),
                       latency_info.end());
}

void ImageTransportSurfaceOverlayMac::SendAcceleratedSurfaceBuffersSwapped(
    gpu::SurfaceHandle surface_handle,
    CAContextID ca_context_id,
    bool fullscreen_low_power_ca_context_valid,
    CAContextID fullscreen_low_power_ca_context_id,
    const gfx::ScopedRefCountedIOSurfaceMachPort& io_surface,
    const gfx::Size& size,
    float scale_factor,
    std::vector<ui::LatencyInfo> latency_info) {
  // TRACE_EVENT for gpu tests:
  TRACE_EVENT_INSTANT2("test_gpu", "SwapBuffers", TRACE_EVENT_SCOPE_THREAD,
                       "GLImpl", static_cast<int>(gfx::GetGLImplementation()),
                       "width", size.width());

  GpuCommandBufferMsg_SwapBuffersCompleted_Params params;
  params.surface_handle = surface_handle;
  params.ca_context_id = ca_context_id;
  params.fullscreen_low_power_ca_context_valid =
      fullscreen_low_power_ca_context_valid;
  params.fullscreen_low_power_ca_context_id =
      fullscreen_low_power_ca_context_id;
  params.io_surface = io_surface;
  params.pixel_size = size;
  params.scale_factor = scale_factor;
  params.latency_info = std::move(latency_info);
  params.result = gfx::SwapResult::SWAP_ACK;
  stub_->SendSwapBuffersCompleted(params);
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

  bool fullscreen_low_power_layer_valid = false;
  ca_layer_tree_coordinator_->CommitPendingTreesToCA(
      pixel_damage_rect, &fullscreen_low_power_layer_valid);
  // TODO(ccameron): Plumb the fullscreen low power layer through to the
  // appropriate window.

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
  CAContextID fullscreen_low_power_ca_context_id = 0;
  gfx::ScopedRefCountedIOSurfaceMachPort io_surface_mach_port;
  if (use_remote_layer_api_) {
    ca_context_id = [ca_context_ contextId];
    fullscreen_low_power_ca_context_id =
        [fullscreen_low_power_ca_context_ contextId];
  } else {
    IOSurfaceRef io_surface =
        ca_layer_tree_coordinator_->GetIOSurfaceForDisplay();
    if (io_surface)
      io_surface_mach_port.reset(IOSurfaceCreateMachPort(io_surface));
  }
  SendAcceleratedSurfaceBuffersSwapped(
      handle_, ca_context_id, fullscreen_low_power_layer_valid,
      fullscreen_low_power_ca_context_id, io_surface_mach_port, pixel_size_,
      scale_factor_, std::move(latency_info_));

  // Reset all state for the next frame.
  latency_info_.clear();
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
  return ca_layer_tree_coordinator_->SetPendingGLRendererBackbuffer(
      static_cast<gl::GLImageIOSurface*>(image)->io_surface());
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
  return ca_layer_tree_coordinator_->GetPendingCARendererLayerTree()
      ->ScheduleCALayer(is_clipped, gfx::ToEnclosingRect(clip_rect),
                        sorting_context_id, transform, io_surface,
                        cv_pixel_buffer, contents_rect,
                        gfx::ToEnclosingRect(rect), background_color,
                        edge_aa_mask, opacity, filter);
}

bool ImageTransportSurfaceOverlayMac::IsSurfaceless() const {
  return true;
}

bool ImageTransportSurfaceOverlayMac::Resize(const gfx::Size& pixel_size,
                                             float scale_factor,
                                             bool has_alpha) {
  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;
  ca_layer_tree_coordinator_->Resize(pixel_size, scale_factor);
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
