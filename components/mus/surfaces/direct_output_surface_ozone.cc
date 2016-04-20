// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/direct_output_surface_ozone.h"

#include <utility>

#include "base/bind.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface_client.h"
#include "components/mus/gles2/mojo_gpu_memory_buffer_manager.h"
#include "components/mus/surfaces/buffer_queue.h"
#include "components/mus/surfaces/surfaces_context_provider.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace mus {

DirectOutputSurfaceOzone::DirectOutputSurfaceOzone(
    const scoped_refptr<SurfacesContextProvider>& context_provider,
    gfx::AcceleratedWidget widget,
    base::SingleThreadTaskRunner* task_runner,
    uint32_t target,
    uint32_t internalformat)
    : cc::OutputSurface(context_provider),
      output_surface_(
          new BufferQueue(context_provider, target, internalformat, widget)),
      synthetic_begin_frame_source_(new cc::SyntheticBeginFrameSource(
          task_runner,
          cc::BeginFrameArgs::DefaultInterval())),
      weak_ptr_factory_(this) {
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.flipped_output_surface = true;
  // Set |max_frames_pending| to 2 for surfaceless, which aligns scheduling
  // more closely with the previous surfaced behavior.
  // With a surface, swap buffer ack used to return early, before actually
  // presenting the back buffer, enabling the browser compositor to run ahead.
  // Surfaceless implementation acks at the time of actual buffer swap, which
  // shifts the start of the new frame forward relative to the old
  // implementation.
  capabilities_.max_frames_pending = 2;

  output_surface_->Initialize();

  context_provider->SetSwapBuffersCompletionCallback(
      base::Bind(&DirectOutputSurfaceOzone::OnGpuSwapBuffersCompleted,
                 base::Unretained(this)));
}

DirectOutputSurfaceOzone::~DirectOutputSurfaceOzone() {
  // TODO(rjkroege): Support cleanup.
}

bool DirectOutputSurfaceOzone::IsDisplayedAsOverlayPlane() const {
  // TODO(rjkroege): implement remaining overlay functionality.
  return true;
}

unsigned DirectOutputSurfaceOzone::GetOverlayTextureId() const {
  DCHECK(output_surface_);
  return output_surface_->current_texture_id();
}

void DirectOutputSurfaceOzone::SwapBuffers(cc::CompositorFrame* frame) {
  DCHECK(output_surface_);
  DCHECK(frame->gl_frame_data);

  output_surface_->SwapBuffers(frame->gl_frame_data->sub_buffer_rect);

  // Code combining GpuBrowserCompositorOutputSurface + DirectOutputSurface
  if (frame->gl_frame_data->sub_buffer_rect ==
      gfx::Rect(frame->gl_frame_data->size)) {
    context_provider_->ContextSupport()->Swap();
  } else {
    context_provider_->ContextSupport()->PartialSwapBuffers(
        frame->gl_frame_data->sub_buffer_rect);
  }

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->ShallowFlushCHROMIUM();

  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

  client_->DidSwapBuffers();
}

bool DirectOutputSurfaceOzone::BindToClient(cc::OutputSurfaceClient* client) {
  if (!cc::OutputSurface::BindToClient(client))
    return false;

  client->SetBeginFrameSource(synthetic_begin_frame_source_.get());

  if (capabilities_.uses_default_gl_framebuffer) {
    capabilities_.flipped_output_surface =
        context_provider()->ContextCapabilities().flips_vertically;
  }
  return true;
}

void DirectOutputSurfaceOzone::OnUpdateVSyncParametersFromGpu(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(HasClient());
  synthetic_begin_frame_source_->OnUpdateVSyncParameters(timebase, interval);
}

void DirectOutputSurfaceOzone::OnGpuSwapBuffersCompleted(
    gfx::SwapResult result) {
  DCHECK(output_surface_);
  bool force_swap = false;
  if (result == gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    // Even through the swap failed, this is a fixable error so we can pretend
    // it succeeded to the rest of the system.
    result = gfx::SwapResult::SWAP_ACK;
    output_surface_->RecreateBuffers();
    force_swap = true;
  }

  output_surface_->PageFlipComplete();
  OnSwapBuffersComplete();

  if (force_swap)
    client_->SetNeedsRedrawRect(gfx::Rect(SurfaceSize()));
}

void DirectOutputSurfaceOzone::BindFramebuffer() {
  DCHECK(output_surface_);
  output_surface_->BindFramebuffer();
}

// We call this on every frame but changing the size once we've allocated
// backing NativePixmapOzone instances will cause a DCHECK because
// Chrome never Reshape(s) after the first one from (0,0). NB: this implies
// that screen size changes need to be plumbed differently. In particular, we
// must create the native window in the size that the hardware reports.
void DirectOutputSurfaceOzone::Reshape(const gfx::Size& size,
                                       float scale_factor,
                                       bool alpha) {
  OutputSurface::Reshape(size, scale_factor, alpha);
  DCHECK(output_surface_);
  output_surface_->Reshape(SurfaceSize(), scale_factor);
}

}  // namespace mus
