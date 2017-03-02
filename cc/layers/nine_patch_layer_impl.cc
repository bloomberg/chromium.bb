// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/nine_patch_layer_impl.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

NinePatchLayerImpl::NinePatchLayerImpl(LayerTreeImpl* tree_impl, int id)
    : UIResourceLayerImpl(tree_impl, id) {}

NinePatchLayerImpl::~NinePatchLayerImpl() {}

std::unique_ptr<LayerImpl> NinePatchLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return NinePatchLayerImpl::Create(tree_impl, id());
}

void NinePatchLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  UIResourceLayerImpl::PushPropertiesTo(layer);
  NinePatchLayerImpl* layer_impl = static_cast<NinePatchLayerImpl*>(layer);

  layer_impl->quad_generator_ = this->quad_generator_;
}

void NinePatchLayerImpl::SetLayout(const gfx::Rect& aperture,
                                   const gfx::Rect& border,
                                   const gfx::Rect& layer_occlusion,
                                   bool fill_center,
                                   bool nearest_neighbor) {
  // This check imposes an ordering on the call sequence.  An UIResource must
  // exist before SetLayout can be called.
  DCHECK(ui_resource_id_);

  if (!quad_generator_.SetLayout(image_bounds_, bounds(), aperture, border,
                                 layer_occlusion, fill_center,
                                 nearest_neighbor))
    return;

  NoteLayerPropertyChanged();
}

void NinePatchLayerImpl::AppendQuads(
    RenderPass* render_pass,
    AppendQuadsData* append_quads_data) {
  quad_generator_.CheckGeometryLimitations();
  SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateSharedQuadState(shared_quad_state);

  AppendDebugBorderQuad(render_pass, bounds(), shared_quad_state,
                        append_quads_data);

  DCHECK(!bounds().IsEmpty());

  std::vector<NinePatchGenerator::Patch> patches =
      quad_generator_.GeneratePatches();

  for (auto& patch : patches)
    patch.output_rect =
        gfx::RectF(gfx::ToFlooredRectDeprecated(patch.output_rect));

  quad_generator_.AppendQuads(this, ui_resource_id_, render_pass,
                              shared_quad_state, patches);
}

const char* NinePatchLayerImpl::LayerTypeAsString() const {
  return "cc::NinePatchLayerImpl";
}

std::unique_ptr<base::DictionaryValue> NinePatchLayerImpl::LayerTreeAsJson() {
  std::unique_ptr<base::DictionaryValue> result = LayerImpl::LayerTreeAsJson();
  quad_generator_.AsJson(result.get());
  return result;
}

}  // namespace cc
