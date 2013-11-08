// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aura/gpu_browser_compositor_output_surface.h"

#include "cc/output/compositor_frame.h"
#include "content/browser/aura/reflector_impl.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"

namespace content {

GpuBrowserCompositorOutputSurface::GpuBrowserCompositorOutputSurface(
    const scoped_refptr<ContextProviderCommandBuffer>& context,
    int surface_id,
    IDMap<BrowserCompositorOutputSurface>* output_surface_map,
    base::MessageLoopProxy* compositor_message_loop,
    base::WeakPtr<ui::Compositor> compositor)
    : BrowserCompositorOutputSurface(context,
                                     surface_id,
                                     output_surface_map,
                                     compositor_message_loop,
                                     compositor) {}

GpuBrowserCompositorOutputSurface::~GpuBrowserCompositorOutputSurface() {}

void GpuBrowserCompositorOutputSurface::SwapBuffers(
    cc::CompositorFrame* frame) {
  DCHECK(frame->gl_frame_data);

  WebGraphicsContext3DCommandBufferImpl* command_buffer_context =
      static_cast<WebGraphicsContext3DCommandBufferImpl*>(
          context_provider_->Context3d());
  CommandBufferProxyImpl* command_buffer_proxy =
      command_buffer_context->GetCommandBufferProxy();
  DCHECK(command_buffer_proxy);
  context_provider_->Context3d()->shallowFlushCHROMIUM();
  command_buffer_proxy->SetLatencyInfo(frame->metadata.latency_info);

  if (reflector_.get()) {
    if (frame->gl_frame_data->sub_buffer_rect ==
        gfx::Rect(frame->gl_frame_data->size))
      reflector_->OnSwapBuffers();
    else
      reflector_->OnPostSubBuffer(frame->gl_frame_data->sub_buffer_rect);
  }

  OutputSurface::SwapBuffers(frame);
}

}  // namespace content
