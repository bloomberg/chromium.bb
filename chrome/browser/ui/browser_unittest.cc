// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/native_web_keyboard_event.h"

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
