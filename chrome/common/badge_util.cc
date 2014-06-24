// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/badge_util.h"

#include <cmath>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace {

// Different platforms need slightly different constants to look good.
#if defined(OS_WIN)
const float kTextSize = 10;
const int kBottomMarginBrowserAction = 0;
const int kBottomMarginPageAction = 2;
const int kPadding = 2;
// The padding between the top of the badge and the top of the text.
const int kTopTextPadding = -1;
#elif defined(OS_MACOSX)
const float kTextSize = 9.0;
const int kBottomMarginBrowserAction = 5;
const int kBottomMarginPageAction = 2;
const int kPadding = 2;
const int kTopTextPadding = 0;
#elif defined(OS_CHROMEOS)
const float kTextSize = 8.0;
const int kBottomMarginBrowserAction = 0;
const int kBottomMarginPageAction = 2;
const int kPadding = 2;
const int kTopTextPadding = 1;
#elif defined(OS_POSIX)
const float kTextSize = 9.0;
const int kBottomMarginBrowserAction = 0;
const int kBottomMarginPageAction = 2;
const int kPadding = 2;
const int kTopTextPadding = 0;
#endif

const int kBadgeHeight = 11;
const int kMaxTextWidth = 23;

// The minimum width for center-aligning the badge.
const int kCenterAlignThreshold = 20;

}  // namespace

namespace badge_util {

SkPaint* GetBadgeTextPaintSingleton() {
#if defined(OS_MACOSX)
  const char kPreferredTypeface[] = "Helvetica Bold";
#else
  const char kPreferredTypeface[] = "Arial";
#endif

  static SkPaint* text_paint = NULL;
  if (!text_paint) {
    text_paint = new SkPaint;
    text_paint->setAntiAlias(true);
    text_paint->setTextAlign(SkPaint::kLeft_Align);

    skia::RefPtr<SkTypeface> typeface = skia::AdoptRef(
        SkTypeface::CreateFromName(kPreferredTypeface, SkTypeface::kBold));
    // Skia doesn't do any font fallback---if the user is missing the font then
    // typeface will be NULL. If we don't do manual fallback then we'll crash.
    if (typeface) {
      text_paint->setFakeBoldText(true);
    } else {
      // Fall back to the system font. We don't bold it because we aren't sure
      // how it will look.
      // For the most part this code path will only be hit on Linux systems
      // that don't have Arial.
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      const gfx::Font& base_font = rb.GetFont(ResourceBundle::BaseFont);
      typeface = skia::AdoptRef(SkTypeface::CreateFromName(
          base_font.GetFontName().c_str(), SkTypeface::kNormal));
      DCHECK(typeface);
    }

    text_paint->setTypeface(typeface.get());
    // |text_paint| adds its own ref. Release the ref from CreateFontName.
  }
  return text_paint;
}

void PaintBadge(gfx::Canvas* canvas,
                const gfx::Rect& bounds,
                const std::string& text,
                const SkColor& text_color_in,
                const SkColor& background_color_in,
                int icon_width,
                extensions::ActionInfo::Type action_type) {
  if (text.empty())
    return;

  SkColor text_color = text_color_in;
  if (SkColorGetA(text_color_in) == 0x00)
    text_color = SK_ColorWHITE;

  SkColor background_color = background_color_in;
  if (SkColorGetA(background_color_in) == 0x00)
      background_color = SkColorSetARGB(255, 218, 0, 24);

  canvas->Save();

  SkPaint* text_paint = badge_util::GetBadgeTextPaintSingleton();
  text_paint->setColor(text_color);
  float scale = canvas->image_scale();

  // Calculate text width. Font width may not be linear with respect to the
  // scale factor (e.g. when hinting is applied), so we need to use the font
  // size that canvas actually uses when drawing a text.
  text_paint->setTextSize(SkFloatToScalar(kTextSize) * scale);
  SkScalar sk_text_width_in_pixel =
      text_paint->measureText(text.c_str(), text.size());
  text_paint->setTextSize(SkFloatToScalar(kTextSize));

  // We clamp the width to a max size. SkPaint::measureText returns the width in
  // pixel (as a result of scale multiplier), so convert sk_text_width_in_pixel
  // back to DIP (density independent pixel) first.
  int text_width =
      std::min(kMaxTextWidth,
               static_cast<int>(
                   std::ceil(SkScalarToFloat(sk_text_width_in_pixel) / scale)));

  // Calculate badge size. It is clamped to a min width just because it looks
  // silly if it is too skinny.
  int badge_width = text_width + kPadding * 2;
  // Force the pixel width of badge to be either odd (if the icon width is odd)
  // or even otherwise. If there is a mismatch you get http://crbug.com/26400.
  if (icon_width != 0 && (badge_width % 2 != icon_width % 2))
    badge_width += 1;
  badge_width = std::max(kBadgeHeight, badge_width);

  // Paint the badge background color in the right location. It is usually
  // right-aligned, but it can also be center-aligned if it is large.
  int rect_height = kBadgeHeight;
  int bottom_margin =
      action_type == extensions::ActionInfo::TYPE_BROWSER ?
      kBottomMarginBrowserAction : kBottomMarginPageAction;
  int rect_y = bounds.bottom() - bottom_margin - kBadgeHeight;
  int rect_width = badge_width;
  int rect_x = (badge_width >= kCenterAlignThreshold) ?
      bounds.x() + (bounds.width() - badge_width) / 2 :
      bounds.right() - badge_width;
  gfx::Rect rect(rect_x, rect_y, rect_width, rect_height);

  SkPaint rect_paint;
  rect_paint.setStyle(SkPaint::kFill_Style);
  rect_paint.setAntiAlias(true);
  rect_paint.setColor(background_color);
  canvas->DrawRoundRect(rect, 2, rect_paint);

  // Overlay the gradient. It is stretchy, so we do this in three parts.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* gradient_left = rb.GetImageSkiaNamed(
      IDR_BROWSER_ACTION_BADGE_LEFT);
  gfx::ImageSkia* gradient_right = rb.GetImageSkiaNamed(
      IDR_BROWSER_ACTION_BADGE_RIGHT);
  gfx::ImageSkia* gradient_center = rb.GetImageSkiaNamed(
      IDR_BROWSER_ACTION_BADGE_CENTER);

  canvas->DrawImageInt(*gradient_left, rect.x(), rect.y());
  canvas->TileImageInt(*gradient_center,
      rect.x() + gradient_left->width(),
      rect.y(),
      rect.width() - gradient_left->width() - gradient_right->width(),
      rect.height());
  canvas->DrawImageInt(*gradient_right,
      rect.right() - gradient_right->width(), rect.y());

  // Finally, draw the text centered within the badge. We set a clip in case the
  // text was too large.
  rect.Inset(kPadding, 0);
  canvas->ClipRect(rect);
  canvas->sk_canvas()->drawText(
      text.c_str(), text.size(),
      SkFloatToScalar(rect.x() +
                      static_cast<float>(rect.width() - text_width) / 2),
      SkFloatToScalar(rect.y() + kTextSize + kTopTextPadding),
      *text_paint);
  canvas->Restore();
}

}  // namespace badge_util
