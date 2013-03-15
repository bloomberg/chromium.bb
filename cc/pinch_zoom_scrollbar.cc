// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/pinch_zoom_scrollbar.h"

#include "cc/layer.h"
#include "cc/layer_tree_host.h"
#include "cc/pinch_zoom_scrollbar_geometry.h"

namespace cc {

const float PinchZoomScrollbar::kDefaultOpacity = 0.5f;
const float PinchZoomScrollbar::kFadeDurationInSeconds = 0.5f;

PinchZoomScrollbar::PinchZoomScrollbar(
    WebKit::WebScrollbar::Orientation orientation, LayerTreeHost* owner)
  : orientation_(orientation),
    owner_(owner) {
  DCHECK(owner_);
}


bool PinchZoomScrollbar::isOverlay() const { return true; }

int PinchZoomScrollbar::value() const {
  const Layer* root_scroll_layer = owner_->RootScrollLayer();
  if (!root_scroll_layer)
    return 0;

  if (orientation_ == WebKit::WebScrollbar::Horizontal)
    return root_scroll_layer->scroll_offset().x();
  else
    return root_scroll_layer->scroll_offset().y();
}

WebKit::WebPoint PinchZoomScrollbar::location() const {
  return WebKit::WebPoint();
}

WebKit::WebSize PinchZoomScrollbar::size() const {
  gfx::Size viewport_size = owner_->layout_viewport_size();
  gfx::Size size;
  int track_width = PinchZoomScrollbarGeometry::kTrackWidth;
  if (orientation_ == WebKit::WebScrollbar::Horizontal)
    size = gfx::Size(viewport_size.width() - track_width, track_width);
  else
    size = gfx::Size(track_width, viewport_size.height() - track_width);
  return WebKit::WebSize(size);
}

bool PinchZoomScrollbar::enabled() const {
  return true;
}

int PinchZoomScrollbar::maximum() const {
  const Layer* root_scroll_layer = owner_->RootScrollLayer();
  if (!root_scroll_layer)
    return 0;

  gfx::Size device_viewport_size = owner_->device_viewport_size();
  gfx::Size root_scroll_bounds = root_scroll_layer->content_bounds();

  if (orientation_ == WebKit::WebScrollbar::Horizontal)
    return root_scroll_bounds.width() - device_viewport_size.width();
  else
    return root_scroll_bounds.height() - device_viewport_size.height();
}

int PinchZoomScrollbar::totalSize() const {
  const Layer* root_scroll_layer = owner_->RootScrollLayer();
  gfx::Size size;
  if (root_scroll_layer)
    size = root_scroll_layer->content_bounds();
  else
    size = gfx::Size();

  if (orientation_ == WebKit::WebScrollbar::Horizontal)
    return size.width();
  else
    return size.height();
}

bool PinchZoomScrollbar::isScrollViewScrollbar() const {
  return false;
}

bool PinchZoomScrollbar::isScrollableAreaActive() const {
  return true;
}

WebKit::WebScrollbar::ScrollbarControlSize PinchZoomScrollbar::controlSize()
    const {
  return WebKit::WebScrollbar::SmallScrollbar;
}

WebKit::WebScrollbar::ScrollbarPart PinchZoomScrollbar::pressedPart() const {
  return WebKit::WebScrollbar::NoPart;
}

WebKit::WebScrollbar::ScrollbarPart PinchZoomScrollbar::hoveredPart() const {
  return WebKit::WebScrollbar::NoPart;
}

WebKit::WebScrollbar::ScrollbarOverlayStyle
PinchZoomScrollbar::scrollbarOverlayStyle() const {
  return WebKit::WebScrollbar::ScrollbarOverlayStyleDefault;
}
bool PinchZoomScrollbar::isCustomScrollbar() const {
  return false;
}

WebKit::WebScrollbar::Orientation PinchZoomScrollbar::orientation() const {
  return orientation_;
}

}  // namespace cc
