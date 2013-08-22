// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/painted_scrollbar_layer_impl.h"

#include <algorithm>

#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/layers/layer.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

scoped_ptr<PaintedScrollbarLayerImpl> PaintedScrollbarLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation) {
  return make_scoped_ptr(
      new PaintedScrollbarLayerImpl(tree_impl, id, orientation));
}

PaintedScrollbarLayerImpl::PaintedScrollbarLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation)
    : LayerImpl(tree_impl, id),
      track_ui_resource_id_(0),
      thumb_ui_resource_id_(0),
      current_pos_(0.f),
      maximum_(0),
      thumb_thickness_(0),
      thumb_length_(0),
      track_start_(0),
      track_length_(0),
      orientation_(orientation),
      vertical_adjust_(0.f),
      visible_to_total_length_ratio_(1.f),
      scroll_layer_id_(Layer::INVALID_ID),
      is_overlay_scrollbar_(false) {}

PaintedScrollbarLayerImpl::~PaintedScrollbarLayerImpl() {}

PaintedScrollbarLayerImpl* PaintedScrollbarLayerImpl::ToScrollbarLayer() {
  return this;
}

scoped_ptr<LayerImpl> PaintedScrollbarLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PaintedScrollbarLayerImpl::Create(tree_impl, id(), orientation_)
      .PassAs<LayerImpl>();
}

void PaintedScrollbarLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);

  PaintedScrollbarLayerImpl* scrollbar_layer =
      static_cast<PaintedScrollbarLayerImpl*>(layer);

  scrollbar_layer->SetThumbThickness(thumb_thickness_);
  scrollbar_layer->SetThumbLength(thumb_length_);
  scrollbar_layer->SetTrackStart(track_start_);
  scrollbar_layer->SetTrackLength(track_length_);
  scrollbar_layer->set_is_overlay_scrollbar(is_overlay_scrollbar_);

  scrollbar_layer->set_track_ui_resource_id(track_ui_resource_id_);
  scrollbar_layer->set_thumb_ui_resource_id(thumb_ui_resource_id_);
}

bool PaintedScrollbarLayerImpl::WillDraw(DrawMode draw_mode,
                                  ResourceProvider* resource_provider) {
  if (draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE &&
      !layer_tree_impl()->settings().solid_color_scrollbars)
    return false;
  return LayerImpl::WillDraw(draw_mode, resource_provider);
}

void PaintedScrollbarLayerImpl::AppendQuads(
    QuadSink* quad_sink,
    AppendQuadsData* append_quads_data) {
  bool premultipled_alpha = true;
  bool flipped = false;
  gfx::PointF uv_top_left(0.f, 0.f);
  gfx::PointF uv_bottom_right(1.f, 1.f);
  gfx::Rect bounds_rect(bounds());
  gfx::Rect content_bounds_rect(content_bounds());

  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  gfx::Rect thumb_quad_rect = ComputeThumbQuadRect();

  if (layer_tree_impl()->settings().solid_color_scrollbars) {
    scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
    quad->SetNew(shared_quad_state,
                 thumb_quad_rect,
                 layer_tree_impl()->settings().solid_color_scrollbar_color,
                 false);
    quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
    return;
  }

  ResourceProvider::ResourceId thumb_resource_id =
      layer_tree_impl()->ResourceIdForUIResource(thumb_ui_resource_id_);
  ResourceProvider::ResourceId track_resource_id =
      layer_tree_impl()->ResourceIdForUIResource(track_ui_resource_id_);

  if (thumb_resource_id && !thumb_quad_rect.IsEmpty()) {
    gfx::Rect opaque_rect;
    const float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
    scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
    quad->SetNew(shared_quad_state,
                 thumb_quad_rect,
                 opaque_rect,
                 thumb_resource_id,
                 premultipled_alpha,
                 uv_top_left,
                 uv_bottom_right,
                 SK_ColorTRANSPARENT,
                 opacity,
                 flipped);
    quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
  }

  gfx::Rect track_quad_rect = content_bounds_rect;
  if (track_resource_id && !track_quad_rect.IsEmpty()) {
    gfx::Rect opaque_rect(contents_opaque() ? track_quad_rect : gfx::Rect());
    const float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
    scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
    quad->SetNew(shared_quad_state,
                 track_quad_rect,
                 opaque_rect,
                 track_resource_id,
                 premultipled_alpha,
                 uv_top_left,
                 uv_bottom_right,
                 SK_ColorTRANSPARENT,
                 opacity,
                 flipped);
    quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
  }
}

ScrollbarOrientation PaintedScrollbarLayerImpl::Orientation() const {
  return orientation_;
}

float PaintedScrollbarLayerImpl::CurrentPos() const {
  return current_pos_;
}

int PaintedScrollbarLayerImpl::Maximum() const {
  return maximum_;
}

gfx::Rect PaintedScrollbarLayerImpl::ScrollbarLayerRectToContentRect(
    gfx::RectF layer_rect) const {
  // Don't intersect with the bounds as in layerRectToContentRect() because
  // layer_rect here might be in coordinates of the containing layer.
  gfx::RectF content_rect = gfx::ScaleRect(layer_rect,
                                           contents_scale_x(),
                                           contents_scale_y());
  return gfx::ToEnclosingRect(content_rect);
}

void PaintedScrollbarLayerImpl::SetThumbThickness(int thumb_thickness) {
  if (thumb_thickness_ == thumb_thickness)
    return;
  thumb_thickness_ = thumb_thickness;
  NoteLayerPropertyChanged();
}

void PaintedScrollbarLayerImpl::SetThumbLength(int thumb_length) {
  if (thumb_length_ == thumb_length)
    return;
  thumb_length_ = thumb_length;
  NoteLayerPropertyChanged();
}
void PaintedScrollbarLayerImpl::SetTrackStart(int track_start) {
  if (track_start_ == track_start)
    return;
  track_start_ = track_start;
  NoteLayerPropertyChanged();
}

void PaintedScrollbarLayerImpl::SetTrackLength(int track_length) {
  if (track_length_ == track_length)
    return;
  track_length_ = track_length;
  NoteLayerPropertyChanged();
}

void PaintedScrollbarLayerImpl::SetVerticalAdjust(float vertical_adjust) {
  if (vertical_adjust_ == vertical_adjust)
    return;
  vertical_adjust_ = vertical_adjust;
  NoteLayerPropertyChanged();
}

void PaintedScrollbarLayerImpl::SetVisibleToTotalLengthRatio(float ratio) {
  if (visible_to_total_length_ratio_ == ratio)
    return;
  visible_to_total_length_ratio_ = ratio;
  NoteLayerPropertyChanged();
}

void PaintedScrollbarLayerImpl::SetCurrentPos(float current_pos) {
  if (current_pos_ == current_pos)
    return;
  current_pos_ = current_pos;
  NoteLayerPropertyChanged();
}

void PaintedScrollbarLayerImpl::SetMaximum(int maximum) {
  if (maximum_ == maximum)
    return;
  maximum_ = maximum;
  NoteLayerPropertyChanged();
}

gfx::Rect PaintedScrollbarLayerImpl::ComputeThumbQuadRect() const {
  // Thumb extent is the length of the thumb in the scrolling direction, thumb
  // thickness is in the perpendicular direction. Here's an example of a
  // horizontal scrollbar - inputs are above the scrollbar, computed values
  // below:
  //
  //    |<------------------- track_length_ ------------------->|
  //
  // |--| <-- start_offset
  //
  // +--+----------------------------+------------------+-------+--+
  // |<||                            |##################|       ||>|
  // +--+----------------------------+------------------+-------+--+
  //
  //                                 |<- thumb_length ->|
  //
  // |<------- thumb_offset -------->|
  //
  // For painted, scrollbars, the length is fixed. For solid color scrollbars we
  // have to compute it. The ratio of the thumb's length to the track's length
  // is the same as that of the visible viewport to the total viewport, unless
  // that would make the thumb's length less than its thickness.
  //
  // vertical_adjust_ is used when the layer geometry from the main thread is
  // not in sync with what the user sees. For instance on Android scrolling the
  // top bar controls out of view reveals more of the page content. We want the
  // root layer scrollbars to reflect what the user sees even if we haven't
  // received new layer geometry from the main thread.  If the user has scrolled
  // down by 50px and the initial viewport size was 950px the geometry would
  // look something like this:
  //
  // vertical_adjust_ = 50, scroll position 0, visible ratios 99%
  // Layer geometry:             Desired thumb positions:
  // +--------------------+-+   +----------------------+   <-- 0px
  // |                    |v|   |                     #|
  // |                    |e|   |                     #|
  // |                    |r|   |                     #|
  // |                    |t|   |                     #|
  // |                    |i|   |                     #|
  // |                    |c|   |                     #|
  // |                    |a|   |                     #|
  // |                    |l|   |                     #|
  // |                    | |   |                     #|
  // |                    |l|   |                     #|
  // |                    |a|   |                     #|
  // |                    |y|   |                     #|
  // |                    |e|   |                     #|
  // |                    |r|   |                     #|
  // +--------------------+-+   |                     #|
  // | horizontal  layer  | |   |                     #|
  // +--------------------+-+   |                     #|  <-- 950px
  // |                      |   |                     #|
  // |                      |   |##################### |
  // +----------------------+   +----------------------+  <-- 1000px
  //
  // The layer geometry is set up for a 950px tall viewport, but the user can
  // actually see down to 1000px. Thus we have to move the quad for the
  // horizontal scrollbar down by the vertical_adjust_ factor and lay the
  // vertical thumb out on a track lengthed by the vertical_adjust_ factor. This
  // means the quads may extend outside the layer's bounds.

  int thumb_length = thumb_length_;
  float track_length = track_length_;
  if (orientation_ == VERTICAL)
    track_length += vertical_adjust_;

  if (layer_tree_impl()->settings().solid_color_scrollbars) {
    thumb_length = std::max(
        static_cast<int>(visible_to_total_length_ratio_ * track_length),
        thumb_thickness_);
  }

  // With the length known, we can compute the thumb's position.
  float clamped_current_pos =
      std::min(std::max(current_pos_, 0.f), static_cast<float>(maximum_));
  float ratio = clamped_current_pos / maximum_;
  float max_offset = track_length - thumb_length;
  int thumb_offset = static_cast<int>(ratio * max_offset) + track_start_;

  gfx::RectF thumb_rect;
  if (orientation_ == HORIZONTAL) {
    thumb_rect = gfx::RectF(thumb_offset, vertical_adjust_,
                            thumb_length, thumb_thickness_);
  } else {
    thumb_rect = gfx::RectF(0.f, thumb_offset,
                            thumb_thickness_, thumb_length);
  }

  return ScrollbarLayerRectToContentRect(thumb_rect);
}

const char* PaintedScrollbarLayerImpl::LayerTypeAsString() const {
  return "cc::PaintedScrollbarLayerImpl";
}

}  // namespace cc
