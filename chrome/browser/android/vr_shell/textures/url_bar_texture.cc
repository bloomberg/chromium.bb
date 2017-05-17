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
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/vector_icons/vector_icons.h"

namespace vr_shell {

namespace {

// TODO(mthiesse): These values are all wrong. The UX spec is unclear.
static constexpr SkColor kBackground = 0x66D6D6D6;
static constexpr SkColor kBackgroundHover = 0x6EF0F0F0;
static constexpr SkColor kBackgroundDown = 0x76F6F6F6;
static constexpr SkColor kForeground = 0xFF333333;
static constexpr SkColor kSeparatorColor = 0x33000000;

static constexpr SkColor kInfoOutlineIconColor = 0xFF5A5A5A;
static constexpr SkColor kLockIconColor = 0xFF0B8043;
static constexpr SkColor kWarningIconColor = 0xFFC73821;

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

SkColor getSecurityIconColor(int level) {
  switch (level) {
    case SecurityLevel::NONE:
    case SecurityLevel::HTTP_SHOW_WARNING:
    case SecurityLevel::SECURITY_WARNING:
      return kInfoOutlineIconColor;
    case SecurityLevel::SECURE:
    case SecurityLevel::EV_SECURE:
      return kLockIconColor;
    case SecurityLevel::SECURE_WITH_POLICY_INSTALLED_CERT:
    case SecurityLevel::DANGEROUS:
    default:
      return kWarningIconColor;
  }
}

gfx::PointF percentToMeters(const gfx::PointF& percent) {
  return gfx::PointF(percent.x() * kWidth, percent.y() * kHeight);
}

}  // namespace

UrlBarTexture::UrlBarTexture() : security_level_(SecurityLevel::DANGEROUS) {}

UrlBarTexture::~UrlBarTexture() = default;

void UrlBarTexture::SetURL(const GURL& gurl) {
  if (gurl_ != gurl)
    set_dirty();
  gurl_ = gurl;
}

void UrlBarTexture::SetSecurityLevel(int level) {
  if (&getSecurityIcon(security_level_) != &getSecurityIcon(level))
    set_dirty();
  security_level_ = level;
}

float UrlBarTexture::ToPixels(float meters) const {
  return meters * size_.width() / kWidth;
}

bool UrlBarTexture::HitsBackButton(const gfx::PointF& position) const {
  const gfx::PointF& meters = percentToMeters(position);
  gfx::RectF rect(gfx::PointF(0, 0), gfx::SizeF(kBackButtonWidth, kHeight));
  return rect.Contains(meters) && !HitsTransparentRegion(meters, true);
}

bool UrlBarTexture::HitsUrlBar(const gfx::PointF& position) const {
  const gfx::PointF& meters = percentToMeters(position);
  gfx::RectF rect(gfx::PointF(kBackButtonWidth, 0),
                  gfx::SizeF(kWidth - kBackButtonWidth, kHeight));
  return rect.Contains(meters) && !HitsTransparentRegion(meters, false);
}

bool UrlBarTexture::HitsTransparentRegion(const gfx::PointF& meters,
                                          bool left) const {
  const float radius = kHeight / 2.0f;
  gfx::PointF circle_center(left ? radius : kWidth - radius, radius);
  if (!left && meters.x() < circle_center.x())
    return false;
  if (left && meters.x() > circle_center.x())
    return false;
  return (meters - circle_center).LengthSquared() > radius * radius;
}

void UrlBarTexture::SetHovered(bool hovered) {
  if (hovered_ != hovered)
    set_dirty();
  hovered_ = hovered;
}

void UrlBarTexture::SetPressed(bool pressed) {
  if (pressed_ != pressed)
    set_dirty();
  pressed_ = pressed;
}

void UrlBarTexture::Draw(SkCanvas* canvas, const gfx::Size& texture_size) {
  size_.set_height(texture_size.height());
  size_.set_width(texture_size.width());

  canvas->save();
  canvas->scale(size_.width() / kWidth, size_.width() / kWidth);

  // Make a gfx canvas to support utility drawing methods.
  cc::SkiaPaintCanvas paint_canvas(canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);

  // Back button area.
  SkRRect round_rect;
  SkVector rounded_corner = {kHeight / 2, kHeight / 2};
  SkVector left_corners[4] = {rounded_corner, {0, 0}, {0, 0}, rounded_corner};
  round_rect.setRectRadii({0, 0, kHeight, kHeight}, left_corners);
  SkColor color = hovered_ ? kBackgroundHover : kBackground;
  color = pressed_ ? kBackgroundDown : color;
  SkPaint paint;
  paint.setColor(color);
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
  if (!gurl_.is_empty()) {
    canvas->save();
    canvas->translate(
        kBackButtonWidth + kSeparatorWidth + kSecurityFieldWidth / 2,
        kHeight / 2);
    canvas->translate(-kSecurityIconHeight / 2, -kSecurityIconHeight / 2);
    const gfx::VectorIcon& icon = getSecurityIcon(security_level_);
    icon_default_height = GetDefaultSizeOfVectorIcon(icon);
    icon_scale = kSecurityIconHeight / icon_default_height;
    SkColor icon_color = getSecurityIconColor(security_level_);
    canvas->scale(icon_scale, icon_scale);
    PaintVectorIcon(&gfx_canvas, icon, icon_color);
    canvas->restore();
  }

  canvas->restore();

  if (!gurl_.is_empty()) {
    if (last_drawn_gurl_ != gurl_) {
      // Draw text based on pixel sizes rather than meters, for correct font
      // sizing.
      int pixel_font_height = texture_size.height() * kFontHeight / kHeight;
      float url_x = kBackButtonWidth + kSeparatorWidth + kSecurityFieldWidth;
      float url_width = kWidth - url_x - kUrlRightMargin;
      gfx::Rect text_bounds(ToPixels(url_x), 0, ToPixels(url_width),
                            ToPixels(kHeight));
      gurl_render_texts_ = PrepareDrawStringRect(
          base::UTF8ToUTF16(gurl_.spec()),
          GetDefaultFontList(pixel_font_height), SK_ColorBLACK, &text_bounds,
          kTextAlignmentLeft, kWrappingBehaviorNoWrap);
      last_drawn_gurl_ = gurl_;
    }
    for (auto& render_text : gurl_render_texts_)
      render_text->Draw(&gfx_canvas);
  }
}

gfx::Size UrlBarTexture::GetPreferredTextureSize(int maximum_width) const {
  return gfx::Size(maximum_width, maximum_width * kHeight / kWidth);
}

gfx::SizeF UrlBarTexture::GetDrawnSize() const {
  return size_;
}

}  // namespace vr_shell
