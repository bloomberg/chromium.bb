// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tiles/tray_tiles.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/tiles/tiles_default_view.h"
#include "ash/common/wm_shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/view.h"

namespace ash {

class TrayTilesTest : public test::AshTestBase {
 public:
  TrayTilesTest() {}
  ~TrayTilesTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();
    tray_tiles_.reset(new TrayTiles(GetPrimarySystemTray()));
  }

  void TearDown() override {
    tray_tiles_.reset();
    test::AshTestBase::TearDown();
  }

  views::CustomButton* GetSettingsButton() {
    return tray_tiles()->default_view_->settings_button_;
  }

  views::CustomButton* GetHelpButton() {
    return tray_tiles()->default_view_->help_button_;
  }

  views::CustomButton* GetLockButton() {
    return tray_tiles()->default_view_->lock_button_;
  }

  views::CustomButton* GetPowerButton() {
    return tray_tiles()->default_view_->power_button_;
  }

  TrayTiles* tray_tiles() { return tray_tiles_.get(); }

 private:
  std::unique_ptr<TrayTiles> tray_tiles_;

  DISALLOW_COPY_AND_ASSIGN(TrayTilesTest);
};

TEST_F(TrayTilesTest, ButtonStatesWithAddingUser) {
  SetUserAddingScreenRunning(true);
  std::unique_ptr<views::View> default_view(
      tray_tiles()->CreateDefaultViewForTesting(LoginStatus::USER));
  EXPECT_EQ(GetSettingsButton()->state(), views::Button::STATE_DISABLED);
  EXPECT_EQ(GetHelpButton()->state(), views::Button::STATE_DISABLED);
  EXPECT_EQ(GetLockButton()->state(), views::Button::STATE_DISABLED);
  EXPECT_EQ(GetPowerButton()->state(), views::Button::STATE_NORMAL);
}

TEST_F(TrayTilesTest, ButtonStatesWithLoginStatusNotLoggedIn) {
  std::unique_ptr<views::View> default_view(
      tray_tiles()->CreateDefaultViewForTesting(LoginStatus::NOT_LOGGED_IN));
  EXPECT_EQ(GetSettingsButton()->state(), views::Button::STATE_DISABLED);
  EXPECT_EQ(GetHelpButton()->state(), views::Button::STATE_DISABLED);
  EXPECT_EQ(GetLockButton()->state(), views::Button::STATE_DISABLED);
  EXPECT_EQ(GetPowerButton()->state(), views::Button::STATE_NORMAL);
}

TEST_F(TrayTilesTest, ButtonStatesWithLoginStatusLocked) {
  std::unique_ptr<views::View> default_view(
      tray_tiles()->CreateDefaultViewForTesting(LoginStatus::LOCKED));
  EXPECT_EQ(GetSettingsButton()->state(), views::Button::STATE_DISABLED);
  EXPECT_EQ(GetHelpButton()->state(), views::Button::STATE_DISABLED);
  EXPECT_EQ(GetLockButton()->state(), views::Button::STATE_DISABLED);
  EXPECT_EQ(GetPowerButton()->state(), views::Button::STATE_NORMAL);
}

TEST_F(TrayTilesTest, ButtonStatesWithLoginStatusUser) {
  std::unique_ptr<views::View> default_view(
      tray_tiles()->CreateDefaultViewForTesting(LoginStatus::USER));
  EXPECT_EQ(GetSettingsButton()->state(), views::Button::STATE_NORMAL);
  EXPECT_EQ(GetHelpButton()->state(), views::Button::STATE_NORMAL);
  EXPECT_EQ(GetLockButton()->state(), views::Button::STATE_NORMAL);
  EXPECT_EQ(GetPowerButton()->state(), views::Button::STATE_NORMAL);
}

}  // namespace ash
