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
}

ParentOutputSurface::~ParentOutputSurface() {
}

void ParentOutputSurface::Reshape(const gfx::Size& size, float scale_factor) {
  DCHECK_EQ(1.f, scale_factor);
  surface_size_ = size;
}

void ParentOutputSurface::SwapBuffers(cc::CompositorFrame* frame) {
  context_provider_->ContextGL()->ShallowFlushCHROMIUM();
  client_->DidSwapBuffers();
}

}  // namespace android_webview
