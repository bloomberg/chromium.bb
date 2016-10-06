// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tiles/tray_tiles.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/tiles/tiles_default_view.h"
#include "ash/common/wm_shell.h"
#include "ash/test/ash_test_base.h"
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

  TrayTiles* tray_tiles() { return tray_tiles_.get(); }

 private:
  std::unique_ptr<TrayTiles> tray_tiles_;

  DISALLOW_COPY_AND_ASSIGN(TrayTilesTest);
};

// TODO(tdanderson|bruthig): Many of the other rows follow the same rules for
// not being shown if the login status is NOT_LOGGED_IN, LOCKED, or if the
// add-user screen is active. Consider applying the test coverage below to
// such rows.

TEST_F(TrayTilesTest, TilesRowNotCreatedWithAddingUser) {
  SetUserAddingScreenRunning(true);
  std::unique_ptr<views::View> default_view(
      tray_tiles()->CreateDefaultView(LoginStatus::USER));
  EXPECT_FALSE(default_view);
}

TEST_F(TrayTilesTest, TilesRowNotCreatedWithLoginStatusNotLoggedIn) {
  std::unique_ptr<views::View> default_view(
      tray_tiles()->CreateDefaultView(LoginStatus::NOT_LOGGED_IN));
  EXPECT_FALSE(default_view);
}

TEST_F(TrayTilesTest, TilesRowNotCreatedWithLoginStatusLocked) {
  std::unique_ptr<views::View> default_view(
      tray_tiles()->CreateDefaultView(LoginStatus::LOCKED));
  EXPECT_FALSE(default_view);
}

TEST_F(TrayTilesTest, TilesRowCreatedWithLoginStatusUser) {
  std::unique_ptr<views::View> default_view(
      tray_tiles()->CreateDefaultView(LoginStatus::USER));
  EXPECT_TRUE(default_view);
}

}  // namespace ash
