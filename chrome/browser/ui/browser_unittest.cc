// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/native_web_keyboard_event.h"

// Various assertions around setting show state.
TEST_F(BrowserWithTestWindowTest, GetSavedWindowShowState) {
  // Default show state is SHOW_STATE_DEFAULT.
  EXPECT_EQ(ui::SHOW_STATE_DEFAULT, browser()->GetSavedWindowShowState());

  // Explicitly specifying a state should stick though.
  browser()->set_show_state(ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, browser()->GetSavedWindowShowState());
  browser()->set_show_state(ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, browser()->GetSavedWindowShowState());
  browser()->set_show_state(ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(ui::SHOW_STATE_MINIMIZED, browser()->GetSavedWindowShowState());
  browser()->set_show_state(ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(ui::SHOW_STATE_FULLSCREEN, browser()->GetSavedWindowShowState());
}

TEST_F(BrowserWithTestWindowTest, IsReservedCommandOrKey) {
#if defined(OS_CHROMEOS)
  const content::NativeWebKeyboardEvent event(ui::ET_KEY_PRESSED,
                                              false,
                                              ui::VKEY_F1,
                                              0,
                                              base::Time::Now().ToDoubleT());
  // F1-4 keys are reserved on Chrome OS.
  EXPECT_TRUE(browser()->IsReservedCommandOrKey(IDC_BACK, event));
  // ..unless |command_id| is -1. crbug.com/122978
  EXPECT_FALSE(browser()->IsReservedCommandOrKey(-1, event));
#endif
}
