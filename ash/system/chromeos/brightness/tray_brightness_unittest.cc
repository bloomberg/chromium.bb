// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/brightness/tray_brightness.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/test/ash_test_base.h"
#include "ui/views/view.h"

namespace ash {
namespace {

user::LoginStatus GetUserLoginStatus() {
  return Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus();
}

}  // namespace

class TrayBrightnessTest : public test::AshTestBase {
 public:
  TrayBrightnessTest() {}
  virtual ~TrayBrightnessTest() {}

 protected:
  views::View* CreateDefaultView() {
    TrayBrightness tray(NULL);
    return tray.CreateDefaultView(GetUserLoginStatus());
  }

  views::View* CreateDetailedView() {
    TrayBrightness tray(NULL);
    return tray.CreateDetailedView(GetUserLoginStatus());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayBrightnessTest);
};

// Tests that when the default view is initially created, that its
// BrightnessView is created not visible.
TEST_F(TrayBrightnessTest, CreateDefaultView) {
  scoped_ptr<views::View> tray(CreateDefaultView());
  EXPECT_FALSE(tray->visible());
}

// Tests the construction of the default view while MaximizeMode is active.
// The BrightnessView should be visible.
TEST_F(TrayBrightnessTest, CreateDefaultViewDuringMaximizeMode) {
  Shell::GetInstance()->EnableMaximizeModeWindowManager(true);
  scoped_ptr<views::View> tray(CreateDefaultView());
  EXPECT_TRUE(tray->visible());
  Shell::GetInstance()->EnableMaximizeModeWindowManager(false);
}

// Tests that the enabling of MaximizeMode affects a previously created
// BrightnessView, changing the visibility.
TEST_F(TrayBrightnessTest, DefaultViewVisibilityChangesDuringMaximizeMode) {
  scoped_ptr<views::View> tray(CreateDefaultView());
  Shell::GetInstance()->EnableMaximizeModeWindowManager(true);
  EXPECT_TRUE(tray->visible());
  Shell::GetInstance()->EnableMaximizeModeWindowManager(false);
  EXPECT_FALSE(tray->visible());
}

// Tests that when the detailed view is initially created that its
// BrightnessView is created as visible.
TEST_F(TrayBrightnessTest, CreateDetailedView) {
  scoped_ptr<views::View> tray(CreateDetailedView());
  EXPECT_TRUE(tray->visible());
}


// Tests that when the detailed view is created during MaximizeMode that its
// BrightnessView is visible.
TEST_F(TrayBrightnessTest, CreateDetailedViewDuringMaximizeMode) {
  Shell::GetInstance()->EnableMaximizeModeWindowManager(true);
  scoped_ptr<views::View> tray(CreateDetailedView());
  EXPECT_TRUE(tray->visible());
  Shell::GetInstance()->EnableMaximizeModeWindowManager(false);
}

// Tests that the enabling of MaximizeMode has no affect on the visibility of a
// previously created BrightnessView that belongs to a detailed view.
TEST_F(TrayBrightnessTest, DetailedViewVisibilityChangesDuringMaximizeMode) {
  scoped_ptr<views::View> tray(CreateDetailedView());
  Shell::GetInstance()->EnableMaximizeModeWindowManager(true);
  EXPECT_TRUE(tray->visible());
  Shell::GetInstance()->EnableMaximizeModeWindowManager(false);
  EXPECT_TRUE(tray->visible());
}

}  // namespace ash

