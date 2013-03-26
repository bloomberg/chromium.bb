// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_geometry_stub.h"

#include <algorithm>

namespace cc {

ScrollbarGeometryStub::ScrollbarGeometryStub(
    scoped_ptr<WebKit::WebScrollbarThemeGeometry> geometry)
    : geometry_(geometry.Pass()) {}

ScrollbarGeometryStub::~ScrollbarGeometryStub() {}

WebKit::WebScrollbarThemeGeometry* ScrollbarGeometryStub::clone() const {
  return geometry_->clone();
}

int ScrollbarGeometryStub::thumbPosition(WebKit::WebScrollbar* scrollbar) {
  return geometry_->thumbPosition(scrollbar);
}

int ScrollbarGeometryStub::thumbLength(WebKit::WebScrollbar* scrollbar) {
  return std::max(0, geometry_->thumbLength(scrollbar));
}

int ScrollbarGeometryStub::trackPosition(WebKit::WebScrollbar* scrollbar) {
  return geometry_->trackPosition(scrollbar);
}

int ScrollbarGeometryStub::trackLength(WebKit::WebScrollbar* scrollbar) {
  return geometry_->trackLength(scrollbar);
}

bool ScrollbarGeometryStub::hasButtons(WebKit::WebScrollbar* scrollbar) {
  return geometry_->hasButtons(scrollbar);
}

bool ScrollbarGeometryStub::hasThumb(WebKit::WebScrollbar* scrollbar) {
  return geometry_->hasThumb(scrollbar);
}

WebKit::WebRect ScrollbarGeometryStub::trackRect(
    WebKit::WebScrollbar* scrollbar) {
  return geometry_->trackRect(scrollbar);
}

WebKit::WebRect ScrollbarGeometryStub::thumbRect(
    WebKit::WebScrollbar* scrollbar) {
  return geometry_->thumbRect(scrollbar);
}

int ScrollbarGeometryStub::minimumThumbLength(WebKit::WebScrollbar* scrollbar) {
  return geometry_->minimumThumbLength(scrollbar);
}

int ScrollbarGeometryStub::scrollbarThickness(WebKit::WebScrollbar* scrollbar) {
  return geometry_->scrollbarThickness(scrollbar);
}

WebKit::WebRect ScrollbarGeometryStub::backButtonStartRect(
    WebKit::WebScrollbar* scrollbar) {
  return geometry_->backButtonStartRect(scrollbar);
}

WebKit::WebRect ScrollbarGeometryStub::backButtonEndRect(
    WebKit::WebScrollbar* scrollbar) {
  return geometry_->backButtonEndRect(scrollbar);
}

WebKit::WebRect ScrollbarGeometryStub::forwardButtonStartRect(
    WebKit::WebScrollbar* scrollbar) {
  return geometry_->forwardButtonStartRect(scrollbar);
}

WebKit::WebRect ScrollbarGeometryStub::forwardButtonEndRect(
    WebKit::WebScrollbar* scrollbar) {
  return geometry_->forwardButtonEndRect(scrollbar);
}

WebKit::WebRect ScrollbarGeometryStub::constrainTrackRectToTrackPieces(
    WebKit::WebScrollbar* scrollbar,
    const WebKit::WebRect& rect) {
  return geometry_->constrainTrackRectToTrackPieces(scrollbar, rect);
}

void ScrollbarGeometryStub::splitTrack(
    WebKit::WebScrollbar* scrollbar,
    const WebKit::WebRect& unconstrained_track_rect,
    WebKit::WebRect& before_thumb_rect,
    WebKit::WebRect& thumb_rect,
    WebKit::WebRect& after_thumb_rect) {
  geometry_->splitTrack(scrollbar,
                        unconstrained_track_rect,
                        before_thumb_rect,
                        thumb_rect,
                        after_thumb_rect);
}

}  // namespace cc
