// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_reboot_dialog_controller_impl_win.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_navigation_util_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/mock_chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

using ::testing::_;
using ::testing::StrictMock;
using ::testing::Return;

class MockPromptDelegate
    : public ChromeCleanerRebootDialogControllerImpl::PromptDelegate {
 public:
  MOCK_METHOD2(ShowChromeCleanerRebootPrompt,
               void(Browser* browser,
                    ChromeCleanerRebootDialogControllerImpl* controller));
  MOCK_METHOD1(OpenSettingsPage, void(Browser* browser));
};

// Parameters for this test:
//  - bool reboot_dialog_enabled_: if kRebootPromptDialogFeature is enabled.
class ChromeCleanerRebootFlowTest : public InProcessBrowserTest,
                                    public ::testing::WithParamInterface<bool> {
 public:
  ChromeCleanerRebootFlowTest() { reboot_dialog_enabled_ = GetParam(); }

  void SetUpInProcessBrowserTestFixture() override {
    if (reboot_dialog_enabled_)
      scoped_feature_list_.InitAndEnableFeature(kRebootPromptDialogFeature);
    else
      scoped_feature_list_.InitAndDisableFeature(kRebootPromptDialogFeature);

    // The implementation of dialog_controller may check state, and we are not
    // interested in ensuring how many times this is done, since it's not part
    // of the main functionality.
    EXPECT_CALL(mock_cleaner_controller_, state())
        .WillRepeatedly(
            Return(ChromeCleanerController::State::kRebootRequired));
  }

 protected:
  bool reboot_dialog_enabled_ = false;

  StrictMock<MockChromeCleanerController> mock_cleaner_controller_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ChromeCleanerRebootFlowTest,
                       OnRebootRequired_SettingsPageActive) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUISettingsURL));
  ASSERT_TRUE(chrome_cleaner_util::SettingsPageIsActiveTab(browser()));

  ChromeCleanerRebootDialogControllerImpl::Create(
      &mock_cleaner_controller_,
      base::MakeUnique<StrictMock<MockPromptDelegate>>());

  // StrictMock ensures no method for mock_prompt_delegate_ is called, and so
  // neither a new tab nor the prompt dialog is opened.
}

IN_PROC_BROWSER_TEST_P(ChromeCleanerRebootFlowTest,
                       OnRebootRequired_SettingsPageNotActive) {
  auto mock_prompt_delegate =
      base::MakeUnique<StrictMock<MockPromptDelegate>>();
  bool close_required = false;
  if (reboot_dialog_enabled_) {
    EXPECT_CALL(*mock_prompt_delegate, ShowChromeCleanerRebootPrompt(_, _))
        .Times(1);
    // If the prompt dialog is shown, the controller object will only be
    // destroyed after user interaction. This will force the object to be
    // deleted when the test ends.
    close_required = true;
  } else {
    EXPECT_CALL(*mock_prompt_delegate, OpenSettingsPage(_)).Times(1);
  }

  ChromeCleanerRebootDialogControllerImpl* dialog_controller =
      ChromeCleanerRebootDialogControllerImpl::Create(
          &mock_cleaner_controller_, std::move(mock_prompt_delegate));

  // Force interaction with the prompt to force deleting |dialog_controller|.
  if (close_required)
    dialog_controller->Close();
}

INSTANTIATE_TEST_CASE_P(AllTests,
                        ChromeCleanerRebootFlowTest,
                        ::testing::Bool());

class ChromeCleanerRebootDialogResponseTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitAndEnableFeature(kRebootPromptDialogFeature);

    // The implementation of dialog_controller may check state, and we are not
    // interested in ensuring how many times this is done, since it's not part
    // of the main functionality.
    EXPECT_CALL(mock_cleaner_controller_, state())
        .WillRepeatedly(
            Return(ChromeCleanerController::State::kRebootRequired));
  }

  ChromeCleanerRebootDialogControllerImpl* dialog_controller() {
    auto mock_prompt_delegate =
        base::MakeUnique<StrictMock<MockPromptDelegate>>();
    EXPECT_CALL(*mock_prompt_delegate, ShowChromeCleanerRebootPrompt(_, _))
        .Times(1);
    return ChromeCleanerRebootDialogControllerImpl::Create(
        &mock_cleaner_controller_, std::move(mock_prompt_delegate));
  }

 protected:
  StrictMock<MockChromeCleanerController> mock_cleaner_controller_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeCleanerRebootDialogResponseTest, Accept) {
  EXPECT_CALL(mock_cleaner_controller_, Reboot());

  dialog_controller()->Accept();
}

IN_PROC_BROWSER_TEST_F(ChromeCleanerRebootDialogResponseTest, Cancel) {
  dialog_controller()->Cancel();
}

IN_PROC_BROWSER_TEST_F(ChromeCleanerRebootDialogResponseTest, Close) {
  dialog_controller()->Close();
}

}  // namespace
}  // namespace safe_browsing
