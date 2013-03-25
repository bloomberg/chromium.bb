// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_layer_impl.h"

#include "cc/layers/quad_sink.h"
#include "cc/quads/solid_color_draw_quad.h"

namespace cc {

SolidColorLayerImpl::SolidColorLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      tile_size_(256) {}

SolidColorLayerImpl::~SolidColorLayerImpl() {}

scoped_ptr<LayerImpl> SolidColorLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SolidColorLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void SolidColorLayerImpl::AppendQuads(QuadSink* quad_sink,
                                      AppendQuadsData* append_quads_data) {
  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  // We create a series of smaller quads instead of just one large one so that
  // the culler can reduce the total pixels drawn.
  int width = content_bounds().width();
  int height = content_bounds().height();
  for (int x = 0; x < width; x += tile_size_) {
    for (int y = 0; y < height; y += tile_size_) {
      gfx::Rect solid_tile_rect(x,
                              y,
                              std::min(width - x, tile_size_),
                              std::min(height - y, tile_size_));
      scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
      quad->SetNew(shared_quad_state, solid_tile_rect, background_color());
      quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
    }
  }
}

const char* SolidColorLayerImpl::LayerTypeAsString() const {
  return "SolidColorLayer";
}

}  // namespace cc
