// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/invert_bubble_view.h"

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "ui/views/view.h"

class InvertBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  InvertBubbleViewBrowserTest() {}

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    chrome::ShowInvertBubbleView(browser(), &anchor_);
  }

 private:
  views::View anchor_;

  DISALLOW_COPY_AND_ASSIGN(InvertBubbleViewBrowserTest);
};

// Invokes a bubble that asks the user if they want to install a high contrast
// Chrome theme. See test_browser_dialog.h.
IN_PROC_BROWSER_TEST_F(InvertBubbleViewBrowserTest, InvokeDialog_default) {
  RunDialog();
}
