// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>

#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_browsertest_base.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_views.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"

using extensions::SafeBrowsingPrivateEventRouter;
using ::testing::_;
using ::testing::Mock;

namespace safe_browsing {

namespace {

class FakeBinaryUploadService : public BinaryUploadService {
 public:
  FakeBinaryUploadService() : BinaryUploadService(nullptr, nullptr, nullptr) {}

  // Sets whether the user is authorized to upload data for Deep Scanning.
  void SetAuthorized(bool authorized) {
    authorization_result_ = authorized
                                ? BinaryUploadService::Result::SUCCESS
                                : BinaryUploadService::Result::UNAUTHORIZED;
  }

  // Finish the authentication request. Called after ShowForWebContents to
  // simulate an async callback.
  void ReturnAuthorizedResponse() {
    authorization_request_->FinishRequest(authorization_result_,
                                          DeepScanningClientResponse());
  }

  void SetResponseForText(BinaryUploadService::Result result,
                          const DeepScanningClientResponse& response) {
    prepared_text_result_ = result;
    prepared_text_response_ = response;
  }

  void SetResponseForFile(const std::string& path,
                          BinaryUploadService::Result result,
                          const DeepScanningClientResponse& response) {
    prepared_file_results_[path] = result;
    prepared_file_responses_[path] = response;
  }

  void SetShouldAutomaticallyAuthorize(bool authorize) {
    should_automatically_authorize_ = authorize;
  }

  int requests_count() const { return requests_count_; }

 private:
  void UploadForDeepScanning(std::unique_ptr<Request> request) override {
    // The first uploaded request is the authentication one.
    if (++requests_count_ == 1) {
      authorization_request_.swap(request);

      if (should_automatically_authorize_)
        ReturnAuthorizedResponse();
    } else {
      std::string file = request->deep_scanning_request().filename();
      if (file.empty()) {
        request->FinishRequest(prepared_text_result_, prepared_text_response_);
      } else {
        ASSERT_TRUE(prepared_file_results_.count(file));
        ASSERT_TRUE(prepared_file_responses_.count(file));
        request->FinishRequest(prepared_file_results_[file],
                               prepared_file_responses_[file]);
      }
    }
  }

  BinaryUploadService::Result authorization_result_;
  std::unique_ptr<Request> authorization_request_;
  BinaryUploadService::Result prepared_text_result_;
  DeepScanningClientResponse prepared_text_response_;
  std::map<std::string, BinaryUploadService::Result> prepared_file_results_;
  std::map<std::string, DeepScanningClientResponse> prepared_file_responses_;
  int requests_count_ = 0;
  bool should_automatically_authorize_ = false;
};

FakeBinaryUploadService* FakeBinaryUploadServiceStorage() {
  static FakeBinaryUploadService service;
  return &service;
}

const std::set<std::string>* DocMimeTypes() {
  static std::set<std::string> set = {
      "application/msword",
      // The 50 MB file can result in no mimetype being found.
      ""};
  return &set;
}

const std::set<std::string>* ExeMimeTypes() {
  static std::set<std::string> set = {"application/x-msdownload",
                                      "application/x-ms-dos-executable",
                                      "application/octet-stream"};
  return &set;
}

const std::set<std::string>* ZipMimeTypes() {
  static std::set<std::string> set = {"application/zip",
                                      "application/x-zip-compressed"};
  return &set;
}

const std::set<std::string>* ShellScriptMimeTypes() {
  static std::set<std::string> set = {"text/x-sh", "application/x-shellscript"};
  return &set;
}

const std::set<std::string>* TextMimeTypes() {
  static std::set<std::string> set = {"text/plain"};
  return &set;
}

// A fake delegate with minimal overrides to obtain behavior that's as close to
// the real one as possible.
class MinimalFakeDeepScanningDialogDelegate
    : public DeepScanningDialogDelegate {
 public:
  MinimalFakeDeepScanningDialogDelegate(
      content::WebContents* web_contents,
      DeepScanningDialogDelegate::Data data,
      DeepScanningDialogDelegate::CompletionCallback callback)
      : DeepScanningDialogDelegate(web_contents,
                                   std::move(data),
                                   std::move(callback),
                                   DeepScanAccessPoint::UPLOAD) {}

  static std::unique_ptr<DeepScanningDialogDelegate> Create(
      content::WebContents* web_contents,
      DeepScanningDialogDelegate::Data data,
      DeepScanningDialogDelegate::CompletionCallback callback) {
    return std::make_unique<MinimalFakeDeepScanningDialogDelegate>(
        web_contents, std::move(data), std::move(callback));
  }

 private:
  BinaryUploadService* GetBinaryUploadService() override {
    return FakeBinaryUploadServiceStorage();
  }
};

constexpr char kDmToken[] = "dm_token";

constexpr char kTestUrl[] = "https://google.com";

}  // namespace

// Tests the behavior of the dialog delegate with minimal overriding of methods.
// Only responses obtained via the BinaryUploadService are faked.
class DeepScanningDialogDelegateBrowserTest
    : public DeepScanningBrowserTestBase,
      public DeepScanningDialogViews::TestObserver {
 public:
  DeepScanningDialogDelegateBrowserTest() {
    DeepScanningDialogViews::SetObserverForTesting(this);
  }

  void EnableUploadsScanningAndReporting() {
    SetDMTokenForTesting(policy::DMToken::CreateValidTokenForTesting(kDmToken));

    SetDlpPolicy(CHECK_UPLOADS);
    SetMalwarePolicy(SEND_UPLOADS);
    SetWaitPolicy(DELAY_UPLOADS);
    SetUnsafeEventsReportingPolicy(true);

    // Add the wildcard pattern to this policy since malware responses are
    // verified for most of these tests.
    ListPrefUpdate(g_browser_process->local_state(),
                   prefs::kURLsToCheckForMalwareOfUploadedContent)
        ->Append("*");

    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
        browser()->profile())
        ->SetCloudPolicyClientForTesting(client_.get());
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
        browser()->profile())
        ->SetBinaryUploadServiceForTesting(FakeBinaryUploadServiceStorage());
  }

  void DestructorCalled(DeepScanningDialogViews* views) override {
    // The test is over once the views are destroyed.
    CallQuitClosure();
  }

  void ExpectNoReport() {
    EXPECT_CALL(*client_, UploadRealtimeReport_(_, _)).Times(0);
  }

  policy::MockCloudPolicyClient* client() { return client_.get(); }

 private:
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(DeepScanningDialogDelegateBrowserTest, Unauthorized) {
  EnableUploadsScanningAndReporting();

  DeepScanningDialogDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeDeepScanningDialogDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(false);

  bool called = false;
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = true;
  data.do_malware_scan = true;
  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  // Nothing should be reported for unauthorized users.
  ExpectNoReport();

  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&quit_closure, &called](
              const DeepScanningDialogDelegate::Data& data,
              const DeepScanningDialogDelegate::Result& result) {
            ASSERT_EQ(result.text_results.size(), 1u);
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_TRUE(result.text_results[0]);
            ASSERT_TRUE(result.paths_results[0]);
            called = true;
            quit_closure.Run();
          }),
      DeepScanAccessPoint::UPLOAD);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // Only 1 request (the authentication one) should have been uploaded.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 1);
}

IN_PROC_BROWSER_TEST_F(DeepScanningDialogDelegateBrowserTest, Files) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  DeepScanningDialogDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeDeepScanningDialogDelegate::Create));

  DeepScanningClientResponse ok_response;
  ok_response.mutable_dlp_scan_verdict()->set_status(
      DlpDeepScanningVerdict::SUCCESS);
  ok_response.mutable_malware_scan_verdict()->set_verdict(
      MalwareDeepScanningVerdict::CLEAN);

  DeepScanningClientResponse bad_response;
  bad_response.mutable_dlp_scan_verdict()->set_status(
      DlpDeepScanningVerdict::SUCCESS);
  bad_response.mutable_malware_scan_verdict()->set_verdict(
      MalwareDeepScanningVerdict::MALWARE);

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // Create the files to be opened and scanned.
  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = true;
  data.do_malware_scan = true;
  CreateFilesForTest({"ok.doc", "bad.exe"},
                     {"ok file content", "bad file content"}, &data);
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  // The malware verdict means an event should be reported.
  EventReportValidator validator(client());
  validator.ExpectDangerousDeepScanningResult(
      /*url*/ "about:blank",
      /*filename*/ created_file_paths()[1].AsUTF8Unsafe(),
      // printf "bad file content" | sha256sum |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "77AE96C38386429D28E53F5005C46C7B4D8D39BE73D757CE61E0AE65CC1A5A5D",
      /*threat_type*/ "DANGEROUS",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*mimetypes*/ ExeMimeTypes(),
      /*size*/ std::string("bad file content").size());

  FakeBinaryUploadServiceStorage()->SetResponseForFile(
      "ok.doc", BinaryUploadService::Result::SUCCESS, ok_response);
  FakeBinaryUploadServiceStorage()->SetResponseForFile(
      "bad.exe", BinaryUploadService::Result::SUCCESS, bad_response);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 2u);
            ASSERT_TRUE(result.paths_results[0]);
            ASSERT_FALSE(result.paths_results[1]);
            called = true;
          }),
      DeepScanAccessPoint::UPLOAD);

  run_loop.Run();

  EXPECT_TRUE(called);

  // There should have been 1 request per file and 1 for authentication.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 3);
}

class DeepScanningDialogDelegatePasswordProtectedFilesBrowserTest
    : public DeepScanningDialogDelegateBrowserTest,
      public testing::WithParamInterface<AllowPasswordProtectedFilesValues> {
 public:
  using DeepScanningDialogDelegateBrowserTest::
      DeepScanningDialogDelegateBrowserTest;

  AllowPasswordProtectedFilesValues allow_password_protected_files() const {
    return GetParam();
  }

  bool expected_result() const {
    switch (allow_password_protected_files()) {
      case ALLOW_NONE:
      case ALLOW_DOWNLOADS:
        return false;
      case ALLOW_UPLOADS:
      case ALLOW_UPLOADS_AND_DOWNLOADS:
        return true;
    }
  }
};

IN_PROC_BROWSER_TEST_P(
    DeepScanningDialogDelegatePasswordProtectedFilesBrowserTest,
    Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  SetAllowPasswordProtectedFilesPolicy(allow_password_protected_files());

  DeepScanningDialogDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeDeepScanningDialogDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = true;
  data.do_malware_scan = true;
  data.paths.emplace_back(test_zip);
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  // The file should be reported as unscanned.
  EventReportValidator validator(client());
  validator.ExpectUnscannedFileEvent(
      /*url*/ "about:blank",
      /*filename*/ test_zip.AsUTF8Unsafe(),
      // sha256sum < chrome/test/data/safe_browsing/download_protection/\
      // encrypted.zip |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "701FCEA8B2112FFAB257A8A8DFD3382ABCF047689AB028D42903E3B3AA488D9A",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*reason*/ "filePasswordProtected",
      /*mimetypes*/ ZipMimeTypes(),
      // du chrome/test/data/safe_browsing/download_protection/encrypted.zip -b
      /*size*/ 20015);

  // Start test.
  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const DeepScanningDialogDelegate::Data& data,
                          const DeepScanningDialogDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_EQ(result.paths_results[0], expected_result());
            called = true;
          }),
      DeepScanAccessPoint::UPLOAD);

  run_loop.Run();
  EXPECT_TRUE(called);

  // Expect 1 request for authentication needed to report the unscanned file
  // event.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    DeepScanningDialogDelegatePasswordProtectedFilesBrowserTest,
    DeepScanningDialogDelegatePasswordProtectedFilesBrowserTest,
    testing::Values(ALLOW_NONE,
                    ALLOW_DOWNLOADS,
                    ALLOW_UPLOADS,
                    ALLOW_UPLOADS_AND_DOWNLOADS));

class DeepScanningDialogDelegateBlockUnsupportedFileTypesBrowserTest
    : public DeepScanningDialogDelegateBrowserTest,
      public testing::WithParamInterface<BlockUnsupportedFiletypesValues> {
 public:
  using DeepScanningDialogDelegateBrowserTest::
      DeepScanningDialogDelegateBrowserTest;

  BlockUnsupportedFiletypesValues block_unsupported_file_types() const {
    return GetParam();
  }

  bool expected_result() const {
    switch (GetParam()) {
      case BLOCK_UNSUPPORTED_FILETYPES_NONE:
      case BLOCK_UNSUPPORTED_FILETYPES_DOWNLOADS:
        return true;
      case BLOCK_UNSUPPORTED_FILETYPES_UPLOADS:
      case BLOCK_UNSUPPORTED_FILETYPES_UPLOADS_AND_DOWNLOADS:
        return false;
    }
  }
};

IN_PROC_BROWSER_TEST_P(
    DeepScanningDialogDelegateBlockUnsupportedFileTypesBrowserTest,
    Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  SetBlockUnsupportedFileTypesPolicy(block_unsupported_file_types());
  ListPrefUpdate(g_browser_process->local_state(),
                 prefs::kURLsToCheckForMalwareOfUploadedContent)
      ->Clear();

  DeepScanningDialogDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeDeepScanningDialogDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // Create the files with unsupported types.
  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = true;
  data.do_malware_scan = false;
  CreateFilesForTest({"a.sh"}, {"file content"}, &data);
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  // The file should be reported as unscanned.
  EventReportValidator validator(client());
  validator.ExpectUnscannedFileEvent(
      /*url*/ "about:blank",
      /*filename*/ created_file_paths()[0].AsUTF8Unsafe(),
      // printf "file content" | sha256sum |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "E0AC3601005DFA1864F5392AABAF7D898B1B5BAB854F1ACB4491BCD806B76B0C",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*reason*/ "unsupportedFileType",
      /*mimetype*/ ShellScriptMimeTypes(),
      /*size*/ std::string("file content").size());

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const DeepScanningDialogDelegate::Data& data,
                          const DeepScanningDialogDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_EQ(result.paths_results[0], expected_result());

            called = true;
          }),
      DeepScanAccessPoint::UPLOAD);

  run_loop.Run();
  EXPECT_TRUE(called);

  // Expect 1 request for authentication needed to report the unscanned file
  // event.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    DeepScanningDialogDelegateBlockUnsupportedFileTypesBrowserTest,
    DeepScanningDialogDelegateBlockUnsupportedFileTypesBrowserTest,
    testing::Values(BLOCK_UNSUPPORTED_FILETYPES_NONE,
                    BLOCK_UNSUPPORTED_FILETYPES_DOWNLOADS,
                    BLOCK_UNSUPPORTED_FILETYPES_UPLOADS,
                    BLOCK_UNSUPPORTED_FILETYPES_UPLOADS_AND_DOWNLOADS));

class DeepScanningDialogDelegateBlockLargeFileTransferBrowserTest
    : public DeepScanningDialogDelegateBrowserTest,
      public testing::WithParamInterface<BlockLargeFileTransferValues> {
 public:
  using DeepScanningDialogDelegateBrowserTest::
      DeepScanningDialogDelegateBrowserTest;

  BlockLargeFileTransferValues block_large_file_transfer() const {
    return GetParam();
  }

  bool expected_result() const {
    switch (block_large_file_transfer()) {
      case BLOCK_NONE:
      case BLOCK_LARGE_DOWNLOADS:
        return true;
      case BLOCK_LARGE_UPLOADS:
      case BLOCK_LARGE_UPLOADS_AND_DOWNLOADS:
        return false;
    }
  }
};

IN_PROC_BROWSER_TEST_P(
    DeepScanningDialogDelegateBlockLargeFileTransferBrowserTest,
    Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  SetBlockLargeFileTransferPolicy(block_large_file_transfer());

  DeepScanningDialogDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeDeepScanningDialogDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // Create the large file.
  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = true;
  data.do_malware_scan = true;

  CreateFilesForTest(
      {"large.doc"},
      {std::string(BinaryUploadService::kMaxUploadSizeBytes + 1, 'a')}, &data);
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  // The file should be reported as unscanned.
  EventReportValidator validator(client());
  validator.ExpectUnscannedFileEvent(
      /*url*/ "about:blank",
      /*filename*/ created_file_paths()[0].AsUTF8Unsafe(),
      // python3 -c "print('a' * (50 * 1024 * 1024 + 1), end='')" | sha256sum |\
      // tr '[:lower:]' '[:upper:]'
      /*sha*/
      "9EB56DB30C49E131459FE735BA6B9D38327376224EC8D5A1233F43A5B4A25942",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*reason*/ "fileTooLarge",
      /*mimetypes*/ DocMimeTypes(),
      /*size*/ BinaryUploadService::kMaxUploadSizeBytes + 1);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const DeepScanningDialogDelegate::Data& data,
                          const DeepScanningDialogDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_EQ(result.paths_results[0], expected_result());

            called = true;
          }),
      DeepScanAccessPoint::UPLOAD);

  run_loop.Run();
  EXPECT_TRUE(called);

  // Expect 1 request for authentication needed to report the unscanned file
  // event.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    DeepScanningDialogDelegateBlockLargeFileTransferBrowserTest,
    DeepScanningDialogDelegateBlockLargeFileTransferBrowserTest,
    testing::Values(BLOCK_NONE,
                    BLOCK_LARGE_DOWNLOADS,
                    BLOCK_LARGE_UPLOADS,
                    BLOCK_LARGE_UPLOADS_AND_DOWNLOADS));

class DeepScanningDialogDelegateDelayDeliveryUntilVerdictTest
    : public DeepScanningDialogDelegateBrowserTest,
      public testing::WithParamInterface<DelayDeliveryUntilVerdictValues> {
 public:
  using DeepScanningDialogDelegateBrowserTest::
      DeepScanningDialogDelegateBrowserTest;

  DelayDeliveryUntilVerdictValues delay_delivery_until_verdict() const {
    return GetParam();
  }

  bool expected_result() const {
    switch (delay_delivery_until_verdict()) {
      case DELAY_NONE:
      case DELAY_DOWNLOADS:
        return true;
      case DELAY_UPLOADS:
      case DELAY_UPLOADS_AND_DOWNLOADS:
        return false;
    }
  }
};

IN_PROC_BROWSER_TEST_P(DeepScanningDialogDelegateDelayDeliveryUntilVerdictTest,
                       Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  SetWaitPolicy(delay_delivery_until_verdict());

  DeepScanningDialogDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeDeepScanningDialogDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  DeepScanningClientResponse response;
  response.mutable_malware_scan_verdict()->set_verdict(
      MalwareDeepScanningVerdict::MALWARE);
  response.mutable_dlp_scan_verdict()->set_status(
      DlpDeepScanningVerdict::SUCCESS);
  response.mutable_dlp_scan_verdict()->add_triggered_rules()->set_action(
      DlpDeepScanningVerdict::TriggeredRule::BLOCK);

  FakeBinaryUploadServiceStorage()->SetResponseForFile(
      "foo.doc", BinaryUploadService::Result::SUCCESS, response);

  // Create a file.
  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = true;
  data.do_malware_scan = true;

  CreateFilesForTest({"foo.doc"}, {"foo content"}, &data);
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  // The file should be reported as malware and sensitive content.
  EventReportValidator validator(client());
  validator.ExpectDangerousDeepScanningResultAndSensitiveDataEvent(
      /*url*/ "about:blank",
      /*filename*/ created_file_paths()[0].AsUTF8Unsafe(),
      // printf "foo content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "B3A2E2EDBAA3C798B4FC267792B1641B94793DE02D870124E5CBE663750B4CFC",
      /*threat_type*/ "DANGEROUS",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*dlp_verdict*/ response.dlp_scan_verdict(),
      /*mimetypes*/ DocMimeTypes(),
      /*size*/ std::string("foo content").size());

  bool called = false;
  base::RunLoop run_loop;

  // If the delivery is not delayed, put the quit closure right after the events
  // are reported instead of when the dialog closes.
  if (expected_result())
    validator.SetDoneClosure(run_loop.QuitClosure());
  else
    SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const DeepScanningDialogDelegate::Data& data,
                          const DeepScanningDialogDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_EQ(result.paths_results[0], expected_result());

            called = true;
          }),
      DeepScanAccessPoint::UPLOAD);

  run_loop.Run();
  EXPECT_TRUE(called);

  // Expect 1 request for authentication and 1 to scan the file in all cases.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 2);
}

INSTANTIATE_TEST_SUITE_P(
    DeepScanningDialogDelegateDelayDeliveryUntilVerdictTest,
    DeepScanningDialogDelegateDelayDeliveryUntilVerdictTest,
    testing::Values(DELAY_NONE,
                    DELAY_DOWNLOADS,
                    DELAY_UPLOADS,
                    DELAY_UPLOADS_AND_DOWNLOADS));

IN_PROC_BROWSER_TEST_F(DeepScanningDialogDelegateBrowserTest, Texts) {
  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  DeepScanningDialogDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeDeepScanningDialogDelegate::Create));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);

  // Prepare a complex DLP response to test that the verdict is reported
  // correctly in the sensitive data event.
  DeepScanningClientResponse response;
  DlpDeepScanningVerdict* verdict = response.mutable_dlp_scan_verdict();
  verdict->set_status(DlpDeepScanningVerdict::SUCCESS);

  DlpDeepScanningVerdict::TriggeredRule* rule1 = verdict->add_triggered_rules();
  rule1->set_rule_id(1);
  rule1->set_action(DlpDeepScanningVerdict::TriggeredRule::REPORT_ONLY);
  rule1->set_rule_resource_name("resource name 1");
  rule1->set_rule_severity("severity 1");
  DlpDeepScanningVerdict::MatchedDetector* detector1 =
      rule1->add_matched_detectors();
  detector1->set_detector_id("id1");
  detector1->set_detector_type("dlp1");
  detector1->set_display_name("display name 1");

  DlpDeepScanningVerdict::TriggeredRule* rule2 = verdict->add_triggered_rules();
  rule2->set_rule_id(3);
  rule2->set_action(DlpDeepScanningVerdict::TriggeredRule::BLOCK);
  rule2->set_rule_resource_name("resource rule 2");
  rule2->set_rule_severity("severity 2");
  DlpDeepScanningVerdict::MatchedDetector* detector2_1 =
      rule2->add_matched_detectors();
  detector2_1->set_detector_id("id2.1");
  detector2_1->set_detector_type("type2.1");
  detector2_1->set_display_name("display name 2.1");
  DlpDeepScanningVerdict::MatchedDetector* detector2_2 =
      rule2->add_matched_detectors();
  detector2_2->set_detector_id("id2.2");
  detector2_2->set_detector_type("type2.2");
  detector2_2->set_display_name("display name 2.2");

  FakeBinaryUploadServiceStorage()->SetResponseForText(
      BinaryUploadService::Result::SUCCESS, response);

  // The DLP verdict means an event should be reported. The content size is
  // equal to the length of the concatenated texts ("text1" and "text2") times 2
  // since they are wide characters ((5 + 5) * 2 = 20).
  EventReportValidator validator(client());
  validator.ExpectSensitiveDataEvent(
      /*url*/ "about:blank",
      /*filename*/ "Text data",
      // The hash should not be included for string requests.
      /*sha*/ "",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      /*dlp_verdict*/ response.dlp_scan_verdict(),
      /*mimetype*/ TextMimeTypes(),
      /*size*/ 20);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  DeepScanningDialogDelegate::Data data;
  data.do_dlp_scan = true;
  data.do_malware_scan = true;
  data.text.emplace_back(base::UTF8ToUTF16("text1"));
  data.text.emplace_back(base::UTF8ToUTF16("text2"));
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY));

  // Start test.
  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
            ASSERT_TRUE(result.paths_results.empty());
            ASSERT_EQ(result.text_results.size(), 2u);
            ASSERT_FALSE(result.text_results[0]);
            ASSERT_FALSE(result.text_results[1]);
            called = true;
          }),
      DeepScanAccessPoint::UPLOAD);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // There should have been 1 request for all texts and 1 for authentication.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 2);
}

}  // namespace safe_browsing
