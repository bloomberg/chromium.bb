// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/overlay/overlay_surface_embedder.h"

#include "components/viz/common/surfaces/stub_surface_reference_factory.h"
#include "ui/compositor/layer.h"

OverlaySurfaceEmbedder::OverlaySurfaceEmbedder(OverlayWindow* window)
    : window_(window) {
  surface_layer_ = base::MakeUnique<ui::Layer>(ui::LAYER_TEXTURED);
  surface_layer_->SetMasksToBounds(true);

  // The frame provided by the parent window's layer needs to show through
  // the surface layer.
  surface_layer_->SetFillsBoundsOpaquely(false);
  window_->GetLayer()->Add(surface_layer_.get());
  ref_factory_ = new viz::StubSurfaceReferenceFactory();
}

OverlaySurfaceEmbedder::~OverlaySurfaceEmbedder() = default;

void OverlaySurfaceEmbedder::SetPrimarySurfaceInfo(
    const viz::SurfaceInfo& surface_info) {
  // SurfaceInfo has information about the embedded surface.
  surface_layer_->SetShowPrimarySurface(surface_info, ref_factory_);
  surface_layer_->SetBounds(gfx::Rect(window_->GetBounds().size()));
}
