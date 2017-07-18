// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_configuration_controller.h"

#include "ash/ash_switches.h"
#include "ash/public/cpp/config.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_configuration_controller_test_api.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "ui/display/manager/display_manager.h"

namespace ash {
namespace {

display::Display::Rotation GetDisplayRotation(int64_t display_id) {
  return Shell::Get()
      ->display_manager()
      ->GetDisplayInfo(display_id)
      .GetActiveRotation();
}

class DisplayConfigurationControllerSmoothRotationTest
    : public test::AshTestBase {
 public:
  DisplayConfigurationControllerSmoothRotationTest() {}
  ~DisplayConfigurationControllerSmoothRotationTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAshDisableSmoothScreenRotation, "false");
    test::AshTestBase::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayConfigurationControllerSmoothRotationTest);
};

}  // namespace

using DisplayConfigurationControllerTest = test::AshTestBase;

TEST_F(DisplayConfigurationControllerTest, OnlyHasOneAnimator) {
  // TODO(wutao): needs display_configuration_controller
  // http://crbug.com/686839.
  if (Shell::GetAshConfig() == Config::MASH)
    return;

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  test::DisplayConfigurationControllerTestApi testapi(
      Shell::Get()->display_configuration_controller());
  ScreenRotationAnimator* old_screen_rotation_animator =
      testapi.GetScreenRotationAnimatorForDisplay(display.id());

  Shell::Get()->display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_0,
      display::Display::RotationSource::ROTATION_SOURCE_USER);
  old_screen_rotation_animator->Rotate(
      display::Display::ROTATE_90,
      display::Display::RotationSource::ROTATION_SOURCE_USER);

  ScreenRotationAnimator* new_screen_rotation_animator =
      testapi.GetScreenRotationAnimatorForDisplay(display.id());
  new_screen_rotation_animator->Rotate(
      display::Display::ROTATE_180,
      display::Display::RotationSource::ROTATION_SOURCE_USER);
  EXPECT_EQ(old_screen_rotation_animator, new_screen_rotation_animator);
}

TEST_F(DisplayConfigurationControllerSmoothRotationTest,
       GetTargetRotationWithAnimation) {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  DisplayConfigurationController* controller =
      Shell::Get()->display_configuration_controller();
  test::DisplayConfigurationControllerTestApi testapi(controller);
  controller->SetDisplayRotation(
      display.id(), display::Display::ROTATE_180,
      display::Display::RotationSource::ROTATION_SOURCE_USER);
  EXPECT_EQ(display::Display::ROTATE_180,
            controller->GetTargetRotation(display.id()));
  EXPECT_EQ(display::Display::ROTATE_0, GetDisplayRotation(display.id()));
}

}  // namespace ash
