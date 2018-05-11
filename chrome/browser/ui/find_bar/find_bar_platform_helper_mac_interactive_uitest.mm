// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/views/scoped_macviews_browser_mode.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/base/cocoa/find_pasteboard.h"

class FindBarPlatformHelperMacInteractiveUITest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  FindBarPlatformHelperMacInteractiveUITest()
      : scoped_macviews_browser_mode_(GetParam()) {}
  ~FindBarPlatformHelperMacInteractiveUITest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    old_find_text_ = [[FindPasteboard sharedInstance] findText];
  }

  void TearDownOnMainThread() override {
    [[FindPasteboard sharedInstance] setFindText:old_find_text_];
    InProcessBrowserTest::TearDownOnMainThread();
  }

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<bool> param_info) {
    return param_info.param ? "Views" : "Cocoa";
  }

 private:
  test::ScopedMacViewsBrowserMode scoped_macviews_browser_mode_;
  NSString* old_find_text_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FindBarPlatformHelperMacInteractiveUITest);
};

// Tests that the pasteboard is updated when the find bar is changed.
IN_PROC_BROWSER_TEST_P(FindBarPlatformHelperMacInteractiveUITest,
                       PasteboardUpdatedFromFindBar) {
  FindBarController* find_bar_controller = browser()->GetFindBarController();
  ASSERT_NE(nullptr, find_bar_controller);

  base::string16 empty_string(base::ASCIIToUTF16(""));
  find_bar_controller->SetText(empty_string);

  chrome::Find(browser());
  EXPECT_TRUE(
      ui_test_utils::IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_S, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_D, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_F, false,
                                              false, false, false));

  base::string16 find_bar_string =
      find_bar_controller->find_bar()->GetFindText();

  ASSERT_EQ(base::ASCIIToUTF16("asdf"), find_bar_string);
  EXPECT_EQ(find_bar_string, base::SysNSStringToUTF16(
                                 [[FindPasteboard sharedInstance] findText]));
}

INSTANTIATE_TEST_CASE_P(
    ,
    FindBarPlatformHelperMacInteractiveUITest,
    ::testing::Bool(),
    FindBarPlatformHelperMacInteractiveUITest::ParamInfoToString);
