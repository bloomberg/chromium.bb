// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/oobe_display_chooser.h"

#include <memory>

#include "ash/display/display_configuration_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"

namespace chromeos {

namespace {

class OobeDisplayChooserTest : public ash::test::AshTestBase {
 public:
  OobeDisplayChooserTest() : ash::test::AshTestBase() {}

  void SetUp() override {
    ash::test::AshTestBase::SetUp();
    display_manager_test_api_.reset(
        new display::test::DisplayManagerTestApi(display_manager()));
  }

  void EnableTouch(int64_t id) {
    display_manager_test_api_->SetTouchSupport(
        id, display::Display::TouchSupport::TOUCH_SUPPORT_AVAILABLE);
  }

  void DisableTouch(int64_t id) {
    display_manager_test_api_->SetTouchSupport(
        id, display::Display::TouchSupport::TOUCH_SUPPORT_UNAVAILABLE);
  }

  int64_t GetPrimaryDisplay() {
    return display::Screen::GetScreen()->GetPrimaryDisplay().id();
  }

 private:
  std::unique_ptr<display::test::DisplayManagerTestApi>
      display_manager_test_api_;

  DISALLOW_COPY_AND_ASSIGN(OobeDisplayChooserTest);
};

}  // namespace

TEST_F(OobeDisplayChooserTest, PreferTouchAsPrimary) {
  OobeDisplayChooser display_chooser;

  UpdateDisplay("3000x2000,800x600");
  display::DisplayIdList ids = display_manager()->GetCurrentDisplayIdList();
  DisableTouch(ids[0]);
  EnableTouch(ids[1]);

  EXPECT_EQ(ids[0], GetPrimaryDisplay());
  display_chooser.TryToPlaceUiOnTouchDisplay();

  EXPECT_EQ(ids[1], GetPrimaryDisplay());
}

TEST_F(OobeDisplayChooserTest, AddingSecondTouchDisplayShouldbeNOP) {
  OobeDisplayChooser display_chooser;

  UpdateDisplay("3000x2000,800x600");
  display::DisplayIdList ids = display_manager()->GetCurrentDisplayIdList();
  EnableTouch(ids[0]);
  EnableTouch(ids[1]);

  EXPECT_EQ(ids[0], GetPrimaryDisplay());
  display_chooser.TryToPlaceUiOnTouchDisplay();

  EXPECT_EQ(ids[0], GetPrimaryDisplay());
}

}  // namespace chromeos
