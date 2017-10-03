// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_layer_impl.h"

#include <algorithm>

#include "cc/layers/append_quads_data.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"

namespace cc {

namespace {
const int kSolidQuadTileSize = 256;
}

SolidColorLayerImpl::SolidColorLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id) {
}

SolidColorLayerImpl::~SolidColorLayerImpl() {}

std::unique_ptr<LayerImpl> SolidColorLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SolidColorLayerImpl::Create(tree_impl, id());
}

void SolidColorLayerImpl::AppendSolidQuads(
    viz::RenderPass* render_pass,
    const Occlusion& occlusion_in_layer_space,
    viz::SharedQuadState* shared_quad_state,
    const gfx::Rect& visible_layer_rect,
    SkColor color,
    bool force_anti_aliasing_off,
    AppendQuadsData* append_quads_data) {
  float alpha =
      (SkColorGetA(color) * (1.0f / 255.0f)) * shared_quad_state->opacity;
  DCHECK_EQ(SkBlendMode::kSrcOver, shared_quad_state->blend_mode);
  if (alpha < std::numeric_limits<float>::epsilon())
    return;
  // We create a series of smaller quads instead of just one large one so that
  // the culler can reduce the total pixels drawn.
  int right = visible_layer_rect.right();
  int bottom = visible_layer_rect.bottom();
  for (int x = visible_layer_rect.x(); x < visible_layer_rect.right();
       x += kSolidQuadTileSize) {
    for (int y = visible_layer_rect.y(); y < visible_layer_rect.bottom();
         y += kSolidQuadTileSize) {
      gfx::Rect quad_rect(x,
                          y,
                          std::min(right - x, kSolidQuadTileSize),
                          std::min(bottom - y, kSolidQuadTileSize));
      gfx::Rect visible_quad_rect =
          occlusion_in_layer_space.GetUnoccludedContentRect(quad_rect);
      if (visible_quad_rect.IsEmpty())
        continue;

      append_quads_data->visible_layer_area +=
          visible_quad_rect.width() * visible_quad_rect.height();

      auto* quad =
          render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
      quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect, color,
                   force_anti_aliasing_off);
    }
  }
}

void SolidColorLayerImpl::AppendQuads(viz::RenderPass* render_pass,
                                      AppendQuadsData* append_quads_data) {
  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateSharedQuadState(shared_quad_state, contents_opaque());

  AppendDebugBorderQuad(render_pass, gfx::Rect(bounds()), shared_quad_state,
                        append_quads_data);

  // TODO(hendrikw): We need to pass the visible content rect rather than
  // |bounds()| here.
  AppendSolidQuads(render_pass, draw_properties().occlusion_in_content_space,
                   shared_quad_state, gfx::Rect(bounds()), background_color(),
                   !layer_tree_impl()->settings().enable_edge_anti_aliasing,
                   append_quads_data);
}

const char* SolidColorLayerImpl::LayerTypeAsString() const {
  return "cc::SolidColorLayerImpl";
}

}  // namespace cc
