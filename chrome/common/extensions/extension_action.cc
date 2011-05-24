// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_action.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/common/badge_util.h"
#include "googleurl/src/gurl.h"
#include "grit/app_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/rect.h"

namespace {

// Different platforms need slightly different constants to look good.
#if defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
const float kTextSize = 9.0;
const int kBottomMargin = 0;
const int kPadding = 2;
const int kTopTextPadding = 0;
#elif defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
const float kTextSize = 8.0;
const int kBottomMargin = 5;
const int kPadding = 2;
const int kTopTextPadding = 1;
#elif defined(OS_MACOSX)
const float kTextSize = 9.0;
const int kBottomMargin = 5;
const int kPadding = 2;
const int kTopTextPadding = 0;
#else
const float kTextSize = 10;
const int kBottomMargin = 5;
const int kPadding = 2;
// The padding between the top of the badge and the top of the text.
const int kTopTextPadding = -1;
#endif

const int kBadgeHeight = 11;
const int kMaxTextWidth = 23;
// The minimum width for center-aligning the badge.
const int kCenterAlignThreshold = 20;


}  // namespace

const int ExtensionAction::kDefaultTabId = -1;

ExtensionAction::ExtensionAction() {
}

ExtensionAction::~ExtensionAction() {
}

void ExtensionAction::SetPopupUrl(int tab_id, const GURL& url) {
  // We store |url| even if it is empty, rather than removing a URL from the
  // map.  If an extension has a default popup, and removes it for a tab via
  // the API, we must remember that there is no popup for that specific tab.
  // If we removed the tab's URL, GetPopupURL would incorrectly return the
  // default URL.
  SetValue(&popup_url_, tab_id, url);
}

bool ExtensionAction::HasPopup(int tab_id) {
  return !GetPopupUrl(tab_id).is_empty();
}

GURL ExtensionAction::GetPopupUrl(int tab_id) {
  return GetValue(&popup_url_, tab_id);
}

void ExtensionAction::SetIcon(int tab_id, const SkBitmap& bitmap) {
  SetValue(&icon_, tab_id, bitmap);
}

SkBitmap ExtensionAction::GetIcon(int tab_id) {
  return GetValue(&icon_, tab_id);
}

void ExtensionAction::SetIconIndex(int tab_id, int index) {
  if (static_cast<size_t>(index) >= icon_paths_.size()) {
    NOTREACHED();
    return;
  }
  SetValue(&icon_index_, tab_id, index);
}

void ExtensionAction::ClearAllValuesForTab(int tab_id) {
  title_.erase(tab_id);
  icon_.erase(tab_id);
  icon_index_.erase(tab_id);
  badge_text_.erase(tab_id);
  badge_text_color_.erase(tab_id);
  badge_background_color_.erase(tab_id);
  visible_.erase(tab_id);
  popup_url_.erase(tab_id);
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
    background_color = SkColorSetARGB(255, 218, 0, 24);  // Default badge color.

  canvas->Save();

  SkPaint* text_paint = badge_util::GetBadgeTextPaintSingleton();
  text_paint->setTextSize(SkFloatToScalar(kTextSize));
  text_paint->setColor(text_color);

  // Calculate text width. We clamp it to a max size.
  SkScalar text_width = text_paint->measureText(text.c_str(), text.size());
  text_width = SkIntToScalar(
      std::min(kMaxTextWidth, SkScalarFloor(text_width)));

  // Calculate badge size. It is clamped to a min width just because it looks
  // silly if it is too skinny.
  int badge_width = SkScalarFloor(text_width) + kPadding * 2;
  int icon_width = GetIcon(tab_id).width();
  // Force the pixel width of badge to be either odd (if the icon width is odd)
  // or even otherwise. If there is a mismatch you get http://crbug.com/26400.
  if (icon_width != 0 && (badge_width % 2 != GetIcon(tab_id).width() % 2))
    badge_width += 1;
  badge_width = std::max(kBadgeHeight, badge_width);

  // Paint the badge background color in the right location. It is usually
  // right-aligned, but it can also be center-aligned if it is large.
  SkRect rect;
  rect.fBottom = SkIntToScalar(bounds.bottom() - kBottomMargin);
  rect.fTop = rect.fBottom - SkIntToScalar(kBadgeHeight);
  if (badge_width >= kCenterAlignThreshold) {
    rect.fLeft = SkIntToScalar(
                     SkScalarFloor(SkIntToScalar(bounds.x()) +
                                   SkIntToScalar(bounds.width() / 2.0) -
                                   SkIntToScalar(badge_width / 2.0)));
    rect.fRight = rect.fLeft + SkIntToScalar(badge_width);
  } else {
    rect.fRight = SkIntToScalar(bounds.right());
    rect.fLeft = rect.fRight - badge_width;
  }

  SkPaint rect_paint;
  rect_paint.setStyle(SkPaint::kFill_Style);
  rect_paint.setAntiAlias(true);
  rect_paint.setColor(background_color);
  canvas->AsCanvasSkia()->drawRoundRect(rect, SkIntToScalar(2),
                                        SkIntToScalar(2), rect_paint);

  // Overlay the gradient. It is stretchy, so we do this in three parts.
  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();
  SkBitmap* gradient_left = resource_bundle.GetBitmapNamed(
      IDR_BROWSER_ACTION_BADGE_LEFT);
  SkBitmap* gradient_right = resource_bundle.GetBitmapNamed(
      IDR_BROWSER_ACTION_BADGE_RIGHT);
  SkBitmap* gradient_center = resource_bundle.GetBitmapNamed(
      IDR_BROWSER_ACTION_BADGE_CENTER);

  canvas->AsCanvasSkia()->drawBitmap(*gradient_left, rect.fLeft, rect.fTop);
  canvas->TileImageInt(*gradient_center,
      SkScalarFloor(rect.fLeft) + gradient_left->width(),
      SkScalarFloor(rect.fTop),
      SkScalarFloor(rect.width()) - gradient_left->width() -
                    gradient_right->width(),
      SkScalarFloor(rect.height()));
  canvas->AsCanvasSkia()->drawBitmap(*gradient_right,
      rect.fRight - SkIntToScalar(gradient_right->width()), rect.fTop);

  // Finally, draw the text centered within the badge. We set a clip in case the
  // text was too large.
  rect.fLeft += kPadding;
  rect.fRight -= kPadding;
  canvas->AsCanvasSkia()->clipRect(rect);
  canvas->AsCanvasSkia()->drawText(text.c_str(), text.size(),
                                   rect.fLeft + (rect.width() - text_width) / 2,
                                   rect.fTop + kTextSize + kTopTextPadding,
                                   *text_paint);
  canvas->Restore();
}
