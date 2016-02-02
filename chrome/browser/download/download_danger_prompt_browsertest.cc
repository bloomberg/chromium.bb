// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing_db/database_manager.h"
#include "content/public/test/mock_download_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using safe_browsing::ClientDownloadResponse;
using safe_browsing::ClientSafeBrowsingReportRequest;
using safe_browsing::SafeBrowsingService;

const char kTestDownloadUrl[] = "http://evildownload.com";

class FakeSafeBrowsingService : public SafeBrowsingService {
 public:
  FakeSafeBrowsingService() {}

  void SendDownloadRecoveryReport(const std::string& report) override {
    report_ = report;
  }

  std::string GetDownloadRecoveryReport() const { return report_; }

 protected:
  ~FakeSafeBrowsingService() override {}

 private:
  std::string report_;
};

// Factory that creates FakeSafeBrowsingService instances.
class TestSafeBrowsingServiceFactory
    : public safe_browsing::SafeBrowsingServiceFactory {
 public:
  TestSafeBrowsingServiceFactory() : fake_safe_browsing_service_(nullptr) {}
  ~TestSafeBrowsingServiceFactory() override {}

  SafeBrowsingService* CreateSafeBrowsingService() override {
    if (!fake_safe_browsing_service_) {
      fake_safe_browsing_service_ = new FakeSafeBrowsingService();
    }
    return fake_safe_browsing_service_.get();
  }

  scoped_refptr<FakeSafeBrowsingService> fake_safe_browsing_service() {
    return fake_safe_browsing_service_;
  }

 private:
  scoped_refptr<FakeSafeBrowsingService> fake_safe_browsing_service_;
};

class DownloadDangerPromptTest : public InProcessBrowserTest {
 public:
  DownloadDangerPromptTest()
      : prompt_(nullptr),
        expected_action_(DownloadDangerPrompt::CANCEL),
        did_receive_callback_(false),
        test_safe_browsing_factory_(new TestSafeBrowsingServiceFactory()),
        report_sent_(false) {}

  ~DownloadDangerPromptTest() override {}

  void SetUp() override {
    SafeBrowsingService::RegisterFactory(test_safe_browsing_factory_.get());
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    SafeBrowsingService::RegisterFactory(nullptr);
    InProcessBrowserTest::TearDown();
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

  void SetUpExpectations(
      const DownloadDangerPrompt::Action& expected_action,
      const content::DownloadDangerType& danger_type,
      const ClientDownloadResponse::Verdict& download_verdict) {
    did_receive_callback_ = false;
    expected_action_ = expected_action;
    SetUpDownloadItemExpectations(danger_type);
    SetUpSafeBrowsingReportExpectations(
        expected_action == DownloadDangerPrompt::ACCEPT, download_verdict);
    CreatePrompt();
    report_sent_ = false;
  }

  void VerifyExpectations() {
    content::RunAllPendingInMessageLoop();
    // At the end of each test, we expect no more activity from the prompt. The
    // prompt shouldn't exist anymore either.
    EXPECT_TRUE(did_receive_callback_);
    EXPECT_FALSE(prompt_);
    testing::Mock::VerifyAndClearExpectations(&download_);
    if (report_sent_) {
      EXPECT_EQ(expected_serialized_report_,
                test_safe_browsing_factory_->fake_safe_browsing_service()
                    ->GetDownloadRecoveryReport());
    }
  }

  void SimulatePromptAction(DownloadDangerPrompt::Action action) {
    prompt_->InvokeActionForTesting(action);
    report_sent_ = true;
  }

  content::MockDownloadItem& download() { return download_; }

  DownloadDangerPrompt* prompt() { return prompt_; }

 private:
  void SetUpDownloadItemExpectations(
      const content::DownloadDangerType& danger_type) {
    EXPECT_CALL(download_, GetFileNameToReportUser()).WillRepeatedly(Return(
        base::FilePath(FILE_PATH_LITERAL("evil.exe"))));
    EXPECT_CALL(download_, GetDangerType()).WillRepeatedly(Return(danger_type));
  }

  void SetUpSafeBrowsingReportExpectations(
      bool did_proceed,
      const ClientDownloadResponse::Verdict& download_verdict) {
    ClientSafeBrowsingReportRequest expected_report;
    expected_report.set_url(GURL(kTestDownloadUrl).spec());
    expected_report.set_type(
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_RECOVERY);
    expected_report.set_download_verdict(download_verdict);
    expected_report.set_did_proceed(did_proceed);
    expected_report.SerializeToString(&expected_serialized_report_);
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
    prompt_ = nullptr;
  }

  content::MockDownloadItem download_;
  DownloadDangerPrompt* prompt_;
  DownloadDangerPrompt::Action expected_action_;
  bool did_receive_callback_;
  scoped_ptr<TestSafeBrowsingServiceFactory> test_safe_browsing_factory_;
  std::string expected_serialized_report_;
  bool report_sent_;

  DISALLOW_COPY_AND_ASSIGN(DownloadDangerPromptTest);
};

// Disabled for flaky timeouts on Windows. crbug.com/446696
#if defined(OS_WIN)
#define MAYBE_TestAll DISABLED_TestAll
#else
#define MAYBE_TestAll TestAll
#endif
IN_PROC_BROWSER_TEST_F(DownloadDangerPromptTest, MAYBE_TestAll) {
  // ExperienceSampling: Set default actions for DownloadItem methods we need.
  GURL download_url(kTestDownloadUrl);
  ON_CALL(download(), GetURL()).WillByDefault(ReturnRef(download_url));
  ON_CALL(download(), GetReferrerUrl())
      .WillByDefault(ReturnRef(GURL::EmptyGURL()));
  ON_CALL(download(), GetBrowserContext())
      .WillByDefault(Return(browser()->profile()));
  base::FilePath empty_file_path;
  ON_CALL(download(), GetTargetFilePath())
      .WillByDefault(ReturnRef(empty_file_path));

  OpenNewTab();

  // Clicking the Accept button should invoke the ACCEPT action.
  SetUpExpectations(DownloadDangerPrompt::ACCEPT,
                    content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
                    ClientDownloadResponse::DANGEROUS);
  SimulatePromptAction(DownloadDangerPrompt::ACCEPT);
  VerifyExpectations();

  // Clicking the Cancel button should invoke the CANCEL action.
  SetUpExpectations(DownloadDangerPrompt::CANCEL,
                    content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,
                    ClientDownloadResponse::UNCOMMON);
  SimulatePromptAction(DownloadDangerPrompt::CANCEL);
  VerifyExpectations();

  // If the download is no longer dangerous (because it was accepted), the
  // dialog should DISMISS itself.
  SetUpExpectations(DownloadDangerPrompt::DISMISS,
                    content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED,
                    ClientDownloadResponse::POTENTIALLY_UNWANTED);
  EXPECT_CALL(download(), IsDangerous()).WillOnce(Return(false));
  download().NotifyObserversDownloadUpdated();
  VerifyExpectations();

  // If the download is in a terminal state then the dialog should DISMISS
  // itself.
  SetUpExpectations(DownloadDangerPrompt::DISMISS,
                    content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST,
                    ClientDownloadResponse::DANGEROUS_HOST);
  EXPECT_CALL(download(), IsDangerous()).WillOnce(Return(true));
  EXPECT_CALL(download(), IsDone()).WillOnce(Return(true));
  download().NotifyObserversDownloadUpdated();
  VerifyExpectations();

  // If the download is dangerous and is not in a terminal state, don't dismiss
  // the dialog.
  SetUpExpectations(DownloadDangerPrompt::ACCEPT,
                    content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT,
                    ClientDownloadResponse::DANGEROUS);
  EXPECT_CALL(download(), IsDangerous()).WillOnce(Return(true));
  EXPECT_CALL(download(), IsDone()).WillOnce(Return(false));
  download().NotifyObserversDownloadUpdated();
  SimulatePromptAction(DownloadDangerPrompt::ACCEPT);
  VerifyExpectations();

  // If the containing tab is closed, the dialog should DISMISS itself.
  OpenNewTab();
  SetUpExpectations(DownloadDangerPrompt::DISMISS,
                    content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
                    ClientDownloadResponse::DANGEROUS);
  chrome::CloseTab(browser());
  VerifyExpectations();
}
