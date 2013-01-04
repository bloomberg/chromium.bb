// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_web_scrollbar_theme_geometry.h"

using WebKit::WebRect;

namespace cc {

WebKit::WebScrollbarThemeGeometry*
    FakeWebScrollbarThemeGeometry::clone() const {
  return new FakeWebScrollbarThemeGeometry(m_hasThumb);
}

int FakeWebScrollbarThemeGeometry::thumbPosition(WebKit::WebScrollbar*) {
  if (!m_hasThumb)
    return 0;
  return 5;
}

int FakeWebScrollbarThemeGeometry::thumbLength(WebKit::WebScrollbar*) {
  if (!m_hasThumb)
    return 0;
  return 2;
}

int FakeWebScrollbarThemeGeometry::trackPosition(WebKit::WebScrollbar*) {
  return 0;
}

int FakeWebScrollbarThemeGeometry::trackLength(WebKit::WebScrollbar*) {
  return 10;
}

bool FakeWebScrollbarThemeGeometry::hasButtons(WebKit::WebScrollbar*) {
  return false;
}

bool FakeWebScrollbarThemeGeometry::hasThumb(WebKit::WebScrollbar*) {
  return m_hasThumb;
}

WebRect FakeWebScrollbarThemeGeometry::trackRect(WebKit::WebScrollbar*) {
  return WebRect(0, 0, 10, 10);
}

WebRect FakeWebScrollbarThemeGeometry::thumbRect(WebKit::WebScrollbar*) {
  if (!m_hasThumb)
    return WebRect(0, 0, 0, 0);
  return WebRect(0, 5, 5, 2);
}

int FakeWebScrollbarThemeGeometry::minimumThumbLength(WebKit::WebScrollbar*) {
  return 0;
}

int FakeWebScrollbarThemeGeometry::scrollbarThickness(WebKit::WebScrollbar*) {
  return 0;
}

WebRect FakeWebScrollbarThemeGeometry::backButtonStartRect(
    WebKit::WebScrollbar*) {
  return WebRect();
}

WebRect FakeWebScrollbarThemeGeometry::backButtonEndRect(
    WebKit::WebScrollbar*) {
  return WebRect();
}

WebRect FakeWebScrollbarThemeGeometry::forwardButtonStartRect(
    WebKit::WebScrollbar*) {
  return WebRect();
}

WebRect FakeWebScrollbarThemeGeometry::forwardButtonEndRect(
    WebKit::WebScrollbar*) {
  return WebRect();
}

WebRect FakeWebScrollbarThemeGeometry::constrainTrackRectToTrackPieces(
   WebKit::WebScrollbar*,
   const WebRect&) {
  return WebRect();
}

void FakeWebScrollbarThemeGeometry::splitTrack(
    WebKit::WebScrollbar*,
    const WebRect& track,
    WebRect& startTrack,
    WebRect& thumb,
    WebRect& endTrack) {
  if (!m_hasThumb) {
    thumb = WebRect(0, 0, 0, 0);
    startTrack = WebRect(0, 0, 10, 10);
    endTrack = WebRect(0, 10, 10, 0);
  } else {
    thumb = WebRect(0, 5, 5, 2);
    startTrack = WebRect(0, 5, 0, 5);
    endTrack = WebRect(0, 0, 0, 5);
  }
}

}  // namespace cc
