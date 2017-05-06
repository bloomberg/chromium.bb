// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget.h"

#include "ash/ash_switches.h"
#include "ash/focus_cycler.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/status_area_focus_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/status_area_widget_test_helper.h"
#include "ash/test/test_session_controller_client.h"
#include "base/command_line.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/aura/env.h"

using session_manager::SessionState;

namespace ash {

using StatusAreaWidgetTest = test::AshTestBase;

// Tests that status area trays are constructed.
TEST_F(StatusAreaWidgetTest, Basics) {
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();

  // Status area is visible by default.
  EXPECT_TRUE(status->IsVisible());

  // No bubbles are open at startup.
  EXPECT_FALSE(status->IsMessageBubbleShown());

  // Auto-hidden shelf would not be forced to be visible.
  EXPECT_FALSE(status->ShouldShowShelf());

  // Default trays are constructed.
  EXPECT_TRUE(status->overview_button_tray());
  EXPECT_TRUE(status->system_tray());
  EXPECT_TRUE(status->web_notification_tray());
  EXPECT_TRUE(status->logout_button_tray_for_testing());
  EXPECT_TRUE(status->ime_menu_tray());
  EXPECT_TRUE(status->virtual_keyboard_tray_for_testing());
  EXPECT_TRUE(status->palette_tray());

  // Needed because WebNotificationTray updates its initial visibility
  // asynchronously.
  RunAllPendingInMessageLoop();

  // Default trays are visible.
  EXPECT_FALSE(status->overview_button_tray()->visible());
  EXPECT_TRUE(status->system_tray()->visible());
  EXPECT_TRUE(status->web_notification_tray()->visible());
  EXPECT_FALSE(status->logout_button_tray_for_testing()->visible());
  EXPECT_FALSE(status->ime_menu_tray()->visible());
  EXPECT_FALSE(status->virtual_keyboard_tray_for_testing()->visible());
}

class StatusAreaFocusTestObserver : public StatusAreaFocusObserver {
 public:
  StatusAreaFocusTestObserver() {}
  ~StatusAreaFocusTestObserver() override {}

  int focus_out_count() { return focus_out_count_; }
  int reverse_focus_out_count() { return reverse_focus_out_count_; }

 protected:
  // StatusAreaFocusObserver overridden:
  void OnFocusOut(bool reverse) override {
    reverse ? ++reverse_focus_out_count_ : ++focus_out_count_;
  }

 private:
  int focus_out_count_ = 0;
  int reverse_focus_out_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaFocusTestObserver);
};

class StatusAreaWidgetFocusTest : public test::AshTestBase {
 public:
  StatusAreaWidgetFocusTest() {}
  ~StatusAreaWidgetFocusTest() override {}

  // test::AshTestBase overridden:
  void SetUp() override {
    AshTestBase::SetUp();
    test_observer_.reset(new StatusAreaFocusTestObserver);
    Shell::Get()->system_tray_notifier()->AddStatusAreaFocusObserver(
        test_observer_.get());
  }

  // test::AshTestBase overridden:
  void TearDown() override {
    Shell::Get()->system_tray_notifier()->RemoveStatusAreaFocusObserver(
        test_observer_.get());
    test_observer_.reset();
    AshTestBase::TearDown();
  }

  void GenerateTabEvent(bool reverse) {
    ui::KeyEvent tab_pressed(ui::ET_KEY_PRESSED, ui::VKEY_TAB,
                             reverse ? ui::EF_SHIFT_DOWN : ui::EF_NONE);
    StatusAreaWidgetTestHelper::GetStatusAreaWidget()->OnKeyEvent(&tab_pressed);
  }

 protected:
  std::unique_ptr<StatusAreaFocusTestObserver> test_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidgetFocusTest);
};

// Tests that tab traversal through status area widget in non-active session
// could properly send FocusOut event.
TEST_F(StatusAreaWidgetFocusTest, FocusOutObserver) {
  // Set session state to LOCKED.
  SessionController* session = Shell::Get()->session_controller();
  ASSERT_TRUE(session->IsActiveUserSessionStarted());
  test::TestSessionControllerClient* client = GetSessionControllerClient();
  client->SetSessionState(SessionState::LOCKED);
  ASSERT_TRUE(session->IsScreenLocked());

  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  // Default trays are constructed.
  ASSERT_TRUE(status->overview_button_tray());
  ASSERT_TRUE(status->system_tray());
  ASSERT_TRUE(status->web_notification_tray());
  ASSERT_TRUE(status->logout_button_tray_for_testing());
  ASSERT_TRUE(status->ime_menu_tray());
  ASSERT_TRUE(status->virtual_keyboard_tray_for_testing());

  // Needed because WebNotificationTray updates its initial visibility
  // asynchronously.
  RunAllPendingInMessageLoop();

  // Default trays are visible.
  ASSERT_FALSE(status->overview_button_tray()->visible());
  ASSERT_TRUE(status->system_tray()->visible());
  ASSERT_TRUE(status->web_notification_tray()->visible());
  ASSERT_FALSE(status->logout_button_tray_for_testing()->visible());
  ASSERT_FALSE(status->ime_menu_tray()->visible());
  ASSERT_FALSE(status->virtual_keyboard_tray_for_testing()->visible());

  // Set focus to status area widget, which will be be system tray.
  ASSERT_TRUE(Shell::Get()->focus_cycler()->FocusWidget(status));
  views::FocusManager* focus_manager = status->GetFocusManager();
  EXPECT_EQ(status->system_tray(), focus_manager->GetFocusedView());

  // A tab key event will move focus to web notification tray.
  GenerateTabEvent(false);
  EXPECT_EQ(status->web_notification_tray(), focus_manager->GetFocusedView());
  EXPECT_EQ(0, test_observer_->focus_out_count());
  EXPECT_EQ(0, test_observer_->reverse_focus_out_count());

  // Another tab key event will send FocusOut event, since we are not handling
  // this event, focus will still be moved to system tray.
  GenerateTabEvent(false);
  EXPECT_EQ(status->system_tray(), focus_manager->GetFocusedView());
  EXPECT_EQ(1, test_observer_->focus_out_count());
  EXPECT_EQ(0, test_observer_->reverse_focus_out_count());

  // A reverse tab key event will send reverse FocusOut event, since we are not
  // handling this event, focus will still be moved to web notification tray.
  GenerateTabEvent(true);
  EXPECT_EQ(status->web_notification_tray(), focus_manager->GetFocusedView());
  EXPECT_EQ(1, test_observer_->focus_out_count());
  EXPECT_EQ(1, test_observer_->reverse_focus_out_count());
}

class StatusAreaWidgetPaletteTest : public test::AshTestBase {
 public:
  StatusAreaWidgetPaletteTest() {}
  ~StatusAreaWidgetPaletteTest() override {}

  // testing::Test:
  void SetUp() override {
    // TODO(erg): The implementation of PaletteTray assumes it can talk directly
    // to ui::InputDeviceManager in a mus environment, which it can't.
    if (aura::Env::GetInstance()->mode() != aura::Env::Mode::MUS) {
      base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
      cmd->AppendSwitch(switches::kAshForceEnableStylusTools);
      // It's difficult to write a test that marks the primary display as
      // internal before the status area is constructed. Just force the palette
      // for all displays.
      cmd->AppendSwitch(switches::kAshEnablePaletteOnAllDisplays);
    }
    AshTestBase::SetUp();
  }
};

// Tests that the stylus palette tray is constructed.
TEST_F(StatusAreaWidgetPaletteTest, Basics) {
  // TODO(erg): The implementation of PaletteTray assumes it can talk directly
  // to ui::InputDeviceManager in a mus environment, which it can't.
  if (aura::Env::GetInstance()->mode() == aura::Env::Mode::MUS)
    return;

  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  EXPECT_TRUE(status->palette_tray());

  // Auto-hidden shelf would not be forced to be visible.
  EXPECT_FALSE(status->ShouldShowShelf());
}

}  // namespace ash
