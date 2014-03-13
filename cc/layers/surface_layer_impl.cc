// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer_impl.h"

#include "cc/debug/debug_colors.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/surface_draw_quad.h"

namespace cc {

SurfaceLayerImpl::SurfaceLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id), surface_id_(0) {}

SurfaceLayerImpl::~SurfaceLayerImpl() {}

scoped_ptr<LayerImpl> SurfaceLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SurfaceLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void SurfaceLayerImpl::SetSurfaceId(int surface_id) {
  if (surface_id_ == surface_id)
    return;

  surface_id_ = surface_id;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  SurfaceLayerImpl* layer_impl = static_cast<SurfaceLayerImpl*>(layer);

  layer_impl->SetSurfaceId(surface_id_);
}

void SurfaceLayerImpl::AppendQuads(QuadSink* quad_sink,
                                   AppendQuadsData* append_quads_data) {
  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  if (!surface_id_)
    return;

  scoped_ptr<SurfaceDrawQuad> quad = SurfaceDrawQuad::Create();
  gfx::Rect quad_rect(content_bounds());
  gfx::Rect visible_quad_rect(quad_rect);
  quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect, surface_id_);
  quad_sink->MaybeAppend(quad.PassAs<DrawQuad>());
}

void SurfaceLayerImpl::GetDebugBorderProperties(SkColor* color,
                                                float* width) const {
  *color = DebugColors::SurfaceLayerBorderColor();
  *width = DebugColors::SurfaceLayerBorderWidth(layer_tree_impl());
}

void SurfaceLayerImpl::AsValueInto(base::DictionaryValue* dict) const {
  LayerImpl::AsValueInto(dict);
  dict->SetInteger("surface_id", surface_id_);
}

const char* SurfaceLayerImpl::LayerTypeAsString() const {
  return "cc::SurfaceLayerImpl";
}

}  // namespace cc
