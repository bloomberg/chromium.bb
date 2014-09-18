// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_change_observer_chromeos.h"

#include "ash/display/display_info.h"
#include "base/memory/scoped_vector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/display/chromeos/test/test_display_snapshot.h"
#include "ui/display/types/display_mode.h"

using ui::DisplayConfigurator;

typedef testing::Test DisplayChangeObserverTest;

namespace ash {

TEST_F(DisplayChangeObserverTest, GetExternalDisplayModeList) {
  ScopedVector<const ui::DisplayMode> modes;
  modes.push_back(new ui::DisplayMode(gfx::Size(1920, 1200), false, 60));

  // All non-interlaced (as would be seen with different refresh rates).
  modes.push_back(new ui::DisplayMode(gfx::Size(1920, 1080), false, 80));
  modes.push_back(new ui::DisplayMode(gfx::Size(1920, 1080), false, 70));
  modes.push_back(new ui::DisplayMode(gfx::Size(1920, 1080), false, 60));

  // Interlaced vs non-interlaced.
  modes.push_back(new ui::DisplayMode(gfx::Size(1280, 720), true, 60));
  modes.push_back(new ui::DisplayMode(gfx::Size(1280, 720), false, 60));

  // Interlaced only.
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 768), true, 70));
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 768), true, 60));

  // Mixed.
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 600), true, 60));
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 600), false, 70));
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 600), false, 60));

  // Just one interlaced mode.
  modes.push_back(new ui::DisplayMode(gfx::Size(640, 480), true, 60));

  ui::TestDisplaySnapshot display_snapshot;
  display_snapshot.set_modes(modes.get());
  DisplayConfigurator::DisplayState output;
  output.display = &display_snapshot;

  std::vector<DisplayMode> display_modes =
      DisplayChangeObserver::GetExternalDisplayModeList(output);
  ASSERT_EQ(6u, display_modes.size());
  EXPECT_EQ("640x480", display_modes[0].size.ToString());
  EXPECT_TRUE(display_modes[0].interlaced);
  EXPECT_EQ(display_modes[0].refresh_rate, 60);

  EXPECT_EQ("1024x600", display_modes[1].size.ToString());
  EXPECT_FALSE(display_modes[1].interlaced);
  EXPECT_EQ(display_modes[1].refresh_rate, 70);

  EXPECT_EQ("1024x768", display_modes[2].size.ToString());
  EXPECT_TRUE(display_modes[2].interlaced);
  EXPECT_EQ(display_modes[2].refresh_rate, 70);

  EXPECT_EQ("1280x720", display_modes[3].size.ToString());
  EXPECT_FALSE(display_modes[3].interlaced);
  EXPECT_EQ(display_modes[3].refresh_rate, 60);

  EXPECT_EQ("1920x1080", display_modes[4].size.ToString());
  EXPECT_FALSE(display_modes[4].interlaced);
  EXPECT_EQ(display_modes[4].refresh_rate, 80);

  EXPECT_EQ("1920x1200", display_modes[5].size.ToString());
  EXPECT_FALSE(display_modes[5].interlaced);
  EXPECT_EQ(display_modes[5].refresh_rate, 60);

  // Outputs without any modes shouldn't cause a crash.
  modes.clear();
  display_snapshot.set_modes(modes.get());

  display_modes = DisplayChangeObserver::GetExternalDisplayModeList(output);
  EXPECT_EQ(0u, display_modes.size());
}

TEST_F(DisplayChangeObserverTest, GetInternalDisplayModeList) {
  ScopedVector<const ui::DisplayMode> modes;
  // Data picked from peppy.
  modes.push_back(new ui::DisplayMode(gfx::Size(1366, 768), false, 60));
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 768), false, 60));
  modes.push_back(new ui::DisplayMode(gfx::Size(800, 600), false, 60));
  modes.push_back(new ui::DisplayMode(gfx::Size(600, 600), false, 56.2));
  modes.push_back(new ui::DisplayMode(gfx::Size(640, 480), false, 59.9));

  ui::TestDisplaySnapshot display_snapshot;
  display_snapshot.set_modes(modes.get());
  display_snapshot.set_native_mode(modes[0]);
  DisplayConfigurator::DisplayState output;
  output.display = &display_snapshot;

  DisplayInfo info;
  info.SetBounds(gfx::Rect(0, 0, 1366, 768));

  std::vector<DisplayMode> display_modes =
      DisplayChangeObserver::GetInternalDisplayModeList(info, output);
  ASSERT_EQ(5u, display_modes.size());
  EXPECT_EQ("1366x768", display_modes[0].size.ToString());
  EXPECT_FALSE(display_modes[0].native);
  EXPECT_NEAR(display_modes[0].ui_scale, 0.5, 0.01);
  EXPECT_EQ(display_modes[0].refresh_rate, 60);

  EXPECT_EQ("1366x768", display_modes[1].size.ToString());
  EXPECT_FALSE(display_modes[1].native);
  EXPECT_NEAR(display_modes[1].ui_scale, 0.6, 0.01);
  EXPECT_EQ(display_modes[1].refresh_rate, 60);

  EXPECT_EQ("1366x768", display_modes[2].size.ToString());
  EXPECT_FALSE(display_modes[2].native);
  EXPECT_NEAR(display_modes[2].ui_scale, 0.75, 0.01);
  EXPECT_EQ(display_modes[2].refresh_rate, 60);

  EXPECT_EQ("1366x768", display_modes[3].size.ToString());
  EXPECT_TRUE(display_modes[3].native);
  EXPECT_NEAR(display_modes[3].ui_scale, 1.0, 0.01);
  EXPECT_EQ(display_modes[3].refresh_rate, 60);

  EXPECT_EQ("1366x768", display_modes[4].size.ToString());
  EXPECT_FALSE(display_modes[4].native);
  EXPECT_NEAR(display_modes[4].ui_scale, 1.125, 0.01);
  EXPECT_EQ(display_modes[4].refresh_rate, 60);
}

TEST_F(DisplayChangeObserverTest, GetInternalHiDPIDisplayModeList) {
  ScopedVector<const ui::DisplayMode> modes;
  // Data picked from peppy.
  modes.push_back(new ui::DisplayMode(gfx::Size(2560, 1700), false, 60));
  modes.push_back(new ui::DisplayMode(gfx::Size(2048, 1536), false, 60));
  modes.push_back(new ui::DisplayMode(gfx::Size(1920, 1440), false, 60));

  ui::TestDisplaySnapshot display_snapshot;
  display_snapshot.set_modes(modes.get());
  display_snapshot.set_native_mode(modes[0]);
  DisplayConfigurator::DisplayState output;
  output.display = &display_snapshot;

  DisplayInfo info;
  info.SetBounds(gfx::Rect(0, 0, 2560, 1700));
  info.set_device_scale_factor(2.0f);

  std::vector<DisplayMode> display_modes =
      DisplayChangeObserver::GetInternalDisplayModeList(info, output);
  ASSERT_EQ(8u, display_modes.size());
  EXPECT_EQ("2560x1700", display_modes[0].size.ToString());
  EXPECT_FALSE(display_modes[0].native);
  EXPECT_NEAR(display_modes[0].ui_scale, 0.5, 0.01);
  EXPECT_EQ(display_modes[0].refresh_rate, 60);

  EXPECT_EQ("2560x1700", display_modes[1].size.ToString());
  EXPECT_FALSE(display_modes[1].native);
  EXPECT_NEAR(display_modes[1].ui_scale, 0.625, 0.01);
  EXPECT_EQ(display_modes[1].refresh_rate, 60);

  EXPECT_EQ("2560x1700", display_modes[2].size.ToString());
  EXPECT_FALSE(display_modes[2].native);
  EXPECT_NEAR(display_modes[2].ui_scale, 0.8, 0.01);
  EXPECT_EQ(display_modes[2].refresh_rate, 60);

  EXPECT_EQ("2560x1700", display_modes[3].size.ToString());
  EXPECT_FALSE(display_modes[3].native);
  EXPECT_NEAR(display_modes[3].ui_scale, 1.0, 0.01);
  EXPECT_EQ(display_modes[3].refresh_rate, 60);

  EXPECT_EQ("2560x1700", display_modes[4].size.ToString());
  EXPECT_FALSE(display_modes[4].native);
  EXPECT_NEAR(display_modes[4].ui_scale, 1.125, 0.01);
  EXPECT_EQ(display_modes[4].refresh_rate, 60);

  EXPECT_EQ("2560x1700", display_modes[5].size.ToString());
  EXPECT_FALSE(display_modes[5].native);
  EXPECT_NEAR(display_modes[5].ui_scale, 1.25, 0.01);
  EXPECT_EQ(display_modes[5].refresh_rate, 60);

  EXPECT_EQ("2560x1700", display_modes[6].size.ToString());
  EXPECT_FALSE(display_modes[6].native);
  EXPECT_NEAR(display_modes[6].ui_scale, 1.5, 0.01);
  EXPECT_EQ(display_modes[6].refresh_rate, 60);

  EXPECT_EQ("2560x1700", display_modes[7].size.ToString());
  EXPECT_TRUE(display_modes[7].native);
  EXPECT_NEAR(display_modes[7].ui_scale, 2.0, 0.01);
  EXPECT_EQ(display_modes[7].refresh_rate, 60);
}

TEST_F(DisplayChangeObserverTest, GetInternalDisplayModeList1_25) {
  ScopedVector<const ui::DisplayMode> modes;
  // Data picked from peppy.
  modes.push_back(new ui::DisplayMode(gfx::Size(1920, 1080), false, 60));

  ui::TestDisplaySnapshot display_snapshot;
  display_snapshot.set_modes(modes.get());
  display_snapshot.set_native_mode(modes[0]);
  DisplayConfigurator::DisplayState output;
  output.display = &display_snapshot;

  DisplayInfo info;
  info.SetBounds(gfx::Rect(0, 0, 1920, 1080));
  info.set_device_scale_factor(1.25);

  std::vector<DisplayMode> display_modes =
      DisplayChangeObserver::GetInternalDisplayModeList(info, output);
  ASSERT_EQ(5u, display_modes.size());
  EXPECT_EQ("1920x1080", display_modes[0].size.ToString());
  EXPECT_FALSE(display_modes[0].native);
  EXPECT_NEAR(display_modes[0].ui_scale, 0.5, 0.01);
  EXPECT_EQ(display_modes[0].refresh_rate, 60);

  EXPECT_EQ("1920x1080", display_modes[1].size.ToString());
  EXPECT_FALSE(display_modes[1].native);
  EXPECT_NEAR(display_modes[1].ui_scale, 0.625, 0.01);
  EXPECT_EQ(display_modes[1].refresh_rate, 60);

  EXPECT_EQ("1920x1080", display_modes[2].size.ToString());
  EXPECT_FALSE(display_modes[2].native);
  EXPECT_NEAR(display_modes[2].ui_scale, 0.8, 0.01);
  EXPECT_EQ(display_modes[2].refresh_rate, 60);

  EXPECT_EQ("1920x1080", display_modes[3].size.ToString());
  EXPECT_FALSE(display_modes[3].native);
  EXPECT_NEAR(display_modes[3].ui_scale, 1.0, 0.01);
  EXPECT_EQ(display_modes[3].refresh_rate, 60);

  EXPECT_EQ("1920x1080", display_modes[4].size.ToString());
  EXPECT_TRUE(display_modes[4].native);
  EXPECT_NEAR(display_modes[4].ui_scale, 1.25, 0.01);
  EXPECT_EQ(display_modes[4].refresh_rate, 60);
}

TEST_F(DisplayChangeObserverTest, GetExternalDisplayModeList4K) {
  ScopedVector<const ui::DisplayMode> modes;
  modes.push_back(new ui::DisplayMode(gfx::Size(3840, 2160), false, 30));
  modes.push_back(new ui::DisplayMode(gfx::Size(1920, 1200), false, 60));

  // All non-interlaced (as would be seen with different refresh rates).
  modes.push_back(new ui::DisplayMode(gfx::Size(1920, 1080), false, 80));
  modes.push_back(new ui::DisplayMode(gfx::Size(1920, 1080), false, 70));
  modes.push_back(new ui::DisplayMode(gfx::Size(1920, 1080), false, 60));

  // Interlaced vs non-interlaced.
  modes.push_back(new ui::DisplayMode(gfx::Size(1280, 720), true, 60));
  modes.push_back(new ui::DisplayMode(gfx::Size(1280, 720), false, 60));

  // Interlaced only.
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 768), true, 70));
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 768), true, 60));

  // Mixed.
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 600), true, 60));
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 600), false, 70));
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 600), false, 60));

  // Just one interlaced mode.
  modes.push_back(new ui::DisplayMode(gfx::Size(640, 480), true, 60));

  ui::TestDisplaySnapshot display_snapshot;
  display_snapshot.set_modes(modes.get());
  display_snapshot.set_native_mode(modes[0]);
  DisplayConfigurator::DisplayState output;
  output.display = &display_snapshot;

  std::vector<DisplayMode> display_modes =
      DisplayChangeObserver::GetExternalDisplayModeList(output);
  ASSERT_EQ(9u, display_modes.size());
  EXPECT_EQ("640x480", display_modes[0].size.ToString());
  EXPECT_TRUE(display_modes[0].interlaced);
  EXPECT_EQ(display_modes[0].refresh_rate, 60);

  EXPECT_EQ("1024x600", display_modes[1].size.ToString());
  EXPECT_FALSE(display_modes[1].interlaced);
  EXPECT_EQ(display_modes[1].refresh_rate, 70);

  EXPECT_EQ("1024x768", display_modes[2].size.ToString());
  EXPECT_TRUE(display_modes[2].interlaced);
  EXPECT_EQ(display_modes[2].refresh_rate, 70);

  EXPECT_EQ("1280x720", display_modes[3].size.ToString());
  EXPECT_FALSE(display_modes[3].interlaced);
  EXPECT_EQ(display_modes[3].refresh_rate, 60);

  EXPECT_EQ("1920x1080", display_modes[4].size.ToString());
  EXPECT_FALSE(display_modes[4].interlaced);
  EXPECT_EQ(display_modes[4].refresh_rate, 80);

  EXPECT_EQ("3840x2160", display_modes[5].size.ToString());
  EXPECT_FALSE(display_modes[5].interlaced);
  EXPECT_FALSE(display_modes[5].native);
  EXPECT_EQ(display_modes[5].refresh_rate, 30);
  EXPECT_EQ(display_modes[5].device_scale_factor, 2.0);

  EXPECT_EQ("1920x1200", display_modes[6].size.ToString());
  EXPECT_FALSE(display_modes[6].interlaced);
  EXPECT_EQ(display_modes[6].refresh_rate, 60);

  EXPECT_EQ("3840x2160", display_modes[7].size.ToString());
  EXPECT_FALSE(display_modes[7].interlaced);
  EXPECT_FALSE(display_modes[7].native);
  EXPECT_EQ(display_modes[7].refresh_rate, 30);
  EXPECT_EQ(display_modes[7].device_scale_factor, 1.25);

  EXPECT_EQ("3840x2160", display_modes[8].size.ToString());
  EXPECT_FALSE(display_modes[8].interlaced);
  EXPECT_TRUE(display_modes[8].native);
  EXPECT_EQ(display_modes[8].refresh_rate, 30);

  // Outputs without any modes shouldn't cause a crash.
  modes.clear();
  display_snapshot.set_modes(modes.get());

  display_modes = DisplayChangeObserver::GetExternalDisplayModeList(output);
  EXPECT_EQ(0u, display_modes.size());
}

TEST_F(DisplayChangeObserverTest, FindDeviceScaleFactor) {
  // 19.5" 1600x900
  EXPECT_EQ(1.0f, DisplayChangeObserver::FindDeviceScaleFactor(94.14f));

  // 21.5" 1920x1080
  EXPECT_EQ(1.0f, DisplayChangeObserver::FindDeviceScaleFactor(102.46f));

  // 12.1" 1280x800
  EXPECT_EQ(1.0f, DisplayChangeObserver::FindDeviceScaleFactor(124.75f));

  // 13.3" 1920x1080
  EXPECT_EQ(1.25f, DisplayChangeObserver::FindDeviceScaleFactor(157.35f));

  // 14" 1920x1080
  EXPECT_EQ(1.25f, DisplayChangeObserver::FindDeviceScaleFactor(165.63f));

  // 12.85" 2560x1700
  EXPECT_EQ(2.0f, DisplayChangeObserver::FindDeviceScaleFactor(239.15f));

  // Erroneous values should still work.
  EXPECT_EQ(1.0f, DisplayChangeObserver::FindDeviceScaleFactor(-100.0f));
  EXPECT_EQ(1.0f, DisplayChangeObserver::FindDeviceScaleFactor(0.0f));
  EXPECT_EQ(2.0f, DisplayChangeObserver::FindDeviceScaleFactor(10000.0f));
}

}  // namespace ash
