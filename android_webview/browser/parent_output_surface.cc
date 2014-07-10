// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/parent_output_surface.h"

#include "cc/output/output_surface_client.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace android_webview {

ParentOutputSurface::ParentOutputSurface(
    scoped_refptr<cc::ContextProvider> context_provider)
    : cc::OutputSurface(context_provider) {
  capabilities_.draw_and_swap_full_viewport_every_frame = true;
}

ParentOutputSurface::~ParentOutputSurface() {
}

void ParentOutputSurface::SwapBuffers(cc::CompositorFrame* frame) {
  context_provider_->ContextGL()->ShallowFlushCHROMIUM();
  client_->DidSwapBuffers();
}

void ParentOutputSurface::SetDrawConstraints(const gfx::Size& surface_size,
                                             const gfx::Rect& clip) {
  DCHECK(client_);
  surface_size_ = surface_size;
  const gfx::Transform identity;
  const gfx::Rect empty;
  const bool resourceless_software_draw = false;
  SetExternalDrawConstraints(identity, empty, clip, resourceless_software_draw);
}

}  // namespace android_webview
