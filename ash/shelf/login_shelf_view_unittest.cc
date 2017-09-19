// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/login_shelf_view.h"

#include "ash/login/mock_lock_screen_client.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/shutdown_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "ash/tray_action/test_tray_action_client.h"
#include "ash/tray_action/tray_action.h"
#include "ash/wm/lock_state_controller.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/label_button.h"

using session_manager::SessionState;

namespace ash {
namespace {

class LoginShelfViewTest : public AshTestBase {
 public:
  LoginShelfViewTest() {}
  ~LoginShelfViewTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    login_shelf_view_ = GetPrimaryShelf()->GetLoginShelfViewForTesting();
    Shell::Get()->tray_action()->SetClient(
        tray_action_client_.CreateInterfacePtrAndBind(),
        mojom::TrayActionState::kNotAvailable);
  }

 protected:
  void SetInitialStates() {
    NotifySessionStateChanged(SessionState::OOBE);
    NotifyShutdownPolicyChanged(false);
    NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kNotAvailable);
  }

  void NotifySessionStateChanged(SessionState state) {
    GetSessionControllerClient()->SetSessionState(state);
  }

  void NotifyShutdownPolicyChanged(bool reboot_on_shutdown) {
    Shell::Get()->shutdown_controller()->SetRebootOnShutdownForTesting(
        reboot_on_shutdown);
  }

  void NotifyLockScreenNoteStateChanged(mojom::TrayActionState state) {
    Shell::Get()->tray_action()->UpdateLockScreenNoteState(state);
  }

  // Simulates a click event on the button.
  void Click(LoginShelfView::ButtonId id) {
    const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), 0, 0);
    login_shelf_view_->ButtonPressed(
        static_cast<views::Button*>(login_shelf_view_->GetViewByID(id)), event);
    RunAllPendingInMessageLoop();
  }

  // Checks if the shelf is only showing the buttons in the list. The IDs in
  // the specified list must be unique.
  bool ShowsShelfButtons(std::vector<LoginShelfView::ButtonId> ids) {
    for (LoginShelfView::ButtonId id : ids) {
      if (!login_shelf_view_->GetViewByID(id)->visible())
        return false;
    }
    size_t visible_button_count = 0;
    for (int i = 0; i < login_shelf_view_->child_count(); ++i) {
      if (login_shelf_view_->child_at(i)->visible())
        visible_button_count++;
    }
    return visible_button_count == ids.size();
  }

  TestTrayActionClient tray_action_client_;

  LoginShelfView* login_shelf_view_;  // Unowned.

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginShelfViewTest);
};

// Checks the login shelf updates UI after session state changes.
TEST_F(LoginShelfViewTest, ShouldUpdateUiAfterSessionStateChange) {
  SetInitialStates();
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  NotifySessionStateChanged(SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  NotifySessionStateChanged(SessionState::ACTIVE);
  EXPECT_TRUE(ShowsShelfButtons({}));

  NotifySessionStateChanged(SessionState::LOGIN_SECONDARY);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kCancel}));

  NotifySessionStateChanged(SessionState::ACTIVE);
  EXPECT_TRUE(ShowsShelfButtons({}));

  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));
}

// Checks the login shelf updates UI after shutdown policy change when the
// screen is locked.
TEST_F(LoginShelfViewTest,
       ShouldUpdateUiAfterShutdownPolicyChangeAtLockScreen) {
  SetInitialStates();
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  NotifyShutdownPolicyChanged(true /*reboot_on_shutdown*/);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kRestart, LoginShelfView::kSignOut}));

  NotifyShutdownPolicyChanged(false /*reboot_on_shutdown*/);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));
}

// Checks shutdown policy change during another session state (e.g. ACTIVE)
// will be reflected when the screen becomes locked.
TEST_F(LoginShelfViewTest, ShouldUpdateUiBasedOnShutdownPolicyInActiveSession) {
  // The initial state of |reboot_on_shutdown| is false.
  SetInitialStates();
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  NotifySessionStateChanged(SessionState::ACTIVE);
  NotifyShutdownPolicyChanged(true /*reboot_on_shutdown*/);

  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kRestart, LoginShelfView::kSignOut}));
}

// Checks the login shelf updates UI after lock screen note state changes.
TEST_F(LoginShelfViewTest, ShouldUpdateUiAfterLockScreenNoteState) {
  SetInitialStates();
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kAvailable);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kLaunching);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kCloseNote}));

  NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kActive);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kCloseNote}));

  NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kBackground);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kNotAvailable);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));
}

TEST_F(LoginShelfViewTest, ClickShutdownButton) {
  Click(LoginShelfView::kShutdown);
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

TEST_F(LoginShelfViewTest, ClickRestartButton) {
  Click(LoginShelfView::kRestart);
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

TEST_F(LoginShelfViewTest, ClickSignOutButton) {
  TestShellDelegate* shell_delegate_ =
      static_cast<TestShellDelegate*>(Shell::Get()->shell_delegate());
  EXPECT_EQ(shell_delegate_->num_exit_requests(), 0);
  Click(LoginShelfView::kSignOut);
  EXPECT_EQ(shell_delegate_->num_exit_requests(), 1);
}

TEST_F(LoginShelfViewTest, ClickUnlockButton) {
  // The unlock button is visible only when session state is LOCKED and note
  // state is kActive or kLaunching.
  NotifySessionStateChanged(SessionState::LOCKED);

  NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kActive);
  EXPECT_TRUE(tray_action_client_.close_note_reasons().empty());
  Click(LoginShelfView::kCloseNote);
  EXPECT_EQ(std::vector<mojom::CloseLockScreenNoteReason>(
                {mojom::CloseLockScreenNoteReason::kUnlockButtonPressed}),
            tray_action_client_.close_note_reasons());

  tray_action_client_.ClearRecordedRequests();
  NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kLaunching);
  EXPECT_TRUE(tray_action_client_.close_note_reasons().empty());
  Click(LoginShelfView::kCloseNote);
  EXPECT_EQ(std::vector<mojom::CloseLockScreenNoteReason>(
                {mojom::CloseLockScreenNoteReason::kUnlockButtonPressed}),
            tray_action_client_.close_note_reasons());
}

TEST_F(LoginShelfViewTest, ClickCancelButton) {
  std::unique_ptr<MockLockScreenClient> client = BindMockLockScreenClient();
  EXPECT_CALL(*client, CancelAddUser());
  Click(LoginShelfView::kCancel);
}

}  // namespace
}  // namespace ash
