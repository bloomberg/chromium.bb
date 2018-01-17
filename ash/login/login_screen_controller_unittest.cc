// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/login_screen_controller.h"

#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"

using ::testing::_;
using namespace session_manager;

namespace ash {

namespace {
using LoginScreenControllerTest = AshTestBase;

void HideSystemTray() {
  Shell::GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->SetSystemTrayVisibility(false);
}

bool IsPrimarySystemTrayVisible() {
  return Shell::GetPrimaryRootWindowController()->GetSystemTray()->visible();
}

TEST_F(LoginScreenControllerTest, RequestAuthentication) {
  LoginScreenController* controller = Shell::Get()->login_screen_controller();
  std::unique_ptr<MockLoginScreenClient> client = BindMockLoginScreenClient();

  AccountId id = AccountId::FromUserEmail("user1@test.com");

  // We hardcode the hashed password. This is fine because the password hash
  // algorithm should never accidentally change; if it does we will need to
  // have cryptohome migration code and one failing test isn't a problem.
  std::string password = "password";
  std::string hashed_password = "40c7b00f3bccc7675ec5b732de4bfbe4";
  EXPECT_NE(password, hashed_password);

  // Verify AuthenticateUser mojo call is run with the same account id, a
  // (hashed) password, and the correct PIN state.
  EXPECT_CALL(*client, AuthenticateUser_(id, hashed_password, _, false, _));
  base::Optional<bool> callback_result;
  base::RunLoop run_loop1;
  controller->AuthenticateUser(
      id, password, false,
      base::BindOnce(
          [](base::Optional<bool>* result, base::RunLoop* run_loop1,
             base::Optional<bool> did_auth) {
            *result = *did_auth;
            run_loop1->Quit();
          },
          &callback_result, &run_loop1));
  run_loop1.Run();

  EXPECT_TRUE(callback_result.has_value());
  EXPECT_TRUE(*callback_result);

  // Verify that pin is hashed correctly.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  EXPECT_TRUE(prefs->FindPreference(prefs::kQuickUnlockPinSalt));

  // We hardcode the hashed PIN. This is fine because the PIN hash algorithm
  // should never accidentally change; if it does we will need to have migration
  // code and one failing test isn't a problem.
  std::string pin = "123456";
  std::string hashed_pin = "cqgMB9rwrcE35iFxm+4vP2toO6qkzW+giCnCcEou92Y=";
  EXPECT_NE(pin, hashed_pin);

  base::RunLoop run_loop2;
  EXPECT_CALL(*client, AuthenticateUser_(id, hashed_pin, _, true, _));
  controller->AuthenticateUser(
      id, pin, true,
      base::BindOnce(
          [](base::Optional<bool>* result, base::RunLoop* run_loop2,
             base::Optional<bool> did_auth) {
            *result = *did_auth;
            run_loop2->Quit();
          },
          &callback_result, &run_loop2));
  run_loop2.Run();

  EXPECT_TRUE(callback_result.has_value());
  EXPECT_TRUE(*callback_result);
}

TEST_F(LoginScreenControllerTest, RequestEasyUnlock) {
  LoginScreenController* controller = Shell::Get()->login_screen_controller();
  std::unique_ptr<MockLoginScreenClient> client = BindMockLoginScreenClient();

  AccountId id = AccountId::FromUserEmail("user1@test.com");

  // Verify AttemptUnlock mojo call is run with the same account id.
  EXPECT_CALL(*client, AttemptUnlock(id));
  controller->AttemptUnlock(id);
  base::RunLoop().RunUntilIdle();

  // Verify HardlockPod mojo call is run with the same account id.
  EXPECT_CALL(*client, HardlockPod(id));
  controller->HardlockPod(id);
  base::RunLoop().RunUntilIdle();

  // Verify RecordClickOnLockIcon mojo call is run with the same account id.
  EXPECT_CALL(*client, RecordClickOnLockIcon(id));
  controller->RecordClickOnLockIcon(id);
  base::RunLoop().RunUntilIdle();
}

TEST_F(LoginScreenControllerTest, RequestUserPodFocus) {
  LoginScreenController* controller = Shell::Get()->login_screen_controller();
  std::unique_ptr<MockLoginScreenClient> client = BindMockLoginScreenClient();

  AccountId id = AccountId::FromUserEmail("user1@test.com");

  // Verify FocusPod mojo call is run with the same account id.
  EXPECT_CALL(*client, OnFocusPod(id));
  controller->OnFocusPod(id);
  base::RunLoop().RunUntilIdle();

  // Verify NoPodFocused mojo call is run.
  EXPECT_CALL(*client, OnNoPodFocused());
  controller->OnNoPodFocused();
  base::RunLoop().RunUntilIdle();
}

TEST_F(LoginScreenControllerTest,
       ShowLoginScreenRequiresLoginPrimarySessionState) {
  auto show_login = [&](session_manager::SessionState state) {
    EXPECT_FALSE(ash::LockScreen::IsShown());

    LoginScreenController* controller = Shell::Get()->login_screen_controller();

    GetSessionControllerClient()->SetSessionState(state);
    base::Optional<bool> result;
    base::RunLoop run_loop;
    controller->ShowLoginScreen(base::BindOnce(
        [](base::Optional<bool>* result, base::RunLoop* run_loop,
           bool did_show) {
          *result = did_show;
          run_loop->Quit();
        },
        &result, &run_loop));
    run_loop.Run();

    EXPECT_TRUE(result.has_value());

    // Verify result matches actual ash::LockScreen state.
    EXPECT_EQ(*result, ash::LockScreen::IsShown());

    // Destroy login if we created it.
    if (*result)
      ash::LockScreen::Get()->Destroy();

    return *result;
  };

  EXPECT_FALSE(show_login(session_manager::SessionState::UNKNOWN));
  EXPECT_FALSE(show_login(session_manager::SessionState::OOBE));
  EXPECT_TRUE(show_login(session_manager::SessionState::LOGIN_PRIMARY));
  EXPECT_FALSE(show_login(session_manager::SessionState::LOGGED_IN_NOT_ACTIVE));
  EXPECT_FALSE(show_login(session_manager::SessionState::ACTIVE));
  EXPECT_FALSE(show_login(session_manager::SessionState::LOCKED));
  EXPECT_FALSE(show_login(session_manager::SessionState::LOGIN_SECONDARY));
}

TEST_F(LoginScreenControllerTest, ShowSystemTrayWhenLoginScreenShown) {
  // Hide system tray to make sure it is shown later.
  GetSessionControllerClient()->SetSessionState(SessionState::UNKNOWN);
  HideSystemTray();
  EXPECT_FALSE(ash::LockScreen::IsShown());
  EXPECT_FALSE(IsPrimarySystemTrayVisible());

  // Show login screen.
  GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);
  base::Optional<bool> result;
  base::RunLoop run_loop;
  Shell::Get()->login_screen_controller()->ShowLoginScreen(base::BindOnce(
      [](base::Optional<bool>* result, base::RunLoop* run_loop, bool did_show) {
        *result = did_show;
        run_loop->Quit();
      },
      &result, &run_loop));
  run_loop.Run();
  EXPECT_TRUE(result.has_value());

  EXPECT_TRUE(ash::LockScreen::IsShown());
  EXPECT_TRUE(IsPrimarySystemTrayVisible());

  if (*result)
    ash::LockScreen::Get()->Destroy();
}

TEST_F(LoginScreenControllerTest, ShowSystemTrayWhenLockScreenShown) {
  // Hide system tray to make sure it is shown later.
  GetSessionControllerClient()->SetSessionState(SessionState::ACTIVE);
  HideSystemTray();
  EXPECT_FALSE(ash::LockScreen::IsShown());
  EXPECT_FALSE(IsPrimarySystemTrayVisible());

  // Show lock screen.
  GetSessionControllerClient()->SetSessionState(SessionState::LOCKED);
  base::Optional<bool> result;
  base::RunLoop run_loop;
  Shell::Get()->login_screen_controller()->ShowLockScreen(base::BindOnce(
      [](base::Optional<bool>* result, base::RunLoop* run_loop, bool did_show) {
        *result = did_show;
        run_loop->Quit();
      },
      &result, &run_loop));
  run_loop.Run();
  EXPECT_TRUE(result.has_value());

  EXPECT_TRUE(ash::LockScreen::IsShown());
  EXPECT_TRUE(IsPrimarySystemTrayVisible());

  if (*result)
    ash::LockScreen::Get()->Destroy();
}

}  // namespace
}  // namespace ash
