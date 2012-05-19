// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#include "chrome/test/base/browser_with_test_window_test.h"

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
