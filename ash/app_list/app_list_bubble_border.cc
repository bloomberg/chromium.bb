// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_bubble_border.h"

#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkBlurDrawLooper.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"

namespace {

const int kCornerRadius = 3;

const int kArrowHeight = 10;
const int kArrowWidth = 20;

const SkColor kBorderColor = SkColorSetARGB(0xFF, 0, 0, 0);
const int kBorderSize = 1;

const SkColor kShadowColor = SkColorSetARGB(0xFF, 0, 0, 0);
const int kShadowRadius = 4;

const SkColor kModelViewGradientColor1 = SkColorSetARGB(0xFF, 0xFE, 0xFE, 0xFE);
const SkColor kModelViewGradientColor2 = SkColorSetARGB(0xFF, 0xF8, 0xF8, 0xF8);
const int kModelViewGradientSize = 10;

const SkColor kFooterBorderGradientColor1 =
    SkColorSetARGB(0xFF, 0xA0, 0xA0, 0xA0);
const SkColor kFooterBorderGradientColor2 =
    SkColorSetARGB(0xFF, 0xD4, 0xD4, 0xD4);
const int kFooterBorderSize = 3;
const SkColor kFooterBackground = SkColorSetARGB(0xFF, 0xDC, 0xDC, 0xDC);

// TODO(xiyuan): Merge this with the one in skia_util.
SkShader* CreateVerticalGradientShader(int start_point,
                                       int end_point,
                                       SkColor start_color,
                                       SkColor end_color,
                                       SkShader::TileMode mode) {
  SkColor grad_colors[2] = { start_color, end_color};
  SkPoint grad_points[2];
  grad_points[0].iset(0, start_point);
  grad_points[1].iset(0, end_point);

  return SkGradientShader::CreateLinear(grad_points,
                                        grad_colors,
                                        NULL,
                                        2,
                                        mode);
}

// Builds a bubble shape for given |bounds|.
void BuildShape(const gfx::Rect& bounds,
                SkScalar padding,
                SkScalar arrow_offset,
                SkPath* path) {
  const SkScalar left = SkIntToScalar(bounds.x()) + padding;
  const SkScalar top = SkIntToScalar(bounds.y()) + padding;
  const SkScalar right = SkIntToScalar(bounds.right()) - padding;
  const SkScalar bottom = SkIntToScalar(bounds.bottom()) - padding;

  const SkScalar center_x = SkIntToScalar((bounds.x() + bounds.right()) / 2);
  const SkScalar center_y = SkIntToScalar((bounds.y() + bounds.bottom()) / 2);

  const SkScalar half_array_width = SkIntToScalar(kArrowWidth / 2);
  const SkScalar arrow_height = SkIntToScalar(kArrowHeight) - padding;

  path->reset();
  path->incReserve(12);

  path->moveTo(center_x, top);
  path->arcTo(left, top, left, center_y, SkIntToScalar(kCornerRadius));
  path->arcTo(left, bottom, center_x  - half_array_width, bottom,
              SkIntToScalar(kCornerRadius));
  path->lineTo(center_x + arrow_offset - half_array_width, bottom);
  path->lineTo(center_x + arrow_offset, bottom + arrow_height);
  path->lineTo(center_x + arrow_offset + half_array_width, bottom);
  path->arcTo(right, bottom, right, center_y, SkIntToScalar(kCornerRadius));
  path->arcTo(right, top, center_x, top, SkIntToScalar(kCornerRadius));
  path->close();
}

}  // namespace

namespace ash {

AppListBubbleBorder::AppListBubbleBorder(views::View* app_list_view)
    : views::BubbleBorder(views::BubbleBorder::BOTTOM_RIGHT,
                          views::BubbleBorder::NO_SHADOW),
      app_list_view_(app_list_view),
      arrow_offset_(0) {
}

AppListBubbleBorder::~AppListBubbleBorder() {
}

void AppListBubbleBorder::PaintModelViewBackground(
    gfx::Canvas* canvas,
    const gfx::Rect& bounds) const {
  const views::View* page_switcher = app_list_view_->child_at(1);
  const gfx::Rect page_switcher_bounds =
      app_list_view_->ConvertRectToWidget(page_switcher->bounds());
  gfx::Rect rect(bounds.x(),
                 bounds.y(),
                 bounds.width(),
                 page_switcher_bounds.y() - bounds.y());

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  SkSafeUnref(paint.setShader(CreateVerticalGradientShader(
      rect.y(),
      rect.y() + kModelViewGradientSize,
      kModelViewGradientColor1,
      kModelViewGradientColor2,
      SkShader::kClamp_TileMode)));
  canvas->DrawRect(rect, paint);
}

void AppListBubbleBorder::PaintPageSwitcherBackground(
    gfx::Canvas* canvas,
    const gfx::Rect& bounds) const {
  const views::View* page_switcher = app_list_view_->child_at(1);
  const gfx::Rect page_switcher_bounds =
      app_list_view_->ConvertRectToWidget(page_switcher->bounds());

  gfx::Rect rect(bounds.x(),
                 page_switcher_bounds.y(),
                 bounds.width(),
                 kFooterBorderSize);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  SkSafeUnref(paint.setShader(CreateVerticalGradientShader(
      rect.y(),
      rect.bottom(),
      kFooterBorderGradientColor1,
      kFooterBorderGradientColor2,
      SkShader::kClamp_TileMode)));
  canvas->DrawRect(rect, paint);

  rect.set_y(rect.bottom());
  rect.set_height(bounds.bottom() - rect.y() + kArrowHeight - kBorderSize);
  canvas->FillRect(rect, kFooterBackground);
}

void AppListBubbleBorder::GetInsets(gfx::Insets* insets) const {
  insets->Set(kShadowRadius + kBorderSize,
              kShadowRadius + kBorderSize,
              kShadowRadius + kBorderSize + kArrowHeight,
              kShadowRadius + kBorderSize);
}

gfx::Rect AppListBubbleBorder::GetBounds(
    const gfx::Rect& position_relative_to,
    const gfx::Size& contents_size) const {
  gfx::Size border_size(contents_size);
  gfx::Insets insets;
  GetInsets(&insets);
  border_size.Enlarge(insets.width(), insets.height());

  int anchor_x = (position_relative_to.x() + position_relative_to.right()) / 2;
  int arrow_tip_x = border_size.width() / 2 + arrow_offset_;

  return gfx::Rect(
      gfx::Point(anchor_x - arrow_tip_x,
                 position_relative_to.y() - border_size.height() +
                     kShadowRadius),
      border_size);
}

void AppListBubbleBorder::Paint(const views::View& view,
                                gfx::Canvas* canvas) const {
  gfx::Insets insets;
  GetInsets(&insets);

  gfx::Rect bounds = view.bounds();
  bounds.Inset(insets);

  SkPath path;

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setColor(kBorderColor);
  SkSafeUnref(paint.setLooper(
      new SkBlurDrawLooper(kShadowRadius,
                           0, 0,
                           kShadowColor,
                           SkBlurDrawLooper::kHighQuality_BlurFlag)));
  // Pads with 0.5 pixel since anti alias is used.
  BuildShape(bounds,
             SkDoubleToScalar(0.5),
             SkIntToScalar(arrow_offset_),
             &path);
  canvas->DrawPath(path, paint);

  // Pads with kBoprderSize pixels to leave space for border lines.
  BuildShape(bounds,
             SkIntToScalar(kBorderSize),
             SkIntToScalar(arrow_offset_),
             &path);
  canvas->Save();
  canvas->ClipPath(path);

  PaintModelViewBackground(canvas, bounds);
  PaintPageSwitcherBackground(canvas, bounds);

  canvas->Restore();
}

}  // namespace ash
