// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_change_observer_chromeos.h"

#include "ash/display/display_info.h"
#include "chromeos/display/output_configurator.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::OutputConfigurator;

typedef testing::Test DisplayChangeObserverTest;

namespace ash {
namespace internal {

TEST_F(DisplayChangeObserverTest, GetResolutionList) {
  OutputConfigurator::OutputSnapshot output;
  output.mode_infos[11] = OutputConfigurator::ModeInfo(1920, 1200, false, 60);

  // All non-interlaced (as would be seen with different refresh rates).
  output.mode_infos[12] = OutputConfigurator::ModeInfo(1920, 1080, false, 80);
  output.mode_infos[13] = OutputConfigurator::ModeInfo(1920, 1080, false, 70);
  output.mode_infos[14] = OutputConfigurator::ModeInfo(1920, 1080, false, 60);

  // Interlaced vs non-interlaced.
  output.mode_infos[15] = OutputConfigurator::ModeInfo(1280, 720, true, 60);
  output.mode_infos[16] = OutputConfigurator::ModeInfo(1280, 720, false, 60);

  // Interlaced only.
  output.mode_infos[17] = OutputConfigurator::ModeInfo(1024, 768, true, 70);
  output.mode_infos[18] = OutputConfigurator::ModeInfo(1024, 768, true, 60);

  // Mixed.
  output.mode_infos[19] = OutputConfigurator::ModeInfo(1024, 600, true, 60);
  output.mode_infos[20] = OutputConfigurator::ModeInfo(1024, 600, false, 70);
  output.mode_infos[21] = OutputConfigurator::ModeInfo(1024, 600, false, 60);

  // Just one interlaced mode.
  output.mode_infos[22] = OutputConfigurator::ModeInfo(640, 480, true, 60);

  std::vector<Resolution> resolutions =
      DisplayChangeObserver::GetResolutionList(output);
  ASSERT_EQ(6u, resolutions.size());
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

  // Outputs without any modes shouldn't cause a crash.
  output.mode_infos.clear();
  resolutions = DisplayChangeObserver::GetResolutionList(output);
  EXPECT_EQ(0u, resolutions.size());
}

}  // namespace internal
}  // namespace ash
