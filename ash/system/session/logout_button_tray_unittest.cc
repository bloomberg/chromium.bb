// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session/logout_button_tray.h"

#include <memory>

#include "ash/kiosk_next/kiosk_next_shell_test_util.h"
#include "ash/kiosk_next/mock_kiosk_next_shell_client.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "base/macros.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/md_text_button.h"

namespace ash {
namespace {

constexpr char kUserEmail[] = "user1@test.com";

class LogoutButtonTrayTest : public NoSessionAshTestBase {
 public:
  LogoutButtonTrayTest() = default;
  ~LogoutButtonTrayTest() override = default;

  // NoSessionAshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    SimulateUserLogin(kUserEmail);
  }

  LogoutButtonTray* logout_button_tray() {
    return Shell::GetPrimaryRootWindowController()
        ->GetStatusAreaWidget()
        ->logout_button_tray_for_testing();
  }

  PrefService* pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUserEmail));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LogoutButtonTrayTest);
};

TEST_F(LogoutButtonTrayTest, Visibility) {
  // Button is not visible before login.
  LogoutButtonTray* button = logout_button_tray();

  ASSERT_TRUE(button);
  EXPECT_FALSE(button->GetVisible());

  // Button is not visible after simulated login.
  EXPECT_FALSE(button->GetVisible());

  // Setting the pref makes the button visible.
  pref_service()->SetBoolean(prefs::kShowLogoutButtonInTray, true);
  EXPECT_TRUE(button->GetVisible());

  // Locking the screen hides the button.
  GetSessionControllerClient()->LockScreen();
  EXPECT_FALSE(button->GetVisible());

  // Unlocking the screen shows the button.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(button->GetVisible());

  // Resetting the pref hides the button.
  pref_service()->SetBoolean(prefs::kShowLogoutButtonInTray, false);
  EXPECT_FALSE(button->GetVisible());
}

TEST_F(LogoutButtonTrayTest, ButtonPressed) {
  constexpr char kUserEmail[] = "user1@test.com";
  constexpr char kUserAction[] = "DemoMode.ExitFromShelf";

  LogoutButtonTray* const tray = logout_button_tray();

  ASSERT_TRUE(tray);
  views::MdTextButton* const button = tray->button_for_test();
  TestSessionControllerClient* const session_client =
      GetSessionControllerClient();
  base::UserActionTester user_action_tester;
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  PrefService* const pref_service =
      Shell::Get()->session_controller()->GetUserPrefServiceForUser(
          AccountId::FromUserEmail(kUserEmail));

  SimulateUserLogin(kUserEmail);
  EXPECT_EQ(0, session_client->request_sign_out_count());
  EXPECT_EQ(0, user_action_tester.GetActionCount(kUserAction));
  EXPECT_EQ(0, Shell::Get()
                   ->logout_confirmation_controller()
                   ->confirm_logout_count_for_test());

  // Sign out immediately when duration is zero.
  pref_service->SetInteger(prefs::kLogoutDialogDurationMs, 0);
  tray->ButtonPressed(button, event);
  session_client->FlushForTest();
  EXPECT_EQ(1, session_client->request_sign_out_count());
  EXPECT_EQ(0, user_action_tester.GetActionCount(kUserAction));
  EXPECT_EQ(0, Shell::Get()
                   ->logout_confirmation_controller()
                   ->confirm_logout_count_for_test());

  // Call |LogoutConfirmationController::ConfirmLogout| when duration is
  // non-zero.
  pref_service->SetInteger(prefs::kLogoutDialogDurationMs, 1000);
  tray->ButtonPressed(button, event);
  session_client->FlushForTest();
  EXPECT_EQ(1, session_client->request_sign_out_count());
  EXPECT_EQ(0, user_action_tester.GetActionCount(kUserAction));
  EXPECT_EQ(1, Shell::Get()
                   ->logout_confirmation_controller()
                   ->confirm_logout_count_for_test());

  // Sign out immediately and record user action when duration is zero and it is
  // demo session.
  pref_service->SetInteger(prefs::kLogoutDialogDurationMs, 0);
  session_client->SetIsDemoSession();
  tray->ButtonPressed(button, event);
  session_client->FlushForTest();
  EXPECT_EQ(2, session_client->request_sign_out_count());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kUserAction));
  EXPECT_EQ(1, Shell::Get()
                   ->logout_confirmation_controller()
                   ->confirm_logout_count_for_test());
}

class KioskNextLogoutButtonTrayTest : public LogoutButtonTrayTest {
 public:
  KioskNextLogoutButtonTrayTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kKioskNextShell);
  }

  void SetUp() override {
    LogoutButtonTrayTest::SetUp();
    client_ = BindMockKioskNextShellClient();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockKioskNextShellClient> client_;

  DISALLOW_COPY_AND_ASSIGN(KioskNextLogoutButtonTrayTest);
};

TEST_F(KioskNextLogoutButtonTrayTest, LogoutButtonHidden) {
  ClearLogin();
  LogInKioskNextUser(GetSessionControllerClient());
  pref_service()->SetBoolean(prefs::kShowLogoutButtonInTray, true);
  EXPECT_FALSE(logout_button_tray()->GetVisible());
}

}  // namespace
}  // namespace ash
