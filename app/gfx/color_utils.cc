// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/color_utils.h"

#include <math.h>
#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/basictypes.h"
#include "base/logging.h"
#include "build/build_config.h"
#if defined(OS_WIN)
#include "skia/ext/skia_utils_win.h"
#endif
#include "third_party/skia/include/core/SkBitmap.h"

namespace color_utils {

// Helper functions -----------------------------------------------------------

namespace {

int GetLumaForColor(SkColor* color) {
  int luma = static_cast<int>((0.3 * SkColorGetR(*color)) +
                              (0.59 * SkColorGetG(*color)) +
                              (0.11 * SkColorGetB(*color)));
  return std::max(std::min(luma, 255), 0);
}

// Next three functions' formulas from:
// http://www.w3.org/TR/WCAG20/#relativeluminancedef
// http://www.w3.org/TR/WCAG20/#contrast-ratiodef

double ConvertSRGB(double eight_bit_component) {
  const double component = eight_bit_component / 255.0;
  return (component <= 0.03928) ?
      (component / 12.92) : pow((component + 0.055) / 1.055, 2.4);
}

double RelativeLuminance(SkColor color) {
  return (0.2126 * ConvertSRGB(SkColorGetR(color))) +
      (0.7152 * ConvertSRGB(SkColorGetG(color))) +
      (0.0722 * ConvertSRGB(SkColorGetB(color)));
}

double ContrastRatio(SkColor color1, SkColor color2) {
  const double l1 = RelativeLuminance(color1) + 0.05;
  const double l2 = RelativeLuminance(color2) + 0.05;
  return (l1 > l2) ? (l1 / l2) : (l2 / l1);
}

}  // namespace

// ----------------------------------------------------------------------------

bool IsColorCloseToTransparent(SkAlpha alpha) {
  const int kCloseToBoundary = 64;
  return alpha < kCloseToBoundary;
}

bool IsColorCloseToGrey(int r, int g, int b) {
  const int kAverageBoundary = 15;
  int average = (r + g + b) / 3;
  return (abs(r - average) < kAverageBoundary) &&
         (abs(g - average) < kAverageBoundary) &&
         (abs(b - average) < kAverageBoundary);
}

SkColor GetAverageColorOfFavicon(SkBitmap* favicon, SkAlpha alpha) {
  int r = 0, g = 0, b = 0;

  SkAutoLockPixels favicon_lock(*favicon);
  SkColor* pixels = static_cast<SkColor*>(favicon->getPixels());
  // Assume ARGB_8888 format.
  DCHECK(favicon->getConfig() == SkBitmap::kARGB_8888_Config);
  SkColor* current_color = pixels;

  DCHECK(favicon->width() <= 16 && favicon->height() <= 16);

  int pixel_count = favicon->width() * favicon->height();
  int color_count = 0;
  for (int i = 0; i < pixel_count; ++i, ++current_color) {
    // Disregard this color if it is close to black, close to white, or close
    // to transparent since any of those pixels do not contribute much to the
    // color makeup of this icon.
    int cr = SkColorGetR(*current_color);
    int cg = SkColorGetG(*current_color);
    int cb = SkColorGetB(*current_color);

    if (IsColorCloseToTransparent(SkColorGetA(*current_color)) ||
        IsColorCloseToGrey(cr, cg, cb))
      continue;

    r += cr;
    g += cg;
    b += cb;
    ++color_count;
  }

  return color_count ?
      SkColorSetARGB(alpha, r / color_count, g / color_count, b / color_count) :
      SkColorSetARGB(alpha, 0, 0, 0);
}

void BuildLumaHistogram(SkBitmap* bitmap, int histogram[256]) {
  SkAutoLockPixels bitmap_lock(*bitmap);
  // Assume ARGB_8888 format.
  DCHECK(bitmap->getConfig() == SkBitmap::kARGB_8888_Config);

  int pixel_width = bitmap->width();
  int pixel_height = bitmap->height();
  for (int y = 0; y < pixel_height; ++y) {
    SkColor* current_color = static_cast<uint32_t*>(bitmap->getAddr32(0, y));
    for (int x = 0; x < pixel_width; ++x, ++current_color)
      histogram[GetLumaForColor(current_color)]++;
  }
}

SkColor AlphaBlend(SkColor foreground, SkColor background, SkAlpha alpha) {
  if (alpha == 0)
    return background;
  if (alpha == 255)
    return foreground;
  return SkColorSetRGB(
    ((SkColorGetR(foreground) * alpha) +
     (SkColorGetR(background) * (255 - alpha))) / 255,
    ((SkColorGetG(foreground) * alpha) +
     (SkColorGetG(background) * (255 - alpha))) / 255,
    ((SkColorGetB(foreground) * alpha) +
     (SkColorGetB(background) * (255 - alpha))) / 255);
}

SkColor PickMoreReadableColor(SkColor foreground1,
                              SkColor foreground2,
                              SkColor background) {
  return (ContrastRatio(foreground1, background) >=
      ContrastRatio(foreground2, background)) ? foreground1 : foreground2;
}

SkColor GetSysSkColor(int which) {
#if defined(OS_WIN)
  return skia::COLORREFToSkColor(GetSysColor(which));
#else
  NOTIMPLEMENTED();
  return SK_ColorLTGRAY;
#endif
}

}  // namespace color_utils
