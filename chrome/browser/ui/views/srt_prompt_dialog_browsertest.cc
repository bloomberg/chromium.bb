// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/srt_prompt_dialog.h"

#include "base/macros.h"
#include "chrome/browser/safe_browsing/srt_prompt_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SRTPromptDialogTest : public DialogBrowserTest {
 public:
  SRTPromptDialogTest() {}

  void ShowDialog(const std::string& name) override {
    chrome::ShowSRTPrompt(browser(), new safe_browsing::SRTPromptController());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SRTPromptDialogTest);
};

IN_PROC_BROWSER_TEST_F(SRTPromptDialogTest, InvokeDialog_default) {
  RunDialog();
}

}  // namespace
