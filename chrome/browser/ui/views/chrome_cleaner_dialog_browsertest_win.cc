// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_cleaner_dialog_win.h"

#include "base/macros.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_dialog_controller_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ChromeCleanerDialogTest : public DialogBrowserTest {
 public:
  ChromeCleanerDialogTest() {}

  void ShowDialog(const std::string& name) override {
    chrome::ShowChromeCleanerPrompt(
        browser(), new safe_browsing::ChromeCleanerDialogController());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeCleanerDialogTest);
};

IN_PROC_BROWSER_TEST_F(ChromeCleanerDialogTest, InvokeDialog_default) {
  RunDialog();
}

}  // namespace
