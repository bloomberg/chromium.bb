// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_scrollbar.h"

#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

FakeScrollbar::FakeScrollbar()
    : paint_(false),
      has_thumb_(false),
      is_overlay_(false),
      fill_color_(SK_ColorGREEN) {}

FakeScrollbar::FakeScrollbar(bool paint, bool has_thumb, bool is_overlay)
    : paint_(paint),
      has_thumb_(has_thumb),
      is_overlay_(is_overlay),
      fill_color_(SK_ColorGREEN) {}

FakeScrollbar::~FakeScrollbar() {}

ScrollbarOrientation FakeScrollbar::Orientation() const {
  return HORIZONTAL;
}

gfx::Point FakeScrollbar::Location() const { return location_; }

bool FakeScrollbar::IsOverlay() const { return is_overlay_; }

bool FakeScrollbar::HasThumb() const { return has_thumb_; }

int FakeScrollbar::ThumbThickness() const {
  return 10;
}

int FakeScrollbar::ThumbLength() const {
  return 10;
}

gfx::Rect FakeScrollbar::TrackRect() const {
  return gfx::Rect(0, 0, 100, 10);
}

void FakeScrollbar::PaintPart(SkCanvas* canvas,
                             ScrollbarPart part,
                             gfx::Rect content_rect) {
  if (!paint_)
    return;

  // Fill the scrollbar with a different color each time.
  fill_color_++;
  canvas->clear(SK_ColorBLACK | fill_color_);
}

}  // namespace cc
