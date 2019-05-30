// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"

#include <memory>

#include "ash/keyboard/ui/keyboard_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/kiosk_next/kiosk_next_shell_test_util.h"
#include "ash/kiosk_next/mock_kiosk_next_shell_client.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"

namespace ash {

class VirtualKeyboardTrayTest : public AshTestBase {
 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    ASSERT_TRUE(keyboard::IsKeyboardEnabled());
    keyboard::test::WaitUntilLoaded();

    // These tests only apply to the floating virtual keyboard, as it is the
    // only case where both the virtual keyboard and the shelf are visible.
    keyboard_controller()->SetContainerType(
        keyboard::mojom::ContainerType::kFloating, base::nullopt,
        base::DoNothing());
  }

  keyboard::KeyboardController* keyboard_controller() {
    return keyboard::KeyboardController::Get();
  }
};

// Tests that the tray action toggles the virtual keyboard.
TEST_F(VirtualKeyboardTrayTest, PerformActionTogglesVirtualKeyboard) {
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  VirtualKeyboardTray* tray = status->virtual_keyboard_tray_for_testing();
  tray->SetVisible(true);
  ASSERT_TRUE(tray->GetVisible());

  // First tap should show the virtual keyboard.
  tray->PerformAction(ui::GestureEvent(
      0, 0, 0, base::TimeTicks(), ui::GestureEventDetails(ui::ET_GESTURE_TAP)));
  EXPECT_TRUE(tray->is_active());
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Second tap should hide the virtual keyboard.
  tray->PerformAction(ui::GestureEvent(
      0, 0, 0, base::TimeTicks(), ui::GestureEventDetails(ui::ET_GESTURE_TAP)));
  EXPECT_FALSE(tray->is_active());
  ASSERT_TRUE(keyboard::WaitUntilHidden());
}

class KioskNextVirtualKeyboardTest : public AshTestBase {
 public:
  KioskNextVirtualKeyboardTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kKioskNextShell);
  }

  void SetUp() override {
    set_start_session(false);
    AshTestBase::SetUp();
    client_ = BindMockKioskNextShellClient();
  }

  void EnableVirtualKeyboardForActiveUser() {
    PrefService* pref_service =
        Shell::Get()->session_controller()->GetActivePrefService();

    pref_service->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled, true);
  }

  VirtualKeyboardTray* tray() {
    StatusAreaWidget* status =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget();
    return status->virtual_keyboard_tray_for_testing();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockKioskNextShellClient> client_;

  DISALLOW_COPY_AND_ASSIGN(KioskNextVirtualKeyboardTest);
};

TEST_F(KioskNextVirtualKeyboardTest, VirtualKeyboardTrayHidden) {
  LogInKioskNextUser(GetSessionControllerClient());
  EnableVirtualKeyboardForActiveUser();
  EXPECT_FALSE(tray()->GetVisible());
}

}  // namespace ash
