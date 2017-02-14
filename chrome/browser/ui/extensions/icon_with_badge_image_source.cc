// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/icon_with_badge_image_source.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

const int kPadding = 2;
const int kBadgeHeight = 11;
const int kMaxTextWidth = 23;

// The minimum width for center-aligning the badge.
const int kCenterAlignThreshold = 20;

gfx::ImageSkiaRep ScaleImageSkiaRep(const gfx::ImageSkiaRep& rep,
                                    int target_width_dp,
                                    float target_scale) {
  int width_px = target_width_dp * target_scale;
  return gfx::ImageSkiaRep(
      skia::ImageOperations::Resize(rep.sk_bitmap(),
                                    skia::ImageOperations::RESIZE_BEST,
                                    width_px, width_px),
      target_scale);
}

}  // namespace

IconWithBadgeImageSource::Badge::Badge(const std::string& text,
                                       SkColor text_color,
                                       SkColor background_color)
    : text(text), text_color(text_color), background_color(background_color) {}

IconWithBadgeImageSource::Badge::~Badge() {}

IconWithBadgeImageSource::IconWithBadgeImageSource(const gfx::Size& size)
    : gfx::CanvasImageSource(size, false),
      grayscale_(false),
      paint_page_action_decoration_(false),
      paint_blocked_actions_decoration_(false) {}

IconWithBadgeImageSource::~IconWithBadgeImageSource() {}

void IconWithBadgeImageSource::SetIcon(const gfx::Image& icon) {
  icon_ = icon;
}

void IconWithBadgeImageSource::SetBadge(std::unique_ptr<Badge> badge) {
  badge_ = std::move(badge);
}

void IconWithBadgeImageSource::Draw(gfx::Canvas* canvas) {
  if (icon_.IsEmpty())
    return;

  gfx::ImageSkia skia = icon_.AsImageSkia();
  gfx::ImageSkiaRep rep = skia.GetRepresentation(canvas->image_scale());
  if (rep.scale() != canvas->image_scale()) {
    skia.AddRepresentation(ScaleImageSkiaRep(
        rep, ExtensionAction::ActionIconSize(), canvas->image_scale()));
  }
  if (grayscale_)
    skia = gfx::ImageSkiaOperations::CreateHSLShiftedImage(skia, {-1, 0, 0.75});

  int x_offset =
      std::floor((size().width() - ExtensionAction::ActionIconSize()) / 2.0);
  int y_offset =
      std::floor((size().height() - ExtensionAction::ActionIconSize()) / 2.0);
  canvas->DrawImageInt(skia, x_offset, y_offset);

  // Draw a badge on the provided browser action icon's canvas.
  PaintBadge(canvas);

  if (paint_page_action_decoration_)
    PaintPageActionDecoration(canvas);

  if (paint_blocked_actions_decoration_)
    PaintBlockedActionDecoration(canvas);
}

// Paints badge with specified parameters to |canvas|.
void IconWithBadgeImageSource::PaintBadge(gfx::Canvas* canvas) {
  if (!badge_ || badge_->text.empty())
    return;

  SkColor text_color = SkColorGetA(badge_->text_color) == SK_AlphaTRANSPARENT
                           ? SK_ColorWHITE
                           : badge_->text_color;

  // Make sure the background color is opaque. See http://crbug.com/619499
  SkColor background_color =
      SkColorGetA(badge_->background_color) == SK_AlphaTRANSPARENT
          ? gfx::kGoogleBlue500
          : SkColorSetA(badge_->background_color, SK_AlphaOPAQUE);

  ResourceBundle* rb = &ResourceBundle::GetSharedInstance();
  gfx::FontList base_font = rb->GetFontList(ResourceBundle::BaseFont)
                                .DeriveWithHeightUpperBound(kBadgeHeight);
  base::string16 utf16_text = base::UTF8ToUTF16(badge_->text);

  // See if we can squeeze a slightly larger font into the badge given the
  // actual string that is to be displayed.
  const int kMaxIncrementAttempts = 5;
  for (size_t i = 0; i < kMaxIncrementAttempts; ++i) {
    int w = 0;
    int h = 0;
    gfx::FontList bigger_font =
        base_font.Derive(1, 0, gfx::Font::Weight::NORMAL);
    gfx::Canvas::SizeStringInt(utf16_text, bigger_font, &w, &h, 0,
                               gfx::Canvas::NO_ELLIPSIS);
    if (h > kBadgeHeight)
      break;
    base_font = bigger_font;
  }

  const int text_width =
        std::min(kMaxTextWidth, canvas->GetStringWidth(utf16_text, base_font));
  // Calculate badge size. It is clamped to a min width just because it looks
  // silly if it is too skinny.
  int badge_width = text_width + kPadding * 2;
  // Force the pixel width of badge to be either odd (if the icon width is odd)
  // or even otherwise. If there is a mismatch you get http://crbug.com/26400.
  if (size().width() != 0 && (badge_width % 2 != size().width() % 2))
    badge_width += 1;
  badge_width = std::max(kBadgeHeight, badge_width);

  // Calculate the badge background rect. It is usually right-aligned, but it
  // can also be center-aligned if it is large.
  gfx::Rect rect(badge_width >= kCenterAlignThreshold
                     ? (size().width() - badge_width) / 2
                     : size().width() - badge_width,
                 size().height() - kBadgeHeight, badge_width, kBadgeHeight);
  cc::PaintFlags rect_flags;
  rect_flags.setStyle(cc::PaintFlags::kFill_Style);
  rect_flags.setAntiAlias(true);
  rect_flags.setColor(background_color);

  // Clear part of the background icon.
  gfx::Rect cutout_rect(rect);
  cutout_rect.Inset(-1, -1);
  cc::PaintFlags cutout_flags = rect_flags;
  cutout_flags.setBlendMode(SkBlendMode::kClear);
  canvas->DrawRoundRect(cutout_rect, 2, cutout_flags);

  // Paint the backdrop.
  canvas->DrawRoundRect(rect, 1, rect_flags);

  // Paint the text.
  rect.Inset(std::max(kPadding, (rect.width() - text_width) / 2),
             kBadgeHeight - base_font.GetHeight(), kPadding, 0);
  canvas->DrawStringRect(utf16_text, base_font, text_color, rect);
}

void IconWithBadgeImageSource::PaintPageActionDecoration(gfx::Canvas* canvas) {
  static const SkColor decoration_color = SkColorSetARGB(255, 70, 142, 226);

  int major_radius = std::ceil(size().width() / 5.0);
  int minor_radius = std::ceil(major_radius / 2.0);
  gfx::Point center_point(major_radius + 1, size().height() - (major_radius)-1);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(SK_ColorTRANSPARENT);
  flags.setBlendMode(SkBlendMode::kSrc);
  canvas->DrawCircle(center_point, major_radius, flags);
  flags.setColor(decoration_color);
  canvas->DrawCircle(center_point, minor_radius, flags);
}

void IconWithBadgeImageSource::PaintBlockedActionDecoration(
    gfx::Canvas* canvas) {
  canvas->Save();
  gfx::ImageSkia img = *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_BLOCKED_EXTENSION_SCRIPT);
  canvas->DrawImageInt(img, size().width() - img.width(), 0);
  canvas->Restore();
}
