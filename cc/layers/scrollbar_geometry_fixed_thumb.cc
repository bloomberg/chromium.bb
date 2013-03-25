// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_geometry_fixed_thumb.h"

#include <algorithm>
#include <cmath>

#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbar.h"

using WebKit::WebRect;
using WebKit::WebScrollbar;
using WebKit::WebScrollbarThemeGeometry;

namespace cc {

scoped_ptr<ScrollbarGeometryFixedThumb> ScrollbarGeometryFixedThumb::Create(
    scoped_ptr<WebScrollbarThemeGeometry> geometry) {
  return make_scoped_ptr(new ScrollbarGeometryFixedThumb(geometry.Pass()));
}

ScrollbarGeometryFixedThumb::~ScrollbarGeometryFixedThumb() {}

WebScrollbarThemeGeometry* ScrollbarGeometryFixedThumb::clone() const {
  ScrollbarGeometryFixedThumb* geometry = new ScrollbarGeometryFixedThumb(
      make_scoped_ptr(ScrollbarGeometryStub::clone()));
  geometry->thumb_size_ = thumb_size_;
  return geometry;
}

int ScrollbarGeometryFixedThumb::thumbLength(WebScrollbar* scrollbar) {
  if (scrollbar->orientation() == WebScrollbar::Horizontal)
    return thumb_size_.width();
  return thumb_size_.height();
}

int ScrollbarGeometryFixedThumb::thumbPosition(WebScrollbar* scrollbar) {
  if (!scrollbar->enabled())
    return 0;

  float size = scrollbar->maximum();
  if (!size)
    return 1;
  int value = std::min(std::max(0, scrollbar->value()), scrollbar->maximum());
  float pos =
      (trackLength(scrollbar) - thumbLength(scrollbar)) * value / size;
      return static_cast<int>(floorf((pos < 1 && pos > 0) ? 1 : pos));
}
void ScrollbarGeometryFixedThumb::splitTrack(
    WebScrollbar* scrollbar,
    const WebRect& unconstrained_track_rect,
    WebRect& before_thumb_rect,
    WebRect& thumb_rect,
    WebRect& after_thumb_rect) {
    // This is a reimplementation of ScrollbarThemeComposite::splitTrack().
    // Because the WebScrollbarThemeGeometry functions call down to native
    // ScrollbarThemeComposite code which uses ScrollbarThemeComposite virtual
    // helpers, there's no way to override a helper like thumbLength() from
    // the WebScrollbarThemeGeometry level. So, these three functions
    // (splitTrack(), thumbPosition(), thumbLength()) are copied here so that
    // the WebScrollbarThemeGeometry helper functions are used instead and
    // a fixed size thumbLength() can be used.

    WebRect track_rect =
        constrainTrackRectToTrackPieces(scrollbar, unconstrained_track_rect);
    int thickness = scrollbar->orientation() == WebScrollbar::Horizontal
                    ? scrollbar->size().height
                    : scrollbar->size().width;
    int thumb_pos = thumbPosition(scrollbar);
    if (scrollbar->orientation() == WebScrollbar::Horizontal) {
      thumb_rect = WebRect(track_rect.x + thumb_pos,
                           track_rect.y + (track_rect.height - thickness) / 2,
                           thumbLength(scrollbar),
                           thickness);
      before_thumb_rect = WebRect(track_rect.x,
                                  track_rect.y,
                                  thumb_pos + thumb_rect.width / 2,
                                  track_rect.height);
      after_thumb_rect = WebRect(track_rect.x + before_thumb_rect.width,
                                 track_rect.y,
                                 track_rect.x + track_rect.width -
                                 before_thumb_rect.x - before_thumb_rect.width,
                                 track_rect.height);
    } else {
      thumb_rect = WebRect(track_rect.x + (track_rect.width - thickness) / 2,
                           track_rect.y + thumb_pos,
                           thickness,
                           thumbLength(scrollbar));
      before_thumb_rect = WebRect(track_rect.x,
                                  track_rect.y,
                                  track_rect.width,
                                  thumb_pos + thumb_rect.height / 2);
      after_thumb_rect =
          WebRect(track_rect.x,
                  track_rect.y + before_thumb_rect.height,
                  track_rect.width,
                  track_rect.y + track_rect.height - before_thumb_rect.y -
                  before_thumb_rect.height);
    }
}

ScrollbarGeometryFixedThumb::ScrollbarGeometryFixedThumb(
    scoped_ptr<WebScrollbarThemeGeometry> geometry)
    : ScrollbarGeometryStub(geometry.Pass()) {}

}  // namespace cc
