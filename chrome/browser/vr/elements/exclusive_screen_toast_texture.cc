// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/exclusive_screen_toast_texture.h"

#include "cc/paint/skia_paint_canvas.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/render_text.h"

namespace vr {

namespace {

constexpr float kHeight = 0.064f;
constexpr float kWidthHeightRatio = 16.0f;
constexpr float kFontHeight = 0.024f;
constexpr float kWidthPadding = 0.02f;
constexpr float kCornerRadius = 0.004f;

}  // namespace

ExclusiveScreenToastTexture::ExclusiveScreenToastTexture() = default;

ExclusiveScreenToastTexture::~ExclusiveScreenToastTexture() = default;

void ExclusiveScreenToastTexture::Draw(SkCanvas* sk_canvas,
                                       const gfx::Size& texture_size) {
  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;

  size_.set_height(texture_size.height());
  SkPaint paint;
  float meter_to_pixel_ratio = texture_size.height() / kHeight;

  paint.setColor(color_scheme().exclusive_screen_toast_background);
  auto text = l10n_util::GetStringUTF16(IDS_PRESS_APP_TO_EXIT);
  gfx::FontList fonts;
  int pixel_font_size = meter_to_pixel_ratio * kFontHeight;
  GetFontList(pixel_font_size, text, &fonts);
  gfx::Rect text_size(0, pixel_font_size);

  std::vector<std::unique_ptr<gfx::RenderText>> lines = PrepareDrawStringRect(
      text, fonts, color_scheme().exclusive_screen_toast_foreground, &text_size,
      kTextAlignmentNone, kWrappingBehaviorNoWrap);

  int pixel_padding = meter_to_pixel_ratio * kWidthPadding;
  size_.set_width(2 * pixel_padding + text_size.width());
  DCHECK_LE(size_.width(), texture_size.width());
  int corner_radius = meter_to_pixel_ratio * kCornerRadius;
  sk_canvas->drawRoundRect(SkRect::MakeWH(size_.width(), size_.height()),
                           corner_radius, corner_radius, paint);

  canvas->Save();
  canvas->Translate(
      gfx::Vector2d(meter_to_pixel_ratio * kWidthPadding,
                    (kHeight - kFontHeight) / 2 * meter_to_pixel_ratio));
  for (auto& render_text : lines)
    render_text->Draw(canvas);
  canvas->Restore();
}

gfx::Size ExclusiveScreenToastTexture::GetPreferredTextureSize(
    int maximum_width) const {
  return gfx::Size(maximum_width, maximum_width / kWidthHeightRatio);
}

gfx::SizeF ExclusiveScreenToastTexture::GetDrawnSize() const {
  return size_;
}

}  // namespace vr
