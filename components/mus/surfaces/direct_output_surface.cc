// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/direct_output_surface.h"

#include <stdint.h>

#include "base/bind.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface_client.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace mus {

DirectOutputSurface::DirectOutputSurface(
    const scoped_refptr<cc::ContextProvider>& context_provider)
    : cc::OutputSurface(context_provider), weak_ptr_factory_(this) {}

DirectOutputSurface::~DirectOutputSurface() {}

bool DirectOutputSurface::BindToClient(cc::OutputSurfaceClient* client) {
  if (!cc::OutputSurface::BindToClient(client))
    return false;

  if (capabilities_.uses_default_gl_framebuffer) {
    capabilities_.flipped_output_surface =
        context_provider()->ContextCapabilities().gpu.flips_vertically;
  }
  return true;
}

void DirectOutputSurface::SwapBuffers(cc::CompositorFrame* frame) {
  DCHECK(context_provider_);
  DCHECK(frame->gl_frame_data);
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

  context_provider_->ContextSupport()->SignalSyncToken(
      sync_token, base::Bind(&OutputSurface::OnSwapBuffersComplete,
                             weak_ptr_factory_.GetWeakPtr()));
  client_->DidSwapBuffers();
}

}  // namespace mus
