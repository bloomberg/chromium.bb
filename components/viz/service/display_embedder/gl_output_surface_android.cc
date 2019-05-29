// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_android.h"

#include "components/viz/service/display_embedder/overlay_candidate_validator_android.h"

namespace viz {

GLOutputSurfaceAndroid::GLOutputSurfaceAndroid(
    scoped_refptr<VizProcessContextProvider> context_provider,
    bool allow_overlays)
    : GLOutputSurface(context_provider) {
  if (allow_overlays) {
    overlay_candidate_validator_ =
        std::make_unique<OverlayCandidateValidatorAndroid>();
  }
}

GLOutputSurfaceAndroid::~GLOutputSurfaceAndroid() = default;

void GLOutputSurfaceAndroid::HandlePartialSwap(
    const gfx::Rect& sub_buffer_rect,
    uint32_t flags,
    gpu::ContextSupport::SwapCompletedCallback swap_callback,
    gpu::ContextSupport::PresentationCallback presentation_callback) {
  DCHECK(sub_buffer_rect.IsEmpty());
  context_provider_->ContextSupport()->CommitOverlayPlanes(
      flags, std::move(swap_callback), std::move(presentation_callback));
}

std::unique_ptr<OverlayCandidateValidator>
GLOutputSurfaceAndroid::TakeOverlayCandidateValidator() {
  return std::move(overlay_candidate_validator_);
}

}  // namespace viz
