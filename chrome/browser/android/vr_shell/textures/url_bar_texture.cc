// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/url_bar_texture.h"

#include "base/strings/utf_string_conversions.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/security_state/core/security_state.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/vector_icons/vector_icons.h"

namespace vr_shell {

namespace {

static constexpr SkColor kTextureBackground = 0x00AAAAAA;
static constexpr SkColor kBackground = 0xCCAAAAAA;
static constexpr SkColor kBackgroundHover = 0xCCDDDDDD;
static constexpr SkColor kForeground = 0xCC444444;
static constexpr SkColor kSeparatorColor =
    SkColorSetARGBMacro(256 * 0.2, 0, 0, 0);

static constexpr float kWidth = 0.672;
static constexpr float kHeight = 0.088;
static constexpr float kFontHeight = 0.027;
static constexpr float kBackButtonWidth = kHeight;
static constexpr float kBackIconHeight = 0.05;
static constexpr float kBackIconOffset = 0.005;
static constexpr float kSecurityFieldWidth = 0.06;
static constexpr float kSecurityIconHeight = 0.03;
static constexpr float kUrlRightMargin = 0.02;
static constexpr float kSeparatorWidth = 0.002;

using security_state::SecurityLevel;

const struct gfx::VectorIcon& getSecurityIcon(int level) {
  switch (level) {
    case SecurityLevel::NONE:
    case SecurityLevel::HTTP_SHOW_WARNING:
    case SecurityLevel::SECURITY_WARNING:
      return ui::kInfoOutlineIcon;
    case SecurityLevel::SECURE:
    case SecurityLevel::EV_SECURE:
      return ui::kLockIcon;
    case SecurityLevel::SECURE_WITH_POLICY_INSTALLED_CERT:
    case SecurityLevel::DANGEROUS:
    default:
      return ui::kWarningIcon;
  }
}

}  // namespace

UrlBarTexture::UrlBarTexture() : security_level_(SecurityLevel::DANGEROUS) {}

UrlBarTexture::~UrlBarTexture() = default;

void UrlBarTexture::SetURL(const GURL& gurl) {
  gurl_ = gurl;
}

void UrlBarTexture::SetSecurityLevel(int level) {
  security_level_ = level;
}

float UrlBarTexture::ToPixels(float meters) const {
  return meters * size_.width() / kWidth;
}

void UrlBarTexture::Draw(SkCanvas* canvas, const gfx::Size& texture_size) {
  size_.set_height(texture_size.height());
  size_.set_width(texture_size.width());

  canvas->save();
  canvas->scale(size_.width() / kWidth, size_.width() / kWidth);

  // Make a gfx canvas to support utility drawing methods.
  cc::SkiaPaintCanvas paint_canvas(canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);

  canvas->drawColor(kTextureBackground);

  // Back button area.
  SkRRect round_rect;
  SkVector rounded_corner = {kHeight / 2, kHeight / 2};
  SkVector left_corners[4] = {rounded_corner, {0, 0}, {0, 0}, rounded_corner};
  round_rect.setRectRadii({0, 0, kHeight, kHeight}, left_corners);
  SkPaint paint;
  paint.setColor((GetDrawFlags() & FLAG_HOVER) ? kBackgroundHover
                                               : kBackground);
  canvas->drawRRect(round_rect, paint);

  // URL area.
  paint.setColor(kBackground);
  SkVector right_corners[4] = {{0, 0}, rounded_corner, rounded_corner, {0, 0}};
  round_rect.setRectRadii({kHeight, 0, kWidth, kHeight}, right_corners);
  canvas->drawRRect(round_rect, paint);

  // Back button / URL separator vertical line.
  paint.setColor(kSeparatorColor);
  canvas->drawRect(SkRect::MakeXYWH(kHeight, 0, kSeparatorWidth, kHeight),
                   paint);

  // Back button icon.
  canvas->save();
  canvas->translate(kHeight / 2 + kBackIconOffset, kHeight / 2);
  canvas->translate(-kBackIconHeight / 2, -kBackIconHeight / 2);
  int icon_default_height = GetDefaultSizeOfVectorIcon(ui::kBackArrowIcon);
  float icon_scale = kBackIconHeight / icon_default_height;
  canvas->scale(icon_scale, icon_scale);
  PaintVectorIcon(&gfx_canvas, ui::kBackArrowIcon, kForeground);
  canvas->restore();

  // Site security state icon.
  // TODO(cjgrant): Plug in the correct icons based on security level.
  if (!gurl_.spec().empty()) {
    canvas->save();
    canvas->translate(
        kBackButtonWidth + kSeparatorWidth + kSecurityFieldWidth / 2,
        kHeight / 2);
    canvas->translate(-kSecurityIconHeight / 2, -kSecurityIconHeight / 2);
    const gfx::VectorIcon& security_icon = getSecurityIcon(security_level_);
    icon_default_height = GetDefaultSizeOfVectorIcon(security_icon);
    icon_scale = kSecurityIconHeight / icon_default_height;
    canvas->scale(icon_scale, icon_scale);
    PaintVectorIcon(&gfx_canvas, security_icon, kForeground);
    canvas->restore();
  }

  canvas->restore();

  // Draw text based on pixel sizes rather than meters, for correct font sizing.
  int pixel_font_height = texture_size.height() * kFontHeight / kHeight;
  int text_flags = gfx::Canvas::TEXT_ALIGN_LEFT;
  float url_x = kBackButtonWidth + kSeparatorWidth + kSecurityFieldWidth;
  float url_width = kWidth - url_x - kUrlRightMargin;
  gfx_canvas.DrawStringRectWithFlags(
      base::UTF8ToUTF16(gurl_.spec()), GetDefaultFontList(pixel_font_height),
      SK_ColorBLACK,
      gfx::Rect(ToPixels(url_x), 0, ToPixels(url_width), ToPixels(kHeight)),
      text_flags);
}

gfx::Size UrlBarTexture::GetPreferredTextureSize(int maximum_width) const {
  return gfx::Size(maximum_width, maximum_width * kHeight / kWidth);
}

gfx::SizeF UrlBarTexture::GetDrawnSize() const {
  return size_;
}

}  // namespace vr_shell
