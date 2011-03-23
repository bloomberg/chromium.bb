// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ThemeServiceTest, AlignmentConversion) {
  // Verify that we get out what we put in.
  std::string top_left = "top left";
  int alignment = ThemeService::StringToAlignment(top_left);
  EXPECT_EQ(ThemeService::ALIGN_TOP | ThemeService::ALIGN_LEFT,
            alignment);
  EXPECT_EQ(top_left, ThemeService::AlignmentToString(alignment));

  alignment = ThemeService::StringToAlignment("top");
  EXPECT_EQ(ThemeService::ALIGN_TOP, alignment);
  EXPECT_EQ("top", ThemeService::AlignmentToString(alignment));

  alignment = ThemeService::StringToAlignment("left");
  EXPECT_EQ(ThemeService::ALIGN_LEFT, alignment);
  EXPECT_EQ("left", ThemeService::AlignmentToString(alignment));

  alignment = ThemeService::StringToAlignment("right");
  EXPECT_EQ(ThemeService::ALIGN_RIGHT, alignment);
  EXPECT_EQ("right", ThemeService::AlignmentToString(alignment));

  alignment = ThemeService::StringToAlignment("righttopbottom");
  EXPECT_EQ(ThemeService::ALIGN_CENTER, alignment);
  EXPECT_EQ("", ThemeService::AlignmentToString(alignment));
}

TEST(ThemeServiceTest, AlignmentConversionInput) {
  // Verify that we output in an expected format.
  int alignment = ThemeService::StringToAlignment("right bottom");
  EXPECT_EQ("bottom right", ThemeService::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = ThemeService::StringToAlignment("new zealand");
  EXPECT_EQ("", ThemeService::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = ThemeService::StringToAlignment("new zealand top");
  EXPECT_EQ("top", ThemeService::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = ThemeService::StringToAlignment("new zealandtop");
  EXPECT_EQ("", ThemeService::AlignmentToString(alignment));
}
