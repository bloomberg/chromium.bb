// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/native_web_keyboard_event.h"

// Various assertions around setting show state.
TEST_F(BrowserWithTestWindowTest, GetSavedWindowShowState) {
  // Default show state is SHOW_STATE_DEFAULT.
  EXPECT_EQ(ui::SHOW_STATE_DEFAULT, chrome::GetSavedWindowShowState(browser()));

  // Explicitly specifying a state should stick though.
  browser()->set_initial_show_state(ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED,
            chrome::GetSavedWindowShowState(browser()));
  browser()->set_initial_show_state(ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, chrome::GetSavedWindowShowState(browser()));
  browser()->set_initial_show_state(ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(ui::SHOW_STATE_MINIMIZED,
            chrome::GetSavedWindowShowState(browser()));
  browser()->set_initial_show_state(ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(ui::SHOW_STATE_FULLSCREEN,
            chrome::GetSavedWindowShowState(browser()));
}

TEST_F(BrowserWithTestWindowTest, IsReservedCommandOrKey) {
#if defined(OS_CHROMEOS)
  const content::NativeWebKeyboardEvent event(ui::ET_KEY_PRESSED,
                                              false,
                                              ui::VKEY_F1,
                                              0,
                                              base::Time::Now().ToDoubleT());
  // F1-4 keys are reserved on Chrome OS.
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(IDC_BACK,
                                                                      event));
  // ..unless |command_id| is -1. crbug.com/122978
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(-1,
                                                                       event));
#endif
}
