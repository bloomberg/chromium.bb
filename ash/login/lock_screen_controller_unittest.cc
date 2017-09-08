// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/lock_screen_controller.h"

#include "ash/login/mock_lock_screen_client.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "components/prefs/pref_service.h"

using ::testing::_;

namespace ash {

namespace {
using LockScreenControllerTest = AshTestBase;

TEST_F(LockScreenControllerTest, RequestAuthentication) {
  LockScreenController* controller = Shell::Get()->lock_screen_controller();
  std::unique_ptr<MockLockScreenClient> client = BindMockLockScreenClient();

  AccountId id = AccountId::FromUserEmail("user1@test.com");

  // We hardcode the hashed password. This is fine because the password hash
  // algorithm should never accidentally change; if it does we will need to
  // have cryptohome migration code and one failing test isn't a problem.
  std::string password = "password";
  std::string hashed_password = "40c7b00f3bccc7675ec5b732de4bfbe4";
  EXPECT_NE(password, hashed_password);

  // Verify AuthenticateUser mojo call is run with the same account id, a
  // (hashed) password, and the correct PIN state.
  EXPECT_CALL(*client, AuthenticateUser_(id, hashed_password, false, _));
  base::Optional<bool> callback_result;
  controller->AuthenticateUser(
      id, password, false,
      base::BindOnce([](base::Optional<bool>* result,
                        bool did_auth) { *result = did_auth; },
                     &callback_result));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_result.has_value());
  EXPECT_TRUE(*callback_result);

  // Verify that pin is hashed correctly.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  EXPECT_TRUE(prefs->FindPreference(prefs::kQuickUnlockPinSalt));

  std::string pin = "123456";
  std::string hashed_pin = "cqgMB9rwrcE35iFxm+4vP2toO6qkzW+giCnCcEou92Y=";
  EXPECT_NE(pin, hashed_pin);

  EXPECT_CALL(*client, AuthenticateUser_(id, hashed_pin, true, _));
  controller->AuthenticateUser(
      id, pin, true,
      base::BindOnce([](base::Optional<bool>* result,
                        bool did_auth) { *result = did_auth; },
                     &callback_result));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_result.has_value());
  EXPECT_TRUE(*callback_result);
}

TEST_F(LockScreenControllerTest, RequestEasyUnlock) {
  LockScreenController* controller = Shell::Get()->lock_screen_controller();
  std::unique_ptr<MockLockScreenClient> client = BindMockLockScreenClient();

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

TEST_F(LockScreenControllerTest, RequestUserPodFocus) {
  LockScreenController* controller = Shell::Get()->lock_screen_controller();
  std::unique_ptr<MockLockScreenClient> client = BindMockLockScreenClient();

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

}  // namespace
}  // namespace ash
