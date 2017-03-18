// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm_shell.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_configuration_controller_test_api.h"
#include "base/macros.h"
#include "ui/display/manager/display_manager.h"

namespace ash {

class DisplayConfigurationControllerTest : public test::AshTestBase {
 public:
  DisplayConfigurationControllerTest() {}
  ~DisplayConfigurationControllerTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayConfigurationControllerTest);
};

TEST_F(DisplayConfigurationControllerTest, ErasesAnimatorOnAnimationEnded) {
  // TODO(wutao): needs display_configuration_controller
  // http://crbug.com/686839.
  if (WmShell::Get()->IsRunningInMash())
    return;

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  test::DisplayConfigurationControllerTestApi testapi(
      Shell::GetInstance()->display_configuration_controller());
  ScreenRotationAnimator* screen_rotation_animator =
      testapi.GetScreenRotationAnimatorForDisplay(display.id());
  EXPECT_EQ(1, testapi.DisplayScreenRotationAnimatorMapSize());

  Shell::GetInstance()->display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_0,
      display::Display::RotationSource::ROTATION_SOURCE_USER);
  screen_rotation_animator->Rotate(
      display::Display::ROTATE_90,
      display::Display::RotationSource::ROTATION_SOURCE_USER);
  EXPECT_EQ(0, testapi.DisplayScreenRotationAnimatorMapSize());
}

}  // namespace ash
