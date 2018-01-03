// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_window.h"

#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/public/interfaces/login_user_info.mojom.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_test_util.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {

class LockWindowVirtualKeyboardTest : public LoginTestBase {
 public:
  LockWindowVirtualKeyboardTest() = default;

  ~LockWindowVirtualKeyboardTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::LOCKED);
    login_controller_ = Shell::Get()->login_screen_controller();
    ASSERT_NE(nullptr, login_controller_);
  }

  void TearDown() override {
    if (ash::LockScreen::IsShown())
      ash::LockScreen::Get()->Destroy();
    AshTestBase::TearDown();
  }

  void ShowLockScreen() {
    base::Optional<bool> result;
    login_controller_->ShowLockScreen(base::BindOnce(
        [](base::Optional<bool>* result, bool did_show) { *result = did_show; },
        &result));
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(*result, true);
  }

  void LoadUser() {
    std::vector<mojom::LoginUserInfoPtr> users;
    users.push_back(CreateUser("user1"));
    login_controller_->LoadUsers(std::move(users), false);
  }

  void ShowKeyboard() {
    keyboard::KeyboardController* controller =
        keyboard::KeyboardController::GetInstance();
    ASSERT_NE(nullptr, controller);

    Shell::GetPrimaryRootWindowController()->ActivateKeyboard(controller);
    controller->ShowKeyboard(false);

    // Set keyboard height to half of the root window - this should overlap with
    // lock/login layout.
    if (controller->ui()->GetContentsWindow()->bounds().height() == 0) {
      int height = Shell::GetPrimaryRootWindow()->bounds().height() / 2;
      controller->ui()->GetContentsWindow()->SetBounds(
          keyboard::KeyboardBoundsFromRootBounds(
              Shell::GetPrimaryRootWindow()->bounds(), height));
    }
    ASSERT_TRUE(controller->keyboard_visible());
  }

  LoginScreenController* login_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LockWindowVirtualKeyboardTest);
};

TEST_F(LockWindowVirtualKeyboardTest, VirtualKeyboardDoesNotCoverAuthView) {
  ASSERT_NO_FATAL_FAILURE(ShowLockScreen());
  LockContentsView* lock_contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();
  ASSERT_NE(nullptr, lock_contents);

  LoadUser();
  LoginAuthUserView* auth_view =
      MakeLockContentsViewTestApi(lock_contents).primary_auth();
  ASSERT_NE(nullptr, auth_view);

  ASSERT_NO_FATAL_FAILURE(ShowKeyboard());
  const gfx::Rect keyboard_bounds_in_screen =
      keyboard::KeyboardController::GetInstance()
          ->ui()
          ->GetContentsWindow()
          ->GetBoundsInScreen();
  EXPECT_FALSE(
      auth_view->GetBoundsInScreen().Intersects(keyboard_bounds_in_screen));
}

}  // namespace ash
