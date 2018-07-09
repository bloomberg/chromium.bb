// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/rounded_rect_view.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"

namespace ash {

RoundedRectView::RoundedRectView(int corner_radius, SkColor background_color)
    : RoundedRectView(corner_radius,
                      corner_radius,
                      corner_radius,
                      corner_radius,
                      background_color) {}

RoundedRectView::RoundedRectView(int top_left_radius,
                                 int top_right_radius,
                                 int bottom_right_radius,
                                 int bottom_left_radius,
                                 SkColor background_color)
    : top_left_radius_(top_left_radius),
      top_right_radius_(top_right_radius),
      bottom_right_radius_(bottom_right_radius),
      bottom_left_radius_(bottom_left_radius),
      background_color_(background_color) {}

RoundedRectView::~RoundedRectView() = default;

void RoundedRectView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  const SkScalar kRadius[8] = {
      SkIntToScalar(top_left_radius_),     SkIntToScalar(top_left_radius_),
      SkIntToScalar(top_right_radius_),    SkIntToScalar(top_right_radius_),
      SkIntToScalar(bottom_right_radius_), SkIntToScalar(bottom_right_radius_),
      SkIntToScalar(bottom_left_radius_),  SkIntToScalar(bottom_left_radius_)};
  SkPath path;
  gfx::Rect bounds(size());
  path.addRoundRect(gfx::RectToSkRect(bounds), kRadius);

  canvas->ClipPath(path, true);
  canvas->DrawColor(background_color_);
}

void RoundedRectView::SetBackgroundColor(SkColor background_color) {
  if (background_color_ == background_color)
    return;

  background_color_ = background_color;
  SchedulePaint();
}

void RoundedRectView::SetCornerRadius(int top_left_radius,
                                      int top_right_radius,
                                      int bottom_right_radius,
                                      int bottom_left_radius) {
  if (top_left_radius_ == top_left_radius &&
      top_right_radius_ == top_right_radius &&
      bottom_right_radius_ == bottom_right_radius &&
      bottom_left_radius_ == bottom_left_radius) {
    return;
  }

  top_left_radius_ = top_left_radius;
  top_right_radius_ = top_right_radius;
  bottom_right_radius_ = bottom_right_radius;
  bottom_left_radius_ = bottom_left_radius;
  SchedulePaint();
}

void RoundedRectView::SetCornerRadius(int radius) {
  SetCornerRadius(radius, radius, radius, radius);
}

}  // namespace ash
