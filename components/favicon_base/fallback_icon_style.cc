// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/fallback_icon_style.h"

#include "ui/gfx/color_utils.h"

namespace favicon_base {

namespace {

// Luminance threshold for background color determine whether to use dark or
// light text color.
int kDarkTextLuminanceThreshold = 190;

// Default values for FallbackIconStyle.
SkColor kDefaultBackgroundColor = SkColorSetRGB(0x80, 0x80, 0x80);
SkColor kDefaultTextColorDark = SK_ColorBLACK;
SkColor kDefaultTextColorLight = SK_ColorWHITE;
double kDefaultFontSizeRatio = 0.8;
double kDefaultRoundness = 0.125;  // 1 / 8.

}  // namespace

FallbackIconStyle::FallbackIconStyle()
    : background_color(kDefaultBackgroundColor),
      text_color(kDefaultTextColorLight),
      font_size_ratio(kDefaultFontSizeRatio),
      roundness(kDefaultRoundness) {
}

FallbackIconStyle::~FallbackIconStyle() {
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

}  // namespace favicon_base
