// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"

#include "third_party/skia/include/core/SkSurface.h"

namespace viz {

SkiaOutputDeviceOffscreen::SkiaOutputDeviceOffscreen(GrContext* gr_context,
                                                     bool flipped,
                                                     bool has_alpha)
    : gr_context_(gr_context), flipped_(flipped), has_alpha_(has_alpha) {}

SkiaOutputDeviceOffscreen::~SkiaOutputDeviceOffscreen() = default;

sk_sp<SkSurface> SkiaOutputDeviceOffscreen::DrawSurface() {
  return draw_surface_;
}

void SkiaOutputDeviceOffscreen::Reshape(const gfx::Size& size) {
  auto image_info = SkImageInfo::Make(
      size.width(), size.height(),
      has_alpha_ ? kRGBA_8888_SkColorType : kRGB_888x_SkColorType,
      has_alpha_ ? kPremul_SkAlphaType : kOpaque_SkAlphaType);
  draw_surface_ = SkSurface::MakeRenderTarget(
      gr_context_, SkBudgeted::kNo, image_info, 0 /* sampleCount */,
      flipped_ ? kTopLeft_GrSurfaceOrigin : kBottomLeft_GrSurfaceOrigin,
      nullptr /* surfaceProps */);
}

gfx::SwapResult SkiaOutputDeviceOffscreen::SwapBuffers() {
  // Reshape should have been called first.
  DCHECK(draw_surface_);
  return gfx::SwapResult::SWAP_ACK;
}

}  // namespace viz
