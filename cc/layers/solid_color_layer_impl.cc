// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_layer_impl.h"

#include <algorithm>

#include "cc/quads/solid_color_draw_quad.h"
#include "cc/trees/occlusion_tracker.h"

namespace cc {

namespace {
const int kSolidQuadTileSize = 256;
}

SolidColorLayerImpl::SolidColorLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id) {
}

SolidColorLayerImpl::~SolidColorLayerImpl() {}

scoped_ptr<LayerImpl> SolidColorLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SolidColorLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void SolidColorLayerImpl::AppendSolidQuads(
    RenderPass* render_pass,
    const OcclusionTracker<LayerImpl>& occlusion_tracker,
    SharedQuadState* shared_quad_state,
    const gfx::Size& content_bounds,
    const gfx::Transform& target_space_transform,
    SkColor color) {
  Occlusion occlusion =
      occlusion_tracker.GetCurrentOcclusionForLayer(target_space_transform);

  // We create a series of smaller quads instead of just one large one so that
  // the culler can reduce the total pixels drawn.
  int width = content_bounds.width();
  int height = content_bounds.height();
  for (int x = 0; x < width; x += kSolidQuadTileSize) {
    for (int y = 0; y < height; y += kSolidQuadTileSize) {
      gfx::Rect quad_rect(x,
                          y,
                          std::min(width - x, kSolidQuadTileSize),
                          std::min(height - y, kSolidQuadTileSize));
      gfx::Rect visible_quad_rect =
          occlusion.GetUnoccludedContentRect(quad_rect);
      if (visible_quad_rect.IsEmpty())
        continue;

      SolidColorDrawQuad* quad =
          render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      quad->SetNew(
          shared_quad_state, quad_rect, visible_quad_rect, color, false);
    }
  }
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

  AppendSolidQuads(render_pass,
                   occlusion_tracker,
                   shared_quad_state,
                   content_bounds(),
                   draw_properties().target_space_transform,
                   background_color());
}

const char* SolidColorLayerImpl::LayerTypeAsString() const {
  return "cc::SolidColorLayerImpl";
}

}  // namespace cc
