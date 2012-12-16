// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_web_scrollbar_theme_geometry.h"

namespace cc {

WebKit::WebScrollbarThemeGeometry*
    FakeWebScrollbarThemeGeometry::clone() const {
  return new FakeWebScrollbarThemeGeometry();
}

int FakeWebScrollbarThemeGeometry::thumbPosition(WebKit::WebScrollbar*) {
  return 0;
}

int FakeWebScrollbarThemeGeometry::thumbLength(WebKit::WebScrollbar*) {
  return 0;
}

int FakeWebScrollbarThemeGeometry::trackPosition(WebKit::WebScrollbar*) {
  return 0;
}

int FakeWebScrollbarThemeGeometry::trackLength(WebKit::WebScrollbar*) {
  return 0;
}

bool FakeWebScrollbarThemeGeometry::hasButtons(WebKit::WebScrollbar*) {
  return false;
}

bool FakeWebScrollbarThemeGeometry::hasThumb(WebKit::WebScrollbar*) {
  return false;
}

WebKit::WebRect FakeWebScrollbarThemeGeometry::trackRect(WebKit::WebScrollbar*) {
  return WebKit::WebRect();
}

WebKit::WebRect FakeWebScrollbarThemeGeometry::thumbRect(WebKit::WebScrollbar*) {
  return WebKit::WebRect();
}

int FakeWebScrollbarThemeGeometry::minimumThumbLength(WebKit::WebScrollbar*) {
  return 0;
}

int FakeWebScrollbarThemeGeometry::scrollbarThickness(WebKit::WebScrollbar*) {
  return 0;
}

WebKit::WebRect FakeWebScrollbarThemeGeometry::backButtonStartRect(
    WebKit::WebScrollbar*) {
  return WebKit::WebRect();
}

WebKit::WebRect FakeWebScrollbarThemeGeometry::backButtonEndRect(
    WebKit::WebScrollbar*) {
  return WebKit::WebRect();
}

WebKit::WebRect FakeWebScrollbarThemeGeometry::forwardButtonStartRect(
    WebKit::WebScrollbar*) {
  return WebKit::WebRect();
}

WebKit::WebRect FakeWebScrollbarThemeGeometry::forwardButtonEndRect(
    WebKit::WebScrollbar*) {
  return WebKit::WebRect();
}

WebKit::WebRect FakeWebScrollbarThemeGeometry::constrainTrackRectToTrackPieces(
   WebKit::WebScrollbar*,
   const WebKit::WebRect&) {
  return WebKit::WebRect();
}

void FakeWebScrollbarThemeGeometry::splitTrack(
    WebKit::WebScrollbar*,
    const WebKit::WebRect& track,
    WebKit::WebRect& startTrack,
    WebKit::WebRect& thumb,
    WebKit::WebRect& endTrack) {
  startTrack = WebKit::WebRect();
  thumb = WebKit::WebRect();
  endTrack = WebKit::WebRect();
}

}  // namespace cc
