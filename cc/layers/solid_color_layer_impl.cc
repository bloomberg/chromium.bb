// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_layer_impl.h"

#include <algorithm>

#include "cc/quads/solid_color_draw_quad.h"
#include "cc/trees/occlusion_tracker.h"

namespace cc {

SolidColorLayerImpl::SolidColorLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      tile_size_(256) {}

SolidColorLayerImpl::~SolidColorLayerImpl() {}

scoped_ptr<LayerImpl> SolidColorLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SolidColorLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void SolidColorLayerImpl::AppendQuads(
    RenderPass* render_pass,
    const OcclusionTracker<LayerImpl>& occlusion_tracker,
    AppendQuadsData* append_quads_data) {
  SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateSharedQuadState(shared_quad_state);

  AppendDebugBorderQuad(
      render_pass, content_bounds(), shared_quad_state, append_quads_data);

  // We create a series of smaller quads instead of just one large one so that
  // the culler can reduce the total pixels drawn.
  int width = content_bounds().width();
  int height = content_bounds().height();
  for (int x = 0; x < width; x += tile_size_) {
    for (int y = 0; y < height; y += tile_size_) {
      gfx::Rect quad_rect(x,
                          y,
                          std::min(width - x, tile_size_),
                          std::min(height - y, tile_size_));
      gfx::Rect visible_quad_rect = occlusion_tracker.UnoccludedContentRect(
          quad_rect, draw_properties().target_space_transform);
      if (visible_quad_rect.IsEmpty())
        continue;

      SolidColorDrawQuad* quad =
          render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      quad->SetNew(shared_quad_state,
                   quad_rect,
                   visible_quad_rect,
                   background_color(),
                   false);
    }
  }
}

const char* SolidColorLayerImpl::LayerTypeAsString() const {
  return "cc::SolidColorLayerImpl";
}

}  // namespace cc
