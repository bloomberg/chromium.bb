// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
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
    : prompt_(NULL),
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

  DownloadDangerPrompt* prompt() { return prompt_; }

 private:
  void SetUpDownloadItemExpectations() {
    EXPECT_CALL(download_, GetFileNameToReportUser()).WillRepeatedly(Return(
        base::FilePath(FILE_PATH_LITERAL("evil.exe"))));
    EXPECT_CALL(download_, GetDangerType())
        .WillRepeatedly(Return(content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL));
  }

  void CreatePrompt() {
    prompt_ = DownloadDangerPrompt::Create(
        &download_,
        browser()->tab_strip_model()->GetActiveWebContents(),
        false,
        base::Bind(&DownloadDangerPromptTest::PromptCallback, this));
    content::RunAllPendingInMessageLoop();
  }

  void PromptCallback(DownloadDangerPrompt::Action action) {
    EXPECT_FALSE(did_receive_callback_);
    EXPECT_EQ(expected_action_, action);
    did_receive_callback_ = true;
    prompt_ = NULL;
  }

  content::MockDownloadItem download_;
  DownloadDangerPrompt* prompt_;
  DownloadDangerPrompt::Action expected_action_;
  bool did_receive_callback_;

  DISALLOW_COPY_AND_ASSIGN(DownloadDangerPromptTest);
};

IN_PROC_BROWSER_TEST_F(DownloadDangerPromptTest, TestAll) {
  OpenNewTab();

  // Clicking the Accept button should invoke the ACCEPT action.
  SetUpExpectations(DownloadDangerPrompt::ACCEPT);
  SimulatePromptAction(DownloadDangerPrompt::ACCEPT);
  VerifyExpectations();

  // Clicking the Cancel button should invoke the CANCEL action.
  SetUpExpectations(DownloadDangerPrompt::CANCEL);
  SimulatePromptAction(DownloadDangerPrompt::CANCEL);
  VerifyExpectations();

  // If the download is no longer dangerous (because it was accepted), the
  // dialog should DISMISS itself.
  SetUpExpectations(DownloadDangerPrompt::DISMISS);
  EXPECT_CALL(download(), IsDangerous()).WillOnce(Return(false));
  download().NotifyObserversDownloadUpdated();
  VerifyExpectations();

  // If the download is in a terminal state then the dialog should DISMISS
  // itself.
  SetUpExpectations(DownloadDangerPrompt::DISMISS);
  EXPECT_CALL(download(), IsDangerous()).WillOnce(Return(true));
  EXPECT_CALL(download(), IsDone()).WillOnce(Return(true));
  download().NotifyObserversDownloadUpdated();
  VerifyExpectations();

  // If the download is dangerous and is not in a terminal state, don't dismiss
  // the dialog.
  SetUpExpectations(DownloadDangerPrompt::ACCEPT);
  EXPECT_CALL(download(), IsDangerous()).WillOnce(Return(true));
  EXPECT_CALL(download(), IsDone()).WillOnce(Return(false));
  download().NotifyObserversDownloadUpdated();
  SimulatePromptAction(DownloadDangerPrompt::ACCEPT);
  VerifyExpectations();

  // If the containing tab is closed, the dialog should DISMISS itself.
  OpenNewTab();
  SetUpExpectations(DownloadDangerPrompt::DISMISS);
  chrome::CloseTab(browser());
  VerifyExpectations();
}
