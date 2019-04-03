// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"

#include <utility>

#include "third_party/skia/include/core/SkSurface.h"

namespace viz {

SkiaOutputDeviceOffscreen::SkiaOutputDeviceOffscreen(
    GrContext* gr_context,
    bool flipped,
    bool has_alpha,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(did_swap_buffer_complete_callback),
      gr_context_(gr_context),
      has_alpha_(has_alpha) {
  capabilities_.flipped_output_surface = flipped;
  capabilities_.supports_post_sub_buffer = true;
}

SkiaOutputDeviceOffscreen::~SkiaOutputDeviceOffscreen() = default;

void SkiaOutputDeviceOffscreen::Reshape(const gfx::Size& size,
                                        float device_scale_factor,
                                        const gfx::ColorSpace& color_space,
                                        bool has_alpha) {
  auto image_info = SkImageInfo::Make(
      size.width(), size.height(),
      has_alpha_ ? kRGBA_8888_SkColorType : kRGB_888x_SkColorType,
      has_alpha_ ? kPremul_SkAlphaType : kOpaque_SkAlphaType);
  draw_surface_ = SkSurface::MakeRenderTarget(
      gr_context_, SkBudgeted::kNo, image_info, 0 /* sampleCount */,
      capabilities_.flipped_output_surface ? kTopLeft_GrSurfaceOrigin
                                           : kBottomLeft_GrSurfaceOrigin,
      nullptr /* surfaceProps */);
}

gfx::SwapResponse SkiaOutputDeviceOffscreen::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback) {
  return SwapBuffers(std::move(feedback));
}

gfx::SwapResponse SkiaOutputDeviceOffscreen::SwapBuffers(
    BufferPresentedCallback feedback) {
  // Reshape should have been called first.
  DCHECK(draw_surface_);

  StartSwapBuffers(std::move(feedback));
  return FinishSwapBuffers(gfx::SwapResult::SWAP_ACK);
}

}  // namespace viz
