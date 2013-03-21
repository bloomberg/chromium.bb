// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_web_scrollbar_theme_geometry.h"

using WebKit::WebRect;

namespace cc {

WebKit::WebScrollbarThemeGeometry*
FakeWebScrollbarThemeGeometry::clone() const {
  return new FakeWebScrollbarThemeGeometry(has_thumb_);
}

int FakeWebScrollbarThemeGeometry::thumbPosition(
    WebKit::WebScrollbar* scrollbar) {
  if (!has_thumb_)
    return 0;
  return 5;
}

int FakeWebScrollbarThemeGeometry::thumbLength(
    WebKit::WebScrollbar* scrollbar) {
  if (!has_thumb_)
    return 0;
  return 2;
}

int FakeWebScrollbarThemeGeometry::trackPosition(
    WebKit::WebScrollbar* scrollbar) {
  return 0;
}

int FakeWebScrollbarThemeGeometry::trackLength(
    WebKit::WebScrollbar* scrollbar) {
  return 10;
}

bool FakeWebScrollbarThemeGeometry::hasButtons(
    WebKit::WebScrollbar* scrollbar) {
  return false;
}

bool FakeWebScrollbarThemeGeometry::hasThumb(WebKit::WebScrollbar* scrollbar) {
  return has_thumb_;
}

WebRect FakeWebScrollbarThemeGeometry::trackRect(
    WebKit::WebScrollbar* scrollbar) {
  return WebRect(0, 0, 10, 10);
}

WebRect FakeWebScrollbarThemeGeometry::thumbRect(
    WebKit::WebScrollbar* scrollbar) {
  if (!has_thumb_)
    return WebRect(0, 0, 0, 0);
  return WebRect(0, 5, 5, 2);
}

int FakeWebScrollbarThemeGeometry::minimumThumbLength(
    WebKit::WebScrollbar* scrollbar) {
  return 0;
}

int FakeWebScrollbarThemeGeometry::scrollbarThickness(
    WebKit::WebScrollbar* scrollbar) {
  return 0;
}

WebRect FakeWebScrollbarThemeGeometry::backButtonStartRect(
    WebKit::WebScrollbar* scrollbar) {
  return WebRect();
}

WebRect FakeWebScrollbarThemeGeometry::backButtonEndRect(
    WebKit::WebScrollbar* scrollbar) {
  return WebRect();
}

WebRect FakeWebScrollbarThemeGeometry::forwardButtonStartRect(
    WebKit::WebScrollbar* scrollbar) {
  return WebRect();
}

WebRect FakeWebScrollbarThemeGeometry::forwardButtonEndRect(
    WebKit::WebScrollbar* scrollbar) {
  return WebRect();
}

WebRect FakeWebScrollbarThemeGeometry::constrainTrackRectToTrackPieces(
    WebKit::WebScrollbar* scrollbar,
    const WebRect& rect) {
  return WebRect();
}

void FakeWebScrollbarThemeGeometry::splitTrack(WebKit::WebScrollbar* scrollbar,
                                               const WebRect& track,
                                               WebRect& start_track,
                                               WebRect& thumb,
                                               WebRect& end_track) {
  if (!has_thumb_) {
    thumb = WebRect(0, 0, 0, 0);
    start_track = WebRect(0, 0, 10, 10);
    end_track = WebRect(0, 10, 10, 0);
  } else {
    thumb = WebRect(0, 5, 5, 2);
    start_track = WebRect(0, 5, 0, 5);
    end_track = WebRect(0, 0, 0, 5);
  }
}

}  // namespace cc
