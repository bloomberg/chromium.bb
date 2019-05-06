// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox_extension.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_utils.h"

namespace internal {

// Defined in searchbox_extension.cc
bool IsNtpBackgroundDark(SkColor ntp_text);

TEST(SearchboxExtensionTest, TestIsNtpBackgroundDark) {
  // Dark font means light background.
  EXPECT_FALSE(IsNtpBackgroundDark(SK_ColorBLACK));

  // Light font means dark background.
  EXPECT_TRUE(IsNtpBackgroundDark(SK_ColorWHITE));

  // Light but close to mid point color text implies dark background.
  EXPECT_TRUE(IsNtpBackgroundDark(SkColorSetARGB(255, 30, 144, 255)));
}

}  // namespace internal
