// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/tray_night_light.h"

#include "ash/shell.h"
#include "ash/system/night_light/night_light_controller.h"
#include "ash/system/tray/system_tray.h"
#include "ash/test/ash_test_base.h"

namespace ash {

namespace {

using TrayNightLightTest = test::AshTestBase;

// Tests that when NightLight is active, its tray icon in the System Tray is
// visible.
TEST_F(TrayNightLightTest, TestNightLightTrayVisibility) {
  SystemTray* tray = GetPrimarySystemTray();
  TrayNightLight* tray_night_light = tray->tray_night_light();
  NightLightController* controller = Shell::Get()->night_light_controller();
  controller->SetEnabled(false);
  ASSERT_FALSE(controller->enabled());
  EXPECT_FALSE(tray_night_light->tray_view()->visible());
  controller->SetEnabled(true);
  EXPECT_TRUE(tray_night_light->tray_view()->visible());
}

}  // namespace

}  // namespace ash
