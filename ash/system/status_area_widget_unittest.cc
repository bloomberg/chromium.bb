// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget.h"

#include "ash/ash_switches.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/status_area_widget_test_helper.h"
#include "base/command_line.h"

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

  // Stylus palette is only constructed if hardware supports it.
  EXPECT_FALSE(status->palette_tray());

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

class StatusAreaWidgetPaletteTest : public test::AshTestBase {
 public:
  StatusAreaWidgetPaletteTest() {}
  ~StatusAreaWidgetPaletteTest() override {}

  // testing::Test:
  void SetUp() override {
    base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
    cmd->AppendSwitch(switches::kAshForceEnablePalette);
    // It's difficult to write a test that marks the primary display as
    // internal before the status area is constructed. Just force the palette
    // for all displays.
    cmd->AppendSwitch(switches::kAshEnablePaletteOnAllDisplays);
    AshTestBase::SetUp();
  }
};

// Tests that the stylus palette tray is constructed.
TEST_F(StatusAreaWidgetPaletteTest, Basics) {
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  EXPECT_TRUE(status->palette_tray());

  // Auto-hidden shelf would not be forced to be visible.
  EXPECT_FALSE(status->ShouldShowShelf());
}

}  // namespace ash
