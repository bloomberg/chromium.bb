// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer_impl.h"

#include "base/debug/trace_event_argument.h"
#include "cc/debug/debug_colors.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/trees/occlusion_tracker.h"

namespace cc {

SurfaceLayerImpl::SurfaceLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id) {
}

SurfaceLayerImpl::~SurfaceLayerImpl() {}

scoped_ptr<LayerImpl> SurfaceLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SurfaceLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void SurfaceLayerImpl::SetSurfaceId(SurfaceId surface_id) {
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

void SurfaceLayerImpl::AppendQuads(
    RenderPass* render_pass,
    const OcclusionTracker<LayerImpl>& occlusion_tracker,
    AppendQuadsData* append_quads_data) {
  SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateSharedQuadState(shared_quad_state);

  AppendDebugBorderQuad(
      render_pass, content_bounds(), shared_quad_state, append_quads_data);

  if (surface_id_.is_null())
    return;

  gfx::Rect quad_rect(content_bounds());
  gfx::Rect visible_quad_rect =
      occlusion_tracker.GetCurrentOcclusionForLayer(
                            draw_properties().target_space_transform)
          .GetUnoccludedContentRect(quad_rect);
  if (visible_quad_rect.IsEmpty())
    return;
  SurfaceDrawQuad* quad =
      render_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect, surface_id_);
}

void SurfaceLayerImpl::GetDebugBorderProperties(SkColor* color,
                                                float* width) const {
  *color = DebugColors::SurfaceLayerBorderColor();
  *width = DebugColors::SurfaceLayerBorderWidth(layer_tree_impl());
}

void SurfaceLayerImpl::AsValueInto(base::debug::TracedValue* dict) const {
  LayerImpl::AsValueInto(dict);
  dict->SetInteger("surface_id", surface_id_.id);
}

const char* SurfaceLayerImpl::LayerTypeAsString() const {
  return "cc::SurfaceLayerImpl";
}

}  // namespace cc
