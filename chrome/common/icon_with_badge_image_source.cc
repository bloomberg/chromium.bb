// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/icon_with_badge_image_source.h"

#include "chrome/common/badge_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"

IconWithBadgeImageSource::IconWithBadgeImageSource(
    const gfx::ImageSkia& icon,
    const gfx::Size& icon_size,
    const gfx::Size& spacing,
    const std::string& text,
    const SkColor& text_color,
    const SkColor& background_color,
    extensions::ActionInfo::Type action_type)
        : gfx::CanvasImageSource(icon_size, false),
          icon_(icon),
          spacing_(spacing),
          text_(text),
          text_color_(text_color),
          background_color_(background_color),
          action_type_(action_type) {
}

IconWithBadgeImageSource::~IconWithBadgeImageSource() {}

void IconWithBadgeImageSource::Draw(gfx::Canvas* canvas) {
  canvas->DrawImageInt(icon_, 0, 0, SkPaint());
  gfx::Rect bounds(size_.width() + spacing_.width(),
                   size_.height() + spacing_.height());

  // Draw a badge on the provided browser action icon's canvas.
  badge_util::PaintBadge(canvas, bounds, text_, text_color_,
                         background_color_, size_.width(),
                         action_type_);
}
