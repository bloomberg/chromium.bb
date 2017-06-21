// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/mock_lock_screen_client.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/focus/focus_manager.h"

using ::testing::_;
using LockScreenSanityTest = ash::LoginTestBase;

namespace ash {

// Verifies that the password input box has focus.
TEST_F(LockScreenSanityTest, PasswordIsInitiallyFocused) {
  // Build lock screen.
  auto* contents = new LockContentsView(data_dispatcher());

  // The lock screen requires at least one user.
  SetUserCount(1);

  ShowWidgetWithContent(contents);

  // Textfield should have focus.
  EXPECT_EQ(MakeLoginPasswordTestApi(contents).textfield(),
            contents->GetFocusManager()->GetFocusedView());
}

// Verifies submitting the password invokes mojo lock screen client.
TEST_F(LockScreenSanityTest, PasswordSubmitCallsLockScreenClient) {
  // TODO: Renable in mash once crbug.com/725257 is fixed.
  if (Shell::GetAshConfig() == Config::MASH)
    return;

  // Build lock screen.
  auto* contents = new LockContentsView(data_dispatcher());

  // The lock screen requires at least one user.
  SetUserCount(1);

  ShowWidgetWithContent(contents);

  // Password submit runs mojo.
  std::unique_ptr<MockLockScreenClient> client = BindMockLockScreenClient();
  client->set_authenticate_user_callback_result(false);
  EXPECT_CALL(*client, AuthenticateUser_(users()[0]->account_id, _, false, _));
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::KeyboardCode::VKEY_A, 0);
  generator.PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash