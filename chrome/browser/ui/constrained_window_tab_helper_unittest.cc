// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "content/browser/tab_contents/constrained_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "content/browser/browser_thread.h"

class ConstrainedWindowTabHelperUnit : public TabContentsWrapperTestHarness {
 public:
  ConstrainedWindowTabHelperUnit()
      : TabContentsWrapperTestHarness(),
        ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 private:
  BrowserThread ui_thread_;
};

class ConstrainedWindowCloseTest : public ConstrainedWindow {
 public:
  explicit ConstrainedWindowCloseTest(TabContentsWrapper* wrapper)
      : wrapper_(wrapper) {
  }

  virtual void ShowConstrainedWindow() {}
  virtual void FocusConstrainedWindow() {}
  virtual ~ConstrainedWindowCloseTest() {}

  virtual void CloseConstrainedWindow() {
    wrapper_->constrained_window_tab_helper()->WillClose(this);
    close_count++;
  }

  int close_count;
  TabContentsWrapper* wrapper_;
};

TEST_F(ConstrainedWindowTabHelperUnit, ConstrainedWindows) {
  ConstrainedWindowCloseTest window(contents_wrapper());
  window.close_count = 0;

  const int kWindowCount = 4;
  for (int i = 0; i < kWindowCount; i++) {
    contents_wrapper()->constrained_window_tab_helper()->AddConstrainedDialog(
        &window);
  }
  EXPECT_EQ(window.close_count, 0);
  contents_wrapper()->constrained_window_tab_helper()->
      CloseConstrainedWindows();
  EXPECT_EQ(window.close_count, kWindowCount);
}
