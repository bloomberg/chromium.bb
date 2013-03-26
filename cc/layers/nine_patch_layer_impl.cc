// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/nine_patch_layer_impl.h"

#include "base/stringprintf.h"
#include "base/values.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/texture_draw_quad.h"
#include "ui/gfx/rect_f.h"

namespace cc {

NinePatchLayerImpl::NinePatchLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      resource_id_(0) {}

NinePatchLayerImpl::~NinePatchLayerImpl() {}

ResourceProvider::ResourceId NinePatchLayerImpl::ContentsResourceId() const {
  return 0;
}

scoped_ptr<LayerImpl> NinePatchLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return NinePatchLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void NinePatchLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  NinePatchLayerImpl* layer_impl = static_cast<NinePatchLayerImpl*>(layer);

  if (!resource_id_)
    return;

  layer_impl->SetResourceId(resource_id_);
  layer_impl->SetLayout(image_bounds_, image_aperture_);
}

void NinePatchLayerImpl::WillDraw(ResourceProvider* resource_provider) {}

static gfx::RectF NormalizedRect(float x,
                                 float y,
                                 float width,
                                 float height,
                                 float total_width,
                                 float total_height) {
  return gfx::RectF(x / total_width,
                    y / total_height,
                    width / total_width,
                    height / total_height);
}

void NinePatchLayerImpl::SetLayout(gfx::Size image_bounds, gfx::Rect aperture) {
  image_bounds_ = image_bounds;
  image_aperture_ = aperture;
}

void NinePatchLayerImpl::AppendQuads(QuadSink* quad_sink,
                                     AppendQuadsData* append_quads_data) {
  if (!resource_id_)
    return;

  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  static const bool flipped = false;
  static const bool premultiplied_alpha = true;

  DCHECK(!bounds().IsEmpty());

  // NinePatch border widths in bitmap pixel space
  int left_width = image_aperture_.x();
  int top_height = image_aperture_.y();
  int right_width = image_bounds_.width() - image_aperture_.right();
  int bottom_height = image_bounds_.height() - image_aperture_.bottom();

  // If layer can't fit the corners, clip to show the outer edges of the
  // image.
  int corner_total_width = left_width + right_width;
  int middle_width = bounds().width() - corner_total_width;
  if (middle_width < 0) {
    float left_width_proportion =
        static_cast<float>(left_width) / corner_total_width;
    int left_width_crop = middle_width * left_width_proportion;
    left_width += left_width_crop;
    right_width = bounds().width() - left_width;
    middle_width = 0;
  }
  int corner_total_height = top_height + bottom_height;
  int middle_height = bounds().height() - corner_total_height;
  if (middle_height < 0) {
    float top_height_proportion =
        static_cast<float>(top_height) / corner_total_height;
    int top_height_crop = middle_height * top_height_proportion;
    top_height += top_height_crop;
    bottom_height = bounds().height() - top_height;
    middle_height = 0;
  }

  // Patch positions in layer space
  gfx::Rect top_left(0, 0, left_width, top_height);
  gfx::Rect top_right(
      bounds().width() - right_width, 0, right_width, top_height);
  gfx::Rect bottom_left(
      0, bounds().height() - bottom_height, left_width, bottom_height);
  gfx::Rect bottom_right(
      top_right.x(), bottom_left.y(), right_width, bottom_height);
  gfx::Rect top(top_left.right(), 0, middle_width, top_height);
  gfx::Rect left(0, top_left.bottom(), left_width, middle_height);
  gfx::Rect right(top_right.x(), top_right.bottom(), right_width, left.height());
  gfx::Rect bottom(top.x(), bottom_left.y(), top.width(), bottom_height);

  float img_width = image_bounds_.width();
  float img_height = image_bounds_.height();

  // Patch positions in bitmap UV space (from zero to one)
  gfx::RectF uv_top_left = NormalizedRect(0,
                                          0,
                                          left_width,
                                          top_height,
                                          img_width,
                                          img_height);
  gfx::RectF uv_top_right = NormalizedRect(img_width - right_width,
                                           0,
                                           right_width,
                                           top_height,
                                           img_width,
                                           img_height);
  gfx::RectF uv_bottom_left = NormalizedRect(0,
                                             img_height - bottom_height,
                                             left_width,
                                             bottom_height,
                                             img_width,
                                             img_height);
  gfx::RectF uv_bottom_right = NormalizedRect(img_width - right_width,
                                              img_height - bottom_height,
                                              right_width,
                                              bottom_height,
                                              img_width,
                                              img_height);
  gfx::RectF uv_top(uv_top_left.right(),
                   0,
                   (img_width - left_width - right_width) / img_width,
                   (top_height) / img_height);
  gfx::RectF uv_left(0,
                    uv_top_left.bottom(),
                    left_width / img_width,
                    (img_height - top_height - bottom_height) / img_height);
  gfx::RectF uv_right(uv_top_right.x(),
                     uv_top_right.bottom(),
                     right_width / img_width,
                     uv_left.height());
  gfx::RectF uv_bottom(uv_top.x(),
                      uv_bottom_left.y(),
                      uv_top.width(),
                      bottom_height / img_height);

  // Nothing is opaque here.
  // TODO(danakj): Should we look at the SkBitmaps to determine opaqueness?
  gfx::Rect opaque_rect;
  const float vertex_opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
  scoped_ptr<TextureDrawQuad> quad;

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               top_left,
               opaque_rect,
               resource_id_,
               premultiplied_alpha,
               uv_top_left.origin(),
               uv_top_left.bottom_right(),
               vertex_opacity, flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               top_right,
               opaque_rect,
               resource_id_,
               premultiplied_alpha,
               uv_top_right.origin(),
               uv_top_right.bottom_right(),
               vertex_opacity, flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               bottom_left,
               opaque_rect,
               resource_id_,
               premultiplied_alpha,
               uv_bottom_left.origin(),
               uv_bottom_left.bottom_right(),
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               bottom_right,
               opaque_rect,
               resource_id_,
               premultiplied_alpha,
               uv_bottom_right.origin(),
               uv_bottom_right.bottom_right(),
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               top,
               opaque_rect,
               resource_id_,
               premultiplied_alpha,
               uv_top.origin(),
               uv_top.bottom_right(),
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               left,
               opaque_rect,
               resource_id_,
               premultiplied_alpha,
               uv_left.origin(),
               uv_left.bottom_right(),
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               right,
               opaque_rect,
               resource_id_,
               premultiplied_alpha,
               uv_right.origin(),
               uv_right.bottom_right(),
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               bottom,
               opaque_rect,
               resource_id_,
               premultiplied_alpha,
               uv_bottom.origin(),
               uv_bottom.bottom_right(),
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
}

void NinePatchLayerImpl::DidDraw(ResourceProvider* resource_provider) {}

void NinePatchLayerImpl::DidLoseOutputSurface() {
  resource_id_ = 0;
}

const char* NinePatchLayerImpl::LayerTypeAsString() const {
  return "NinePatchLayer";
}

void NinePatchLayerImpl::DumpLayerProperties(std::string* str, int indent)
    const {
  str->append(IndentString(indent));
  base::StringAppendF(
      str, "imageAperture: %s\n", image_aperture_.ToString().c_str());
  base::StringAppendF(
      str, "image_bounds: %s\n", image_bounds_.ToString().c_str());
  LayerImpl::DumpLayerProperties(str, indent);
}

base::DictionaryValue* NinePatchLayerImpl::LayerTreeAsJson() const {
  base::DictionaryValue* result = LayerImpl::LayerTreeAsJson();

  base::ListValue* list = new base::ListValue;
  list->AppendInteger(image_aperture_.origin().x());
  list->AppendInteger(image_aperture_.origin().y());
  list->AppendInteger(image_aperture_.size().width());
  list->AppendInteger(image_aperture_.size().height());
  result->Set("ImageAperture", list);

  list = new base::ListValue;
  list->AppendInteger(image_bounds_.width());
  list->AppendInteger(image_bounds_.height());
  result->Set("ImageBounds", list);

  return result;
}

}  // namespace cc
