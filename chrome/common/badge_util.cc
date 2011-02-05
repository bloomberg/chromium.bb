// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/badge_util.h"

#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"

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
          UTF16ToUTF8(base_font.GetFontName()).c_str(), SkTypeface::kNormal);
    }

    text_paint->setTypeface(typeface);
    // |text_paint| adds its own ref. Release the ref from CreateFontName.
    typeface->unref();
  }
  return text_paint;
}

SkBitmap DrawBadgeIconOverlay(const SkBitmap& icon,
                              float font_size,
                              const string16& text,
                              const string16& fallback) {
  const int kMinPadding = 1;

  // Calculate the proper style/text overlay to render on the badge.
  SkPaint* paint = badge_util::GetBadgeTextPaintSingleton();
  paint->setTextSize(SkFloatToScalar(font_size));
  paint->setColor(SK_ColorWHITE);

  std::string badge_text = UTF16ToUTF8(text);

  // See if the text will fit - otherwise use a default.
  SkScalar text_width = paint->measureText(badge_text.c_str(),
                                           badge_text.size());

  if (SkScalarRound(text_width) > (icon.width() - kMinPadding * 2)) {
    // String is too large - use the alternate text.
    badge_text = UTF16ToUTF8(fallback);
    text_width = paint->measureText(badge_text.c_str(), badge_text.size());
  }

  // When centering the text, we need to make sure there are an equal number
  // of pixels on each side as otherwise the text looks off-center. So if the
  // padding would be uneven, clip one pixel off the right side.
  int badge_width = icon.width();
  if ((SkScalarRound(text_width) % 1) != (badge_width % 1))
    badge_width--;

  // Render the badge bitmap and overlay into a canvas.
  scoped_ptr<gfx::CanvasSkia> canvas(
      new gfx::CanvasSkia(badge_width, icon.height(), false));
  canvas->DrawBitmapInt(icon, 0, 0);

  // Draw the text overlay centered horizontally and vertically. Skia expects
  // us to specify the lower left coordinate of the text box, which is why we
  // add 'font_size - 1' to the height.
  SkScalar x = (badge_width - text_width)/2;
  SkScalar y = (icon.height() - font_size)/2 + font_size - 1;
  canvas->drawText(badge_text.c_str(), badge_text.size(), x, y, *paint);

  // Return the generated image.
  return canvas->ExtractBitmap();
}

}  // namespace badge_util
