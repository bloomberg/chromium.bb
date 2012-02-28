// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ThemeServiceTest, AlignmentConversion) {
  // Verify that we get out what we put in.
  std::string top_left = "left top";
  int alignment = ThemeService::StringToAlignment(top_left);
  EXPECT_EQ(ThemeService::ALIGN_TOP | ThemeService::ALIGN_LEFT,
            alignment);
  EXPECT_EQ(top_left, ThemeService::AlignmentToString(alignment));

  // We get back a normalized version of what we put in.
  alignment = ThemeService::StringToAlignment("top");
  EXPECT_EQ(ThemeService::ALIGN_TOP, alignment);
  EXPECT_EQ("center top", ThemeService::AlignmentToString(alignment));

  alignment = ThemeService::StringToAlignment("left");
  EXPECT_EQ(ThemeService::ALIGN_LEFT, alignment);
  EXPECT_EQ("left center", ThemeService::AlignmentToString(alignment));

  alignment = ThemeService::StringToAlignment("right");
  EXPECT_EQ(ThemeService::ALIGN_RIGHT, alignment);
  EXPECT_EQ("right center", ThemeService::AlignmentToString(alignment));

  alignment = ThemeService::StringToAlignment("righttopbottom");
  EXPECT_EQ(ThemeService::ALIGN_CENTER, alignment);
  EXPECT_EQ("center center", ThemeService::AlignmentToString(alignment));
}

TEST(ThemeServiceTest, AlignmentConversionInput) {
  // Verify that we output in an expected format.
  int alignment = ThemeService::StringToAlignment("bottom right");
  EXPECT_EQ("right bottom", ThemeService::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = ThemeService::StringToAlignment("new zealand");
  EXPECT_EQ("center center", ThemeService::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = ThemeService::StringToAlignment("new zealand top");
  EXPECT_EQ("center top", ThemeService::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = ThemeService::StringToAlignment("new zealandtop");
  EXPECT_EQ("center center", ThemeService::AlignmentToString(alignment));
}
