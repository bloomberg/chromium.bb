// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/insecure_content_transient_texture.h"

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

const SkColor kBackgroundColor = 0xCC1A1A1A;
const SkColor kForegroundColor = SK_ColorWHITE;
constexpr float kBorderFactor = 0.045;
constexpr float kIconSizeFactor = 0.25;
constexpr float kFontSizeFactor = 0.045;
constexpr float kTextWidthFactor = 1.0 - 3 * kBorderFactor - kIconSizeFactor;

}  // namespace

InsecureContentTransientTexture::InsecureContentTransientTexture(
    int texture_handle,
    int texture_size)
    : UITexture(texture_handle, texture_size) {
}

InsecureContentTransientTexture::~InsecureContentTransientTexture() = default;

void InsecureContentTransientTexture::Draw(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setColor(kBackgroundColor);

  int text_flags = gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::MULTI_LINE;
  auto text =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_INSECURE_WEBVR_CONTENT_TRANSIENT);
  auto fonts = GetFontList(texture_size_ * kFontSizeFactor, text);

  int text_width = texture_size_ * kTextWidthFactor;
  int text_height = 0;  // Will be increased during text measurement.
  gfx::Canvas::SizeStringInt(text, fonts, &text_width, &text_height, 0,
                             text_flags);
  // Making sure that the icon fits in the box.
  text_height = std::max(
      text_height, static_cast<int>(ceil(texture_size_ * kIconSizeFactor)));
  // Making sure the drawing fits within the texture.
  text_height = std::min(
      text_height, static_cast<int>((1.0 - 2 * kBorderFactor) * texture_size_));
  height_ =
      static_cast<int>(ceil(texture_size_ * 2 * kBorderFactor + text_height));

  canvas->DrawRoundRect(gfx::Rect(texture_size_, height_),
                        height_ * kBorderFactor, flags);

  canvas->Save();
  canvas->Translate({IsRTL() ? 2 * kBorderFactor * texture_size_ + text_width
                             : texture_size_ * kBorderFactor,
                     (height_ - kIconSizeFactor * texture_size_) / 2.0});
  PaintVectorIcon(canvas, ui::kInfoOutlineIcon, texture_size_ * kIconSizeFactor,
                  kForegroundColor);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(
      {IsRTL() ? kBorderFactor * texture_size_
               : texture_size_ * (2 * kBorderFactor + kIconSizeFactor),
       texture_size_ * kBorderFactor});
  canvas->DrawStringRectWithFlags(
      text, fonts, kForegroundColor,
      gfx::Rect(kTextWidthFactor * texture_size_, text_height), text_flags);
  canvas->Restore();
}

void InsecureContentTransientTexture::SetSize() {
  size_.SetSize(texture_size_, height_);
}

}  // namespace vr_shell
