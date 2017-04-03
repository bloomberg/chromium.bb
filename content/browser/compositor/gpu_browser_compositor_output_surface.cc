// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"

#include <utility>

#include "build/build_config.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/output_surface_frame.h"
#include "components/display_compositor/compositor_overlay_candidate_validator.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/browser/compositor/reflector_texture.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"

namespace content {

GpuBrowserCompositorOutputSurface::GpuBrowserCompositorOutputSurface(
    scoped_refptr<ui::ContextProviderCommandBuffer> context,
    const UpdateVSyncParametersCallback& update_vsync_parameters_callback,
    std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
        overlay_candidate_validator)
    : BrowserCompositorOutputSurface(std::move(context),
                                     update_vsync_parameters_callback,
                                     std::move(overlay_candidate_validator)),
      weak_ptr_factory_(this) {
  if (capabilities_.uses_default_gl_framebuffer) {
    capabilities_.flipped_output_surface =
        context_provider()->ContextCapabilities().flips_vertically;
  }
  capabilities_.supports_stencil =
      context_provider()->ContextCapabilities().num_stencil_bits > 0;
}

GpuBrowserCompositorOutputSurface::~GpuBrowserCompositorOutputSurface() {
  GetCommandBufferProxy()->SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback());
}

void GpuBrowserCompositorOutputSurface::SetNeedsVSync(bool needs_vsync) {
#if defined(OS_WIN)
  GetCommandBufferProxy()->SetNeedsVSync(needs_vsync);
#else
  NOTREACHED();
#endif  // defined(OS_WIN)
}

void GpuBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted(
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::SwapResult result,
    const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) {
  RenderWidgetHostImpl::CompositorFrameDrawn(latency_info);
  client_->DidReceiveSwapBuffersAck();
}

void GpuBrowserCompositorOutputSurface::OnReflectorChanged() {
  if (!reflector_) {
    reflector_texture_.reset();
  } else {
    reflector_texture_.reset(new ReflectorTexture(context_provider()));
    reflector_->OnSourceTextureMailboxUpdated(reflector_texture_->mailbox());
  }
  reflector_texture_defined_ = false;
}

void GpuBrowserCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;

  GetCommandBufferProxy()->SetSwapBuffersCompletionCallback(
      base::Bind(&GpuBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
  GetCommandBufferProxy()->SetUpdateVSyncParametersCallback(
      update_vsync_parameters_callback_);
}

void GpuBrowserCompositorOutputSurface::EnsureBackbuffer() {}

void GpuBrowserCompositorOutputSurface::DiscardBackbuffer() {
  context_provider()->ContextGL()->DiscardBackbufferCHROMIUM();
}

void GpuBrowserCompositorOutputSurface::BindFramebuffer() {
  context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GpuBrowserCompositorOutputSurface::Reshape(
    const gfx::Size& size,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha,
    bool use_stencil) {
  size_ = size;
  has_set_draw_rectangle_since_last_resize_ = false;
  context_provider()->ContextGL()->ResizeCHROMIUM(
      size.width(), size.height(), device_scale_factor, has_alpha);
}

void GpuBrowserCompositorOutputSurface::SwapBuffers(
    cc::OutputSurfaceFrame frame) {
  GetCommandBufferProxy()->SetLatencyInfo(frame.latency_info);

  gfx::Size surface_size = frame.size;
  if (reflector_) {
    if (frame.sub_buffer_rect && reflector_texture_defined_) {
      reflector_texture_->CopyTextureSubImage(*frame.sub_buffer_rect);
      reflector_->OnSourcePostSubBuffer(*frame.sub_buffer_rect, surface_size);
    } else {
      reflector_texture_->CopyTextureFullImage(surface_size);
      reflector_->OnSourceSwapBuffers(surface_size);
      reflector_texture_defined_ = true;
    }
  }

  set_draw_rectangle_for_frame_ = false;

  if (frame.sub_buffer_rect) {
    DCHECK(frame.content_bounds.empty());
    context_provider_->ContextSupport()->PartialSwapBuffers(
        *frame.sub_buffer_rect);
  } else if (!frame.content_bounds.empty()) {
    context_provider_->ContextSupport()->SwapWithBounds(frame.content_bounds);
  } else {
    context_provider_->ContextSupport()->Swap();
  }
}

uint32_t GpuBrowserCompositorOutputSurface::GetFramebufferCopyTextureFormat() {
  auto* gl = static_cast<ui::ContextProviderCommandBuffer*>(context_provider());
  return gl->GetCopyTextureInternalFormat();
}

bool GpuBrowserCompositorOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned GpuBrowserCompositorOutputSurface::GetOverlayTextureId() const {
  return 0;
}

bool GpuBrowserCompositorOutputSurface::SurfaceIsSuspendForRecycle() const {
  return false;
}

#if defined(OS_MACOSX)
void GpuBrowserCompositorOutputSurface::SetSurfaceSuspendedForRecycle(
    bool suspended) {}
#endif

void GpuBrowserCompositorOutputSurface::SetDrawRectangle(
    const gfx::Rect& rect) {
  if (set_draw_rectangle_for_frame_)
    return;
  DCHECK(gfx::Rect(size_).Contains(rect));
  DCHECK(has_set_draw_rectangle_since_last_resize_ ||
         (gfx::Rect(size_) == rect));
  set_draw_rectangle_for_frame_ = true;
  has_set_draw_rectangle_since_last_resize_ = true;
  context_provider()->ContextGL()->SetDrawRectangleCHROMIUM(
      rect.x(), rect.y(), rect.width(), rect.height());
}

gpu::CommandBufferProxyImpl*
GpuBrowserCompositorOutputSurface::GetCommandBufferProxy() {
  ui::ContextProviderCommandBuffer* provider_command_buffer =
      static_cast<ui::ContextProviderCommandBuffer*>(context_provider_.get());
  gpu::CommandBufferProxyImpl* command_buffer_proxy =
      provider_command_buffer->GetCommandBufferProxy();
  DCHECK(command_buffer_proxy);
  return command_buffer_proxy;
}

}  // namespace content
