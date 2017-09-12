// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/lock_screen_action/lock_screen_action_tray.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/tray_action/test_tray_action_client.h"
#include "ash/tray_action/tray_action.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chromeos/chromeos_switches.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {

namespace {

LockScreenActionTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
      ->lock_screen_action_tray_for_testing();
}

class LockScreenActionTrayTest : public AshTestBase {
 public:
  LockScreenActionTrayTest() = default;
  ~LockScreenActionTrayTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kShowMdLogin);
    AshTestBase::SetUp();
  }

  void ClickOnTray() {
    // Perform click on the tray view.
    ui::test::EventGenerator& generator = GetEventGenerator();
    gfx::Point center = GetTray()->GetBoundsInScreen().CenterPoint();
    generator.MoveMouseTo(center.x(), center.y());
    generator.ClickLeftButton();

    RunAllPendingInMessageLoop();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LockScreenActionTrayTest);
};

using LockScreenActionTrayWithoutMdLoginTest = AshTestBase;

}  // namespace

TEST_F(LockScreenActionTrayTest, NoClient) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kAvailable);

  EXPECT_FALSE(GetTray()->visible());
}

TEST_F(LockScreenActionTrayTest, NotAvailableState) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  TestTrayActionClient tray_action_client;
  tray_action->SetClient(tray_action_client.CreateInterfacePtrAndBind(),
                         mojom::TrayActionState::kNotAvailable);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  EXPECT_FALSE(GetTray()->visible());
}

TEST_F(LockScreenActionTrayTest, ActionAvailableState) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  TestTrayActionClient tray_action_client;
  tray_action->SetClient(tray_action_client.CreateInterfacePtrAndBind(),
                         mojom::TrayActionState::kNotAvailable);
  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kAvailable);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  EXPECT_TRUE(GetTray()->visible());

  ASSERT_FALSE(GetTray()->GetImageForTesting().isNull());

  EXPECT_EQ(0, tray_action_client.action_requests_count());
  ClickOnTray();
  ASSERT_EQ(1, tray_action_client.action_requests_count());
  EXPECT_TRUE(GetTray()->visible());
  EXPECT_FALSE(GetTray()->GetImageForTesting().isNull());

  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kActive);
  EXPECT_FALSE(GetTray()->visible());
}

TEST_F(LockScreenActionTrayTest, ActionAvailableStateSetWithClient) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  TestTrayActionClient tray_action_client;
  tray_action->SetClient(tray_action_client.CreateInterfacePtrAndBind(),
                         mojom::TrayActionState::kAvailable);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  EXPECT_TRUE(GetTray()->visible());
  ASSERT_FALSE(GetTray()->GetImageForTesting().isNull());

  EXPECT_EQ(0, tray_action_client.action_requests_count());
  ClickOnTray();
  ASSERT_EQ(1, tray_action_client.action_requests_count());
  EXPECT_TRUE(GetTray()->visible());
  EXPECT_FALSE(GetTray()->GetImageForTesting().isNull());

  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kActive);
  EXPECT_FALSE(GetTray()->visible());
}

TEST_F(LockScreenActionTrayTest, ActionActiveState) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  TestTrayActionClient tray_action_client;
  tray_action->SetClient(tray_action_client.CreateInterfacePtrAndBind(),
                         mojom::TrayActionState::kNotAvailable);
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kActive);
  EXPECT_FALSE(GetTray()->visible());
}

TEST_F(LockScreenActionTrayTest, ActionBackgroundState) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  TestTrayActionClient tray_action_client;
  tray_action->SetClient(tray_action_client.CreateInterfacePtrAndBind(),
                         mojom::TrayActionState::kNotAvailable);
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kBackground);
  EXPECT_FALSE(GetTray()->visible());
}

TEST_F(LockScreenActionTrayTest, LaunchingState) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  TestTrayActionClient tray_action_client;
  tray_action->SetClient(tray_action_client.CreateInterfacePtrAndBind(),
                         mojom::TrayActionState::kNotAvailable);
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kLaunching);
  EXPECT_TRUE(GetTray()->visible());
  ASSERT_FALSE(GetTray()->GetImageForTesting().isNull());

  EXPECT_EQ(0, tray_action_client.action_requests_count());
  ClickOnTray();
  // Clicking on the item while the action is launching should not repeat action
  // request.
  ASSERT_EQ(0, tray_action_client.action_requests_count());
  EXPECT_TRUE(GetTray()->visible());
  EXPECT_FALSE(GetTray()->GetImageForTesting().isNull());

  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kActive);
  EXPECT_FALSE(GetTray()->visible());
}

TEST_F(LockScreenActionTrayTest, TrayNotVisibleWhenSessionNotLocked) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  TestTrayActionClient tray_action_client;
  tray_action->SetClient(tray_action_client.CreateInterfacePtrAndBind(),
                         mojom::TrayActionState::kAvailable);
  EXPECT_FALSE(GetTray()->visible());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  EXPECT_TRUE(GetTray()->visible());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_FALSE(GetTray()->visible());

  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kAvailable);
  EXPECT_FALSE(GetTray()->visible());

  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kLaunching);
  EXPECT_FALSE(GetTray()->visible());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  EXPECT_TRUE(GetTray()->visible());

  ClearLogin();
  EXPECT_FALSE(GetTray()->visible());
}

TEST_F(LockScreenActionTrayWithoutMdLoginTest, NotVisible) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  TestTrayActionClient tray_action_client;
  tray_action->SetClient(tray_action_client.CreateInterfacePtrAndBind(),
                         mojom::TrayActionState::kNotAvailable);

  EXPECT_FALSE(GetTray()->visible());

  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kAvailable);
  EXPECT_FALSE(GetTray()->visible());

  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kLaunching);
  EXPECT_FALSE(GetTray()->visible());

  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kActive);
  EXPECT_FALSE(GetTray()->visible());

  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kBackground);
  EXPECT_FALSE(GetTray()->visible());
}

}  // namespace ash
