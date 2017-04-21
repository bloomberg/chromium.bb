// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/insecure_content_permanent_texture.h"

#include "cc/paint/skia_paint_canvas.h"
#include "chrome/grit/generated_resources.h"
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
constexpr float kFontSizeFactor = 0.45;
constexpr float kTextHeightFactor = 1.0 - 2 * kBorderFactor;
constexpr float kTextWidthFactor = 4.0 - 3 * kBorderFactor - kIconSizeFactor;

}  // namespace

InsecureContentPermanentTexture::InsecureContentPermanentTexture(
    int texture_handle,
    int texture_size)
    : UITexture(texture_handle, texture_size) {
  // Ensuring height is a quarter of the width.
  DCHECK(texture_size_ % 4 == 0);
  height_ = texture_size_ / 4;
}

InsecureContentPermanentTexture::~InsecureContentPermanentTexture() = default;

void InsecureContentPermanentTexture::Draw(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setColor(kBackgroundColor);

  int text_flags = gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_ELLIPSIS;
  auto text =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_INSECURE_WEBVR_CONTENT_PERMANENT);
  auto fonts = GetFontList(height_ * kFontSizeFactor, text);
  int text_height = kTextHeightFactor * height_;
  int text_width = kTextWidthFactor * height_;
  gfx::Canvas::SizeStringInt(text, fonts, &text_width, &text_height, 0,
                             text_flags);
  // Giving some extra width without reaching the texture limit.
  text_width = static_cast<int>(std::min(
      text_width + 2 * kBorderFactor * height_, kTextWidthFactor * height_));
  width_ = static_cast<int>(
      ceil((3 * kBorderFactor + kIconSizeFactor) * height_ + text_width));

  canvas->DrawRoundRect(gfx::Rect(width_, height_), height_ * kBorderFactor,
                        flags);

  canvas->Save();
  canvas->Translate({IsRTL() ? 2 * kBorderFactor * height_ + text_width
                             : height_ * kBorderFactor,
                     height_ * (1.0 - kIconSizeFactor) / 2.0});
  PaintVectorIcon(canvas, ui::kInfoOutlineIcon, height_ * kIconSizeFactor,
                  kForegroundColor);
  canvas->Restore();

  canvas->Save();
  canvas->Translate({height_ * (IsRTL() ? kBorderFactor
                                        : 2 * kBorderFactor + kIconSizeFactor),
                     height_ * kBorderFactor});
  canvas->DrawStringRectWithFlags(
      text, fonts, kForegroundColor,
      gfx::Rect(text_width, kTextHeightFactor * height_), text_flags);
  canvas->Restore();
}

void InsecureContentPermanentTexture::SetSize() {
  size_.SetSize(width_, height_);
}

}  // namespace vr_shell
