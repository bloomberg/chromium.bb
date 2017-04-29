// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/csd.pb.h"
#include "components/safe_browsing_db/database_manager.h"
#include "content/public/test/mock_download_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;

namespace safe_browsing {

namespace {

const char kTestDownloadUrl[] = "http://evildownload.com";
const char kDownloadResponseToken[] = "default_token";

// An enum to parameterize the tests so that the tests can be run with and
// without the secondary-ui-md flag.
enum class SecondaryUiMd {
  ENABLED,
  DISABLED,
};

// Generates the test name suffix depending on the value of the SecondaryUiMd
// param.
std::string SecondaryUiMdStatusToString(
    const ::testing::TestParamInfo<SecondaryUiMd>& info) {
  switch (info.param) {
    case SecondaryUiMd::ENABLED:
      return "SecondaryUiMdEnabled";
    case SecondaryUiMd::DISABLED:
      return "SecondaryUiMdDisabled";
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

class DownloadDangerPromptTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<SecondaryUiMd> {
 public:
  DownloadDangerPromptTest()
      : prompt_(nullptr),
        expected_action_(DownloadDangerPrompt::CANCEL),
        did_receive_callback_(false),
        test_safe_browsing_factory_(
            base::MakeUnique<TestSafeBrowsingServiceFactory>()) {}

  ~DownloadDangerPromptTest() override {}

  void SetUp() override {
    SafeBrowsingService::RegisterFactory(test_safe_browsing_factory_.get());
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    SafeBrowsingService::RegisterFactory(nullptr);
    InProcessBrowserTest::TearDown();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(crbug.com/630357): Remove parameterized testing for this class when
    // secondary-ui-md is enabled by default on all platforms.
    if (GetParam() == SecondaryUiMd::ENABLED)
      command_line->AppendSwitch(switches::kExtendMdToSecondaryUi);
  }

  // Opens a new tab and waits for navigations to finish. If there are pending
  // navigations, the constrained prompt might be dismissed when the navigation
  // completes.
  void OpenNewTab() {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("about:blank"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
            ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  }

  void SetUpExpectations(
      const DownloadDangerPrompt::Action& expected_action,
      const content::DownloadDangerType& danger_type,
      const ClientDownloadResponse::Verdict& download_verdict,
      const std::string& token,
      bool from_download_api) {
    did_receive_callback_ = false;
    expected_action_ = expected_action;
    SetUpDownloadItemExpectations(danger_type, token);
    SetUpSafeBrowsingReportExpectations(
        expected_action == DownloadDangerPrompt::ACCEPT,
        download_verdict,
        token,
        from_download_api);
    CreatePrompt(from_download_api);
  }

  void VerifyExpectations(bool should_send_report) {
    content::RunAllPendingInMessageLoop();
    // At the end of each test, we expect no more activity from the prompt. The
    // prompt shouldn't exist anymore either.
    EXPECT_TRUE(did_receive_callback_);
    EXPECT_FALSE(prompt_);

    if (should_send_report) {
      EXPECT_EQ(expected_serialized_report_,
                test_safe_browsing_factory_->test_safe_browsing_service()
                    ->serilized_download_report());
    } else {
      EXPECT_TRUE(test_safe_browsing_factory_->test_safe_browsing_service()
                      ->serilized_download_report()
                      .empty());
    }
    testing::Mock::VerifyAndClearExpectations(&download_);
    test_safe_browsing_factory_->test_safe_browsing_service()
        ->ClearDownloadReport();
  }

  void SimulatePromptAction(DownloadDangerPrompt::Action action) {
    prompt_->InvokeActionForTesting(action);
  }

  content::MockDownloadItem& download() { return download_; }

  DownloadDangerPrompt* prompt() { return prompt_; }

 private:
  void SetUpDownloadItemExpectations(
      const content::DownloadDangerType& danger_type,
      const std::string& token) {
    EXPECT_CALL(download_, GetFileNameToReportUser()).WillRepeatedly(Return(
        base::FilePath(FILE_PATH_LITERAL("evil.exe"))));
    EXPECT_CALL(download_, GetDangerType()).WillRepeatedly(Return(danger_type));
    auto token_obj =
        base::MakeUnique<DownloadProtectionService::DownloadPingToken>(token);
    download_.SetUserData(DownloadProtectionService::kDownloadPingTokenKey,
                          std::move(token_obj));
  }

  void SetUpSafeBrowsingReportExpectations(
      bool did_proceed,
      const ClientDownloadResponse::Verdict& download_verdict,
      const std::string& token,
      bool from_download_api) {
    ClientSafeBrowsingReportRequest expected_report;
    expected_report.set_url(GURL(kTestDownloadUrl).spec());
    if (from_download_api)
      expected_report.set_type(
          ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_BY_API);
    else
      expected_report.set_type(
          ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_RECOVERY);
    expected_report.set_download_verdict(download_verdict);
    expected_report.set_did_proceed(did_proceed);
    if (!token.empty())
      expected_report.set_token(token);
    expected_report.SerializeToString(&expected_serialized_report_);
  }

  void CreatePrompt(bool from_download_api) {
    prompt_ = DownloadDangerPrompt::Create(
        &download_,
        browser()->tab_strip_model()->GetActiveWebContents(),
        from_download_api,
        base::Bind(&DownloadDangerPromptTest::PromptCallback,
                   base::Unretained(this)));
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
  std::unique_ptr<TestSafeBrowsingServiceFactory> test_safe_browsing_factory_;
  std::string expected_serialized_report_;

  DISALLOW_COPY_AND_ASSIGN(DownloadDangerPromptTest);
};

// Disabled for flaky timeouts on Windows. crbug.com/446696
#if defined(OS_WIN)
#define MAYBE_TestAll DISABLED_TestAll
#else
#define MAYBE_TestAll TestAll
#endif
IN_PROC_BROWSER_TEST_P(DownloadDangerPromptTest, MAYBE_TestAll) {
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
                    ClientDownloadResponse::DANGEROUS,
                    kDownloadResponseToken,
                    false);
  EXPECT_CALL(download(), IsDangerous()).WillRepeatedly(Return(true));
  SimulatePromptAction(DownloadDangerPrompt::ACCEPT);
  VerifyExpectations(true);

  // Clicking the Cancel button should invoke the CANCEL action.
  SetUpExpectations(DownloadDangerPrompt::CANCEL,
                    content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,
                    ClientDownloadResponse::UNCOMMON,
                    std::string(),
                    false);
  EXPECT_CALL(download(), IsDangerous()).WillRepeatedly(Return(true));
  SimulatePromptAction(DownloadDangerPrompt::CANCEL);
  VerifyExpectations(true);

  // If the download is no longer dangerous (because it was accepted), the
  // dialog should DISMISS itself.
  SetUpExpectations(DownloadDangerPrompt::DISMISS,
                    content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED,
                    ClientDownloadResponse::POTENTIALLY_UNWANTED,
                    kDownloadResponseToken,
                    false);
  EXPECT_CALL(download(), IsDangerous()).WillRepeatedly(Return(false));
  download().NotifyObserversDownloadUpdated();
  VerifyExpectations(false);

  // If the download is in a terminal state then the dialog should DISMISS
  // itself.
  SetUpExpectations(DownloadDangerPrompt::DISMISS,
                    content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST,
                    ClientDownloadResponse::DANGEROUS_HOST,
                    kDownloadResponseToken,
                    false);
  EXPECT_CALL(download(), IsDangerous()).WillRepeatedly(Return(true));
  EXPECT_CALL(download(), IsDone()).WillRepeatedly(Return(true));
  download().NotifyObserversDownloadUpdated();
  VerifyExpectations(false);

  // If the download is dangerous and is not in a terminal state, don't dismiss
  // the dialog.
  SetUpExpectations(DownloadDangerPrompt::ACCEPT,
                    content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT,
                    ClientDownloadResponse::DANGEROUS,
                    kDownloadResponseToken,
                    false);
  EXPECT_CALL(download(), IsDangerous()).WillRepeatedly(Return(true));
  EXPECT_CALL(download(), IsDone()).WillRepeatedly(Return(false));
  download().NotifyObserversDownloadUpdated();
  EXPECT_TRUE(prompt());
  SimulatePromptAction(DownloadDangerPrompt::ACCEPT);
  VerifyExpectations(true);

  // If the download is not dangerous, no report will be sent.
  SetUpExpectations(DownloadDangerPrompt::ACCEPT,
                    content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                    ClientDownloadResponse::SAFE,
                    kDownloadResponseToken,
                    false);
  SimulatePromptAction(DownloadDangerPrompt::ACCEPT);
  VerifyExpectations(false);

  // If the containing tab is closed, the dialog should DISMISS itself.
  OpenNewTab();
  SetUpExpectations(DownloadDangerPrompt::DISMISS,
                    content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
                    ClientDownloadResponse::DANGEROUS,
                    kDownloadResponseToken,
                    false);
  chrome::CloseTab(browser());
  VerifyExpectations(false);

  // If file is downloaded through download api, a confirm download dialog
  // instead of a recovery dialog is shown. Clicking the Accept button should
  // invoke the ACCEPT action, a report will be sent with type
  // DANGEROUS_DOWNLOAD_BY_API.
  SetUpExpectations(DownloadDangerPrompt::ACCEPT,
                    content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
                    ClientDownloadResponse::DANGEROUS,
                    kDownloadResponseToken,
                    true);
  EXPECT_CALL(download(), IsDangerous()).WillRepeatedly(Return(true));
  SimulatePromptAction(DownloadDangerPrompt::ACCEPT);
  VerifyExpectations(true);

  // If file is downloaded through download api, a confirm download dialog
  // instead of a recovery dialog is shown. Clicking the Cancel button should
  // invoke the CANCEL action, a report will be sent with type
  // DANGEROUS_DOWNLOAD_BY_API.
  SetUpExpectations(DownloadDangerPrompt::CANCEL,
                    content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,
                    ClientDownloadResponse::UNCOMMON,
                    std::string(),
                    true);
  EXPECT_CALL(download(), IsDangerous()).WillRepeatedly(Return(true));
  SimulatePromptAction(DownloadDangerPrompt::CANCEL);
  VerifyExpectations(true);
}

// Prefix for test instantiations intentionally left blank since the test
// fixture class has a single parameterization.
INSTANTIATE_TEST_CASE_P(,
                        DownloadDangerPromptTest,
                        ::testing::Values(SecondaryUiMd::ENABLED,
                                          SecondaryUiMd::DISABLED),
                        &SecondaryUiMdStatusToString);

}  // namespace safe_browsing
