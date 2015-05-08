// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/fallback_icon_style.h"

#include <algorithm>

#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"

namespace favicon_base {

namespace {

// Luminance threshold for background color determine whether to use dark or
// light text color.
const int kDarkTextLuminanceThreshold = 190;

// The maximum luminance of the background color to ensure light text is
// readable.
const double kMaxBackgroundColorLuminance = 0.67;

// Default values for FallbackIconStyle.
const SkColor kDefaultBackgroundColor = SkColorSetRGB(0x78, 0x78, 0x78);
const SkColor kDefaultTextColorDark = SK_ColorBLACK;
const SkColor kDefaultTextColorLight = SK_ColorWHITE;
const double kDefaultFontSizeRatio = 0.44;
const double kDefaultRoundness = 0;  // Square. Round corners are applied
                                     // externally (Javascript or Java).

}  // namespace

FallbackIconStyle::FallbackIconStyle()
    : background_color(kDefaultBackgroundColor),
      text_color(kDefaultTextColorLight),
      font_size_ratio(kDefaultFontSizeRatio),
      roundness(kDefaultRoundness) {
}

FallbackIconStyle::~FallbackIconStyle() {
}

bool FallbackIconStyle::operator==(const FallbackIconStyle& other) const {
  return background_color == other.background_color &&
      text_color == other.text_color &&
      font_size_ratio == other.font_size_ratio &&
      roundness == other.roundness;
}

void MatchFallbackIconTextColorAgainstBackgroundColor(
    FallbackIconStyle* style) {
  int luminance = color_utils::GetLuminanceForColor(style->background_color);
  style->text_color = (luminance >= kDarkTextLuminanceThreshold ?
      kDefaultTextColorDark : kDefaultTextColorLight);
}

bool ValidateFallbackIconStyle(const FallbackIconStyle& style) {
  return style.font_size_ratio >= 0.0 && style.font_size_ratio <= 1.0 &&
      style.roundness >= 0.0 && style.roundness <= 1.0;
}

void SetDominantColorAsBackground(
    const scoped_refptr<base::RefCountedMemory>& bitmap_data,
    FallbackIconStyle* style) {
  SkColor dominant_color =
      color_utils::CalculateKMeanColorOfPNG(bitmap_data);
  // Assumes |style.text_color| is light, and clamps luminance down to a
  // reasonable maximum value so text is readable.
  color_utils::HSL color_hsl;
  color_utils::SkColorToHSL(dominant_color, &color_hsl);
  color_hsl.l = std::min(color_hsl.l, kMaxBackgroundColorLuminance);
  style->background_color =
      color_utils::HSLToSkColor(color_hsl, SK_AlphaOPAQUE);
}

}  // namespace favicon_base
