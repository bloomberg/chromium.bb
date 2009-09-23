// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GFX_COLOR_UTILS_H_
#define APP_GFX_COLOR_UTILS_H_

#include "third_party/skia/include/core/SkColor.h"

class SkBitmap;

namespace color_utils {

// Represents an HSL color.
struct HSL {
  double h;
  double s;
  double l;
};

// Note: these transformations assume sRGB as the source color space
void SkColorToHSL(SkColor c, HSL* hsl);
SkColor HSLToSkColor(const HSL& hsl, SkAlpha alpha);

// HSL-Shift an SkColor. The shift values are in the range of 0-1, with the
// option to specify -1 for 'no change'. The shift values are defined as:
// hsl_shift[0] (hue): The absolute hue value - 0 and 1 map
//    to 0 and 360 on the hue color wheel (red).
// hsl_shift[1] (saturation): A saturation shift, with the
//    following key values:
//    0 = remove all color.
//    0.5 = leave unchanged.
//    1 = fully saturate the image.
// hsl_shift[2] (lightness): A lightness shift, with the
//    following key values:
//    0 = remove all lightness (make all pixels black).
//    0.5 = leave unchanged.
//    1 = full lightness (make all pixels white).
SkColor HSLShift(SkColor color, const HSL& shift);

// Determine if a given alpha value is nearly completely transparent.
bool IsColorCloseToTransparent(SkAlpha alpha);

// Determine if a color is near grey.
bool IsColorCloseToGrey(int r, int g, int b);

// Gets a color representing a bitmap. The definition of "representing" is the
// average color in the bitmap. The color returned is modified to have the
// specified alpha.
SkColor GetAverageColorOfFavicon(SkBitmap* bitmap, SkAlpha alpha);

// Builds a histogram based on the Y' of the Y'UV representation of
// this image.
void BuildLumaHistogram(SkBitmap* bitmap, int histogram[256]);

// Returns a blend of the supplied colors, ranging from |background| (for
// |alpha| == 0) to |foreground| (for |alpha| == 255).
SkColor AlphaBlend(SkColor foreground, SkColor background, SkAlpha alpha);

// Given two possible foreground colors, return the one that is more readable
// over |background|.
SkColor PickMoreReadableColor(SkColor foreground1,
                              SkColor foreground2,
                              SkColor background);

// Gets a Windows system color as a SkColor
SkColor GetSysSkColor(int which);

}  // namespace color_utils

#endif  // APP_GFX_COLOR_UTILS_H_
