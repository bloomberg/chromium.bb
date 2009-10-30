// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_action.h"

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/resource_bundle.h"
#include "base/gfx/rect.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "grit/app_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

namespace {

// Different platforms need slightly different constants to look good.
#if defined(OS_LINUX)
const int kTextSize = 9;
const int kBottomMargin = 0;
const int kPadding = 2;
const int kTopTextPadding = 0;
const int kBadgeHeight = 11;
const int kMaxTextWidth = 23;
// The minimum width for center-aligning the badge.
const int kCenterAlignThreshold = 20;
#else
const int kTextSize = 8;
const int kBottomMargin = 5;
const int kPadding = 2;
// The padding between the top of the badge and the top of the text.
const int kTopTextPadding = 1;
const int kBadgeHeight = 11;
const int kMaxTextWidth = 23;
// The minimum width for center-aligning the badge.
const int kCenterAlignThreshold = 20;
#endif

#if defined(OS_MACOSX)
const char kPreferredTypeface[] = "Helvetica";
#else
const char kPreferredTypeface[] = "Arial";
#endif

SkPaint* GetTextPaint() {
  static SkPaint* text_paint = NULL;
  if (!text_paint) {
    text_paint = new SkPaint;
    text_paint->setAntiAlias(true);

    text_paint->setTextAlign(SkPaint::kLeft_Align);
    text_paint->setTextSize(SkIntToScalar(kTextSize));

    SkTypeface* typeface = SkTypeface::CreateFromName(
        kPreferredTypeface, SkTypeface::kBold);
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
      typeface = SkTypeface::CreateFromName(
          WideToUTF8(base_font.FontName()).c_str(), SkTypeface::kNormal);
    }

    text_paint->setTypeface(typeface);
    // |text_paint| adds its own ref. Release the ref from CreateFontName.
    typeface->unref();
  }
  return text_paint;
}

}  // namespace

const int ExtensionAction::kDefaultTabId = -1;

void ExtensionAction::ClearAllValuesForTab(int tab_id) {
  title_.erase(tab_id);
  icon_.erase(tab_id);
  icon_index_.erase(tab_id);
  badge_text_.erase(tab_id);
  badge_text_color_.erase(tab_id);
  badge_background_color_.erase(tab_id);
  visible_.erase(tab_id);
}

void ExtensionAction::PaintBadge(gfx::Canvas* canvas,
                                 const gfx::Rect& bounds,
                                 int tab_id) {
  std::string text = GetBadgeText(tab_id);
  if (text.empty())
    return;

  SkColor text_color = GetBadgeTextColor(tab_id);
  SkColor background_color = GetBadgeBackgroundColor(tab_id);

  if (SkColorGetA(text_color) == 0x00)
    text_color = SK_ColorWHITE;

  if (SkColorGetA(background_color) == 0x00)
    background_color = SkColorSetARGB(255, 218, 0, 24);  // default badge color

  canvas->save();

  SkPaint* text_paint = GetTextPaint();
  text_paint->setColor(text_color);

  // Calculate text width. We clamp it to a max size.
  SkScalar text_width = text_paint->measureText(text.c_str(), text.size());
  text_width = SkIntToScalar(
      std::min(kMaxTextWidth, SkScalarFloor(text_width)));

  // Cacluate badge size. It is clamped to a min width just because it looks
  // silly if it is too skinny.
  int badge_width = SkScalarFloor(text_width) + kPadding * 2;
  badge_width = std::max(kBadgeHeight, badge_width);

  // Paint the badge background color in the right location. It is usually
  // right-aligned, but it can also be center-aligned if it is large.
  SkRect rect;
  rect.fBottom = SkIntToScalar(bounds.bottom() - kBottomMargin);
  rect.fTop = rect.fBottom - SkIntToScalar(kBadgeHeight);
  if (badge_width >= kCenterAlignThreshold) {
    rect.fLeft = SkIntToScalar(bounds.CenterPoint().x() - badge_width / 2);
    rect.fRight = rect.fLeft + SkIntToScalar(badge_width);
  } else {
    rect.fRight = SkIntToScalar(bounds.right());
    rect.fLeft = rect.fRight - badge_width;
  }

  SkPaint rect_paint;
  rect_paint.setStyle(SkPaint::kFill_Style);
  rect_paint.setAntiAlias(true);
  rect_paint.setColor(background_color);
  canvas->drawRoundRect(rect, SkIntToScalar(2), SkIntToScalar(2), rect_paint);

  // Overlay the gradient. It is stretchy, so we do this in three parts.
  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();
  SkBitmap* gradient_left = resource_bundle.GetBitmapNamed(
      IDR_BROWSER_ACTION_BADGE_LEFT);
  SkBitmap* gradient_right = resource_bundle.GetBitmapNamed(
      IDR_BROWSER_ACTION_BADGE_RIGHT);
  SkBitmap* gradient_center = resource_bundle.GetBitmapNamed(
      IDR_BROWSER_ACTION_BADGE_CENTER);

  canvas->drawBitmap(*gradient_left, rect.fLeft, rect.fTop);
  canvas->TileImageInt(*gradient_center,
      SkScalarFloor(rect.fLeft) + gradient_left->width(),
      SkScalarFloor(rect.fTop),
      SkScalarFloor(rect.width()) - gradient_left->width() -
                    gradient_right->width(),
      SkScalarFloor(rect.height()));
  canvas->drawBitmap(*gradient_right,
      rect.fRight - SkIntToScalar(gradient_right->width()), rect.fTop);

  // Finally, draw the text centered within the badge. We set a clip in case the
  // text was too large.
  rect.fLeft += kPadding;
  rect.fRight -= kPadding;
  canvas->clipRect(rect);
  canvas->drawText(text.c_str(), text.size(),
                   rect.fLeft + (rect.width() - text_width) / 2,
                   rect.fTop + kTextSize + kTopTextPadding,
                   *text_paint);
  canvas->restore();
}
