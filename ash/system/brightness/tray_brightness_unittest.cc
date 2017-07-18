// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/tray_brightness.h"

#include <memory>

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ui/views/view.h"

namespace ash {

class TrayBrightnessTest : public AshTestBase {
 public:
  TrayBrightnessTest() {}
  ~TrayBrightnessTest() override {}

 protected:
  views::View* CreateDefaultView() {
    TrayBrightness tray(NULL);
    // The login status doesn't matter here; supply any random value.
    return tray.CreateDefaultView(LoginStatus::USER);
  }

  views::View* CreateDetailedView() {
    TrayBrightness tray(NULL);
    // The login status doesn't matter here; supply any random value.
    return tray.CreateDetailedView(LoginStatus::USER);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayBrightnessTest);
};

// Tests that when the default view is initially created, that its
// BrightnessView is created not visible.
TEST_F(TrayBrightnessTest, CreateDefaultView) {
  std::unique_ptr<views::View> tray(CreateDefaultView());
  EXPECT_FALSE(tray->visible());
}

// Tests the construction of the default view while MaximizeMode is active.
// The BrightnessView should be visible.
TEST_F(TrayBrightnessTest, CreateDefaultViewDuringMaximizeMode) {
  Shell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      true);
  std::unique_ptr<views::View> tray(CreateDefaultView());
  EXPECT_TRUE(tray->visible());
  Shell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      false);
}

// Tests that the enabling of MaximizeMode affects a previously created
// BrightnessView, changing the visibility.
TEST_F(TrayBrightnessTest, DefaultViewVisibilityChangesDuringMaximizeMode) {
  std::unique_ptr<views::View> tray(CreateDefaultView());
  Shell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      true);
  EXPECT_TRUE(tray->visible());
  Shell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      false);
  EXPECT_FALSE(tray->visible());
}

// Tests that when the detailed view is initially created that its
// BrightnessView is created as visible.
TEST_F(TrayBrightnessTest, CreateDetailedView) {
  std::unique_ptr<views::View> tray(CreateDetailedView());
  EXPECT_TRUE(tray->visible());
}

// Tests that when the detailed view is created during MaximizeMode that its
// BrightnessView is visible.
TEST_F(TrayBrightnessTest, CreateDetailedViewDuringMaximizeMode) {
  Shell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      true);
  std::unique_ptr<views::View> tray(CreateDetailedView());
  EXPECT_TRUE(tray->visible());
  Shell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      false);
}

// Tests that the enabling of MaximizeMode has no affect on the visibility of a
// previously created BrightnessView that belongs to a detailed view.
TEST_F(TrayBrightnessTest, DetailedViewVisibilityChangesDuringMaximizeMode) {
  std::unique_ptr<views::View> tray(CreateDetailedView());
  Shell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      true);
  EXPECT_TRUE(tray->visible());
  Shell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      false);
  EXPECT_TRUE(tray->visible());
}

}  // namespace ash
