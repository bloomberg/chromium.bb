// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_util_x11.h"

#include <X11/extensions/Xrandr.h>

// Undefine X's macros used in gtest.
#undef Bool
#undef None

#include "chromeos/display/output_util.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test DisplayUtilX11Test;

namespace ash {
namespace internal {

TEST_F(DisplayUtilX11Test, TestBlackListedDisplay) {
  EXPECT_TRUE(ShouldIgnoreSize(10, 10));
  EXPECT_TRUE(ShouldIgnoreSize(40, 30));
  EXPECT_TRUE(ShouldIgnoreSize(50, 40));
  EXPECT_TRUE(ShouldIgnoreSize(160, 90));
  EXPECT_TRUE(ShouldIgnoreSize(160, 100));

  EXPECT_FALSE(ShouldIgnoreSize(50, 60));
  EXPECT_FALSE(ShouldIgnoreSize(100, 70));
  EXPECT_FALSE(ShouldIgnoreSize(272, 181));
}

TEST_F(DisplayUtilX11Test, GetResolutionList) {
  XRRScreenResources resources = {0};
  RROutput outputs[] = {1};
  resources.noutput = arraysize(outputs);
  resources.outputs = outputs;
  XRRModeInfo modes[] = {
    // id, width, height, interlaced, refresh rate
    chromeos::test::CreateModeInfo(11, 1920, 1200, false, 60.0f),

    // different rates
    chromeos::test::CreateModeInfo(12, 1920, 1080, false, 30.0f),
    chromeos::test::CreateModeInfo(13, 1920, 1080, false, 50.0f),
    chromeos::test::CreateModeInfo(14, 1920, 1080, false, 40.0f),

    // interlace vs non interlace
    chromeos::test::CreateModeInfo(15, 1280, 720, true, 60.0f),
    chromeos::test::CreateModeInfo(16, 1280, 720, false, 40.0f),

    // interlace only
    chromeos::test::CreateModeInfo(17, 1024, 768, true, 40.0f),
    chromeos::test::CreateModeInfo(18, 1024, 768, true, 60.0f),

    // mixed
    chromeos::test::CreateModeInfo(19, 1024, 600, true, 60.0f),
    chromeos::test::CreateModeInfo(20, 1024, 600, false, 40.0f),
    chromeos::test::CreateModeInfo(21, 1024, 600, false, 50.0f),

    // just one interlaced mode
    chromeos::test::CreateModeInfo(22, 640, 480, true, 60.0f),
  };
  resources.nmode = arraysize(modes);
  resources.modes = modes;

  XRROutputInfo output_info = {0};
  RRMode output_modes[] = {
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22
  };
  output_info.nmode = arraysize(output_modes);
  output_info.modes = output_modes;

  std::vector<Resolution> resolutions =
      GetResolutionList(&resources, &output_info);
  EXPECT_EQ(6u, resolutions.size());
  EXPECT_EQ("1920x1200", resolutions[0].size.ToString());
  EXPECT_FALSE(resolutions[0].interlaced);

  EXPECT_EQ("1920x1080", resolutions[1].size.ToString());
  EXPECT_FALSE(resolutions[1].interlaced);

  EXPECT_EQ("1280x720", resolutions[2].size.ToString());
  EXPECT_FALSE(resolutions[2].interlaced);

  EXPECT_EQ("1024x768", resolutions[3].size.ToString());
  EXPECT_TRUE(resolutions[3].interlaced);

  EXPECT_EQ("1024x600", resolutions[4].size.ToString());
  EXPECT_FALSE(resolutions[4].interlaced);

  EXPECT_EQ("640x480", resolutions[5].size.ToString());
  EXPECT_TRUE(resolutions[5].interlaced);

  // Empty output shouldn't crash.
  RRMode empty_output_modes[] = {};
  output_info.nmode = 0;
  output_info.modes = empty_output_modes;

  resolutions = GetResolutionList(&resources, &output_info);
  EXPECT_EQ(0u, resolutions.size());
}

}  // namespace internal
}  // namespace ash
