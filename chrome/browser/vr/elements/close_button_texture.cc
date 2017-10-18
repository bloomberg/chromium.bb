// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/close_button_texture.h"

#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/color_scheme.h"
#include "chrome/browser/vr/elements/button.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace vr {

namespace {

constexpr float kIconScaleFactor = 0.5;

}  // namespace

CloseButtonTexture::CloseButtonTexture() = default;

CloseButtonTexture::~CloseButtonTexture() = default;

void CloseButtonTexture::Draw(SkCanvas* sk_canvas,
                              const gfx::Size& texture_size) {
  DCHECK_EQ(texture_size.height(), texture_size.width());
  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;

  size_.set_height(texture_size.height());
  size_.set_width(texture_size.width());

  cc::PaintFlags flags;
  SkColor color = hovered() ? color_scheme().close_button_background_hover
                            : color_scheme().close_button_background;
  color = pressed() ? color_scheme().close_button_background_down : color;
  flags.setColor(color);
  canvas->DrawCircle(gfx::PointF(size_.width() / 2, size_.height() / 2),
                     size_.width() / 2, flags);

  float icon_size = size_.height() * kIconScaleFactor;
  float icon_corner_offset = (size_.height() - icon_size) / 2;
  VectorIcon::DrawVectorIcon(
      canvas, vector_icons::kClose16Icon, icon_size,
      gfx::PointF(icon_corner_offset, icon_corner_offset),
      color_scheme().close_button_foreground);
}

gfx::Size CloseButtonTexture::GetPreferredTextureSize(int maximum_width) const {
  return gfx::Size(maximum_width, maximum_width);
}

gfx::SizeF CloseButtonTexture::GetDrawnSize() const {
  return size_;
}

bool CloseButtonTexture::HitTest(const gfx::PointF& point) const {
  return (point - gfx::PointF(0.5, 0.5)).LengthSquared() < 0.25;
}

}  // namespace vr
