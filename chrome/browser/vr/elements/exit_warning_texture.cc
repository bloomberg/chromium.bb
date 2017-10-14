// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/exit_warning_texture.h"

#include "cc/paint/skia_paint_canvas.h"
#include "chrome/grit/generated_resources.h"
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

constexpr float kBorderFactor = 0.045f;
constexpr float kFontSizeFactor = 0.048f;
constexpr float kTextWidthFactor = 1.0f - 3 * kBorderFactor;

}  // namespace

ExitWarningTexture::ExitWarningTexture() = default;

ExitWarningTexture::~ExitWarningTexture() = default;

void ExitWarningTexture::Draw(SkCanvas* sk_canvas,
                              const gfx::Size& texture_size) {
  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;

  size_.set_width(texture_size.width());
  SkPaint paint;

  paint.setColor(color_scheme().exit_warning_background);
  auto text =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_VR_BROWSER_UNSUPPORTED_MODE);
  gfx::FontList fonts;
  GetFontList(size_.width() * kFontSizeFactor, text, &fonts);
  gfx::Rect text_size(size_.width() * kTextWidthFactor, 0);

  std::vector<std::unique_ptr<gfx::RenderText>> lines = PrepareDrawStringRect(
      text, fonts, color_scheme().exit_warning_foreground, &text_size,
      kTextAlignmentCenter, kWrappingBehaviorWrap);

  DCHECK_LE(text_size.height(),
            static_cast<int>((1.0 - 2 * kBorderFactor) * size_.width()));
  size_.set_height(size_.width() * 2 * kBorderFactor + text_size.height());
  float radius = size_.height() * kBorderFactor;
  sk_canvas->drawRoundRect(SkRect::MakeWH(size_.width(), size_.height()),
                           radius, radius, paint);

  canvas->Save();
  canvas->Translate(gfx::Vector2d(size_.width() * kBorderFactor,
                                  size_.width() * kBorderFactor));
  for (auto& render_text : lines)
    render_text->Draw(canvas);
  canvas->Restore();
}

gfx::Size ExitWarningTexture::GetPreferredTextureSize(int maximum_width) const {
  return gfx::Size(maximum_width, maximum_width);
}

gfx::SizeF ExitWarningTexture::GetDrawnSize() const {
  return size_;
}

}  // namespace vr
