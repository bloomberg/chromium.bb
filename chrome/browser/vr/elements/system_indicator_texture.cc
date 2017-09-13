// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/system_indicator_texture.h"

#include "base/strings/utf_string_conversions.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/render_text.h"

namespace vr {

namespace {

constexpr int kHeightWidthRatio = 8.0;
constexpr float kBorderFactor = 0.1;
constexpr float kIconSizeFactor = 0.7;
constexpr float kFontSizeFactor = 0.40;

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

  size_.set_height(texture_size.height());

  float border_pixels = size_.height() * kBorderFactor;
  float icon_pixels = size_.height() * kIconSizeFactor;
  size_.set_width(icon_pixels + 2 * border_pixels);

  SkPaint paint;
  paint.setColor(color_scheme().system_indicator_background);

  std::unique_ptr<gfx::RenderText> rendered_text;

  if (has_text_) {
    base::string16 text = l10n_util::GetStringUTF16(message_id_);

    gfx::FontList fonts;
    GetFontList(size_.height() * kFontSizeFactor, text, &fonts);
    gfx::Rect text_size(0, size_.height());
    std::vector<std::unique_ptr<gfx::RenderText>> lines;
    lines = PrepareDrawStringRect(
        text, fonts, color_scheme().system_indicator_foreground, &text_size,
        kTextAlignmentNone, kWrappingBehaviorNoWrap);
    DCHECK_EQ(lines.size(), 1u);
    rendered_text = std::move(lines.front());

    // Adust texture width according to text size.
    size_.set_width(size_.width() + text_size.width() + 2 * border_pixels);
  }

  sk_canvas->drawRoundRect(SkRect::MakeWH(size_.width(), size_.height()),
                           border_pixels, border_pixels, paint);

  gfx::PointF icon_location(
      (IsRTL() ? size_.width() - border_pixels - icon_pixels : border_pixels),
      (size_.height() - icon_pixels) / 2.0);
  DrawVectorIcon(canvas, icon_, size_.height() * kIconSizeFactor, icon_location,
                 color_scheme().system_indicator_foreground);

  if (rendered_text) {
    canvas->Save();
    canvas->Translate(gfx::Vector2d(
        (IsRTL() ? border_pixels : 3 * border_pixels + icon_pixels), 0));
    rendered_text->Draw(canvas);
    canvas->Restore();
  }
}

gfx::Size SystemIndicatorTexture::GetPreferredTextureSize(
    int maximum_width) const {
  // All indicators need to be the same height, so compute height, and then
  // re-compute with based on whether the indicator has text or not.
  int height = maximum_width / kHeightWidthRatio;
  return gfx::Size(has_text_ ? maximum_width : height, height);
}

gfx::SizeF SystemIndicatorTexture::GetDrawnSize() const {
  return size_;
}

}  // namespace vr
