// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/first_run_bubble.h"

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"

class FirstRunBubbleBrowserTest : public DialogBrowserTest {
 public:
  FirstRunBubbleBrowserTest() {}

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    FirstRunBubble::Show(browser());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FirstRunBubbleBrowserTest);
};

// Invokes a dialog that tells the user they can type into the omnibox to search
// with Google.
IN_PROC_BROWSER_TEST_F(FirstRunBubbleBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
