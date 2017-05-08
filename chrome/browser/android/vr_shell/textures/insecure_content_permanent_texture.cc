// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/insecure_content_permanent_texture.h"

#include "cc/paint/skia_paint_canvas.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/vector_icons/vector_icons.h"

namespace vr_shell {

namespace {

const SkColor kBackgroundColor = SK_ColorWHITE;
const SkColor kForegroundColor = 0xFF444444;
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
  int max_width = texture_size.width();
  cc::PaintFlags flags;
  flags.setColor(kBackgroundColor);

  int text_flags = gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_ELLIPSIS;
  auto text =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_INSECURE_WEBVR_CONTENT_PERMANENT);
  auto fonts = GetFontList(size_.height() * kFontSizeFactor, text);
  int text_height = kTextHeightFactor * size_.height();
  int text_width = kTextWidthFactor * size_.height();
  gfx::Canvas::SizeStringInt(text, fonts, &text_width, &text_height, 0,
                             text_flags);
  // Giving some extra width without reaching the texture limit.
  text_width =
      static_cast<int>(std::min(text_width + 2 * kBorderFactor * size_.height(),
                                kTextWidthFactor * size_.height()));
  size_.set_width((3 * kBorderFactor + kIconSizeFactor) * size_.height() +
                  text_width);
  DCHECK_LE(size_.width(), max_width);
  canvas->DrawRoundRect(gfx::Rect(size_.width(), size_.height()),
                        size_.height() * kBorderFactor, flags);

  canvas->Save();
  canvas->Translate(
      gfx::Vector2d(IsRTL() ? 2 * kBorderFactor * size_.height() + text_width
                            : size_.height() * kBorderFactor,
                    size_.height() * (1.0 - kIconSizeFactor) / 2.0));
  PaintVectorIcon(canvas, ui::kInfoOutlineIcon,
                  size_.height() * kIconSizeFactor, kForegroundColor);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(gfx::Vector2d(
      size_.height() *
          (IsRTL() ? kBorderFactor : 2 * kBorderFactor + kIconSizeFactor),
      size_.height() * kBorderFactor));
  canvas->DrawStringRectWithFlags(
      text, fonts, kForegroundColor,
      gfx::Rect(text_width, kTextHeightFactor * size_.height()), text_flags);
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

}  // namespace vr_shell
