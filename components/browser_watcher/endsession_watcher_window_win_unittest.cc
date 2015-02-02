// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/endsession_watcher_window_win.h"

#include <windows.h>
#include <vector>

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_watcher {

namespace {

class EndSessionWatcherWindowTest : public testing::Test {
 public:
  EndSessionWatcherWindowTest()
      : num_callbacks_(0), last_message_(0), last_lparam_(0) {
  }

  void OnEndSessionMessage(UINT message, LPARAM lparam) {
    ++num_callbacks_;
    last_message_ = message;
    last_lparam_ = lparam;
  }

  size_t num_callbacks_;
  UINT last_message_;
  UINT last_lparam_;
};

}  // namespace browser_watcher

TEST_F(EndSessionWatcherWindowTest, NoCallbackOnDestruction) {
  {
    EndSessionWatcherWindow watcher_window(
        base::Bind(&EndSessionWatcherWindowTest::OnEndSessionMessage,
                   base::Unretained(this)));
  }

  EXPECT_EQ(num_callbacks_, 0);
  EXPECT_EQ(last_lparam_, 0);
}

TEST_F(EndSessionWatcherWindowTest, IssuesCallbackOnMessage) {
  EndSessionWatcherWindow watcher_window(
      base::Bind(&EndSessionWatcherWindowTest::OnEndSessionMessage,
                 base::Unretained(this)));

  ::SendMessage(watcher_window.window(), WM_QUERYENDSESSION, TRUE, 0xCAFEBABE);
  EXPECT_EQ(num_callbacks_, 1);
  EXPECT_EQ(last_message_, WM_QUERYENDSESSION);
  EXPECT_EQ(last_lparam_, 0xCAFEBABE);

  ::SendMessage(watcher_window.window(), WM_ENDSESSION, TRUE, 0xCAFE);
  EXPECT_EQ(num_callbacks_, 2);
  EXPECT_EQ(last_message_, WM_ENDSESSION);
  EXPECT_EQ(last_lparam_, 0xCAFE);

  // Verify that other messages don't pass through.
  ::SendMessage(watcher_window.window(), WM_CLOSE, TRUE, 0xCAFE);
  EXPECT_EQ(num_callbacks_, 2);
}

}  // namespace browser_watcher
