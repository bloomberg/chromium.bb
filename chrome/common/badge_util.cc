// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/badge_util.h"

#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "gfx/font.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"

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
          WideToUTF8(base_font.GetFontName()).c_str(), SkTypeface::kNormal);
    }

    text_paint->setTypeface(typeface);
    // |text_paint| adds its own ref. Release the ref from CreateFontName.
    typeface->unref();
  }
  return text_paint;
}

}  // namespace badge_util
