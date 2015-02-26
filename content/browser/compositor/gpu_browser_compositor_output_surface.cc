// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"

#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface_client.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace content {

GpuBrowserCompositorOutputSurface::GpuBrowserCompositorOutputSurface(
    const scoped_refptr<ContextProviderCommandBuffer>& context,
    const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
    scoped_ptr<cc::OverlayCandidateValidator> overlay_candidate_validator)
    : BrowserCompositorOutputSurface(context,
                                     vsync_manager),
#if defined(OS_MACOSX)
      should_not_show_frames_(false),
#endif
      swap_buffers_completion_callback_(
          base::Bind(&GpuBrowserCompositorOutputSurface::OnSwapBuffersCompleted,
                     base::Unretained(this))),
      update_vsync_parameters_callback_(base::Bind(
          &BrowserCompositorOutputSurface::OnUpdateVSyncParametersFromGpu,
          base::Unretained(this))) {
  overlay_candidate_validator_ = overlay_candidate_validator.Pass();
}

GpuBrowserCompositorOutputSurface::~GpuBrowserCompositorOutputSurface() {}

CommandBufferProxyImpl*
GpuBrowserCompositorOutputSurface::GetCommandBufferProxy() {
  ContextProviderCommandBuffer* provider_command_buffer =
      static_cast<content::ContextProviderCommandBuffer*>(
          context_provider_.get());
  CommandBufferProxyImpl* command_buffer_proxy =
      provider_command_buffer->GetCommandBufferProxy();
  DCHECK(command_buffer_proxy);
  return command_buffer_proxy;
}

bool GpuBrowserCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  if (!BrowserCompositorOutputSurface::BindToClient(client))
    return false;

  GetCommandBufferProxy()->SetSwapBuffersCompletionCallback(
      swap_buffers_completion_callback_.callback());
  GetCommandBufferProxy()->SetUpdateVSyncParametersCallback(
      update_vsync_parameters_callback_.callback());
  return true;
}

void GpuBrowserCompositorOutputSurface::SwapBuffers(
    cc::CompositorFrame* frame) {
  DCHECK(frame->gl_frame_data);

  GetCommandBufferProxy()->SetLatencyInfo(frame->metadata.latency_info);

  if (reflector_) {
    if (frame->gl_frame_data->sub_buffer_rect ==
        gfx::Rect(frame->gl_frame_data->size))
      reflector_->OnSourceSwapBuffers();
    else
      reflector_->OnSourcePostSubBuffer(frame->gl_frame_data->sub_buffer_rect);
  }

  if (frame->gl_frame_data->sub_buffer_rect ==
      gfx::Rect(frame->gl_frame_data->size)) {
    context_provider_->ContextSupport()->Swap();
  } else {
    context_provider_->ContextSupport()->PartialSwapBuffers(
        frame->gl_frame_data->sub_buffer_rect);
  }

  client_->DidSwapBuffers();

#if defined(OS_MACOSX)
  if (should_not_show_frames_)
    should_not_show_frames_ = false;
#endif
}

void GpuBrowserCompositorOutputSurface::OnSwapBuffersCompleted(
    const std::vector<ui::LatencyInfo>& latency_info) {
#if defined(OS_MACOSX)
  // On Mac, delay acknowledging the swap to the output surface client until
  // it has been drawn, see OnSurfaceDisplayed();
  NOTREACHED();
#else
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    RenderWidgetHostImpl::CompositorFrameDrawn(latency_info);
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&RenderWidgetHostImpl::CompositorFrameDrawn, latency_info));
  }
  OnSwapBuffersComplete();
#endif
}

#if defined(OS_MACOSX)
void GpuBrowserCompositorOutputSurface::OnSurfaceDisplayed() {
  cc::OutputSurface::OnSwapBuffersComplete();
}

void GpuBrowserCompositorOutputSurface::OnSurfaceRecycled() {
  // Discard the backbuffer immediately. This is necessary only when using a
  // ImageTransportSurfaceFBO with a CALayerStorageProvider. Discarding the
  // backbuffer results in the next frame using a new CALayer and CAContext,
  // which guarantees that the browser will not flash stale content when adding
  // the remote CALayer to the NSView hierarchy (it could flash stale content
  // because the system window server is not synchronized with any signals we
  // control or observe).
  DiscardBackbuffer();
  // It may be that there are frames in-flight from the GPU process back to the
  // browser. Make sure that these frames are not displayed by ignoring them in
  // GpuProcessHostUIShim, until the browser issues a SwapBuffers for the new
  // content.
  should_not_show_frames_ = true;
}

bool GpuBrowserCompositorOutputSurface::ShouldNotShowFramesAfterRecycle()
    const {
  return should_not_show_frames_;
}
#endif

}  // namespace content
