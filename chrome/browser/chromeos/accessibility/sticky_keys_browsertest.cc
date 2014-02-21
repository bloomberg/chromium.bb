// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

class StickyKeysBrowserTest : public InProcessBrowserTest {
 protected:
  StickyKeysBrowserTest() {}
  virtual ~StickyKeysBrowserTest() {}

  void EnableStickyKeys() {
    AccessibilityManager::Get()->EnableStickyKeys(true);
  }

  void DisableStickyKeys() {
    AccessibilityManager::Get()->EnableStickyKeys(false);
  }

  ash::SystemTray* GetSystemTray() {
    return ash::Shell::GetInstance()->GetPrimarySystemTray();
  }

  void SendKeyPress(ui::KeyboardCode key) {
    gfx::NativeWindow root_window =
        ash::Shell::GetInstance()->GetPrimaryRootWindow();
    ASSERT_TRUE(
        ui_test_utils::SendKeyPressToWindowSync(root_window,
                                                key,
                                                false, // control
                                                false, // shift
                                                false, // alt
                                                false)); // command
  }

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(StickyKeysBrowserTest);
};

IN_PROC_BROWSER_TEST_F(StickyKeysBrowserTest, OpenTrayMenu) {
  EnableStickyKeys();

  // Open system tray bubble with shortcut.
  SendKeyPress(ui::VKEY_MENU); // alt key.
  SendKeyPress(ui::VKEY_SHIFT);
  SendKeyPress(ui::VKEY_S);
  EXPECT_TRUE(GetSystemTray()->HasSystemBubble());

  // Hide system bubble.
  GetSystemTray()->CloseSystemBubble();
  EXPECT_FALSE(GetSystemTray()->HasSystemBubble());

  // Pressing S again should not reopen the bubble.
  SendKeyPress(ui::VKEY_S);
  EXPECT_FALSE(GetSystemTray()->HasSystemBubble());

  // With sticky keys disabled, we will fail to perform the shortcut.
  DisableStickyKeys();
  SendKeyPress(ui::VKEY_MENU); // alt key.
  SendKeyPress(ui::VKEY_SHIFT);
  SendKeyPress(ui::VKEY_S);
  EXPECT_FALSE(GetSystemTray()->HasSystemBubble());
}

IN_PROC_BROWSER_TEST_F(StickyKeysBrowserTest, OpenNewTabs) {
  // Lock the modifier key.
  EnableStickyKeys();
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_CONTROL);

  // In the locked state, pressing 't' should open a new tab each time.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  int tab_count = 1;
  for (; tab_count < 5; ++tab_count) {
    EXPECT_EQ(tab_count, tab_strip_model->count());
    SendKeyPress(ui::VKEY_T);
  }

  // Unlock the modifier key and shortcut should no longer activate.
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_T);
  EXPECT_EQ(tab_count, tab_strip_model->count());

  // Shortcut should not work after disabling sticky keys.
  DisableStickyKeys();
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_T);
  EXPECT_EQ(tab_count, tab_strip_model->count());
}

IN_PROC_BROWSER_TEST_F(StickyKeysBrowserTest, CtrlClickHomeButton) {
  // Show home page button.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  int tab_count = 1;
  EXPECT_EQ(tab_count, tab_strip_model->count());

  // Test sticky keys with modified mouse click action.
  EnableStickyKeys();
  SendKeyPress(ui::VKEY_CONTROL);
  ui_test_utils::ClickOnView(browser(), VIEW_ID_HOME_BUTTON);
  EXPECT_EQ(++tab_count, tab_strip_model->count());
  ui_test_utils::ClickOnView(browser(), VIEW_ID_HOME_BUTTON);
  EXPECT_EQ(tab_count, tab_strip_model->count());

  // Test locked modifier key with mouse click.
  SendKeyPress(ui::VKEY_CONTROL);
  SendKeyPress(ui::VKEY_CONTROL);
  for (; tab_count < 5; ++tab_count) {
    EXPECT_EQ(tab_count, tab_strip_model->count());
    ui_test_utils::ClickOnView(browser(), VIEW_ID_HOME_BUTTON);
  }
  SendKeyPress(ui::VKEY_CONTROL);
  ui_test_utils::ClickOnView(browser(), VIEW_ID_HOME_BUTTON);
  EXPECT_EQ(tab_count, tab_strip_model->count());

  // Test disabling sticky keys prevent modified mouse click.
  DisableStickyKeys();
  SendKeyPress(ui::VKEY_CONTROL);
  ui_test_utils::ClickOnView(browser(), VIEW_ID_HOME_BUTTON);
  EXPECT_EQ(tab_count, tab_strip_model->count());
}

}  // namespace chromeos
