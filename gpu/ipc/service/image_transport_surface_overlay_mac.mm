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
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "ui/accelerated_widget_mac/ca_layer_tree_coordinator.h"
#include "ui/accelerated_widget_mac/io_surface_context.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/swap_result.h"
#include "ui/gfx/transform.h"
#include "ui/gl/ca_renderer_layer_params.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_image_io_surface.h"
#include "ui/gl/gpu_switching_manager.h"
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

ImageTransportSurfaceOverlayMac::ImageTransportSurfaceOverlayMac(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate)
    : delegate_(delegate),
      use_remote_layer_api_(ui::RemoteLayerAPISupported()),
      scale_factor_(1),
      gl_renderer_id_(0) {
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);

  static bool av_disabled_at_command_line =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAVFoundationOverlays);

  bool allow_av_sample_buffer_display_layer =
      !av_disabled_at_command_line &&
      !delegate_->GetFeatureInfo()
           ->workarounds()
           .disable_av_sample_buffer_display_layer;

  ca_layer_tree_coordinator_.reset(new ui::CALayerTreeCoordinator(
      use_remote_layer_api_, allow_av_sample_buffer_display_layer));
}

ImageTransportSurfaceOverlayMac::~ImageTransportSurfaceOverlayMac() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  if (delegate_.get()) {
    delegate_->SetLatencyInfoCallback(
        base::Callback<void(const std::vector<ui::LatencyInfo>&)>());
  }
  Destroy();
}

bool ImageTransportSurfaceOverlayMac::Initialize(gl::GLSurfaceFormat format) {
  delegate_->SetLatencyInfoCallback(
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
  if (previous_frame_fence_) {
    // Ensure we are using the context with which the fence was created.
    gl::ScopedCGLSetCurrentContext scoped_set_current(fence_context_obj_);
    CheckGLErrors("Before destroy fence");
    previous_frame_fence_.reset();
    CheckGLErrors("After destroy fence");
  }
}

bool ImageTransportSurfaceOverlayMac::IsOffscreen() {
  return false;
}

void ImageTransportSurfaceOverlayMac::SetLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  latency_info_.insert(latency_info_.end(), latency_info.begin(),
                       latency_info.end());
}

void ImageTransportSurfaceOverlayMac::ApplyBackpressure(
    base::TimeTicks* before_flush_time,
    base::TimeTicks* after_flush_before_commit_time) {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::ApplyBackpressure");

  // If supported, use GLFence to ensure that we haven't gotten more than one
  // frame ahead of GL.
  if (gl::GLFence::IsSupported()) {
    CheckGLErrors("Before fence/flush");

    // If we have gotten more than one frame ahead of GL, wait for the previous
    // frame to complete.
    if (previous_frame_fence_) {
      TRACE_EVENT0("gpu", "ClientWait");

      // Ensure we are using the context with which the fence was created.
      gl::ScopedCGLSetCurrentContext scoped_set_current(fence_context_obj_);

      // While we could call GLFence::ClientWait, this performs a busy wait on
      // Mac, leading to high CPU usage. Instead we poll with a 1ms delay. This
      // should have minimal impact, as we will only hit this path when we are
      // more than one frame (16ms) behind.
      //
      // Note that on some platforms (10.9), fences appear to sometimes get
      // lost and will never pass. Add a 32ms timout to prevent these
      // situations from causing a GPU process hang. crbug.com/618075
      int timeout_msec = 32;
      while (!previous_frame_fence_->HasCompleted() && timeout_msec > 0) {
        --timeout_msec;
        base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
      }
      if (!previous_frame_fence_->HasCompleted()) {
        // We timed out waiting for the above fence, just issue a glFinish.
        glFinish();
      }
    }

    *before_flush_time = base::TimeTicks::Now();

    // Create a fence for the current frame's work and save the context.
    previous_frame_fence_.reset(gl::GLFence::Create());
    fence_context_obj_.reset(CGLGetCurrentContext(),
                             base::scoped_policy::RETAIN);

    // A glFlush is necessary to ensure correct content appears.
    {
      TRACE_EVENT0("gpu", "glFlush");
      glFlush();
      CheckGLErrors("After fence/flush");
    }

    *after_flush_before_commit_time = base::TimeTicks::Now();
    UMA_HISTOGRAM_TIMES("GPU.IOSurface.GLFlushTime",
                        *after_flush_before_commit_time - *before_flush_time);
  } else {
    // GLFence isn't supported - issue a glFinish on each frame to ensure
    // there is backpressure from GL.
    TRACE_EVENT0("gpu", "glFinish");
    CheckGLErrors("Before finish");
    glFinish();
    CheckGLErrors("After finish");
    *after_flush_before_commit_time = base::TimeTicks::Now();
  }
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::SwapBuffersInternal(
    const gfx::Rect& pixel_damage_rect) {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::SwapBuffersInternal");

  // Do a GL fence for flush to apply back-pressure before drawing.
  base::TimeTicks before_flush_time;
  base::TimeTicks after_flush_before_commit_time;
  ApplyBackpressure(&before_flush_time, &after_flush_before_commit_time);

  // Do the CATransaction to update the CALayer tree.
  bool fullscreen_low_power_layer_valid = false;
  {
    TRACE_EVENT0("gpu", "CommitPendingTreesToCA");
    ca_layer_tree_coordinator_->CommitPendingTreesToCA(
        pixel_damage_rect, &fullscreen_low_power_layer_valid);
  }

  base::TimeTicks after_transaction_time = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("GPU.IOSurface.CATransactionTime",
                      after_transaction_time - after_flush_before_commit_time);

  // Update the latency info to reflect the swap time.
  for (auto& latency_info : latency_info_) {
    latency_info.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0, 0,
        after_flush_before_commit_time, 1);
    latency_info.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0,
        after_flush_before_commit_time, 1);
  }

  // Populate the swap-complete parameters to send to the browser.
  SwapBuffersCompleteParams params;
  {
    TRACE_EVENT_INSTANT2("test_gpu", "SwapBuffers", TRACE_EVENT_SCOPE_THREAD,
                         "GLImpl", static_cast<int>(gl::GetGLImplementation()),
                         "width", pixel_size_.width());
    if (use_remote_layer_api_) {
      params.ca_context_id = [ca_context_ contextId];
      params.fullscreen_low_power_ca_context_id =
          [fullscreen_low_power_ca_context_ contextId];
      params.fullscreen_low_power_ca_context_valid =
          fullscreen_low_power_layer_valid;
    } else {
      IOSurfaceRef io_surface =
          ca_layer_tree_coordinator_->GetIOSurfaceForDisplay();
      if (io_surface) {
        params.io_surface.reset(IOSurfaceCreateMachPort(io_surface));
      }
    }
    params.pixel_size = pixel_size_;
    params.scale_factor = scale_factor_;
    params.latency_info = std::move(latency_info_);
    params.result = gfx::SwapResult::SWAP_ACK;
    for (auto& query : ca_layer_in_use_queries_) {
      gpu::TextureInUseResponse response;
      response.texture = query.texture;
      bool in_use = false;
      gl::GLImageIOSurface* io_surface_image =
          gl::GLImageIOSurface::FromGLImage(query.image.get());
      if (io_surface_image) {
        in_use = io_surface_image->CanCheckIOSurfaceIsInUse() &&
                 IOSurfaceIsInUse(io_surface_image->io_surface());
      }
      response.in_use = in_use;
      params.in_use_responses.push_back(std::move(response));
    }
    ca_layer_in_use_queries_.clear();
  }

  // Send the swap parameters to the browser.
  delegate_->DidSwapBuffersComplete(std::move(params));

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

gl::GLSurfaceFormat ImageTransportSurfaceOverlayMac::GetFormat() {
  return gl::GLSurfaceFormat();
}

bool ImageTransportSurfaceOverlayMac::OnMakeCurrent(gl::GLContext* context) {
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
  gl::GLImageIOSurface* io_surface_image =
      gl::GLImageIOSurface::FromGLImage(image);
  if (!io_surface_image) {
    DLOG(ERROR) << "Not an IOSurface image.";
    return false;
  }
  const ui::CARendererLayerParams overlay_as_calayer_params(
      false,        // is_clipped
      gfx::Rect(),  // clip_rect
      0,            // sorting_context_id
      gfx::Transform(), image,
      crop_rect,            // contents_rect
      pixel_frame_rect,     // rect
      SK_ColorTRANSPARENT,  // background_color
      0,                    // edge_aa_mask
      1.f,                  // opacity
      GL_LINEAR);           // filter;
  return ca_layer_tree_coordinator_->GetPendingCARendererLayerTree()
      ->ScheduleCALayer(overlay_as_calayer_params);
}

bool ImageTransportSurfaceOverlayMac::ScheduleCALayer(
    const ui::CARendererLayerParams& params) {
  if (params.image) {
    gl::GLImageIOSurface* io_surface_image =
        gl::GLImageIOSurface::FromGLImage(params.image);
    if (!io_surface_image) {
      DLOG(ERROR) << "Cannot schedule CALayer with non-IOSurface GLImage";
      return false;
    }
  }
  return ca_layer_tree_coordinator_->GetPendingCARendererLayerTree()
      ->ScheduleCALayer(params);
}

void ImageTransportSurfaceOverlayMac::ScheduleCALayerInUseQuery(
    std::vector<CALayerInUseQuery> queries) {
  ca_layer_in_use_queries_.swap(queries);
}

bool ImageTransportSurfaceOverlayMac::IsSurfaceless() const {
  return true;
}

bool ImageTransportSurfaceOverlayMac::Resize(const gfx::Size& pixel_size,
                                             float scale_factor,
                                             ColorSpace color_space,
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&IOSurfaceContextNoOp, context_on_new_gpu));
}

}  // namespace gpu
