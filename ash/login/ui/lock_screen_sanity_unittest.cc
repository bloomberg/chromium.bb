// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/login/lock_screen_controller.h"
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
#include "ui/views/widget/widget.h"

using ::testing::_;
using ::testing::Invoke;
using LockScreenSanityTest = ash::LoginTestBase;

namespace ash {
namespace {

class LockScreenAppFocuser {
 public:
  explicit LockScreenAppFocuser(views::Widget* lock_screen_app_widget)
      : lock_screen_app_widget_(lock_screen_app_widget) {}
  ~LockScreenAppFocuser() = default;

  bool reversed_tab_order() const { return reversed_tab_order_; }

  void FocusLockScreenApp(bool reverse) {
    reversed_tab_order_ = reverse;
    lock_screen_app_widget_->Activate();
  }

 private:
  bool reversed_tab_order_ = false;
  views::Widget* lock_screen_app_widget_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenAppFocuser);
};

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

// Keeps tabbing through |view| until the view loses focus.
// The number of generated tab events will be limited - if the focus is still
// within the view by the time the limit is hit, this will return false.
bool TabThroughView(ui::test::EventGenerator* event_generator,
                    views::View* view,
                    bool reverse) {
  if (!HasFocusInAnyChildView(view)) {
    ADD_FAILURE() << "View not focused initially.";
    return false;
  }

  for (int i = 0; i < 50; ++i) {
    event_generator->PressKey(ui::KeyboardCode::VKEY_TAB,
                              reverse ? ui::EF_SHIFT_DOWN : 0);
    if (!HasFocusInAnyChildView(view))
      return true;
  }

  return false;
}

testing::AssertionResult VerifyFocused(views::View* view) {
  if (!view->GetWidget()->IsActive())
    return testing::AssertionFailure() << "Widget not active.";
  if (!HasFocusInAnyChildView(view))
    return testing::AssertionFailure() << "No focused descendant.";
  return testing::AssertionSuccess();
}

testing::AssertionResult VerifyNotFocused(views::View* view) {
  if (view->GetWidget()->IsActive())
    return testing::AssertionFailure() << "Widget active";
  if (HasFocusInAnyChildView(view))
    return testing::AssertionFailure() << "Has focused descendant.";
  return testing::AssertionSuccess();
}

}  // namespace

// Verifies that the password input box has focus.
TEST_F(LockScreenSanityTest, PasswordIsInitiallyFocused) {
  // Build lock screen.
  auto* contents = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                        data_dispatcher());

  // The lock screen requires at least one user.
  SetUserCount(1);

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

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
  auto* contents = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                        data_dispatcher());

  // The lock screen requires at least one user.
  SetUserCount(1);

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  // Password submit runs mojo.
  std::unique_ptr<MockLockScreenClient> client = BindMockLockScreenClient();
  client->set_authenticate_user_callback_result(false);
  EXPECT_CALL(
      *client,
      AuthenticateUser_(users()[0]->basic_user_info->account_id, _, false, _));
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
  auto* lock = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                    data_dispatcher());
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(lock);
  views::View* shelf = Shelf::ForWindow(lock->GetWidget()->GetNativeWindow())
                           ->shelf_widget()
                           ->GetContentsView();

  // Lock has focus.
  EXPECT_TRUE(VerifyFocused(lock));
  EXPECT_TRUE(VerifyNotFocused(shelf));

  // Tab (eventually) goes to the shelf.
  ASSERT_TRUE(TabThroughView(&GetEventGenerator(), lock, false /*reverse*/));
  EXPECT_TRUE(VerifyNotFocused(lock));
  EXPECT_TRUE(VerifyFocused(shelf));

  // A single shift+tab brings focus back to the lock screen.
  GetEventGenerator().PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(VerifyFocused(lock));
  EXPECT_TRUE(VerifyNotFocused(shelf));
}

// Verifies that shift-tabbing from the lock screen will eventually focus the
// status area. Then, a tab will bring focus back to the lock screen.
TEST_F(LockScreenSanityTest, ShiftTabGoesFromLockToStatusAreaAndBackToLock) {
  // The status area will not focus out unless we are on the lock screen. See
  // StatusAreaWidgetDelegate::ShouldFocusOut.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  auto* lock = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                    data_dispatcher());
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(lock);
  views::View* status_area =
      RootWindowController::ForWindow(lock->GetWidget()->GetNativeWindow())
          ->GetSystemTray()
          ->GetWidget()
          ->GetContentsView();

  // Lock screen has focus.
  EXPECT_TRUE(VerifyFocused(lock));
  EXPECT_TRUE(VerifyNotFocused(status_area));

  // Two shift+tab bring focus to the status area.
  // TODO(crbug.com/768076): Only one shift+tab is needed as the focus should
  // go directly to status area from password view.
  //
  // Focus from password view to user view (user dropdown button).
  GetEventGenerator().PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);

  // Focus from user view to the status area.
  GetEventGenerator().PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(VerifyNotFocused(lock));
  EXPECT_TRUE(VerifyFocused(status_area));

  // A single tab brings focus back to the lock screen.
  GetEventGenerator().PressKey(ui::KeyboardCode::VKEY_TAB, 0);
  EXPECT_TRUE(VerifyFocused(lock));
  EXPECT_TRUE(VerifyNotFocused(status_area));
}

TEST_F(LockScreenSanityTest, TabWithLockScreenAppActive) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  auto* lock = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                    data_dispatcher());
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(lock);

  views::View* shelf = Shelf::ForWindow(lock->GetWidget()->GetNativeWindow())
                           ->shelf_widget()
                           ->GetContentsView();

  views::View* status_area =
      RootWindowController::ForWindow(lock->GetWidget()->GetNativeWindow())
          ->GetSystemTray()
          ->GetWidget()
          ->GetContentsView();

  LockScreenController* lock_screen_controller =
      Shell::Get()->lock_screen_controller();

  // Initialize lock screen action state.
  data_dispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kActive);

  // Create and focus a lock screen app window.
  auto* lock_screen_app = new views::View();
  lock_screen_app->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  std::unique_ptr<views::Widget> app_widget =
      CreateWidgetWithContent(lock_screen_app);
  app_widget->Show();

  // Lock screen app focus is requested using lock screen mojo client - set up
  // the mock client.
  LockScreenAppFocuser app_widget_focuser(app_widget.get());
  std::unique_ptr<MockLockScreenClient> client = BindMockLockScreenClient();
  EXPECT_CALL(*client, FocusLockScreenApps(_))
      .WillRepeatedly(Invoke(&app_widget_focuser,
                             &LockScreenAppFocuser::FocusLockScreenApp));

  // Initially, focus should be with the lock screen app - when the app loses
  // focus (notified via mojo interface), shelf should get the focus next.
  EXPECT_TRUE(VerifyFocused(lock_screen_app));
  lock_screen_controller->HandleFocusLeavingLockScreenApps(false /*reverse*/);
  EXPECT_TRUE(VerifyFocused(shelf));

  // Reversing focus should bring focus back to the lock screen app.
  GetEventGenerator().PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  // Focus is passed to lock screen apps via mojo - flush the request.
  lock_screen_controller->FlushForTesting();
  EXPECT_TRUE(VerifyFocused(lock_screen_app));
  EXPECT_TRUE(app_widget_focuser.reversed_tab_order());

  // Have the app tab out in reverse tab order - in this case, the status area
  // should get the focus.
  lock_screen_controller->HandleFocusLeavingLockScreenApps(true /*reverse*/);
  EXPECT_TRUE(VerifyFocused(status_area));

  // Tabbing out of the status area (in default order) should focus the lock
  // screen app again.
  GetEventGenerator().PressKey(ui::KeyboardCode::VKEY_TAB, 0);
  // Focus is passed to lock screen apps via mojo - flush the request.
  lock_screen_controller->FlushForTesting();
  EXPECT_TRUE(VerifyFocused(lock_screen_app));
  EXPECT_FALSE(app_widget_focuser.reversed_tab_order());

  // Tab out of the lock screen app once more - the shelf should get the focus
  // again.
  lock_screen_controller->HandleFocusLeavingLockScreenApps(false /*reverse*/);
  EXPECT_TRUE(VerifyFocused(shelf));
}

TEST_F(LockScreenSanityTest, FocusLockScreenWhenLockScreenAppExit) {
  // Set up lock screen.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  auto* lock = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                    data_dispatcher());
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(lock);

  views::View* shelf = Shelf::ForWindow(lock->GetWidget()->GetNativeWindow())
                           ->shelf_widget()
                           ->GetContentsView();

  // Setup and focus a lock screen app.
  data_dispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kActive);
  auto* lock_screen_app = new views::View();
  lock_screen_app->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  std::unique_ptr<views::Widget> app_widget =
      CreateWidgetWithContent(lock_screen_app);
  app_widget->Show();
  EXPECT_TRUE(VerifyFocused(lock_screen_app));

  // Tab out of the lock screen app - shelf should get the focus.
  Shell::Get()->lock_screen_controller()->HandleFocusLeavingLockScreenApps(
      false /*reverse*/);
  EXPECT_TRUE(VerifyFocused(shelf));

  // Move the lock screen note taking to available state (which happens when the
  // app session ends) - this should focus the lock screen.
  data_dispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kAvailable);
  EXPECT_TRUE(VerifyFocused(lock));

  // Tab through the lock screen - the focus should eventually get to the shelf.
  ASSERT_TRUE(TabThroughView(&GetEventGenerator(), lock, false /*reverse*/));
  EXPECT_TRUE(VerifyFocused(shelf));
}

}  // namespace ash
