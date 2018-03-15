// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/overlay/overlay_surface_embedder.h"

#include "ui/compositor/layer.h"

OverlaySurfaceEmbedder::OverlaySurfaceEmbedder(OverlayWindow* window)
    : window_(window) {
  DCHECK(window_);
  surface_layer_ = std::make_unique<ui::Layer>(ui::LAYER_TEXTURED);
  surface_layer_->SetMasksToBounds(true);

  // The frame provided by the parent window's layer needs to show through
  // |surface_layer_|.
  surface_layer_->SetFillsBoundsOpaquely(false);
  // |surface_layer_| bounds are set with the (0, 0) origin point. The
  // positioning of |window_| is dictated by itself.
  // TODO(apacible): Update |surface_layer_| size when the window is resized.
  // http://crbug.com/726621
  surface_layer_->SetBounds(
      gfx::Rect(gfx::Point(0, 0), window_->GetBounds().size()));
  window_->GetLayer()->Add(surface_layer_.get());
}

OverlaySurfaceEmbedder::~OverlaySurfaceEmbedder() = default;

void OverlaySurfaceEmbedder::SetPrimarySurfaceId(
    const viz::SurfaceId& surface_id) {
  // SurfaceInfo has information about the embedded surface.
  surface_layer_->SetShowPrimarySurface(
      surface_id, window_->GetBounds().size(), SK_ColorBLACK,
      cc::DeadlinePolicy::UseDefaultDeadline(),
      true /* stretch_content_to_fill_bounds */);
}
