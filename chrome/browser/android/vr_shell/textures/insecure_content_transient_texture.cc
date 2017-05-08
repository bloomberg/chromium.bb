// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/insecure_content_transient_texture.h"

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

const SkColor kBackgroundColor = 0xCC1A1A1A;
const SkColor kForegroundColor = SK_ColorWHITE;
constexpr float kBorderFactor = 0.045;
constexpr float kFontSizeFactor = 0.048;
constexpr float kTextWidthFactor = 1.0 - 3 * kBorderFactor;

}  // namespace

InsecureContentTransientTexture::InsecureContentTransientTexture() = default;

InsecureContentTransientTexture::~InsecureContentTransientTexture() = default;

void InsecureContentTransientTexture::Draw(SkCanvas* sk_canvas,
                                           const gfx::Size& texture_size) {
  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;

  size_.set_width(texture_size.width());
  int max_height = texture_size.height();
  cc::PaintFlags flags;
  flags.setColor(kBackgroundColor);

  int text_flags = gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::MULTI_LINE;
  auto text =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_INSECURE_WEBVR_CONTENT_TRANSIENT);
  auto fonts = GetFontList(size_.width() * kFontSizeFactor, text);
  int text_width = size_.width() * kTextWidthFactor;
  int text_height = 0;  // Will be increased during text measurement.
  gfx::Canvas::SizeStringInt(text, fonts, &text_width, &text_height, 0,
                             text_flags);

  // Making sure the drawing fits within the texture.
  text_height = std::min(
      text_height, static_cast<int>((1.0 - 2 * kBorderFactor) * size_.width()));
  size_.set_height(size_.width() * 2 * kBorderFactor + text_height);
  DCHECK_LE(size_.height(), max_height);
  canvas->DrawRoundRect(gfx::Rect(size_.width(), size_.height()),
                        size_.height() * kBorderFactor, flags);

  canvas->Save();
  canvas->Translate(gfx::Vector2d(size_.width() * kBorderFactor,
                                  size_.width() * kBorderFactor));
  canvas->DrawStringRectWithFlags(
      text, fonts, kForegroundColor,
      gfx::Rect(kTextWidthFactor * size_.width(), text_height), text_flags);
  canvas->Restore();
}

gfx::Size InsecureContentTransientTexture::GetPreferredTextureSize(
    int maximum_width) const {
  return gfx::Size(maximum_width, maximum_width);
}

gfx::SizeF InsecureContentTransientTexture::GetDrawnSize() const {
  return size_;
}

}  // namespace vr_shell
