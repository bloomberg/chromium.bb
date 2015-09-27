// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/icon_with_badge_image_source.h"

#include <algorithm>

#include "chrome/common/badge_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia_operations.h"

IconWithBadgeImageSource::Badge::Badge(const std::string& text,
                                       SkColor text_color,
                                       SkColor background_color)
    : text(text), text_color(text_color), background_color(background_color) {}

IconWithBadgeImageSource::Badge::~Badge() {}

IconWithBadgeImageSource::IconWithBadgeImageSource(const gfx::Size& size)
    : gfx::CanvasImageSource(size, false),
      grayscale_(false),
      paint_decoration_(false) {
}

IconWithBadgeImageSource::~IconWithBadgeImageSource() {
}

void IconWithBadgeImageSource::SetIcon(const gfx::Image& icon) {
  icon_ = icon;
}

void IconWithBadgeImageSource::SetBadge(scoped_ptr<Badge> badge) {
  badge_ = badge.Pass();
}

void IconWithBadgeImageSource::Draw(gfx::Canvas* canvas) {
  if (icon_.IsEmpty())
    return;

  int x_offset = std::floor((size().width() - icon_.Width()) / 2.0);
  int y_offset = std::floor((size().height() - icon_.Height()) / 2.0);
  gfx::ImageSkia skia = icon_.AsImageSkia();
  if (grayscale_)
    skia = gfx::ImageSkiaOperations::CreateHSLShiftedImage(skia, {-1, 0, 0.6});
  canvas->DrawImageInt(skia, x_offset, y_offset);

  // Draw a badge on the provided browser action icon's canvas.
  // TODO(devlin): Pull PaintBadge() into here.
  if (badge_) {
    badge_util::PaintBadge(canvas, gfx::Rect(size()), badge_->text,
                           badge_->text_color, badge_->background_color,
                           size().width());
  }

  if (paint_decoration_)
    PaintDecoration(canvas);
}

void IconWithBadgeImageSource::PaintDecoration(gfx::Canvas* canvas) {
  static const SkColor decoration_color = SkColorSetARGB(255, 70, 142, 226);

  int major_radius = std::ceil(size().width() / 5.0);
  int minor_radius = std::ceil(major_radius / 2.0);
  gfx::Point center_point(major_radius + 1,
                          size().height() - (major_radius) - 1);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorTRANSPARENT);
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  canvas->DrawCircle(center_point,
                     major_radius,
                     paint);
  paint.setColor(decoration_color);
  canvas->DrawCircle(center_point,
                     minor_radius,
                     paint);
}

