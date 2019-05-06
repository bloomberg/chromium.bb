// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/kiosk_next/kiosk_next_shell_test_util.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

const char kTestUserEmail[] = "primary_user1@test.com";

}  // namespace

void LogInKioskNextUser(
    TestSessionControllerClient* session_controller_client) {
  // Create session for user.
  session_controller_client->AddUserSession(
      kTestUserEmail, user_manager::USER_TYPE_REGULAR,
      true /* enable_settings */, true /* provide_pref_service */);

  // Set the user's KioskNextShell preference.
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetUserPrefServiceForUser(
          AccountId::FromUserEmail(kTestUserEmail));
  pref_service->Set(prefs::kKioskNextShellEnabled, base::Value(true));

  // Start the session after setting the pref.
  session_controller_client->SwitchActiveUser(
      AccountId::FromUserEmail(kTestUserEmail));
  session_controller_client->SetSessionState(
      session_manager::SessionState::ACTIVE);
}

}  // namespace ash
