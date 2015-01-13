// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/endsession_watcher_window_win.h"

#include <windows.h>

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_watcher {

namespace {

void OnEndSession(bool* called, LPARAM* out_lparam, LPARAM lparam) {
  *called = true;
  *out_lparam = lparam;
}

}  // namespace browser_watcher

TEST(EndSessionWatcherWindowTest, NoCallbackOnDestruction) {
  LPARAM lparam = 0;
  bool was_called = false;

  {
    EndSessionWatcherWindow watcher_window(
        base::Bind(&OnEndSession, &was_called, &lparam));
  }

  EXPECT_FALSE(was_called);
  EXPECT_EQ(lparam, 0);
}

TEST(EndSessionWatcherWindowTest, IssuesCallbackOnMessage) {
  LPARAM lparam = 0;
  bool was_called = false;

  EndSessionWatcherWindow watcher_window(
      base::Bind(&OnEndSession, &was_called, &lparam));

  ::SendMessage(watcher_window.window(), WM_ENDSESSION, TRUE, 0xCAFEBABE);

  EXPECT_TRUE(was_called);
  EXPECT_EQ(lparam, 0xCAFEBABE);
}

}  // namespace browser_watcher
