// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_action.h"

#include "app/gfx/canvas.h"
#include "app/resource_bundle.h"
#include "base/gfx/rect.h"
#include "chrome/app/chrome_dll_resource.h"
#include "grit/app_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

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

  // Different platforms need slightly different constants to look good.
#if defined(OS_LINUX)
  const int kTextSize = 9;
  const int kBottomMargin = 4;
  const int kPadding = 2;
  const int kBadgeHeight = 12;
  const int kMaxTextWidth = 23;
  // The minimum width for center-aligning the badge.
  const int kCenterAlignThreshold = 20;
#else
  const int kTextSize = 8;
  const int kBottomMargin = 5;
  const int kPadding = 2;
  const int kBadgeHeight = 11;
  const int kMaxTextWidth = 23;
  // The minimum width for center-aligning the badge.
  const int kCenterAlignThreshold = 20;
#endif

  canvas->save();

#if defined(OS_MACOSX)
  SkTypeface* typeface =
      SkTypeface::CreateFromName("Helvetica", SkTypeface::kBold);
#else
  SkTypeface* typeface = SkTypeface::CreateFromName("Arial", SkTypeface::kBold);
#endif
  SkPaint text_paint;
  text_paint.setAntiAlias(true);
  text_paint.setColor(text_color);
  text_paint.setFakeBoldText(true);
  text_paint.setTextAlign(SkPaint::kLeft_Align);
  text_paint.setTextSize(SkIntToScalar(kTextSize));
  text_paint.setTypeface(typeface);

  // Calculate text width. We clamp it to a max size.
  SkScalar text_width = text_paint.measureText(text.c_str(), text.size());
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
    rect.fLeft = SkIntToScalar((bounds.right() - badge_width) / 2);
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
                   rect.fTop + kTextSize + 1,
                   text_paint);
  canvas->restore();
}
