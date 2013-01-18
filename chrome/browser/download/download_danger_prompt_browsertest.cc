// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_path.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/mock_download_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Return;
using ::testing::SaveArg;

class DownloadDangerPromptTest : public InProcessBrowserTest {
 public:
  DownloadDangerPromptTest()
    : download_observer_(NULL),
      prompt_(NULL),
      expected_action_(DownloadDangerPrompt::CANCEL),
      did_receive_callback_(false) {
  }

  virtual ~DownloadDangerPromptTest() {
  }

  // Opens a new tab and waits for navigations to finish. If there are pending
  // navigations, the constrained prompt might be dismissed when the navigation
  // completes.
  void OpenNewTab() {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("about:blank"),
        NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  }

  void SetUpExpectations(DownloadDangerPrompt::Action expected_action) {
    did_receive_callback_ = false;
    expected_action_ = expected_action;
    SetUpDownloadItemExpectations();
    CreatePrompt();
  }

  void VerifyExpectations() {
    content::RunAllPendingInMessageLoop();
    // At the end of each test, we expect no more activity from the prompt. The
    // prompt shouldn't exist anymore either.
    EXPECT_TRUE(did_receive_callback_);
    EXPECT_FALSE(prompt_);
    testing::Mock::VerifyAndClearExpectations(&download_);
  }

  void SimulatePromptAction(DownloadDangerPrompt::Action action) {
    prompt_->InvokeActionForTesting(action);
  }

  content::MockDownloadItem& download() { return download_; }

  content::DownloadItem::Observer* download_observer() {
    return download_observer_;
  }

  DownloadDangerPrompt* prompt() { return prompt_; }

 private:
  void SetUpDownloadItemExpectations() {
    EXPECT_CALL(download_, GetFileNameToReportUser()).WillRepeatedly(Return(
        FilePath(FILE_PATH_LITERAL("evil.exe"))));
    EXPECT_CALL(download_, AddObserver(_))
      .WillOnce(SaveArg<0>(&download_observer_));
    EXPECT_CALL(download_, RemoveObserver(Eq(ByRef(download_observer_))));
  }

  void CreatePrompt() {
    prompt_ = DownloadDangerPrompt::Create(
        &download_,
        browser()->tab_strip_model()->GetActiveWebContents(),
        false,
        base::Bind(&DownloadDangerPromptTest::PromptCallback, this,
                   DownloadDangerPrompt::ACCEPT),
        base::Bind(&DownloadDangerPromptTest::PromptCallback, this,
                   DownloadDangerPrompt::CANCEL));
    content::RunAllPendingInMessageLoop();
  }

  void PromptCallback(DownloadDangerPrompt::Action action) {
    EXPECT_FALSE(did_receive_callback_);
    EXPECT_EQ(expected_action_, action);
    did_receive_callback_ = true;
    prompt_ = NULL;
  }

  content::MockDownloadItem download_;
  content::DownloadItem::Observer* download_observer_;
  DownloadDangerPrompt* prompt_;
  DownloadDangerPrompt::Action expected_action_;
  bool did_receive_callback_;

  DISALLOW_COPY_AND_ASSIGN(DownloadDangerPromptTest);
};

IN_PROC_BROWSER_TEST_F(DownloadDangerPromptTest, TestAll) {
  OpenNewTab();

  // The Accept action should cause the accept callback to be invoked.
  SetUpExpectations(DownloadDangerPrompt::ACCEPT);
  SimulatePromptAction(DownloadDangerPrompt::ACCEPT);
  VerifyExpectations();

  // The Discard action should cause the discard callback to be invoked.
  SetUpExpectations(DownloadDangerPrompt::CANCEL);
  SimulatePromptAction(DownloadDangerPrompt::CANCEL);
  VerifyExpectations();

  // If the download is no longer in-progress, the dialog should dismiss itself.
  SetUpExpectations(DownloadDangerPrompt::CANCEL);
  EXPECT_CALL(download(), IsInProgress()).WillOnce(Return(false));
  download_observer()->OnDownloadUpdated(&download());
  VerifyExpectations();

  // If the download is no longer dangerous (because it was accepted), the
  // dialog should dismiss itself.
  SetUpExpectations(DownloadDangerPrompt::CANCEL);
  EXPECT_CALL(download(), IsInProgress()).WillOnce(Return(true));
  EXPECT_CALL(download(), IsDangerous()).WillOnce(Return(false));
  download_observer()->OnDownloadUpdated(&download());
  VerifyExpectations();

  // If the containing tab is closed, the dialog should be canceled.
  OpenNewTab();
  SetUpExpectations(DownloadDangerPrompt::CANCEL);
  chrome::CloseTab(browser());
  VerifyExpectations();
}
