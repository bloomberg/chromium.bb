// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/insecure_content_permanent_texture.h"

#include "cc/paint/skia_paint_canvas.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

namespace {

constexpr float kBorderFactor = 0.1;
constexpr float kIconSizeFactor = 0.7;
constexpr float kFontSizeFactor = 0.40;
constexpr float kTextHeightFactor = 1.0 - 2 * kBorderFactor;
constexpr float kTextWidthFactor = 4.0 - 3 * kBorderFactor - kIconSizeFactor;

}  // namespace

InsecureContentPermanentTexture::InsecureContentPermanentTexture() = default;

InsecureContentPermanentTexture::~InsecureContentPermanentTexture() = default;

void InsecureContentPermanentTexture::Draw(SkCanvas* sk_canvas,
                                           const gfx::Size& texture_size) {
  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;

  DCHECK(texture_size.height() * 4 == texture_size.width());
  size_.set_height(texture_size.height());
  SkPaint paint;
  paint.setColor(color_scheme().permanent_warning_background);
  auto text =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_INSECURE_WEBVR_CONTENT_PERMANENT);
  gfx::FontList fonts;
  GetFontList(size_.height() * kFontSizeFactor, text, &fonts);
  gfx::Rect text_size(0, kTextHeightFactor * size_.height());

  std::vector<std::unique_ptr<gfx::RenderText>> lines = PrepareDrawStringRect(
      text, fonts, color_scheme().permanent_warning_foreground, &text_size,
      kTextAlignmentNone, kWrappingBehaviorNoWrap);

  DCHECK_LE(text_size.width(), kTextWidthFactor * size_.height());
  // Setting background size giving some extra lateral padding to the text.
  size_.set_width((5 * kBorderFactor + kIconSizeFactor) * size_.height() +
                  text_size.width());
  float radius = size_.height() * kBorderFactor;
  sk_canvas->drawRoundRect(SkRect::MakeWH(size_.width(), size_.height()),
                           radius, radius, paint);

  canvas->Save();
  canvas->Translate(gfx::Vector2d(
      IsRTL() ? 4 * kBorderFactor * size_.height() + text_size.width()
              : size_.height() * kBorderFactor,
      size_.height() * (1.0 - kIconSizeFactor) / 2.0));
  PaintVectorIcon(canvas, vector_icons::kInfoOutlineIcon,
                  size_.height() * kIconSizeFactor,
                  color_scheme().permanent_warning_foreground);
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

gfx::Size InsecureContentPermanentTexture::GetPreferredTextureSize(
    int maximum_width) const {
  // Ensuring height is a quarter of the width.
  int height = maximum_width / 4;
  return gfx::Size(height * 4, height);
}

gfx::SizeF InsecureContentPermanentTexture::GetDrawnSize() const {
  return size_;
}

}  // namespace vr
