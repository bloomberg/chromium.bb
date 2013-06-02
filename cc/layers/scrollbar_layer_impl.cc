// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer_impl.h"

#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/layers/layer.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

scoped_ptr<ScrollbarLayerImpl> ScrollbarLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation) {
  return make_scoped_ptr(new ScrollbarLayerImpl(tree_impl,
                                                id,
                                                orientation));
}

ScrollbarLayerImpl::ScrollbarLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation)
    : LayerImpl(tree_impl, id),
      track_resource_id_(0),
      thumb_resource_id_(0),
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
      is_scrollable_area_active_(false),
      is_scroll_view_scrollbar_(false),
      enabled_(false),
      is_custom_scrollbar_(false),
      is_overlay_scrollbar_(false) {}

ScrollbarLayerImpl::~ScrollbarLayerImpl() {}

ScrollbarLayerImpl* ScrollbarLayerImpl::ToScrollbarLayer() {
  return this;
}

scoped_ptr<LayerImpl> ScrollbarLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return ScrollbarLayerImpl::Create(tree_impl,
                                    id(),
                                    orientation_).PassAs<LayerImpl>();
}

void ScrollbarLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);

  ScrollbarLayerImpl* scrollbar_layer = static_cast<ScrollbarLayerImpl*>(layer);

  scrollbar_layer->set_thumb_thickness(thumb_thickness_);
  scrollbar_layer->set_thumb_length(thumb_length_);
  scrollbar_layer->set_track_start(track_start_);
  scrollbar_layer->set_track_length(track_length_);

  scrollbar_layer->set_track_resource_id(track_resource_id_);
  scrollbar_layer->set_thumb_resource_id(thumb_resource_id_);
}

bool ScrollbarLayerImpl::WillDraw(DrawMode draw_mode,
                                  ResourceProvider* resource_provider) {
  LayerImpl::WillDraw(draw_mode, resource_provider);
  return draw_mode != DRAW_MODE_RESOURCELESS_SOFTWARE ||
         layer_tree_impl()->settings().solid_color_scrollbars;
}

void ScrollbarLayerImpl::AppendQuads(QuadSink* quad_sink,
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

  if (thumb_resource_id_ && !thumb_quad_rect.IsEmpty()) {
    gfx::Rect opaque_rect;
    const float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
    scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
    quad->SetNew(shared_quad_state,
                 thumb_quad_rect,
                 opaque_rect,
                 thumb_resource_id_,
                 premultipled_alpha,
                 uv_top_left,
                 uv_bottom_right,
                 opacity,
                 flipped);
    quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
  }

  if (!track_resource_id_)
    return;

  // Order matters here: since the back track texture is being drawn to the
  // entire contents rect, we must append it after the thumb and fore track
  // quads. The back track texture contains (and displays) the buttons.
  if (!content_bounds_rect.IsEmpty()) {
    gfx::Rect quad_rect(content_bounds_rect);
    gfx::Rect opaque_rect(contents_opaque() ? quad_rect : gfx::Rect());
    const float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
    scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
    quad->SetNew(shared_quad_state,
                 quad_rect,
                 opaque_rect,
                 track_resource_id_,
                 premultipled_alpha,
                 uv_top_left,
                 uv_bottom_right,
                 opacity,
                 flipped);
    quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
  }
}

ScrollbarOrientation ScrollbarLayerImpl::Orientation() const {
  return orientation_;
}

float ScrollbarLayerImpl::CurrentPos() const {
  return current_pos_;
}

int ScrollbarLayerImpl::Maximum() const {
  return maximum_;
}

gfx::Rect ScrollbarLayerImpl::ScrollbarLayerRectToContentRect(
    gfx::RectF layer_rect) const {
  // Don't intersect with the bounds as in layerRectToContentRect() because
  // layer_rect here might be in coordinates of the containing layer.
  gfx::RectF content_rect = gfx::ScaleRect(layer_rect,
                                           contents_scale_x(),
                                           contents_scale_y());
  return gfx::ToEnclosingRect(content_rect);
}

gfx::Rect ScrollbarLayerImpl::ComputeThumbQuadRect() const {
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
  float ratio = current_pos_ / maximum_;
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

void ScrollbarLayerImpl::DidLoseOutputSurface() {
  track_resource_id_ = 0;
  thumb_resource_id_ = 0;
}

const char* ScrollbarLayerImpl::LayerTypeAsString() const {
  return "cc::ScrollbarLayerImpl";
}

}  // namespace cc
