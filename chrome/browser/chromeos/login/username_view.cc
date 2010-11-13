// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/username_view.h"

#include "base/logging.h"
#include "gfx/canvas.h"
#include "gfx/canvas_skia.h"
#include "gfx/rect.h"
#include "third_party/skia/include/core/SkComposeShader.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "views/background.h"

namespace {
// Username label background color.
const SkColor kLabelBackgoundColor = 0x55000000;
// Holds margin to height ratio.
const double kMarginRatio = 1.0 / 3.0;
}  // namespace

namespace chromeos {

UsernameView::UsernameView(const std::wstring& username)
    : views::Label(username) {
}

void UsernameView::Paint(gfx::Canvas* canvas) {
  gfx::Rect bounds = GetLocalBounds(false);
  if (!text_image_.get())
    PaintUsername(bounds);

  DCHECK(bounds.size() ==
         gfx::Size(text_image_->width(), text_image_->height()));

  // Only alpha channel counts.
  SkColor gradient_colors[2];
  gradient_colors[0] = 0xFFFFFFFF;
  gradient_colors[1] = 0x00FFFFFF;

  int gradient_start = std::min(margin_width_ + text_width_,
                                bounds.width() - bounds.height());

  SkPoint gradient_borders[2];
  gradient_borders[0].set(SkIntToScalar(gradient_start), SkIntToScalar(0));
  gradient_borders[1].set(SkIntToScalar(
      gradient_start + bounds.height()), SkIntToScalar(0));

  SkShader* gradient_shader =
      SkGradientShader::CreateLinear(gradient_borders, gradient_colors, NULL, 2,
                                     SkShader::kClamp_TileMode, NULL);
  SkShader* image_shader = SkShader::CreateBitmapShader(
      *text_image_,
      SkShader::kRepeat_TileMode,
      SkShader::kRepeat_TileMode);

  SkXfermode* mode = SkXfermode::Create(SkXfermode::kSrcIn_Mode);
  SkShader* composite_shader = new SkComposeShader(gradient_shader,
                                                   image_shader, mode);
  gradient_shader->unref();
  image_shader->unref();

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setFilterBitmap(true);
  paint.setShader(composite_shader)->unref();
  canvas->DrawRectInt(bounds.x(), bounds.y(),
                      bounds.width(), bounds.height(), paint);
}

void UsernameView::PaintUsername(const gfx::Rect& bounds) {
  margin_width_ = bounds.height() * kMarginRatio;
  gfx::CanvasSkia canvas(bounds.width(), bounds.height(), false);
  // Draw background.
  canvas.drawColor(kLabelBackgoundColor);
  // Calculate needed space.
  int flags = gfx::Canvas::TEXT_ALIGN_LEFT |
      gfx::Canvas::TEXT_VALIGN_MIDDLE |
      gfx::Canvas::NO_ELLIPSIS;
  int text_height;
  gfx::CanvasSkia::SizeStringInt(GetText(), font(),
                                 &text_width_, &text_height,
                                 flags);
  text_width_ = std::min(text_width_, bounds.width() - margin_width_);
  // Draw the text.
  canvas.DrawStringInt(GetText(), font(), GetColor(),
                        bounds.x() + margin_width_, bounds.y(),
                        bounds.width() - margin_width_, bounds.height(),
                        flags);

  text_image_.reset(new SkBitmap(canvas.ExtractBitmap()));
  text_image_->buildMipMap(false);
}

}  // namespace chromeos
