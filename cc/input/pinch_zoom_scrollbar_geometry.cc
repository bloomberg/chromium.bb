// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/pinch_zoom_scrollbar_geometry.h"

#include <algorithm>

#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbar.h"

namespace cc {

const int PinchZoomScrollbarGeometry::kTrackWidth = 10;

WebScrollbarThemeGeometry* PinchZoomScrollbarGeometry::clone() const {
  return static_cast<WebScrollbarThemeGeometry*>(
    new PinchZoomScrollbarGeometry());
}

int PinchZoomScrollbarGeometry::thumbPosition(WebScrollbar* scrollbar) {
  if (scrollbar->enabled()) {
    float max_value = scrollbar->maximum();
    if (!max_value)
      return 1;
    int value = std::min(std::max(0, scrollbar->value()), scrollbar->maximum());
    float pos = (trackLength(scrollbar) - thumbLength(scrollbar)) *
                value / max_value;
    return static_cast<int>(floorf((pos > 0 && pos < 1) ? 1 : pos));
  }
  return 0;
}

int PinchZoomScrollbarGeometry::thumbLength(WebScrollbar* scrollbar) {
  if (!scrollbar->enabled())
    return 0;

  float size = std::max(scrollbar->size().width, scrollbar->size().height);
  float proportion = size / scrollbar->totalSize();
  int track_length = this->trackLength(scrollbar);
  int length =  proportion * track_length + 0.5f;
  length = std::max(length, kTrackWidth);
  if (length > track_length)
    length = 0;
  return length;
}

int PinchZoomScrollbarGeometry::trackPosition(WebScrollbar* scrollbar) {
  return 0;
}

int PinchZoomScrollbarGeometry::trackLength(WebScrollbar* scrollbar) {
  WebRect track = trackRect(scrollbar);
  if (scrollbar->orientation() == WebScrollbar::Horizontal)
    return track.width;
  else
    return track.height;
}

bool PinchZoomScrollbarGeometry::hasButtons(WebScrollbar* scrollbar) {
  return false;
}

bool PinchZoomScrollbarGeometry::hasThumb(WebScrollbar* scrollbar) {
  return true;
}

WebRect PinchZoomScrollbarGeometry::trackRect(WebScrollbar* scrollbar) {
  int thickness = scrollbarThickness(scrollbar);
  if (scrollbar->orientation() == WebScrollbar::Horizontal) {
    return WebRect(scrollbar->location().x, scrollbar->location().y,
                   scrollbar->size().width, thickness);
  } else {
    return WebRect(scrollbar->location().x, scrollbar->location().y,
                   thickness, scrollbar->size().height);
  }
}

WebRect PinchZoomScrollbarGeometry::thumbRect(WebScrollbar* scrollbar) {
  WebRect track = trackRect(scrollbar);
  int thumb_pos = thumbPosition(scrollbar);
  int thickness = scrollbarThickness(scrollbar);
  if (scrollbar->orientation() == WebScrollbar::Horizontal) {
    return WebRect(track.x + thumb_pos, track.y + (track.height - thickness) /
        2, thumbLength(scrollbar), thickness);
  } else {
    return WebRect(track.x + (track.width - thickness) / 2, track.y + thumb_pos,
                   thickness, thumbLength(scrollbar));
  }
}

int PinchZoomScrollbarGeometry::minimumThumbLength(WebScrollbar* scrollbar) {
  return scrollbarThickness(scrollbar);
}

int PinchZoomScrollbarGeometry::scrollbarThickness(WebScrollbar* scrollbar) {
  return kTrackWidth;
}

WebRect PinchZoomScrollbarGeometry::backButtonStartRect(
    WebScrollbar* scrollbar) {
  return WebRect();
}

WebRect PinchZoomScrollbarGeometry::backButtonEndRect(WebScrollbar* scrollbar) {
  return WebRect();
}

WebRect PinchZoomScrollbarGeometry::forwardButtonStartRect(
    WebScrollbar* scrollbar) {
  return WebRect();
}

WebRect PinchZoomScrollbarGeometry::forwardButtonEndRect(
    WebScrollbar* scrollbar) {
  return WebRect();
}

WebRect PinchZoomScrollbarGeometry::constrainTrackRectToTrackPieces(
    WebScrollbar* scrollbar, const WebRect& rect) {
  return rect;
}

void PinchZoomScrollbarGeometry::splitTrack(
    WebScrollbar* scrollbar, const WebRect& track, WebRect& start_track,
    WebRect& thumb, WebRect& end_track) {
  thumb = thumbRect(scrollbar);
  start_track = WebRect();
  end_track = WebRect();
}

}  // namespace cc
