// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/ui_resource_layer_impl.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/rect_f.h"

namespace cc {

UIResourceLayerImpl::UIResourceLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      ui_resource_id_(0) {}

UIResourceLayerImpl::~UIResourceLayerImpl() {}

scoped_ptr<LayerImpl> UIResourceLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return UIResourceLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void UIResourceLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  UIResourceLayerImpl* layer_impl = static_cast<UIResourceLayerImpl*>(layer);

  layer_impl->SetUIResourceId(ui_resource_id_);
  layer_impl->SetImageBounds(image_bounds_);
}

void UIResourceLayerImpl::SetUIResourceId(UIResourceId uid) {
  if (uid == ui_resource_id_)
    return;
  ui_resource_id_ = uid;
  NoteLayerPropertyChanged();
}

void UIResourceLayerImpl::SetImageBounds(gfx::Size image_bounds) {
  // This check imposes an ordering on the call sequence.  An UIResource must
  // exist before SetImageBounds can be called.
  DCHECK(ui_resource_id_);

  if (image_bounds_ == image_bounds)
    return;

  image_bounds_ = image_bounds;

  NoteLayerPropertyChanged();
}

bool UIResourceLayerImpl::WillDraw(DrawMode draw_mode,
                                  ResourceProvider* resource_provider) {
  if (!ui_resource_id_ || draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE)
    return false;
  return LayerImpl::WillDraw(draw_mode, resource_provider);
}

void UIResourceLayerImpl::AppendQuads(QuadSink* quad_sink,
                                     AppendQuadsData* append_quads_data) {
  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  if (!ui_resource_id_)
    return;

  ResourceProvider::ResourceId resource =
      layer_tree_impl()->ResourceIdForUIResource(ui_resource_id_);

  if (!resource)
    return;

  static const bool flipped = false;
  static const bool premultiplied_alpha = true;

  DCHECK(!bounds().IsEmpty());

  // TODO(clholgat): Properly calculate opacity: crbug.com/300027
  gfx::Rect opaque_rect;
  if (contents_opaque())
    opaque_rect = gfx::Rect(bounds());

  gfx::Rect quad_rect(bounds());
  gfx::Rect uv_top_left(0.f, 0.f);
  gfx::Rect uv_bottom_right(1.f, 1.f);

  const float vertex_opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
  scoped_ptr<TextureDrawQuad> quad;

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               quad_rect,
               opaque_rect,
               resource,
               premultiplied_alpha,
               uv_top_left.origin(),
               uv_bottom_right.bottom_right(),
               SK_ColorTRANSPARENT,
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
}

const char* UIResourceLayerImpl::LayerTypeAsString() const {
  return "cc::UIResourceLayerImpl";
}

base::DictionaryValue* UIResourceLayerImpl::LayerTreeAsJson() const {
  base::DictionaryValue* result = LayerImpl::LayerTreeAsJson();

  result->Set("ImageBounds", MathUtil::AsValue(image_bounds_).release());

  return result;
}

}  // namespace cc
