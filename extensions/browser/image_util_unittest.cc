// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "extensions/browser/image_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace extensions {

void RunPassTest(const std::string& css_string, SkColor expected_result) {
  SkColor color = 0;
  EXPECT_TRUE(image_util::ParseCSSColorString(css_string, &color));
  EXPECT_EQ(color, expected_result);
}

void RunFailTest(const std::string& css_string) {
  SkColor color = 0;
  EXPECT_FALSE(image_util::ParseCSSColorString(css_string, &color));
}

TEST(ImageUtilTest, ChangeBadgeBackgroundNormalCSS) {
  RunPassTest("#34006A", SkColorSetARGB(0xFF, 0x34, 0, 0x6A));
}

TEST(ImageUtilTest, ChangeBadgeBackgroundShortCSS) {
  RunPassTest("#A1E", SkColorSetARGB(0xFF, 0xAA, 0x11, 0xEE));
}

TEST(ImageUtilTest, ChangeBadgeBackgroundCSSNoHash) {
  RunFailTest("11FF22");
}

TEST(ImageUtilTest, ChangeBadgeBackgroundCSSTooShort) {
  RunFailTest("#FF22");
}

TEST(ImageUtilTest, ChangeBadgeBackgroundCSSTooLong) {
  RunFailTest("#FF22128");
}

TEST(ImageUtilTest, ChangeBadgeBackgroundCSSInvalid) {
  RunFailTest("#-22128");
}

TEST(ImageUtilTest, ChangeBadgeBackgroundCSSInvalidWithPlus) {
  RunFailTest("#+22128");
}

}  // namespace extensions
