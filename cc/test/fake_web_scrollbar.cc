// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_web_scrollbar.h"

namespace cc {

void FakeWebScrollbar::setOverlay(bool is_overlay) {
  is_overlay_ = is_overlay;
}

bool FakeWebScrollbar::isOverlay() const {
  return is_overlay_;
}

int FakeWebScrollbar::value() const {
  return 0;
}

WebKit::WebPoint FakeWebScrollbar::location() const {
  return WebKit::WebPoint();
}

WebKit::WebSize FakeWebScrollbar::size() const {
  return WebKit::WebSize();
}

bool FakeWebScrollbar::enabled() const {
  return true;
}

int FakeWebScrollbar::maximum() const {
  return 0;
}

int FakeWebScrollbar::totalSize() const {
  return 0;
}

bool FakeWebScrollbar::isScrollViewScrollbar() const {
  return false;
}

bool FakeWebScrollbar::isScrollableAreaActive() const {
  return true;
}

WebKit::WebScrollbar::ScrollbarControlSize FakeWebScrollbar::controlSize()
    const {
  return WebScrollbar::RegularScrollbar;
}

WebKit::WebScrollbar::ScrollbarPart FakeWebScrollbar::pressedPart() const {
  return WebScrollbar::NoPart;
}

WebKit::WebScrollbar::ScrollbarPart FakeWebScrollbar::hoveredPart() const {
  return WebScrollbar::NoPart;
}

WebKit::WebScrollbar::ScrollbarOverlayStyle
FakeWebScrollbar::scrollbarOverlayStyle() const {
  return WebScrollbar::ScrollbarOverlayStyleDefault;
}

bool FakeWebScrollbar::isCustomScrollbar() const {
  return false;
}

WebKit::WebScrollbar::Orientation FakeWebScrollbar::orientation() const {
  return WebScrollbar::Horizontal;
}

FakeWebScrollbar::FakeWebScrollbar() : is_overlay_(false) {}

}  // namespace cc
