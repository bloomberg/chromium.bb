// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scrollbar_layer_impl.h"

#include "cc/layer_tree_impl.h"
#include "cc/layer_tree_settings.h"
#include "cc/quad_sink.h"
#include "cc/scrollbar_animation_controller.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/texture_draw_quad.h"
#include "ui/gfx/rect_conversions.h"

using WebKit::WebRect;
using WebKit::WebScrollbar;

namespace cc {

scoped_ptr<ScrollbarLayerImpl> ScrollbarLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    scoped_ptr<ScrollbarGeometryFixedThumb> geometry) {
  return make_scoped_ptr(new ScrollbarLayerImpl(tree_impl,
                                                id,
                                                geometry.Pass()));
}

ScrollbarLayerImpl::ScrollbarLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    scoped_ptr<ScrollbarGeometryFixedThumb> geometry)
    : ScrollbarLayerImplBase(tree_impl, id),
      scrollbar_(this),
      back_track_resource_id_(0),
      fore_track_resource_id_(0),
      thumb_resource_id_(0),
      geometry_(geometry.Pass()),
      current_pos_(0),
      total_size_(0),
      maximum_(0),
      scroll_layer_id_(-1),
      scrollbar_overlay_style_(WebScrollbar::ScrollbarOverlayStyleDefault),
      orientation_(WebScrollbar::Horizontal),
      control_size_(WebScrollbar::RegularScrollbar),
      pressed_part_(WebScrollbar::NoPart),
      hovered_part_(WebScrollbar::NoPart),
      is_scrollable_area_active_(false),
      is_scroll_view_scrollbar_(false),
      enabled_(false),
      is_custom_scrollbar_(false),
      is_overlay_scrollbar_(false) {}

ScrollbarLayerImpl::~ScrollbarLayerImpl() {}

ScrollbarLayerImpl* ScrollbarLayerImpl::toScrollbarLayer() {
  return this;
}

void ScrollbarLayerImpl::SetScrollbarData(WebScrollbar* scrollbar) {
  scrollbar_overlay_style_ = scrollbar->scrollbarOverlayStyle();
  orientation_ = scrollbar->orientation();
  control_size_ = scrollbar->controlSize();
  pressed_part_ = scrollbar->pressedPart();
  hovered_part_ = scrollbar->hoveredPart();
  is_scrollable_area_active_ = scrollbar->isScrollableAreaActive();
  is_scroll_view_scrollbar_ = scrollbar->isScrollViewScrollbar();
  enabled_ = scrollbar->enabled();
  is_custom_scrollbar_ = scrollbar->isCustomScrollbar();
  is_overlay_scrollbar_ = scrollbar->isOverlay();

  scrollbar->getTickmarks(tickmarks_);
}

void ScrollbarLayerImpl::SetThumbSize(gfx::Size size) {
  thumb_size_ = size;
  if (!geometry_) {
    // In impl-side painting, the ScrollbarLayerImpl in the pending tree
    // simply holds properties that are later pushed to the active tree's
    // layer, but it doesn't hold geometry or append quads.
    DCHECK(layerTreeImpl()->IsPendingTree());
    return;
  }
  geometry_->setThumbSize(size);
}

float ScrollbarLayerImpl::CurrentPos() const {
  return current_pos_;
}

int ScrollbarLayerImpl::TotalSize() const {
  return total_size_;
}

int ScrollbarLayerImpl::Maximum() const {
  return maximum_;
}

WebKit::WebScrollbar::Orientation ScrollbarLayerImpl::Orientation() const {
  return orientation_;
}

static gfx::RectF ToUVRect(gfx::Rect r, gfx::Rect bounds) {
  return gfx::ScaleRect(r, 1.f / bounds.width(), 1.f / bounds.height());
}

gfx::Rect ScrollbarLayerImpl::ScrollbarLayerRectToContentRect(
    gfx::Rect layer_rect) const {
  // Don't intersect with the bounds as in layerRectToContentRect() because
  // layer_rect here might be in coordinates of the containing layer.
  gfx::RectF content_rect = gfx::ScaleRect(layer_rect,
                                           contentsScaleX(),
                                           contentsScaleY());
  return gfx::ToEnclosingRect(content_rect);
}

scoped_ptr<LayerImpl> ScrollbarLayerImpl::createLayerImpl(
    LayerTreeImpl* tree_impl) {
  return ScrollbarLayerImpl::Create(tree_impl,
                                    id(),
                                    geometry_.Pass()).PassAs<LayerImpl>();
}

void ScrollbarLayerImpl::pushPropertiesTo(LayerImpl* layer) {
  LayerImpl::pushPropertiesTo(layer);

  ScrollbarLayerImpl* scrollbar_layer = static_cast<ScrollbarLayerImpl*>(layer);

  scrollbar_layer->SetScrollbarData(&scrollbar_);
  scrollbar_layer->SetThumbSize(thumb_size_);

  scrollbar_layer->set_back_track_resource_id(back_track_resource_id_);
  scrollbar_layer->set_fore_track_resource_id(fore_track_resource_id_);
  scrollbar_layer->set_thumb_resource_id(thumb_resource_id_);
}

void ScrollbarLayerImpl::appendQuads(QuadSink& quad_sink,
                                     AppendQuadsData& append_quads_data) {
  bool premultipled_alpha = true;
  bool flipped = false;
  gfx::PointF uv_top_left(0.f, 0.f);
  gfx::PointF uv_bottom_right(1.f, 1.f);
  gfx::Rect boundsRect(gfx::Point(), bounds());
  gfx::Rect contentBoundsRect(gfx::Point(), contentBounds());

  SharedQuadState* shared_quad_state =
      quad_sink.useSharedQuadState(createSharedQuadState());
  appendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  WebRect thumb_rect, back_track_rect, foreTrackRect;
  geometry_->splitTrack(&scrollbar_,
                        geometry_->trackRect(&scrollbar_),
                        back_track_rect,
                        thumb_rect,
                        foreTrackRect);
  if (!geometry_->hasThumb(&scrollbar_))
    thumb_rect = WebRect();

  if (layerTreeImpl()->settings().solidColorScrollbars) {
    int thickness_override =
        layerTreeImpl()->settings().solidColorScrollbarThicknessDIP;
    if (thickness_override != -1) {
      if (scrollbar_.orientation() == WebScrollbar::Vertical)
        thumb_rect.width = thickness_override;
      else
        thumb_rect.height = thickness_override;
    }
    gfx::Rect quad_rect(ScrollbarLayerRectToContentRect(thumb_rect));
    scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
    quad->SetNew(shared_quad_state,
                 quad_rect,
                 layerTreeImpl()->settings().solidColorScrollbarColor);
    quad_sink.append(quad.PassAs<DrawQuad>(), append_quads_data);
    return;
  }

  if (thumb_resource_id_ && !thumb_rect.isEmpty()) {
    gfx::Rect quad_rect(ScrollbarLayerRectToContentRect(thumb_rect));
    gfx::Rect opaque_rect;
    const float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
    scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
    quad->SetNew(shared_quad_state,
                 quad_rect,
                 opaque_rect,
                 thumb_resource_id_,
                 premultipled_alpha,
                 uv_top_left,
                 uv_bottom_right,
                 opacity,
                 flipped);
    quad_sink.append(quad.PassAs<DrawQuad>(), append_quads_data);
  }

  if (!back_track_resource_id_)
    return;

  // We only paint the track in two parts if we were given a texture for the
  // forward track part.
  if (fore_track_resource_id_ && !foreTrackRect.isEmpty()) {
    gfx::Rect quad_rect(ScrollbarLayerRectToContentRect(foreTrackRect));
    gfx::Rect opaque_rect(contentsOpaque() ? quad_rect : gfx::Rect());
    gfx::RectF uv_rect(ToUVRect(foreTrackRect, boundsRect));
    const float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
    scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
    quad->SetNew(shared_quad_state,
                 quad_rect,
                 opaque_rect,
                 fore_track_resource_id_,
                 premultipled_alpha,
                 uv_rect.origin(),
                 uv_rect.bottom_right(),
                 opacity,
                 flipped);
    quad_sink.append(quad.PassAs<DrawQuad>(), append_quads_data);
  }

  // Order matters here: since the back track texture is being drawn to the
  // entire contents rect, we must append it after the thumb and fore track
  // quads. The back track texture contains (and displays) the buttons.
  if (!contentBoundsRect.IsEmpty()) {
    gfx::Rect quad_rect(contentBoundsRect);
    gfx::Rect opaque_rect(contentsOpaque() ? quad_rect : gfx::Rect());
    const float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
    scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
    quad->SetNew(shared_quad_state,
                 quad_rect,
                 opaque_rect,
                 back_track_resource_id_,
                 premultipled_alpha,
                 uv_top_left,
                 uv_bottom_right,
                 opacity,
                 flipped);
    quad_sink.append(quad.PassAs<DrawQuad>(), append_quads_data);
  }
}

void ScrollbarLayerImpl::didLoseOutputSurface() {
  back_track_resource_id_ = 0;
  fore_track_resource_id_ = 0;
  thumb_resource_id_ = 0;
}

bool ScrollbarLayerImpl::Scrollbar::isOverlay() const {
  return owner_->is_overlay_scrollbar_;
}

int ScrollbarLayerImpl::Scrollbar::value() const {
  return owner_->CurrentPos();
}

WebKit::WebPoint ScrollbarLayerImpl::Scrollbar::location() const {
  return WebKit::WebPoint();
}

WebKit::WebSize ScrollbarLayerImpl::Scrollbar::size() const {
  return WebKit::WebSize(owner_->bounds().width(), owner_->bounds().height());
}

bool ScrollbarLayerImpl::Scrollbar::enabled() const {
  return owner_->enabled_;
}

int ScrollbarLayerImpl::Scrollbar::maximum() const {
  return owner_->Maximum();
}

int ScrollbarLayerImpl::Scrollbar::totalSize() const {
  return owner_->TotalSize();
}

bool ScrollbarLayerImpl::Scrollbar::isScrollViewScrollbar() const {
  return owner_->is_scroll_view_scrollbar_;
}

bool ScrollbarLayerImpl::Scrollbar::isScrollableAreaActive() const {
  return owner_->is_scrollable_area_active_;
}

void ScrollbarLayerImpl::Scrollbar::getTickmarks(
    WebKit::WebVector<WebRect>& tickmarks) const {
  tickmarks = owner_->tickmarks_;
}

WebScrollbar::ScrollbarControlSize ScrollbarLayerImpl::Scrollbar::controlSize()
    const {
  return owner_->control_size_;
}

WebScrollbar::ScrollbarPart ScrollbarLayerImpl::Scrollbar::pressedPart() const {
  return owner_->pressed_part_;
}

WebScrollbar::ScrollbarPart ScrollbarLayerImpl::Scrollbar::hoveredPart() const {
  return owner_->hovered_part_;
}

WebScrollbar::ScrollbarOverlayStyle
ScrollbarLayerImpl::Scrollbar::scrollbarOverlayStyle() const {
  return owner_->scrollbar_overlay_style_;
}

WebScrollbar::Orientation ScrollbarLayerImpl::Scrollbar::orientation() const {
  return owner_->orientation_;
}

bool ScrollbarLayerImpl::Scrollbar::isCustomScrollbar() const {
  return owner_->is_custom_scrollbar_;
}

const char* ScrollbarLayerImpl::layerTypeAsString() const {
  return "ScrollbarLayer";
}

}  // namespace cc
