// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/system_indicator_texture.h"

#include "base/strings/utf_string_conversions.h"
#include "cc/paint/skia_paint_canvas.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr_shell {

namespace {

constexpr int kHeightWidthRatio = 8.0;
constexpr float kBorderFactor = 0.1;
constexpr float kIconSizeFactor = 0.7;
constexpr float kFontSizeFactor = 0.40;
constexpr float kTextHeightFactor = 1.0 - 2 * kBorderFactor;
constexpr float kTextWidthFactor =
    kHeightWidthRatio - 3 * kBorderFactor - kIconSizeFactor;

}  // namespace

SystemIndicatorTexture::SystemIndicatorTexture(const gfx::VectorIcon& icon,
                                               int message_id)
    : icon_(icon), message_id_(message_id), has_text_(true) {}

SystemIndicatorTexture::SystemIndicatorTexture(const gfx::VectorIcon& icon)
    : icon_(icon), has_text_(false) {}

SystemIndicatorTexture::~SystemIndicatorTexture() = default;

void SystemIndicatorTexture::Draw(SkCanvas* sk_canvas,
                                  const gfx::Size& texture_size) {
  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;

  DCHECK(texture_size.height() * kHeightWidthRatio == texture_size.width());
  size_.set_height(texture_size.height());
  SkPaint paint;
  paint.setColor(color_scheme().system_indicator_background);

  gfx::Rect text_size(0, 0);
  std::vector<std::unique_ptr<gfx::RenderText>> lines;

  if (has_text_) {
    base::string16 text = l10n_util::GetStringUTF16(message_id_);

    gfx::FontList fonts;
    GetFontList(size_.height() * kFontSizeFactor, text, &fonts);
    text_size.set_height(kTextHeightFactor * size_.height());

    lines = PrepareDrawStringRect(
        text, fonts, color_scheme().system_indicator_foreground, &text_size,
        kTextAlignmentNone, kWrappingBehaviorNoWrap);

    DCHECK_LE(text_size.width(), kTextWidthFactor * size_.height());

    // Setting background size giving some extra lateral padding to the text.
    size_.set_width((kHeightWidthRatio * kBorderFactor + kIconSizeFactor) *
                        size_.height() +
                    text_size.width());
  } else {
    size_.set_width((2 * kBorderFactor + kIconSizeFactor) * size_.height() +
                    text_size.width());
  }

  float radius = size_.height() * kBorderFactor;
  sk_canvas->drawRoundRect(SkRect::MakeWH(size_.width(), size_.height()),
                           radius, radius, paint);

  canvas->Save();
  canvas->Translate(gfx::Vector2d(
      IsRTL() ? 4 * kBorderFactor * size_.height() + text_size.width()
              : size_.height() * kBorderFactor,
      size_.height() * (1.0 - kIconSizeFactor) / 2.0));
  PaintVectorIcon(canvas, icon_, size_.height() * kIconSizeFactor,
                  color_scheme().system_indicator_foreground);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(gfx::Vector2d(
      size_.height() *
          (IsRTL() ? 2 * kBorderFactor : 3 * kBorderFactor + kIconSizeFactor),
      size_.height() * kBorderFactor));
  for (auto& render_text : lines)
    render_text->Draw(canvas);
  canvas->Restore();
}

gfx::Size SystemIndicatorTexture::GetPreferredTextureSize(
    int maximum_width) const {
  // Ensuring height is a quarter of the width.
  int height = maximum_width / kHeightWidthRatio;
  return gfx::Size(height * kHeightWidthRatio, height);
}

gfx::SizeF SystemIndicatorTexture::GetDrawnSize() const {
  return size_;
}

}  // namespace vr_shell
