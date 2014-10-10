// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/cc/direct_output_surface.h"

#include "base/bind.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface_client.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace mojo {

DirectOutputSurface::DirectOutputSurface(
    const scoped_refptr<cc::ContextProvider>& context_provider)
    : cc::OutputSurface(context_provider), weak_ptr_factory_(this) {
}

DirectOutputSurface::~DirectOutputSurface() {}

void DirectOutputSurface::SwapBuffers(cc::CompositorFrame* frame) {
  DCHECK(context_provider_.get());
  DCHECK(frame->gl_frame_data);
  if (frame->gl_frame_data->sub_buffer_rect ==
      gfx::Rect(frame->gl_frame_data->size)) {
    context_provider_->ContextSupport()->Swap();
  } else {
    context_provider_->ContextSupport()->PartialSwapBuffers(
        frame->gl_frame_data->sub_buffer_rect);
  }
  uint32_t sync_point =
      context_provider_->ContextGL()->InsertSyncPointCHROMIUM();
  context_provider_->ContextSupport()->SignalSyncPoint(
      sync_point,
      base::Bind(&OutputSurface::OnSwapBuffersComplete,
                 weak_ptr_factory_.GetWeakPtr()));
  client_->DidSwapBuffers();
}

}  // namespace mojo
