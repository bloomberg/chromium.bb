// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "app/gfx/color_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"

TEST(ColorUtils, SkColorToHSLRed) {
  color_utils::HSL hsl = { 0, 0, 0 };
  color_utils::SkColorToHSL(SK_ColorRED, &hsl);
  EXPECT_EQ(hsl.h, 0);
  EXPECT_EQ(hsl.s, 1);
  EXPECT_EQ(hsl.l, 0.5);
}

TEST(ColorUtils, SkColorToHSLGrey) {
  color_utils::HSL hsl = { 0, 0, 0 };
  color_utils::SkColorToHSL(SkColorSetARGB(255, 128, 128, 128), &hsl);
  EXPECT_EQ(hsl.h, 0);
  EXPECT_EQ(hsl.s, 0);
  EXPECT_EQ(static_cast<int>(hsl.l * 100),
            static_cast<int>(0.5 * 100));  // Accurate to two decimal places.
}

TEST(ColorUtils, HSLToSkColorWithAlpha) {
  SkColor red = SkColorSetARGB(128, 255, 0, 0);
  color_utils::HSL hsl = { 0, 1, 0.5 };
  SkColor result = color_utils::HSLToSkColor(hsl, 128);
  EXPECT_EQ(SkColorGetA(red), SkColorGetA(result));
  EXPECT_EQ(SkColorGetR(red), SkColorGetR(result));
  EXPECT_EQ(SkColorGetG(red), SkColorGetG(result));
  EXPECT_EQ(SkColorGetB(red), SkColorGetB(result));
}

TEST(ColorUtils, ColorToHSLRegisterSpill) {
  // In a opt build on Linux, this was causing a register spill on my laptop
  // (Pentium M) when converting from SkColor to HSL.
  SkColor input = SkColorSetARGB(255, 206, 154, 89);
  color_utils::HSL hsl = { -1, -1, -1 };
  SkColor result = color_utils::HSLShift(input, hsl);
  EXPECT_EQ(255U, SkColorGetA(result));
  EXPECT_EQ(206U, SkColorGetR(result));
  EXPECT_EQ(153U, SkColorGetG(result));
  EXPECT_EQ(88U, SkColorGetB(result));
}
