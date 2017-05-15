// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection_service.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sha1.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/safe_browsing/download_feedback_service.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"
#include "chrome/browser/safe_browsing/local_database_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/file_type_policies_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/common/safebrowsing_switches.h"
#include "components/safe_browsing/csd.pb.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_fetcher_impl.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"
#include "url/gurl.h"

using ::testing::Assign;
using ::testing::ContainerEq;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::_;
using base::RunLoop;
using content::BrowserThread;

namespace safe_browsing {

namespace {

// A SafeBrowsingDatabaseManager implementation that returns a fixed result for
// a given URL.
class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager() {}

  MOCK_METHOD1(MatchDownloadWhitelistUrl, bool(const GURL&));
  MOCK_METHOD1(MatchDownloadWhitelistString, bool(const std::string&));
  MOCK_METHOD2(CheckDownloadUrl, bool(
      const std::vector<GURL>& url_chain,
      SafeBrowsingDatabaseManager::Client* client));

 private:
  virtual ~MockSafeBrowsingDatabaseManager() {}
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};

class FakeSafeBrowsingService : public SafeBrowsingService,
                                public ServicesDelegate::ServicesCreator {
 public:
  FakeSafeBrowsingService() {
    services_delegate_ = ServicesDelegate::CreateForTest(this, this);
  }

  // Returned pointer has the same lifespan as the database_manager_ refcounted
  // object.
  MockSafeBrowsingDatabaseManager* mock_database_manager() {
    return mock_database_manager_;
  }

 protected:
  ~FakeSafeBrowsingService() override {}

  SafeBrowsingDatabaseManager* CreateDatabaseManager() override {
    mock_database_manager_ = new MockSafeBrowsingDatabaseManager();
    return mock_database_manager_;
  }

  SafeBrowsingProtocolManagerDelegate* GetProtocolManagerDelegate() override {
    // Our SafeBrowsingDatabaseManager doesn't implement this delegate.
    return NULL;
  }

  void RegisterAllDelayedAnalysis() override {}

 private:
  // ServicesDelegate::ServicesCreator:
  bool CanCreateDownloadProtectionService() override { return false; }
  bool CanCreateIncidentReportingService() override { return true; }
  bool CanCreateResourceRequestDetector() override { return false; }
  DownloadProtectionService* CreateDownloadProtectionService() override {
    NOTREACHED();
    return nullptr;
  }
  IncidentReportingService* CreateIncidentReportingService() override {
    return new IncidentReportingService(nullptr);
  }
  ResourceRequestDetector* CreateResourceRequestDetector() override {
    NOTREACHED();
    return nullptr;
  }

  MockSafeBrowsingDatabaseManager* mock_database_manager_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingService);
};

class MockBinaryFeatureExtractor : public BinaryFeatureExtractor {
 public:
  MockBinaryFeatureExtractor() {}
  MOCK_METHOD2(CheckSignature, void(const base::FilePath&,
                                    ClientDownloadRequest_SignatureInfo*));
  MOCK_METHOD4(ExtractImageFeatures, bool(
      const base::FilePath&,
      ExtractHeadersOption,
      ClientDownloadRequest_ImageHeaders*,
      google::protobuf::RepeatedPtrField<std::string>*));

 protected:
  virtual ~MockBinaryFeatureExtractor() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBinaryFeatureExtractor);
};

class TestURLFetcherWatcher : public net::TestURLFetcherDelegateForTests {
 public:
  explicit TestURLFetcherWatcher(net::TestURLFetcherFactory* factory)
      : factory_(factory), fetcher_id_(-1) {
    factory_->SetDelegateForTests(this);
  }
  ~TestURLFetcherWatcher() {
    factory_->SetDelegateForTests(NULL);
  }

  // TestURLFetcherDelegateForTests impl:
  void OnRequestStart(int fetcher_id) override {
    fetcher_id_ = fetcher_id;
    run_loop_.Quit();
  }
  void OnChunkUpload(int fetcher_id) override {}
  void OnRequestEnd(int fetcher_id) override {}

  int WaitForRequest() {
    run_loop_.Run();
    return fetcher_id_;
  }

 private:
  net::TestURLFetcherFactory* factory_;
  int fetcher_id_;
  RunLoop run_loop_;
};

using NiceMockDownloadItem = NiceMock<content::MockDownloadItem>;

}  // namespace

ACTION_P(SetCertificateContents, contents) {
  arg1->add_certificate_chain()->add_element()->set_certificate(contents);
}

ACTION_P(SetDosHeaderContents, contents) {
  arg2->mutable_pe_headers()->set_dos_header(contents);
  return true;
}

ACTION_P(TrustSignature, contents) {
  arg1->set_trusted(true);
  // Add a certificate chain.  Note that we add the certificate twice so that
  // it appears as its own issuer.

  ClientDownloadRequest_CertificateChain* chain =
      arg1->add_certificate_chain();
  chain->add_element()->set_certificate(contents.data(), contents.size());
  chain->add_element()->set_certificate(contents.data(), contents.size());
}

// We can't call OnSafeBrowsingResult directly because SafeBrowsingCheck does
// not have any copy constructor which means it can't be stored in a callback
// easily.  Note: check will be deleted automatically when the callback is
// deleted.
void OnSafeBrowsingResult(
    LocalSafeBrowsingDatabaseManager::SafeBrowsingCheck* check) {
  check->OnSafeBrowsingResult();
}

ACTION_P(CheckDownloadUrlDone, threat_type) {
  // TODO(nparker): Remove use of SafeBrowsingCheck and instead call
  // client->OnCheckDownloadUrlResult(..) directly.
  LocalSafeBrowsingDatabaseManager::SafeBrowsingCheck* check =
      new LocalSafeBrowsingDatabaseManager::SafeBrowsingCheck(
          arg0, std::vector<SBFullHash>(), arg1, BINURL,
          std::vector<SBThreatType>(1, SB_THREAT_TYPE_BINARY_MALWARE_URL));
  for (size_t i = 0; i < check->url_results.size(); ++i)
    check->url_results[i] = threat_type;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&OnSafeBrowsingResult, base::Owned(check)));
}

class DownloadProtectionServiceTest : public testing::Test {
 protected:
  DownloadProtectionServiceTest()
      : test_browser_thread_bundle_(
            content::TestBrowserThreadBundle::IO_MAINLOOP) {
  }
  void SetUp() override {
    // Start real threads for the IO and File threads so that the DCHECKs
    // to test that we're on the correct thread work.
    sb_service_ = new StrictMock<FakeSafeBrowsingService>();
    sb_service_->Initialize();
    binary_feature_extractor_ = new StrictMock<MockBinaryFeatureExtractor>();
    ON_CALL(*binary_feature_extractor_, ExtractImageFeatures(_, _, _, _))
        .WillByDefault(Return(true));
    download_service_ = sb_service_->download_protection_service();
    download_service_->binary_feature_extractor_ = binary_feature_extractor_;
    download_service_->SetEnabled(true);
    client_download_request_subscription_ =
        download_service_->RegisterClientDownloadRequestCallback(
            base::Bind(&DownloadProtectionServiceTest::OnClientDownloadRequest,
                       base::Unretained(this)));
    ppapi_download_request_subscription_ =
        download_service_->RegisterPPAPIDownloadRequestCallback(
            base::Bind(&DownloadProtectionServiceTest::OnPPAPIDownloadRequest,
                       base::Unretained(this)));
    RunLoop().RunUntilIdle();
    has_result_ = false;

    base::FilePath source_path;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
    testdata_path_ = source_path
        .AppendASCII("chrome")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("safe_browsing")
        .AppendASCII("download_protection");

    // Setup a profile
    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
    profile_.reset(new TestingProfile(profile_dir_.GetPath()));
    ASSERT_TRUE(profile_->CreateHistoryService(true /* delete_file */,
                                               false /* no_db */));

    // Setup a directory to place test files in.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Turn off binary sampling by default.
    SetBinarySamplingProbability(0.0);
  }

  void TearDown() override {
    client_download_request_subscription_.reset();
    ppapi_download_request_subscription_.reset();
    sb_service_->ShutDown();
    // Flush all of the thread message loops to ensure that there are no
    // tasks currently running.
    FlushThreadMessageLoops();
    sb_service_ = NULL;
  }

  void SetWhitelistedDownloadSampleRate(double target_rate) {
    download_service_->whitelist_sample_rate_ = target_rate;
  }

  void SetBinarySamplingProbability(double target_rate) {
    std::unique_ptr<DownloadFileTypeConfig> config =
        policies_.DuplicateConfig();
    config->set_sampled_ping_probability(target_rate);
    policies_.SwapConfig(config);
  }

  bool RequestContainsResource(const ClientDownloadRequest& request,
                               ClientDownloadRequest::ResourceType type,
                               const std::string& url,
                               const std::string& referrer) {
    for (int i = 0; i < request.resources_size(); ++i) {
      if (request.resources(i).url() == url &&
          request.resources(i).type() == type &&
          (referrer.empty() || request.resources(i).referrer() == referrer)) {
        return true;
      }
    }
    return false;
  }

  // At this point we only set the server IP for the download itself.
  bool RequestContainsServerIp(const ClientDownloadRequest& request,
                               const std::string& remote_address) {
    for (int i = 0; i < request.resources_size(); ++i) {
      // We want the last DOWNLOAD_URL in the chain.
      if (request.resources(i).type() == ClientDownloadRequest::DOWNLOAD_URL &&
          (i + 1 == request.resources_size() ||
           request.resources(i + 1).type() !=
           ClientDownloadRequest::DOWNLOAD_URL)) {
        return remote_address == request.resources(i).remote_ip();
      }
    }
    return false;
  }

  static const ClientDownloadRequest_ArchivedBinary* GetRequestArchivedBinary(
      const ClientDownloadRequest& request,
      const std::string& file_basename) {
    for (const auto& archived_binary : request.archived_binary()) {
      if (archived_binary.file_basename() == file_basename)
        return &archived_binary;
    }
    return nullptr;
  }

  // Flushes any pending tasks in the message loops of all threads.
  void FlushThreadMessageLoops() {
    BrowserThread::GetBlockingPool()->FlushForTesting();
    FlushMessageLoop(BrowserThread::IO);
    RunLoop().RunUntilIdle();
  }

  // Proxy for private method.
  static void GetCertificateWhitelistStrings(
      const net::X509Certificate& certificate,
      const net::X509Certificate& issuer,
      std::vector<std::string>* whitelist_strings) {
    DownloadProtectionService::GetCertificateWhitelistStrings(
        certificate, issuer, whitelist_strings);
  }

  // Reads a single PEM-encoded certificate from the testdata directory.
  // Returns NULL on failure.
  scoped_refptr<net::X509Certificate> ReadTestCertificate(
      const std::string& filename) {
    std::string cert_data;
    if (!base::ReadFileToString(testdata_path_.AppendASCII(filename),
                                &cert_data)) {
      return NULL;
    }
    net::CertificateList certs =
        net::X509Certificate::CreateCertificateListFromBytes(
            cert_data.data(),
            cert_data.size(),
            net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
    return certs.empty() ? NULL : certs[0];
  }

  const ClientDownloadRequest* GetClientDownloadRequest() const {
    return last_client_download_request_.get();
  }

  bool HasClientDownloadRequest() const {
    return last_client_download_request_.get() != NULL;
  }

  void ClearClientDownloadRequest() { last_client_download_request_.reset(); }

  void PrepareResponse(net::FakeURLFetcherFactory* factory,
                       ClientDownloadResponse::Verdict verdict,
                       net::HttpStatusCode response_code,
                       net::URLRequestStatus::Status status,
                       bool upload_requested = false) {
    ClientDownloadResponse response;
    response.set_verdict(verdict);
    response.set_upload(upload_requested);
    factory->SetFakeResponse(
        DownloadProtectionService::GetDownloadRequestUrl(),
        response.SerializeAsString(),
        response_code, status);
  }

  void PrepareBasicDownloadItem(
      NiceMockDownloadItem* item,
      const std::vector<std::string> url_chain_items,
      const std::string& referrer_url,
      const base::FilePath::StringType& tmp_path_literal,
      const base::FilePath::StringType& final_path_literal) {
    base::FilePath tmp_path = temp_dir_.GetPath().Append(tmp_path_literal);
    base::FilePath final_path = temp_dir_.GetPath().Append(final_path_literal);

    PrepareBasicDownloadItemWithFullPaths(item, url_chain_items, referrer_url,
      tmp_path, final_path);
  }

  void PrepareBasicDownloadItemWithFullPaths(
      NiceMockDownloadItem* item,
      const std::vector<std::string> url_chain_items,
      const std::string& referrer_url,
      const base::FilePath& tmp_full_path,
      const base::FilePath& final_full_path) {
    url_chain_.clear();
    for (std::string item: url_chain_items)
      url_chain_.push_back(GURL(item));
    if (url_chain_.empty())
      url_chain_.push_back(GURL());
    referrer_ = GURL(referrer_url);
    tmp_path_ = tmp_full_path;
    final_path_ = final_full_path;
    hash_ = "hash";

    EXPECT_CALL(*item, GetURL()).WillRepeatedly(ReturnRef(url_chain_.back()));
    EXPECT_CALL(*item, GetFullPath()).WillRepeatedly(ReturnRef(tmp_path_));
    EXPECT_CALL(*item, GetTargetFilePath())
        .WillRepeatedly(ReturnRef(final_path_));
    EXPECT_CALL(*item, GetUrlChain()).WillRepeatedly(ReturnRef(url_chain_));
    EXPECT_CALL(*item, GetReferrerUrl()).WillRepeatedly(ReturnRef(referrer_));
    EXPECT_CALL(*item, GetTabUrl())
        .WillRepeatedly(ReturnRef(GURL::EmptyGURL()));
    EXPECT_CALL(*item, GetTabReferrerUrl())
        .WillRepeatedly(ReturnRef(GURL::EmptyGURL()));
    EXPECT_CALL(*item, GetHash()).WillRepeatedly(ReturnRef(hash_));
    EXPECT_CALL(*item, GetReceivedBytes()).WillRepeatedly(Return(100));
    EXPECT_CALL(*item, HasUserGesture()).WillRepeatedly(Return(true));
    EXPECT_CALL(*item, GetRemoteAddress()).WillRepeatedly(Return(""));
  }

 private:
  // Helper functions for FlushThreadMessageLoops.
  void RunAllPendingAndQuitUI(const base::Closure& quit_closure) {
    RunLoop().RunUntilIdle();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, quit_closure);
  }

  void PostRunMessageLoopTask(BrowserThread::ID thread,
                              const base::Closure& quit_closure) {
    BrowserThread::PostTask(
        thread, FROM_HERE,
        base::BindOnce(&DownloadProtectionServiceTest::RunAllPendingAndQuitUI,
                       base::Unretained(this), quit_closure));
  }

  void FlushMessageLoop(BrowserThread::ID thread) {
    RunLoop run_loop;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&DownloadProtectionServiceTest::PostRunMessageLoopTask,
                       base::Unretained(this), thread, run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnClientDownloadRequest(content::DownloadItem* download,
                               const ClientDownloadRequest* request) {
    if (request)
      last_client_download_request_.reset(new ClientDownloadRequest(*request));
    else
      last_client_download_request_.reset();
  }

  void OnPPAPIDownloadRequest(const ClientDownloadRequest* request) {
    if (request)
      last_client_download_request_.reset(new ClientDownloadRequest(*request));
    else
      last_client_download_request_.reset();
  }

 public:
  enum ArchiveType { ZIP, DMG };

  void CheckDoneCallback(
      const base::Closure& quit_closure,
      DownloadProtectionService::DownloadCheckResult result) {
    result_ = result;
    has_result_ = true;
    quit_closure.Run();
  }

  void SyncCheckDoneCallback(
      DownloadProtectionService::DownloadCheckResult result) {
    result_ = result;
    has_result_ = true;
  }

  void SendURLFetchComplete(net::TestURLFetcher* fetcher) {
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  testing::AssertionResult IsResult(
      DownloadProtectionService::DownloadCheckResult expected) {
    if (!has_result_)
      return testing::AssertionFailure() << "No result";
    has_result_ = false;
    return result_ == expected ?
        testing::AssertionSuccess() :
        testing::AssertionFailure() << "Expected " << expected <<
                                       ", got " << result_;
  }

  void SetExtendedReportingPreference(bool is_extended_reporting) {
    SetExtendedReportingPref(profile_->GetPrefs(), is_extended_reporting);
  }

  // Verify that corrupted ZIP/DMGs do send a ping.
  void CheckClientDownloadReportCorruptArchive(ArchiveType type);

 protected:
  // This will effectivly mask the global Singleton while this is in scope.
  FileTypePoliciesTestOverlay policies_;

  scoped_refptr<FakeSafeBrowsingService> sb_service_;
  scoped_refptr<MockBinaryFeatureExtractor> binary_feature_extractor_;
  DownloadProtectionService* download_service_;
  DownloadProtectionService::DownloadCheckResult result_;
  bool has_result_;
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper_;
  base::FilePath testdata_path_;
  DownloadProtectionService::ClientDownloadRequestSubscription
      client_download_request_subscription_;
  DownloadProtectionService::PPAPIDownloadRequestSubscription
      ppapi_download_request_subscription_;
  std::unique_ptr<ClientDownloadRequest> last_client_download_request_;
  base::ScopedTempDir profile_dir_;
  std::unique_ptr<TestingProfile> profile_;
  // The following 5 fields are used by PrepareBasicDownloadItem() function to
  // store attributes of the last download item. They can be modified
  // afterwards and the *item will return the new values.
  std::vector<GURL> url_chain_;
  GURL referrer_;
  base::FilePath tmp_path_;
  base::FilePath final_path_;
  std::string hash_;
  base::ScopedTempDir temp_dir_;
};

void DownloadProtectionServiceTest::CheckClientDownloadReportCorruptArchive(
    ArchiveType type) {
  net::FakeURLFetcherFactory factory(NULL);
  PrepareResponse(
      &factory, ClientDownloadResponse::SAFE, net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);

  NiceMockDownloadItem item;
  if (type == ZIP) {
    PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.zip"},  // url_chain
                             "http://www.google.com/",     // referrer
                             FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                             FILE_PATH_LITERAL("a.zip"));  // final_path
  } else if (type == DMG) {
    PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.dmg"},  // url_chain
                             "http://www.google.com/",     // referrer
                             FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                             FILE_PATH_LITERAL("a.dmg"));  // final_path
  }


  std::string file_contents = "corrupt archive file";
  ASSERT_EQ(static_cast<int>(file_contents.size()), base::WriteFile(
      tmp_path_, file_contents.data(), file_contents.size()));

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                        base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  EXPECT_EQ(0, GetClientDownloadRequest()->archived_binary_size());
  EXPECT_TRUE(GetClientDownloadRequest()->has_download_type());
  ClientDownloadRequest::DownloadType expected_type =
      type == ZIP ? ClientDownloadRequest_DownloadType_INVALID_ZIP
      : ClientDownloadRequest_DownloadType_INVALID_MAC_ARCHIVE;
  EXPECT_EQ(expected_type, GetClientDownloadRequest()->download_type());
  ClearClientDownloadRequest();

  Mock::VerifyAndClearExpectations(sb_service_.get());
  Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadInvalidUrl) {
  NiceMockDownloadItem item;
  {
    PrepareBasicDownloadItem(&item,
                             std::vector<std::string>(),   // empty url_chain
                             "http://www.google.com/",     // referrer
                             FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                             FILE_PATH_LITERAL("a.exe"));  // final_path
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
    Mock::VerifyAndClearExpectations(&item);
  }

  {
    PrepareBasicDownloadItem(&item, {"file://www.google.com/"},  // url_chain
                             "http://www.google.com/",           // referrer
                             FILE_PATH_LITERAL("a.tmp"),         // tmp_path
                             FILE_PATH_LITERAL("a.exe"));        // final_path
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadNotABinary) {
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
    &item,
    std::vector<std::string>(),   // empty url_chain
    "http://www.google.com/",     // referrer
    FILE_PATH_LITERAL("a.tmp"),   // tmp_path
    FILE_PATH_LITERAL("a.txt"));  // final_path
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                        base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
  EXPECT_FALSE(HasClientDownloadRequest());
}

TEST_F(DownloadProtectionServiceTest,
    CheckClientDownloadWhitelistedUrlWithoutSampling) {
  // Response to any requests will be DANGEROUS.
  net::FakeURLFetcherFactory factory(NULL);
  PrepareResponse(
      &factory, ClientDownloadResponse::DANGEROUS, net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
    &item,
    std::vector<std::string>(),   // empty url_chain
    "",                           // referrer
    FILE_PATH_LITERAL("a.tmp"),   // tmp_path
    FILE_PATH_LITERAL("a.exe"));  // final_path

  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(4);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(4);

  // We should not get whilelist checks for other URLs than specified below.
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_)).Times(0);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(GURL("http://www.evil.com/bla.exe")))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(GURL("http://www.google.com/a.exe")))
      .WillRepeatedly(Return(true));

  // Set sample rate to 0 to prevent sampling.
  SetWhitelistedDownloadSampleRate(0);
  {
    // With no referrer and just the bad url, should be marked DANGEROUS.
    url_chain_.push_back(GURL("http://www.evil.com/bla.exe"));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_url_whitelist());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_certificate_whitelist());
    ClearClientDownloadRequest();
  }
  {
    // Check that the referrer is not matched against the whitelist.
    referrer_ = GURL("http://www.google.com/");
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_url_whitelist());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_certificate_whitelist());
    ClearClientDownloadRequest();
  }

  {
    // Redirect from a site shouldn't be checked either.
    url_chain_.insert(url_chain_.begin(),
                      GURL("http://www.google.com/redirect"));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_url_whitelist());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_certificate_whitelist());
    ClearClientDownloadRequest();
  }
  {
    // Only if the final url is whitelisted should it be SAFE.
    url_chain_.push_back(GURL("http://www.google.com/a.exe"));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
    // TODO(grt): Make the service produce the request even when the URL is
    // whitelisted.
    EXPECT_FALSE(HasClientDownloadRequest());
  }
}

TEST_F(DownloadProtectionServiceTest,
    CheckClientDownloadWhitelistedUrlWithSampling) {
  // Server responses "SAFE" to every requests coming from whitelisted
  // download.
  net::FakeURLFetcherFactory factory(NULL);
  PrepareResponse(
      &factory, ClientDownloadResponse::SAFE, net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
    &item,
    std::vector<std::string>(),   // empty url_chain
    "http://www.google.com/",     // referrer
    FILE_PATH_LITERAL("a.tmp"),   // tmp_path
    FILE_PATH_LITERAL("a.exe"));  // final_path
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(4);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(6);
  // Assume http://www.whitelist.com/a.exe is on the whitelist.
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_)).Times(0);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(GURL("http://www.whitelist.com/a.exe")))
      .WillRepeatedly(Return(true));
  url_chain_.push_back(GURL("http://www.whitelist.com/a.exe"));
  // Set sample rate to 1.00, so download_service_ will always send download
  // pings for whitelisted downloads.
  SetWhitelistedDownloadSampleRate(1.00);

  {
    // Case (1): is_extended_reporting && is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    SetExtendedReportingPreference(true);
    EXPECT_CALL(item, GetBrowserContext())
        .WillRepeatedly(Return(profile_->GetOffTheRecordProfile()));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (2): !is_extended_reporting && is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    SetExtendedReportingPreference(false);
    EXPECT_CALL(item, GetBrowserContext())
        .WillRepeatedly(Return(profile_->GetOffTheRecordProfile()));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (3): !is_extended_reporting && !is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    EXPECT_CALL(item, GetBrowserContext())
        .WillRepeatedly(Return(profile_.get()));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (4): is_extended_reporting && !is_incognito &&
    //           Download matches URL whitelist.
    //           ClientDownloadRequest should be sent.
    SetExtendedReportingPreference(true);
    EXPECT_CALL(item, GetBrowserContext())
        .WillRepeatedly(Return(profile_.get()));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_TRUE(GetClientDownloadRequest()->skipped_url_whitelist());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_certificate_whitelist());
    ClearClientDownloadRequest();
  }

  // Setup trusted and whitelisted certificates for test cases (5) and (6).
  scoped_refptr<net::X509Certificate> test_cert(
      ReadTestCertificate("test_cn.pem"));
  ASSERT_TRUE(test_cert.get());
  std::string test_cert_der;
  net::X509Certificate::GetDEREncoded(test_cert->os_cert_handle(),
                                      &test_cert_der);
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
        .WillRepeatedly(TrustSignature(test_cert_der));
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistString(_))
      .WillRepeatedly(Return(true));

  {
    // Case (5): is_extended_reporting && !is_incognito &&
    //           Download matches certificate whitelist.
    //           ClientDownloadRequest should be sent.
    EXPECT_CALL(item, GetBrowserContext())
        .WillRepeatedly(Return(profile_.get()));
    EXPECT_CALL(
        *sb_service_->mock_database_manager(),
        MatchDownloadWhitelistUrl(GURL("http://www.whitelist.com/a.exe")))
        .WillRepeatedly(Return(false));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_url_whitelist());
    EXPECT_TRUE(GetClientDownloadRequest()->skipped_certificate_whitelist());
    ClearClientDownloadRequest();
  }
  {
    // Case (6): is_extended_reporting && !is_incognito &&
    //           Download matches both URL and certificate whitelists.
    //           ClientDownloadRequest should be sent.
    EXPECT_CALL(item, GetBrowserContext())
        .WillRepeatedly(Return(profile_.get()));
    EXPECT_CALL(
        *sb_service_->mock_database_manager(),
        MatchDownloadWhitelistUrl(GURL("http://www.whitelist.com/a.exe")))
        .WillRepeatedly(Return(true));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_TRUE(GetClientDownloadRequest()->skipped_url_whitelist());
    // Since download matches URL whitelist and gets sampled, no need to
    // do certificate whitelist checking and sampling.
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_certificate_whitelist());
    ClearClientDownloadRequest();
  }
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadSampledFile) {
  // Server response will be discarded.
  net::FakeURLFetcherFactory factory(NULL);
  PrepareResponse(
      &factory, ClientDownloadResponse::DANGEROUS, net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
      &item,
      // Add paths so we can check they are properly removed.
      {"http://referrer.com/1/2", "http://referrer.com/3/4",
       "http://download.com/path/a.foobar_unknown_type"},
      "http://referrer.com/3/4",                    // Referrer
      FILE_PATH_LITERAL("a.tmp"),                   // tmp_path
      FILE_PATH_LITERAL("a.foobar_unknown_type"));  // final_path
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(1);

  // Set ping sample rate to 1.00 so download_service_ will always send a
  // "light" ping for unknown types if allowed.
  SetBinarySamplingProbability(1.0);

  {
    // Case (1): is_extended_reporting && is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    SetExtendedReportingPreference(true);
    EXPECT_CALL(item, GetBrowserContext())
        .WillRepeatedly(Return(profile_->GetOffTheRecordProfile()));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (2): is_extended_reporting && !is_incognito.
    //           A "light" ClientDownloadRequest should be sent.
    EXPECT_CALL(item, GetBrowserContext())
        .WillRepeatedly(Return(profile_.get()));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
    ASSERT_TRUE(HasClientDownloadRequest());

    // Verify it's a "light" ping, check that URLs don't have paths.
    auto* req = GetClientDownloadRequest();
    EXPECT_EQ(ClientDownloadRequest::SAMPLED_UNSUPPORTED_FILE,
              req->download_type());
    EXPECT_EQ(GURL(req->url()).GetOrigin().spec(), req->url());
    for (auto resource : req->resources()) {
      EXPECT_EQ(GURL(resource.url()).GetOrigin().spec(), resource.url());
      EXPECT_EQ(GURL(resource.referrer()).GetOrigin().spec(),
                resource.referrer());
    }
    ClearClientDownloadRequest();
  }
  {
    // Case (3): !is_extended_reporting && is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    SetExtendedReportingPreference(false);
    EXPECT_CALL(item, GetBrowserContext())
        .WillRepeatedly(Return(profile_->GetOffTheRecordProfile()));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (4): !is_extended_reporting && !is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    EXPECT_CALL(item, GetBrowserContext())
        .WillRepeatedly(Return(profile_.get()));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadFetchFailed) {
  // HTTP request will fail.
  net::FakeURLFetcherFactory factory(NULL);
  PrepareResponse(
      &factory, ClientDownloadResponse::SAFE, net::HTTP_INTERNAL_SERVER_ERROR,
      net::URLRequestStatus::FAILED);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
    &item,
    {"http://www.evil.com/a.exe"},  // url_chain
    "http://www.google.com/",       // referrer
    FILE_PATH_LITERAL("a.tmp"),     // tmp_path
    FILE_PATH_LITERAL("a.exe"));    // final_path

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(
      *binary_feature_extractor_.get(),
      ExtractImageFeatures(tmp_path_, BinaryFeatureExtractor::kDefaultOptions,
                           _, _));
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                        base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadSuccess) {
  net::FakeURLFetcherFactory factory(NULL);
  PrepareResponse(&factory, ClientDownloadResponse::SAFE, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(8);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(8);
  std::string feedback_ping;
  std::string feedback_response;
  ClientDownloadResponse expected_response;

  {
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // Invalid response should result in SAFE (default value in proto).
    ClientDownloadResponse invalid_response;
    factory.SetFakeResponse(DownloadProtectionService::GetDownloadRequestUrl(),
                            invalid_response.SerializePartialAsString(),
                            net::HTTP_OK, net::URLRequestStatus::SUCCESS);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
    EXPECT_FALSE(DownloadFeedbackService::GetPingsForDownloadForTesting(
        item, &feedback_ping, &feedback_response));
  }
  {
    // If the response is dangerous the result should also be marked as
    // dangerous, and should not upload if not requested.
    PrepareResponse(&factory, ClientDownloadResponse::DANGEROUS, net::HTTP_OK,
                    net::URLRequestStatus::SUCCESS,
                    false /* upload_requested */);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_FALSE(DownloadFeedbackService::GetPingsForDownloadForTesting(
        item, &feedback_ping, &feedback_response));
    EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // If the response is dangerous and the server requests an upload,
    // we should upload.
    PrepareResponse(&factory, ClientDownloadResponse::DANGEROUS, net::HTTP_OK,
                    net::URLRequestStatus::SUCCESS,
                    true /* upload_requested */);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(DownloadFeedbackService::GetPingsForDownloadForTesting(
        item, &feedback_ping, &feedback_response));
    EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // If the response is uncommon the result should also be marked as uncommon.
    PrepareResponse(&factory, ClientDownloadResponse::UNCOMMON, net::HTTP_OK,
                    net::URLRequestStatus::SUCCESS,
                    true /* upload_requested */);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::UNCOMMON));
    EXPECT_TRUE(DownloadFeedbackService::GetPingsForDownloadForTesting(
        item, &feedback_ping, &feedback_response));
    ClientDownloadRequest decoded_request;
    EXPECT_TRUE(decoded_request.ParseFromString(feedback_ping));
    EXPECT_EQ(url_chain_.back().spec(), decoded_request.url());
    expected_response.set_verdict(ClientDownloadResponse::UNCOMMON);
    expected_response.set_upload(true);
    EXPECT_EQ(expected_response.SerializeAsString(), feedback_response);
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // If the response is dangerous_host the result should also be marked as
    // dangerous_host.
    PrepareResponse(&factory, ClientDownloadResponse::DANGEROUS_HOST,
                    net::HTTP_OK, net::URLRequestStatus::SUCCESS,
                    true /* upload_requested */);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS_HOST));
    EXPECT_TRUE(DownloadFeedbackService::GetPingsForDownloadForTesting(
        item, &feedback_ping, &feedback_response));
    expected_response.set_verdict(ClientDownloadResponse::DANGEROUS_HOST);
    expected_response.set_upload(true);
    EXPECT_EQ(expected_response.SerializeAsString(), feedback_response);
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // If the response is POTENTIALLY_UNWANTED the result should also be marked
    // as POTENTIALLY_UNWANTED.
    PrepareResponse(&factory, ClientDownloadResponse::POTENTIALLY_UNWANTED,
                    net::HTTP_OK, net::URLRequestStatus::SUCCESS);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::POTENTIALLY_UNWANTED));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // If the response is UNKNOWN the result should also be marked as
    // UNKNOWN
    PrepareResponse(&factory, ClientDownloadResponse::UNKNOWN, net::HTTP_OK,
                    net::URLRequestStatus::SUCCESS);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadHTTPS) {
  net::FakeURLFetcherFactory factory(NULL);
  PrepareResponse(
      &factory, ClientDownloadResponse::DANGEROUS, net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item,
                           {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",       // referrer
                           FILE_PATH_LITERAL("a.tmp"),     // tmp_path
                           FILE_PATH_LITERAL("a.exe"));    // final_path

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(1);
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                        base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
  EXPECT_TRUE(HasClientDownloadRequest());
  ClearClientDownloadRequest();
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadBlob) {
  net::FakeURLFetcherFactory factory(NULL);
  PrepareResponse(
      &factory, ClientDownloadResponse::DANGEROUS, net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
    &item,
    {"blob:http://www.evil.com/50b85f60-71e4-11e4-82f8-0800200c9a66"},
    "http://www.google.com/",     // referrer
    FILE_PATH_LITERAL("a.tmp"),   // tmp_path
    FILE_PATH_LITERAL("a.exe"));  // final_path

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(1);

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                        base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
  EXPECT_TRUE(HasClientDownloadRequest());
  ClearClientDownloadRequest();
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadData) {
  net::FakeURLFetcherFactory factory(NULL);
  PrepareResponse(
      &factory, ClientDownloadResponse::DANGEROUS, net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
    &item,
    {"data:text/html:base64,", "data:text/html:base64,blahblahblah",
     "data:application/octet-stream:base64,blahblah"},  // url_chain
    "data:text/html:base64,foobar",                     // referrer
    FILE_PATH_LITERAL("a.tmp"),                         // tmp_path
    FILE_PATH_LITERAL("a.exe"));                        // final_path

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(1);

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                        base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
  ASSERT_TRUE(HasClientDownloadRequest());
  const ClientDownloadRequest& request = *GetClientDownloadRequest();
  const char kExpectedUrl[] =
      "data:application/octet-stream:base64,"
      "ACBF6DFC6F907662F566CA0241DFE8690C48661F440BA1BBD0B86C582845CCC8";
  const char kExpectedRedirect1[] = "data:text/html:base64,";
  const char kExpectedRedirect2[] =
      "data:text/html:base64,"
      "620680767E15717A57DB11D94D1BEBD32B3344EBC5994DF4FB07B0D473F4EF6B";
  const char kExpectedReferrer[] =
      "data:text/html:base64,"
      "06E2C655B9F7130B508FFF86FD19B57E6BF1A1CFEFD6EFE1C3EB09FE24EF456A";
  EXPECT_EQ(hash_, request.digests().sha256());
  EXPECT_EQ(kExpectedUrl, request.url());
  EXPECT_EQ(3, request.resources_size());
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_REDIRECT,
                                      kExpectedRedirect1, ""));
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_REDIRECT,
                                      kExpectedRedirect2, ""));
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_URL,
                                      kExpectedUrl, kExpectedReferrer));
  ClearClientDownloadRequest();
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadZip) {
  net::FakeURLFetcherFactory factory(NULL);
  PrepareResponse(
      &factory, ClientDownloadResponse::SAFE, net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
    &item,
    {"http://www.evil.com/a.zip"},  // url_chain
    "http://www.google.com/",       // referrer
    FILE_PATH_LITERAL("a.tmp"),     // tmp_path
    FILE_PATH_LITERAL("a.zip"));    // final_path

  // Write out a zip archive to the temporary file.
  base::ScopedTempDir zip_source_dir;
  ASSERT_TRUE(zip_source_dir.CreateUniqueTempDir());
  std::string file_contents = "dummy file";
  {
    // In this case, it only contains a text file.
    ASSERT_EQ(static_cast<int>(file_contents.size()),
              base::WriteFile(zip_source_dir.GetPath().Append(
                                  FILE_PATH_LITERAL("file.txt")),
                              file_contents.data(), file_contents.size()));
    ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path_, false));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
    Mock::VerifyAndClearExpectations(sb_service_.get());
    Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
  }
  {
    // Now check with an executable in the zip file as well.
    ASSERT_EQ(static_cast<int>(file_contents.size()),
              base::WriteFile(zip_source_dir.GetPath().Append(
                                  FILE_PATH_LITERAL("file.exe")),
                              file_contents.data(), file_contents.size()));
    ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path_, false));
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadWhitelistUrl(_))
        .WillRepeatedly(Return(false));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
    ASSERT_TRUE(HasClientDownloadRequest());
    const ClientDownloadRequest& request = *GetClientDownloadRequest();
    EXPECT_TRUE(request.has_download_type());
    EXPECT_EQ(ClientDownloadRequest_DownloadType_ZIPPED_EXECUTABLE,
              request.download_type());
    EXPECT_EQ(1, request.archived_binary_size());
    const ClientDownloadRequest_ArchivedBinary* archived_binary =
        GetRequestArchivedBinary(request, "file.exe");
    ASSERT_NE(nullptr, archived_binary);
    EXPECT_EQ(ClientDownloadRequest_DownloadType_WIN_EXECUTABLE,
              archived_binary->download_type());
    EXPECT_EQ(static_cast<int64_t>(file_contents.size()),
              archived_binary->length());
    ClearClientDownloadRequest();
    Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
  }
  {
    // If the response is dangerous the result should also be marked as
    // dangerous.
    PrepareResponse(&factory, ClientDownloadResponse::DANGEROUS, net::HTTP_OK,
                    net::URLRequestStatus::SUCCESS);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
    Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
  }
  {
    // Repeat the test with an archive inside the zip file in addition to the
    // executable.
    ASSERT_EQ(static_cast<int>(file_contents.size()),
              base::WriteFile(zip_source_dir.GetPath().Append(
                                  FILE_PATH_LITERAL("file.rar")),
                              file_contents.data(), file_contents.size()));
    ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path_, false));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_EQ(1, GetClientDownloadRequest()->archived_binary_size());
    EXPECT_TRUE(GetClientDownloadRequest()->has_download_type());
    EXPECT_EQ(ClientDownloadRequest_DownloadType_ZIPPED_EXECUTABLE,
              GetClientDownloadRequest()->download_type());
    ClearClientDownloadRequest();
    Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
  }
  {
    // Repeat the test with just the archive inside the zip file.
    ASSERT_TRUE(base::DeleteFile(
        zip_source_dir.GetPath().AppendASCII("file.exe"), false));
    ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path_, false));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_EQ(0, GetClientDownloadRequest()->archived_binary_size());
    EXPECT_TRUE(GetClientDownloadRequest()->has_download_type());
    EXPECT_EQ(ClientDownloadRequest_DownloadType_ZIPPED_ARCHIVE,
              GetClientDownloadRequest()->download_type());
    ClearClientDownloadRequest();
    Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
  }
}

TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadReportCorruptZip) {
  CheckClientDownloadReportCorruptArchive(ZIP);
}

#if defined(OS_MACOSX)
TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadReportCorruptDmg) {
  CheckClientDownloadReportCorruptArchive(DMG);
}
#endif

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadValidateRequest) {
  net::TestURLFetcherFactory factory;

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
    &item,
    {"http://www.google.com/",
     "http://www.google.com/bla.exe"},  // url_chain
    "http://www.google.com/",           // referrer
    FILE_PATH_LITERAL("bla.tmp"),       // tmp_path
    FILE_PATH_LITERAL("bla.exe"));      // final_path

  std::string remote_address = "10.11.12.13";
  EXPECT_CALL(item, GetRemoteAddress()).WillRepeatedly(Return(remote_address));

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .WillOnce(SetCertificateContents("dummy cert data"));
  EXPECT_CALL(
      *binary_feature_extractor_.get(),
      ExtractImageFeatures(tmp_path_, BinaryFeatureExtractor::kDefaultOptions,
                           _, _))
      .WillOnce(SetDosHeaderContents("dummy dos header"));
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                        base::Unretained(this), run_loop.QuitClosure()));

  // Run the message loop(s) until SendRequest is called.
  FlushThreadMessageLoops();
  EXPECT_TRUE(HasClientDownloadRequest());
  ClearClientDownloadRequest();
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  ClientDownloadRequest request;
  EXPECT_TRUE(request.ParseFromString(fetcher->upload_data()));
  EXPECT_EQ("http://www.google.com/bla.exe", request.url());
  EXPECT_EQ(hash_, request.digests().sha256());
  EXPECT_EQ(item.GetReceivedBytes(), request.length());
  EXPECT_EQ(item.HasUserGesture(), request.user_initiated());
  EXPECT_TRUE(RequestContainsServerIp(request, remote_address));
  EXPECT_EQ(2, request.resources_size());
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_REDIRECT,
                                      "http://www.google.com/", ""));
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_URL,
                                      "http://www.google.com/bla.exe",
                                      referrer_.spec()));
  EXPECT_TRUE(request.has_signature());
  ASSERT_EQ(1, request.signature().certificate_chain_size());
  const ClientDownloadRequest_CertificateChain& chain =
      request.signature().certificate_chain(0);
  ASSERT_EQ(1, chain.element_size());
  EXPECT_EQ("dummy cert data", chain.element(0).certificate());
  EXPECT_TRUE(request.has_image_headers());
  const ClientDownloadRequest_ImageHeaders& headers =
      request.image_headers();
  EXPECT_TRUE(headers.has_pe_headers());
  EXPECT_TRUE(headers.pe_headers().has_dos_header());
  EXPECT_EQ("dummy dos header", headers.pe_headers().dos_header());

  // Simulate the request finishing.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadProtectionServiceTest::SendURLFetchComplete,
                     base::Unretained(this), fetcher));
  run_loop.Run();
}

// Similar to above, but with an unsigned binary.
TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadValidateRequestNoSignature) {
  net::TestURLFetcherFactory factory;

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
    &item,
    {"http://www.google.com/",
     "ftp://www.google.com/bla.exe"},  // url_chain
    "http://www.google.com/",          // referrer
    FILE_PATH_LITERAL("bla.tmp"),      // tmp_path
    FILE_PATH_LITERAL("bla.exe"));     // final_path
  std::string remote_address = "10.11.12.13";
  EXPECT_CALL(item, GetRemoteAddress()).WillRepeatedly(Return(remote_address));

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(tmp_path_,
                                   BinaryFeatureExtractor::kDefaultOptions,
                                   _, _));
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                        base::Unretained(this), run_loop.QuitClosure()));

  // Run the message loop(s) until SendRequest is called.
  FlushThreadMessageLoops();
  EXPECT_TRUE(HasClientDownloadRequest());
  ClearClientDownloadRequest();
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  ClientDownloadRequest request;
  EXPECT_TRUE(request.ParseFromString(fetcher->upload_data()));
  EXPECT_EQ("ftp://www.google.com/bla.exe", request.url());
  EXPECT_EQ(hash_, request.digests().sha256());
  EXPECT_EQ(item.GetReceivedBytes(), request.length());
  EXPECT_EQ(item.HasUserGesture(), request.user_initiated());
  EXPECT_EQ(2, request.resources_size());
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_REDIRECT,
                                      "http://www.google.com/", ""));
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_URL,
                                      "ftp://www.google.com/bla.exe",
                                      referrer_.spec()));
  EXPECT_TRUE(request.has_signature());
  EXPECT_EQ(0, request.signature().certificate_chain_size());

  // Simulate the request finishing.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadProtectionServiceTest::SendURLFetchComplete,
                     base::Unretained(this), fetcher));
  run_loop.Run();
}

// Similar to above, but with tab history.
TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadValidateRequestTabHistory) {
  net::TestURLFetcherFactory factory;

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
    &item,
    {"http://www.google.com/",
     "http://www.google.com/bla.exe"},  // url_chain
    "http://www.google.com/",           // referrer
    FILE_PATH_LITERAL("bla.tmp"),       // tmp_path
    FILE_PATH_LITERAL("bla.exe"));      // final_path

  GURL tab_url("http://tab.com/final");
  GURL tab_referrer("http://tab.com/referrer");
  std::string remote_address = "10.11.12.13";
  EXPECT_CALL(item, GetTabUrl()).WillRepeatedly(ReturnRef(tab_url));
  EXPECT_CALL(item, GetTabReferrerUrl())
      .WillRepeatedly(ReturnRef(tab_referrer));
  EXPECT_CALL(item, GetRemoteAddress()).WillRepeatedly(Return(remote_address));
  EXPECT_CALL(item, GetBrowserContext()).WillRepeatedly(Return(profile_.get()));
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .WillRepeatedly(SetCertificateContents("dummy cert data"));
  EXPECT_CALL(
      *binary_feature_extractor_.get(),
      ExtractImageFeatures(tmp_path_, BinaryFeatureExtractor::kDefaultOptions,
                           _, _))
      .WillRepeatedly(SetDosHeaderContents("dummy dos header"));

  // First test with no history match for the tab URL.
  {
    TestURLFetcherWatcher fetcher_watcher(&factory);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));

    EXPECT_EQ(0, fetcher_watcher.WaitForRequest());
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
    net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    ClientDownloadRequest request;
    EXPECT_TRUE(request.ParseFromString(fetcher->upload_data()));
    EXPECT_EQ("http://www.google.com/bla.exe", request.url());
    EXPECT_EQ(hash_, request.digests().sha256());
    EXPECT_EQ(item.GetReceivedBytes(), request.length());
    EXPECT_EQ(item.HasUserGesture(), request.user_initiated());
    EXPECT_TRUE(RequestContainsServerIp(request, remote_address));
    EXPECT_EQ(3, request.resources_size());
    EXPECT_TRUE(
        RequestContainsResource(request,
                                ClientDownloadRequest::DOWNLOAD_REDIRECT,
                                "http://www.google.com/",
                                ""));
    EXPECT_TRUE(RequestContainsResource(request,
                                        ClientDownloadRequest::DOWNLOAD_URL,
                                        "http://www.google.com/bla.exe",
                                        referrer_.spec()));
    EXPECT_TRUE(RequestContainsResource(request,
                                        ClientDownloadRequest::TAB_URL,
                                        tab_url.spec(),
                                        tab_referrer.spec()));
    EXPECT_TRUE(request.has_signature());
    ASSERT_EQ(1, request.signature().certificate_chain_size());
    const ClientDownloadRequest_CertificateChain& chain =
        request.signature().certificate_chain(0);
    ASSERT_EQ(1, chain.element_size());
    EXPECT_EQ("dummy cert data", chain.element(0).certificate());
    EXPECT_TRUE(request.has_image_headers());
    const ClientDownloadRequest_ImageHeaders& headers =
        request.image_headers();
    EXPECT_TRUE(headers.has_pe_headers());
    EXPECT_TRUE(headers.pe_headers().has_dos_header());
    EXPECT_EQ("dummy dos header", headers.pe_headers().dos_header());

    // Simulate the request finishing.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&DownloadProtectionServiceTest::SendURLFetchComplete,
                       base::Unretained(this), fetcher));
    run_loop.Run();
  }

  // Now try with a history match.
  {
    history::RedirectList redirects;
    redirects.push_back(GURL("http://tab.com/ref1"));
    redirects.push_back(GURL("http://tab.com/ref2"));
    redirects.push_back(tab_url);
    HistoryServiceFactory::GetForProfile(profile_.get(),
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->AddPage(tab_url,
                  base::Time::Now(),
                  reinterpret_cast<history::ContextID>(1),
                  0,
                  GURL(),
                  redirects,
                  ui::PAGE_TRANSITION_TYPED,
                  history::SOURCE_BROWSED,
                  false);

    TestURLFetcherWatcher fetcher_watcher(&factory);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    EXPECT_EQ(0, fetcher_watcher.WaitForRequest());
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
    net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    ClientDownloadRequest request;
    EXPECT_TRUE(request.ParseFromString(fetcher->upload_data()));
    EXPECT_EQ("http://www.google.com/bla.exe", request.url());
    EXPECT_EQ(hash_, request.digests().sha256());
    EXPECT_EQ(item.GetReceivedBytes(), request.length());
    EXPECT_EQ(item.HasUserGesture(), request.user_initiated());
    EXPECT_TRUE(RequestContainsServerIp(request, remote_address));
    EXPECT_EQ(5, request.resources_size());
    EXPECT_TRUE(
        RequestContainsResource(request,
                                ClientDownloadRequest::DOWNLOAD_REDIRECT,
                                "http://www.google.com/",
                                ""));
    EXPECT_TRUE(RequestContainsResource(request,
                                        ClientDownloadRequest::DOWNLOAD_URL,
                                        "http://www.google.com/bla.exe",
                                        referrer_.spec()));
    EXPECT_TRUE(RequestContainsResource(request,
                                        ClientDownloadRequest::TAB_REDIRECT,
                                        "http://tab.com/ref1",
                                        ""));
    EXPECT_TRUE(RequestContainsResource(request,
                                        ClientDownloadRequest::TAB_REDIRECT,
                                        "http://tab.com/ref2",
                                        ""));
    EXPECT_TRUE(RequestContainsResource(request,
                                        ClientDownloadRequest::TAB_URL,
                                        tab_url.spec(),
                                        tab_referrer.spec()));
    EXPECT_TRUE(request.has_signature());
    ASSERT_EQ(1, request.signature().certificate_chain_size());
    const ClientDownloadRequest_CertificateChain& chain =
        request.signature().certificate_chain(0);
    ASSERT_EQ(1, chain.element_size());
    EXPECT_EQ("dummy cert data", chain.element(0).certificate());

    // Simulate the request finishing.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&DownloadProtectionServiceTest::SendURLFetchComplete,
                       base::Unretained(this), fetcher));
    run_loop.Run();
  }
}

TEST_F(DownloadProtectionServiceTest, TestCheckDownloadUrl) {
  net::TestURLFetcherFactory factory;

  std::vector<GURL> url_chain;
  url_chain.push_back(GURL("http://www.google.com/"));
  url_chain.push_back(GURL("http://www.google.com/bla.exe"));
  GURL referrer("http://www.google.com/");
  std::string hash = "hash";

  NiceMockDownloadItem item;
  EXPECT_CALL(item, GetURL()).WillRepeatedly(ReturnRef(url_chain.back()));
  EXPECT_CALL(item, GetUrlChain()).WillRepeatedly(ReturnRef(url_chain));
  EXPECT_CALL(item, GetReferrerUrl()).WillRepeatedly(ReturnRef(referrer));
  EXPECT_CALL(item, GetHash()).WillRepeatedly(ReturnRef(hash));

  {
    // CheckDownloadURL returns immediately which means the client object
    // callback will never be called.  Nevertheless the callback provided
    // to CheckClientDownload must still be called.
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                CheckDownloadUrl(ContainerEq(url_chain), NotNull()))
        .WillOnce(Return(true));
    RunLoop run_loop;
    download_service_->CheckDownloadUrl(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
    Mock::VerifyAndClearExpectations(sb_service_.get());
  }
  {
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                CheckDownloadUrl(ContainerEq(url_chain), NotNull()))
        .WillOnce(
            DoAll(CheckDownloadUrlDone(SB_THREAT_TYPE_SAFE), Return(false)));
    RunLoop run_loop;
    download_service_->CheckDownloadUrl(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
    Mock::VerifyAndClearExpectations(sb_service_.get());
  }
  {
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                CheckDownloadUrl(ContainerEq(url_chain), NotNull()))
        .WillOnce(DoAll(CheckDownloadUrlDone(SB_THREAT_TYPE_URL_MALWARE),
                        Return(false)));
    RunLoop run_loop;
    download_service_->CheckDownloadUrl(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
    Mock::VerifyAndClearExpectations(sb_service_.get());
  }
  {
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                CheckDownloadUrl(ContainerEq(url_chain), NotNull()))
        .WillOnce(DoAll(CheckDownloadUrlDone(SB_THREAT_TYPE_BINARY_MALWARE_URL),
                        Return(false)));
    RunLoop run_loop;
    download_service_->CheckDownloadUrl(
        &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
  }
}

TEST_F(DownloadProtectionServiceTest, TestDownloadRequestTimeout) {
  net::TestURLFetcherFactory factory;

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
    &item,
    {"http://www.evil.com/bla.exe"},  // url_chain
    "http://www.google.com/",         // referrer
    FILE_PATH_LITERAL("a.tmp"),       // tmp_path
    FILE_PATH_LITERAL("a.exe"));      // final_path

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(tmp_path_,
                                   BinaryFeatureExtractor::kDefaultOptions,
                                   _, _));

  download_service_->download_request_timeout_ms_ = 10;
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                        base::Unretained(this), run_loop.QuitClosure()));

  // The request should time out because the HTTP request hasn't returned
  // anything yet.
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
  EXPECT_TRUE(HasClientDownloadRequest());
  ClearClientDownloadRequest();
}

TEST_F(DownloadProtectionServiceTest, TestDownloadItemDestroyed) {
  {
    NiceMockDownloadItem item;
    PrepareBasicDownloadItem(
      &item,
      {"http://www.evil.com/bla.exe"},  // url_chain
      "http://www.google.com/",         // referrer
      FILE_PATH_LITERAL("a.tmp"),       // tmp_path
      FILE_PATH_LITERAL("a.exe"));      // final_path
    GURL tab_url("http://www.google.com/tab");
    EXPECT_CALL(item, GetTabUrl()).WillRepeatedly(ReturnRef(tab_url));
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadWhitelistUrl(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

    download_service_->CheckClientDownload(
        &item,
        base::Bind(&DownloadProtectionServiceTest::SyncCheckDoneCallback,
                   base::Unretained(this)));
    // MockDownloadItem going out of scope triggers the OnDownloadDestroyed
    // notification.
  }

  EXPECT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
  EXPECT_FALSE(HasClientDownloadRequest());
}

TEST_F(DownloadProtectionServiceTest,
       TestDownloadItemDestroyedDuringWhitelistCheck) {
  net::TestURLFetcherFactory factory;
  std::unique_ptr<NiceMockDownloadItem> item(new NiceMockDownloadItem);
  PrepareBasicDownloadItem(
    item.get(),
    {"http://www.evil.com/bla.exe"},  // url_chain
    "http://www.google.com/",         // referrer
    FILE_PATH_LITERAL("a.tmp"),       // tmp_path
    FILE_PATH_LITERAL("a.exe"));      // final_path
  GURL tab_url("http://www.google.com/tab");
  EXPECT_CALL(*item, GetTabUrl()).WillRepeatedly(ReturnRef(tab_url));

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Invoke([&item](const GURL&) {
        item.reset();
        return false;
      }));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(tmp_path_,
                                   BinaryFeatureExtractor::kDefaultOptions,
                                   _, _));

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      item.get(), base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                             base::Unretained(this), run_loop.QuitClosure()));

  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
  EXPECT_FALSE(HasClientDownloadRequest());
}

TEST_F(DownloadProtectionServiceTest, GetCertificateWhitelistStrings) {
  // We'll pass this cert in as the "issuer", even though it isn't really
  // used to sign the certs below.  GetCertificateWhitelistStirngs doesn't care
  // about this.
  scoped_refptr<net::X509Certificate> issuer_cert(
      ReadTestCertificate("issuer.pem"));
  ASSERT_TRUE(issuer_cert.get());
  std::string issuer_der;
  net::X509Certificate::GetDEREncoded(issuer_cert->os_cert_handle(),
                                      &issuer_der);
  std::string hashed = base::SHA1HashString(issuer_der);
  std::string cert_base =
      "cert/" + base::HexEncode(hashed.data(), hashed.size());

  scoped_refptr<net::X509Certificate> cert(ReadTestCertificate("test_cn.pem"));
  ASSERT_TRUE(cert.get());
  std::vector<std::string> whitelist_strings;
  GetCertificateWhitelistStrings(
      *cert.get(), *issuer_cert.get(), &whitelist_strings);
  // This also tests escaping of characters in the certificate attributes.
  EXPECT_THAT(whitelist_strings, ElementsAre(
      cert_base + "/CN=subject%2F%251"));

  cert = ReadTestCertificate("test_cn_o.pem");
  ASSERT_TRUE(cert.get());
  whitelist_strings.clear();
  GetCertificateWhitelistStrings(
      *cert.get(), *issuer_cert.get(), &whitelist_strings);
  EXPECT_THAT(whitelist_strings,
              ElementsAre(cert_base + "/CN=subject",
                          cert_base + "/CN=subject/O=org",
                          cert_base + "/O=org"));

  cert = ReadTestCertificate("test_cn_o_ou.pem");
  ASSERT_TRUE(cert.get());
  whitelist_strings.clear();
  GetCertificateWhitelistStrings(
      *cert.get(), *issuer_cert.get(), &whitelist_strings);
  EXPECT_THAT(whitelist_strings,
              ElementsAre(cert_base + "/CN=subject",
                          cert_base + "/CN=subject/O=org",
                          cert_base + "/CN=subject/O=org/OU=unit",
                          cert_base + "/CN=subject/OU=unit",
                          cert_base + "/O=org",
                          cert_base + "/O=org/OU=unit",
                          cert_base + "/OU=unit"));

  cert = ReadTestCertificate("test_cn_ou.pem");
  ASSERT_TRUE(cert.get());
  whitelist_strings.clear();
  GetCertificateWhitelistStrings(
      *cert.get(), *issuer_cert.get(), &whitelist_strings);
  EXPECT_THAT(whitelist_strings,
              ElementsAre(cert_base + "/CN=subject",
                          cert_base + "/CN=subject/OU=unit",
                          cert_base + "/OU=unit"));

  cert = ReadTestCertificate("test_o.pem");
  ASSERT_TRUE(cert.get());
  whitelist_strings.clear();
  GetCertificateWhitelistStrings(
      *cert.get(), *issuer_cert.get(), &whitelist_strings);
  EXPECT_THAT(whitelist_strings, ElementsAre(cert_base + "/O=org"));

  cert = ReadTestCertificate("test_o_ou.pem");
  ASSERT_TRUE(cert.get());
  whitelist_strings.clear();
  GetCertificateWhitelistStrings(
      *cert.get(), *issuer_cert.get(), &whitelist_strings);
  EXPECT_THAT(whitelist_strings,
              ElementsAre(cert_base + "/O=org",
                          cert_base + "/O=org/OU=unit",
                          cert_base + "/OU=unit"));

  cert = ReadTestCertificate("test_ou.pem");
  ASSERT_TRUE(cert.get());
  whitelist_strings.clear();
  GetCertificateWhitelistStrings(
      *cert.get(), *issuer_cert.get(), &whitelist_strings);
  EXPECT_THAT(whitelist_strings, ElementsAre(cert_base + "/OU=unit"));

  cert = ReadTestCertificate("test_c.pem");
  ASSERT_TRUE(cert.get());
  whitelist_strings.clear();
  GetCertificateWhitelistStrings(
      *cert.get(), *issuer_cert.get(), &whitelist_strings);
  EXPECT_THAT(whitelist_strings, ElementsAre());
}

namespace {

class MockPageNavigator : public content::PageNavigator {
 public:
  MOCK_METHOD1(OpenURL, content::WebContents*(const content::OpenURLParams&));
};

// A custom matcher that matches a OpenURLParams value with a url with a query
// parameter patching |value|.
MATCHER_P(OpenURLParamsWithContextValue, value, "") {
  std::string query_value;
  return net::GetValueForKeyInQuery(arg.url, "ctx", &query_value) &&
         query_value == value;
}

}  // namespace

// ShowDetailsForDownload() should open a URL showing more information about why
// a download was flagged by SafeBrowsing. The URL should have a &ctx= parameter
// whose value is the DownloadDangerType.
TEST_F(DownloadProtectionServiceTest, ShowDetailsForDownloadHasContext) {
  StrictMock<MockPageNavigator> mock_page_navigator;
  StrictMock<content::MockDownloadItem> mock_download_item;

  EXPECT_CALL(mock_download_item, GetDangerType())
      .WillOnce(Return(content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST));
  EXPECT_CALL(mock_page_navigator, OpenURL(OpenURLParamsWithContextValue("7")));

  download_service_->ShowDetailsForDownload(mock_download_item,
                                            &mock_page_navigator);
}

TEST_F(DownloadProtectionServiceTest, GetAndSetDownloadPingToken) {
  NiceMockDownloadItem item;
  EXPECT_TRUE(DownloadProtectionService::GetDownloadPingToken(&item).empty());
  std::string token = "download_ping_token";
  DownloadProtectionService::SetDownloadPingToken(&item, token);
  EXPECT_EQ(token, DownloadProtectionService::GetDownloadPingToken(&item));

  DownloadProtectionService::SetDownloadPingToken(&item, std::string());
  EXPECT_TRUE(DownloadProtectionService::GetDownloadPingToken(&item).empty());
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_Unsupported) {
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.txt"));
  std::vector<base::FilePath::StringType> alternate_extensions{
      FILE_PATH_LITERAL(".tmp"), FILE_PATH_LITERAL(".asdfasdf")};
  download_service_->CheckPPAPIDownloadRequest(
      GURL("http://example.com/foo"), GURL(), nullptr, default_file_path,
      alternate_extensions, profile_.get(),
      base::Bind(&DownloadProtectionServiceTest::SyncCheckDoneCallback,
                 base::Unretained(this)));
  ASSERT_TRUE(IsResult(DownloadProtectionService::SAFE));
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_SupportedDefault) {
  net::FakeURLFetcherFactory factory(nullptr);
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.crx"));
  std::vector<base::FilePath::StringType> alternate_extensions;
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  struct {
    ClientDownloadResponse::Verdict verdict;
    DownloadProtectionService::DownloadCheckResult expected_result;
  } kExpectedResults[] = {
      {ClientDownloadResponse::SAFE, DownloadProtectionService::SAFE},
      {ClientDownloadResponse::DANGEROUS, DownloadProtectionService::DANGEROUS},
      {ClientDownloadResponse::UNCOMMON, DownloadProtectionService::UNCOMMON},
      {ClientDownloadResponse::DANGEROUS_HOST,
       DownloadProtectionService::DANGEROUS_HOST},
      {ClientDownloadResponse::POTENTIALLY_UNWANTED,
       DownloadProtectionService::POTENTIALLY_UNWANTED},
      {ClientDownloadResponse::UNKNOWN, DownloadProtectionService::UNKNOWN},
  };

  for (const auto& test_case : kExpectedResults) {
    factory.ClearFakeResponses();
    PrepareResponse(&factory, test_case.verdict, net::HTTP_OK,
                    net::URLRequestStatus::SUCCESS);
    SetExtendedReportingPreference(true);
    RunLoop run_loop;
    download_service_->CheckPPAPIDownloadRequest(
        GURL("http://example.com/foo"), GURL(), nullptr, default_file_path,
        alternate_extensions, profile_.get(),
        base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                   base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_TRUE(IsResult(test_case.expected_result));
    ASSERT_EQ(ChromeUserPopulation::EXTENDED_REPORTING,
              GetClientDownloadRequest()->population().user_population());
  }
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_SupportedAlternate) {
  net::FakeURLFetcherFactory factory(nullptr);
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.txt"));
  std::vector<base::FilePath::StringType> alternate_extensions{
      FILE_PATH_LITERAL(".tmp"), FILE_PATH_LITERAL(".crx")};
  PrepareResponse(&factory, ClientDownloadResponse::DANGEROUS, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  SetExtendedReportingPreference(false);
  RunLoop run_loop;
  download_service_->CheckPPAPIDownloadRequest(
      GURL("http://example.com/foo"), GURL(), nullptr, default_file_path,
      alternate_extensions, profile_.get(),
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
  ASSERT_EQ(ChromeUserPopulation::SAFE_BROWSING,
            GetClientDownloadRequest()->population().user_population());
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_WhitelistedURL) {
  net::FakeURLFetcherFactory factory(nullptr);
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.crx"));
  std::vector<base::FilePath::StringType> alternate_extensions;
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(true));

  RunLoop run_loop;
  download_service_->CheckPPAPIDownloadRequest(
      GURL("http://example.com/foo"), GURL(), nullptr, default_file_path,
      alternate_extensions, profile_.get(),
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(IsResult(DownloadProtectionService::SAFE));
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_FetchFailed) {
  net::FakeURLFetcherFactory factory(nullptr);
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.crx"));
  std::vector<base::FilePath::StringType> alternate_extensions;
  PrepareResponse(&factory, ClientDownloadResponse::DANGEROUS, net::HTTP_OK,
                  net::URLRequestStatus::FAILED);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  RunLoop run_loop;
  download_service_->CheckPPAPIDownloadRequest(
      GURL("http://example.com/foo"), GURL(), nullptr, default_file_path,
      alternate_extensions, profile_.get(),
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_InvalidResponse) {
  net::FakeURLFetcherFactory factory(nullptr);
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.crx"));
  std::vector<base::FilePath::StringType> alternate_extensions;
  factory.SetFakeResponse(DownloadProtectionService::GetDownloadRequestUrl(),
                          "Hello world!", net::HTTP_OK,
                          net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  RunLoop run_loop;
  download_service_->CheckPPAPIDownloadRequest(
      GURL("http://example.com/foo"), GURL(), nullptr, default_file_path,
      alternate_extensions, profile_.get(),
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_Timeout) {
  net::FakeURLFetcherFactory factory(nullptr);
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.crx"));
  std::vector<base::FilePath::StringType> alternate_extensions;
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  PrepareResponse(&factory, ClientDownloadResponse::SAFE, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  download_service_->download_request_timeout_ms_ = 0;
  RunLoop run_loop;
  download_service_->CheckPPAPIDownloadRequest(
      GURL("http://example.com/foo"), GURL(), nullptr, default_file_path,
      alternate_extensions, profile_.get(),
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(IsResult(DownloadProtectionService::UNKNOWN));
}

namespace {

std::unique_ptr<net::FakeURLFetcher> FakeURLFetcherCreatorFunc(
    std::string* upload_data_receiver,
    const GURL& url,
    net::URLFetcherDelegate* delegate,
    const std::string& response_body,
    net::HttpStatusCode response_code,
    net::URLRequestStatus::Status status) {
  class URLFetcher : public net::FakeURLFetcher {
   public:
    URLFetcher(std::string* upload_data_receiver,
               const GURL& url,
               net::URLFetcherDelegate* delegate,
               const std::string& response_body,
               net::HttpStatusCode response_code,
               net::URLRequestStatus::Status status)
        : FakeURLFetcher(url, delegate, response_body, response_code, status),
          upload_data_receiver_(upload_data_receiver) {}

    void SetUploadData(const std::string& upload_content_type,
                       const std::string& upload_content) override {
      *upload_data_receiver_ = upload_content;
      FakeURLFetcher::SetUploadData(upload_content_type, upload_content);
    }

   private:
    std::string* upload_data_receiver_;
  };

  return std::unique_ptr<net::FakeURLFetcher>(
      new URLFetcher(upload_data_receiver, url, delegate, response_body,
                     response_code, status));
}

}  // namespace

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_Payload) {
  std::string upload_data;
  net::FakeURLFetcherFactory factory(
      nullptr, base::Bind(&FakeURLFetcherCreatorFunc, &upload_data));
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.crx"));
  std::vector<base::FilePath::StringType> alternate_extensions{
      FILE_PATH_LITERAL(".txt"), FILE_PATH_LITERAL(".abc"),
      FILE_PATH_LITERAL(""), FILE_PATH_LITERAL(".sdF")};
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  PrepareResponse(&factory, ClientDownloadResponse::SAFE, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  const GURL kRequestorUrl("http://example.com/foo");
  RunLoop run_loop;
  download_service_->CheckPPAPIDownloadRequest(
      kRequestorUrl, GURL(), nullptr, default_file_path, alternate_extensions,
      profile_.get(),
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_FALSE(upload_data.empty());

  ClientDownloadRequest request;
  ASSERT_TRUE(request.ParseFromString(upload_data));

  EXPECT_EQ(ClientDownloadRequest::PPAPI_SAVE_REQUEST, request.download_type());
  EXPECT_EQ(kRequestorUrl.spec(), request.url());
  EXPECT_EQ("test.crx", request.file_basename());
  ASSERT_EQ(3, request.alternate_extensions_size());
  EXPECT_EQ(".txt", request.alternate_extensions(0));
  EXPECT_EQ(".abc", request.alternate_extensions(1));
  EXPECT_EQ(".sdF", request.alternate_extensions(2));
}

// ------------ class DownloadProtectionServiceFlagTest ----------------
class DownloadProtectionServiceFlagTest : public DownloadProtectionServiceTest {
 protected:
  DownloadProtectionServiceFlagTest()
      // Matches unsigned.exe within zipfile_one_unsigned_binary.zip
      : blacklisted_hash_hex_("1e954d9ce0389e2ba7447216f21761f98d1e6540c2abecdbecff570e36c493db") {}

  void SetUp() override {
    std::vector<uint8_t> bytes;
    ASSERT_TRUE(base::HexStringToBytes(blacklisted_hash_hex_, &bytes) &&
                bytes.size() == 32);
    blacklisted_hash_ = std::string(bytes.begin(), bytes.end());

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        safe_browsing::switches::kSbManualDownloadBlacklist,
        blacklisted_hash_hex_);

    DownloadProtectionServiceTest::SetUp();
  }

  // Hex 64 chars
  const std::string blacklisted_hash_hex_;
  // Binary 32 bytes
  std::string blacklisted_hash_;
};

TEST_F(DownloadProtectionServiceFlagTest, CheckClientDownloadOverridenByFlag) {
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
    &item,
    {"http://www.evil.com/a.exe"},  // url_chain
    "http://www.google.com/",       // referrer
    FILE_PATH_LITERAL("a.tmp"),     // tmp_path
    FILE_PATH_LITERAL("a.exe"));    // final_path
  EXPECT_CALL(item, GetHash()).WillRepeatedly(ReturnRef(blacklisted_hash_));
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                        base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(HasClientDownloadRequest());
  // Overriden by flag:
  EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
}

// Test a real .zip with a real .exe in it, where the .exe is manually
// blacklisted by hash.
TEST_F(DownloadProtectionServiceFlagTest,
       CheckClientDownloadZipOverridenByFlag) {
  NiceMockDownloadItem item;

  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/a.exe"},  // url_chain
      "http://www.google.com/",              // referrer
      testdata_path_.AppendASCII(
          "zipfile_one_unsigned_binary.zip"),                   // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("a.zip")));  // final_path

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                        base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(HasClientDownloadRequest());
  // Overriden by flag:
  EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
}

}  // namespace safe_browsing
