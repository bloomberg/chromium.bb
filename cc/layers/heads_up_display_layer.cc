// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/heads_up_display_layer.h"

#include <algorithm>

#include "base/trace_event/trace_event.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

scoped_refptr<HeadsUpDisplayLayer> HeadsUpDisplayLayer::Create() {
  return base::WrapRefCounted(new HeadsUpDisplayLayer());
}

HeadsUpDisplayLayer::HeadsUpDisplayLayer()
    : typeface_(SkTypeface::MakeFromName("times new roman", SkFontStyle())) {
  if (!typeface_) {
    typeface_ = SkTypeface::MakeFromName("monospace", SkFontStyle::Bold());
  }
  DCHECK(typeface_.get());
  SetIsDrawable(true);
  UpdateDrawsContent(HasDrawableContent());
}

HeadsUpDisplayLayer::~HeadsUpDisplayLayer() = default;

void HeadsUpDisplayLayer::PrepareForCalculateDrawProperties(
    const gfx::Size& device_viewport, float device_scale_factor) {
  gfx::Size device_viewport_in_layout_pixels = gfx::Size(
      device_viewport.width() / device_scale_factor,
      device_viewport.height() / device_scale_factor);

  gfx::Size bounds;
  gfx::Transform matrix;
  matrix.MakeIdentity();

  if (layer_tree_host()->GetDebugState().ShowHudRects()) {
    bounds = device_viewport_in_layout_pixels;
  } else {
    int size = 256;
    bounds.SetSize(size, size);
    matrix.Translate(device_viewport_in_layout_pixels.width() - size, 0.0);
  }

  SetBounds(bounds);
  SetTransform(matrix);
}

bool HeadsUpDisplayLayer::HasDrawableContent() const {
  return true;
}

std::unique_ptr<LayerImpl> HeadsUpDisplayLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return HeadsUpDisplayLayerImpl::Create(tree_impl, id());
}

void HeadsUpDisplayLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  TRACE_EVENT0("cc", "HeadsUpDisplayLayer::PushPropertiesTo");
  HeadsUpDisplayLayerImpl* layer_impl =
      static_cast<HeadsUpDisplayLayerImpl*>(layer);

  layer_impl->SetHUDTypeface(typeface_);
}

}  // namespace cc
