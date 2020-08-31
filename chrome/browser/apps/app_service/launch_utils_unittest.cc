// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"

class LaunchUtilsTest : public testing::Test {
 protected:
  apps::AppLaunchParams CreateLaunchParams(
      apps::mojom::LaunchContainer container,
      WindowOpenDisposition disposition,
      bool preferred_container,
      apps::mojom::LaunchContainer fallback_container =
          apps::mojom::LaunchContainer::kLaunchContainerNone) {
    return apps::CreateAppIdLaunchParamsWithEventFlags(
        app_id,
        apps::GetEventFlags(container, disposition, preferred_container),
        apps::mojom::AppLaunchSource::kSourceChromeInternal,
        display::kInvalidDisplayId, fallback_container);
  }

  std::string app_id = "aaa";
};

TEST_F(LaunchUtilsTest, WindowContainerAndWindowDisposition) {
  auto container = apps::mojom::LaunchContainer::kLaunchContainerWindow;
  auto disposition = WindowOpenDisposition::NEW_WINDOW;
  auto params = CreateLaunchParams(container, disposition, false);

  EXPECT_EQ(container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, TabContainerAndForegoundTabDisposition) {
  auto container = apps::mojom::LaunchContainer::kLaunchContainerTab;
  auto disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  auto params = CreateLaunchParams(container, disposition, false);

  EXPECT_EQ(container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, TabContainerAndBackgoundTabDisposition) {
  auto container = apps::mojom::LaunchContainer::kLaunchContainerTab;
  auto disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  auto params = CreateLaunchParams(container, disposition, false);

  EXPECT_EQ(container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, PreferContainerWithTab) {
  auto container = apps::mojom::LaunchContainer::kLaunchContainerNone;
  auto disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  auto preferred_container =
      apps::mojom::LaunchContainer::kLaunchContainerWindow;
  auto params =
      CreateLaunchParams(container, disposition, true, preferred_container);

  EXPECT_EQ(preferred_container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, PreferContainerWithWindow) {
  auto container = apps::mojom::LaunchContainer::kLaunchContainerNone;
  auto disposition = WindowOpenDisposition::NEW_WINDOW;
  auto preferred_container =
      apps::mojom::LaunchContainer::kLaunchContainerWindow;
  auto params =
      CreateLaunchParams(container, disposition, true, preferred_container);

  EXPECT_EQ(preferred_container, params.container);
  EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB, params.disposition);
}
