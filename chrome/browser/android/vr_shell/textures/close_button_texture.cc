// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/close_button_texture.h"

#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/android/vr_shell/color_scheme.h"
#include "chrome/browser/android/vr_shell/ui_elements/button.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/vector_icons/vector_icons.h"

namespace vr_shell {

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
  SkColor color = hovered() ? color_scheme().element_background_hover
                            : color_scheme().element_background;
  color = pressed() ? color_scheme().element_background_down : color;
  flags.setColor(color);
  canvas->DrawCircle(gfx::PointF(size_.width() / 2, size_.height() / 2),
                     size_.width() / 2, flags);

  canvas->Save();
  canvas->Translate(gfx::Vector2d(size_.height() * (1 - kIconScaleFactor) / 2,
                                  size_.height() * (1 - kIconScaleFactor) / 2));
  PaintVectorIcon(canvas, ui::kCloseIcon, size_.height() * kIconScaleFactor,
                  color_scheme().element_foreground);
  canvas->Restore();
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

}  // namespace vr_shell
