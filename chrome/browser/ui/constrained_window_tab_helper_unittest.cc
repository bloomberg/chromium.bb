// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class ConstrainedWindowTabHelperUnit : public TabContentsTestHarness {
 public:
  ConstrainedWindowTabHelperUnit()
      : TabContentsTestHarness(),
        ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 private:
  content::TestBrowserThread ui_thread_;
};

class ConstrainedWindowCloseTest : public ConstrainedWindow {
 public:
  explicit ConstrainedWindowCloseTest(TabContents* tab_contents)
      : tab_contents_(tab_contents) {
  }

  virtual void ShowConstrainedWindow() {}
  virtual void FocusConstrainedWindow() {}
  virtual ~ConstrainedWindowCloseTest() {}

  virtual void CloseConstrainedWindow() {
    tab_contents_->constrained_window_tab_helper()->WillClose(this);
    close_count++;
  }

  int close_count;
  TabContents* tab_contents_;
};

TEST_F(ConstrainedWindowTabHelperUnit, ConstrainedWindows) {
  ConstrainedWindowCloseTest window(tab_contents());
  window.close_count = 0;

  const int kWindowCount = 4;
  for (int i = 0; i < kWindowCount; i++) {
    tab_contents()->constrained_window_tab_helper()->AddConstrainedDialog(
        &window);
  }
  EXPECT_EQ(window.close_count, 0);
  tab_contents()->constrained_window_tab_helper()->CloseConstrainedWindows();
  EXPECT_EQ(window.close_count, kWindowCount);
}
