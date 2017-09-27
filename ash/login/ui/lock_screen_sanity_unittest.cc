// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/mock_lock_screen_client.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/public/cpp/config.h"
#include "ash/root_window_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/focus/focus_manager.h"

using ::testing::_;
using LockScreenSanityTest = ash::LoginTestBase;

namespace ash {
namespace {

// Returns true if |view| or any child of it has focus.
bool HasFocusInAnyChildView(views::View* view) {
  if (view->HasFocus())
    return true;
  for (int i = 0; i < view->child_count(); ++i) {
    if (HasFocusInAnyChildView(view->child_at(i)))
      return true;
  }
  return false;
}

void ExpectFocused(views::View* view) {
  EXPECT_TRUE(view->GetWidget()->IsActive());
  EXPECT_TRUE(HasFocusInAnyChildView(view));
}

void ExpectNotFocused(views::View* view) {
  EXPECT_FALSE(view->GetWidget()->IsActive());
  EXPECT_FALSE(HasFocusInAnyChildView(view));
}

}  // namespace

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

// Verifies that tabbing from the lock screen will eventually focus the shelf.
// Then, a shift+tab will bring focus back to the lock screen.
TEST_F(LockScreenSanityTest, TabGoesFromLockToShelfAndBackToLock) {
  // Make lock screen shelf visible.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  // Create lock screen.
  auto* lock = new LockContentsView(data_dispatcher());
  SetUserCount(1);
  ShowWidgetWithContent(lock);
  views::View* shelf = Shelf::ForWindow(lock->GetWidget()->GetNativeWindow())
                           ->shelf_widget()
                           ->GetContentsView();

  // Lock has focus.
  ExpectFocused(lock);
  ExpectNotFocused(shelf);

  // Tab (eventually) goes to the shelf.
  for (int i = 0; i < 50; ++i) {
    GetEventGenerator().PressKey(ui::KeyboardCode::VKEY_TAB, 0);
    if (!HasFocusInAnyChildView(lock))
      break;
  }
  ExpectNotFocused(lock);
  ExpectFocused(shelf);

  // A single shift+tab brings focus back to the lock screen.
  GetEventGenerator().PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ExpectFocused(lock);
  ExpectNotFocused(shelf);
}

// Verifies that shift-tabbing from the lock screen will eventually focus the
// status area. Then, a tab will bring focus back to the lock screen.
TEST_F(LockScreenSanityTest, ShiftTabGoesFromLockToStatusAreaAndBackToLock) {
  // The status area will not focus out unless we are on the lock screen. See
  // StatusAreaWidgetDelegate::ShouldFocusOut.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  auto* lock = new LockContentsView(data_dispatcher());
  SetUserCount(1);
  ShowWidgetWithContent(lock);
  views::View* status_area =
      RootWindowController::ForWindow(lock->GetWidget()->GetNativeWindow())
          ->GetSystemTray()
          ->GetWidget()
          ->GetContentsView();

  // Lock screen has focus.
  ExpectFocused(lock);
  ExpectNotFocused(status_area);

  // Two shift+tab bring focus to the status area.
  // TODO(crbug.com/768076): Only one shift+tab is needed as the focus should
  // go directly to status area from password view.
  //
  // Focus from password view to user view (user dropdown button).
  GetEventGenerator().PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);

  // Focus from user view to the status area.
  GetEventGenerator().PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);

  ExpectNotFocused(lock);
  ExpectFocused(status_area);

  // A single tab brings focus back to the lock screen.
  GetEventGenerator().PressKey(ui::KeyboardCode::VKEY_TAB, 0);
  ExpectFocused(lock);
  ExpectNotFocused(status_area);
}

}  // namespace ash
