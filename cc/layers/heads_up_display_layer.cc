// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/heads_up_display_layer.h"

#include "base/debug/trace_event.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_refptr<HeadsUpDisplayLayer> HeadsUpDisplayLayer::Create() {
  return make_scoped_refptr(new HeadsUpDisplayLayer());
}

HeadsUpDisplayLayer::HeadsUpDisplayLayer() : Layer() {
  SetBounds(gfx::Size(256, 256));
}

HeadsUpDisplayLayer::~HeadsUpDisplayLayer() {}

void HeadsUpDisplayLayer::Update(ResourceUpdateQueue*,
                                 const OcclusionTracker*,
                                 RenderingStats*) {
  const LayerTreeDebugState& debug_state = layer_tree_host()->debug_state();
  int max_texture_size =
      layer_tree_host()->GetRendererCapabilities().max_texture_size;

  int device_viewport_in_layout_pixels_width =
      layer_tree_host()->device_viewport_size().width() /
      layer_tree_host()->device_scale_factor();
  int device_viewport_in_layout_pixels_height =
      layer_tree_host()->device_viewport_size().height() /
      layer_tree_host()->device_scale_factor();

  gfx::Size bounds;
  gfx::Transform matrix;
  matrix.MakeIdentity();

  if (debug_state.show_platform_layer_tree || debug_state.ShowHudRects()) {
    int width =
        std::min(max_texture_size, device_viewport_in_layout_pixels_width);
    int height =
        std::min(max_texture_size, device_viewport_in_layout_pixels_height);
    bounds = gfx::Size(width, height);
  } else {
    bounds = gfx::Size(256, 256);
    matrix.Translate(device_viewport_in_layout_pixels_width - 256.0, 0.0);
  }

  SetBounds(bounds);
  SetTransform(matrix);
}

bool HeadsUpDisplayLayer::DrawsContent() const { return true; }

scoped_ptr<LayerImpl> HeadsUpDisplayLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return HeadsUpDisplayLayerImpl::Create(tree_impl, layer_id_).
      PassAs<LayerImpl>();
}

}  // namespace cc
