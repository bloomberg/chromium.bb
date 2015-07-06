// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/icon_with_badge_image_source.h"

#include "chrome/common/badge_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"

IconWithBadgeImageSource::Badge::Badge(std::string text,
                                       SkColor text_color,
                                       SkColor background_color)
    : text(text), text_color(text_color), background_color(background_color) {
}

IconWithBadgeImageSource::Badge::~Badge() {}

IconWithBadgeImageSource::IconWithBadgeImageSource(const gfx::Size& size)
    : gfx::CanvasImageSource(size, false) {
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
  canvas->DrawImageInt(icon_.AsImageSkia(), x_offset, y_offset);

  // Draw a badge on the provided browser action icon's canvas.
  // TODO(devlin): Pull PaintBadge() into here.
  if (badge_) {
    badge_util::PaintBadge(canvas, gfx::Rect(size()), badge_->text,
                           badge_->text_color, badge_->background_color,
                           size().width());
  }
}
