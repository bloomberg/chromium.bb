// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_separator_view.h"

#include "base/logging.h"
#include "ui/gfx/canvas.h"

LocationBarSeparatorView::LocationBarSeparatorView()
    : separator_color_(SK_ColorBLACK) {
}

LocationBarSeparatorView::~LocationBarSeparatorView() {
}

gfx::Size LocationBarSeparatorView::GetPreferredSize() {
  // Height doesn't matter.
  return gfx::Size(1, 0);
}

void LocationBarSeparatorView::OnPaint(gfx::Canvas* canvas) {
  // Note that views::Border::CreateSolidSidedBorder is not used because it
  // paints a 2-px thick border in retina display, whereas we want a 1-px thick
  // vertical line in both regular and retina displays.
  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setStrokeWidth(0);
  paint.setColor(separator_color_);
  canvas->DrawLine(gfx::Point(0, 0), gfx::Point(0, height()), paint);
}
