// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection_service.h"

#include <map>
#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/signature_util.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/common/zip.h"
#include "content/public/browser/download_item.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/x509_certificate.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainerEq;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::_;
using content::BrowserThread;
namespace safe_browsing {
namespace {
// A SafeBrowsingDatabaseManager implementation that returns a fixed result for
// a given URL.
class MockSafeBrowsingDatabaseManager : public SafeBrowsingDatabaseManager {
 public:
  explicit MockSafeBrowsingDatabaseManager(SafeBrowsingService* service)
      : SafeBrowsingDatabaseManager(service) { }

  MOCK_METHOD1(MatchDownloadWhitelistUrl, bool(const GURL&));
  MOCK_METHOD1(MatchDownloadWhitelistString, bool(const std::string&));
  MOCK_METHOD2(CheckDownloadUrl, bool(
      const std::vector<GURL>& url_chain,
      SafeBrowsingDatabaseManager::Client* client));

 private:
  virtual ~MockSafeBrowsingDatabaseManager() {}
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};

class FakeSafeBrowsingService : public SafeBrowsingService {
 public:
  FakeSafeBrowsingService() { }

  // Returned pointer has the same lifespan as the database_manager_ refcounted
  // object.
  MockSafeBrowsingDatabaseManager* mock_database_manager() {
    return mock_database_manager_;
  }

 protected:
  virtual ~FakeSafeBrowsingService() { }

  virtual SafeBrowsingDatabaseManager* CreateDatabaseManager() OVERRIDE {
    mock_database_manager_ = new MockSafeBrowsingDatabaseManager(this);
    return mock_database_manager_;
  }

 private:
  MockSafeBrowsingDatabaseManager* mock_database_manager_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingService);
};

class MockSignatureUtil : public SignatureUtil {
 public:
  MockSignatureUtil() {}
  MOCK_METHOD2(CheckSignature,
               void(const FilePath&, ClientDownloadRequest_SignatureInfo*));

 protected:
  virtual ~MockSignatureUtil() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSignatureUtil);
};
}  // namespace

ACTION_P(SetCertificateContents, contents) {
  arg1->add_certificate_chain()->add_element()->set_certificate(contents);
}

ACTION_P(TrustSignature, certificate_file) {
  arg1->set_trusted(true);
  // Add a certificate chain.  Note that we add the certificate twice so that
  // it appears as its own issuer.
  std::string cert_data;
  ASSERT_TRUE(file_util::ReadFileToString(certificate_file, &cert_data));
  ClientDownloadRequest_CertificateChain* chain =
      arg1->add_certificate_chain();
  chain->add_element()->set_certificate(cert_data);
  chain->add_element()->set_certificate(cert_data);
}

// We can't call OnSafeBrowsingResult directly because SafeBrowsingCheck does
// not have any copy constructor which means it can't be stored in a callback
// easily.  Note: check will be deleted automatically when the callback is
// deleted.
void OnSafeBrowsingResult(
    SafeBrowsingDatabaseManager::SafeBrowsingCheck* check) {
  check->client->OnSafeBrowsingResult(*check);
}

ACTION_P(CheckDownloadUrlDone, threat_type) {
  SafeBrowsingDatabaseManager::SafeBrowsingCheck* check =
      new SafeBrowsingDatabaseManager::SafeBrowsingCheck(
          arg0,
          std::vector<SBFullHash>(),
          arg1,
          safe_browsing_util::BINURL);
  for (size_t i = 0; i < check->url_results.size(); ++i)
    check->url_results[i] = threat_type;
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&OnSafeBrowsingResult,
                                     base::Owned(check)));
}

class DownloadProtectionServiceTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ui_thread_.reset(new content::TestBrowserThread(BrowserThread::UI,
                                                    &msg_loop_));
    // Start real threads for the IO and File threads so that the DCHECKs
    // to test that we're on the correct thread work.
    io_thread_.reset(new content::TestBrowserThread(BrowserThread::IO));
    ASSERT_TRUE(io_thread_->Start());
    sb_service_ = new StrictMock<FakeSafeBrowsingService>();
    sb_service_->Initialize();
    signature_util_ = new StrictMock<MockSignatureUtil>();
    download_service_ = sb_service_->download_protection_service();
    download_service_->signature_util_ = signature_util_;
    download_service_->SetEnabled(true);
    msg_loop_.RunUntilIdle();
    has_result_ = false;

    FilePath source_path;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
    testdata_path_ = source_path
        .AppendASCII("chrome")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("safe_browsing")
        .AppendASCII("download_protection");
  }

  virtual void TearDown() {
    sb_service_->ShutDown();
    // Flush all of the thread message loops to ensure that there are no
    // tasks currently running.
    FlushThreadMessageLoops();
    sb_service_ = NULL;
    io_thread_.reset();
    ui_thread_.reset();
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

  // Flushes any pending tasks in the message loops of all threads.
  void FlushThreadMessageLoops() {
    BrowserThread::GetBlockingPool()->FlushForTesting();
    FlushMessageLoop(BrowserThread::IO);
    msg_loop_.RunUntilIdle();
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
    if (!file_util::ReadFileToString(testdata_path_.AppendASCII(filename),
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

 private:
  // Helper functions for FlushThreadMessageLoops.
  void RunAllPendingAndQuitUI() {
    MessageLoop::current()->RunUntilIdle();
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DownloadProtectionServiceTest::QuitMessageLoop,
                   base::Unretained(this)));
  }

  void QuitMessageLoop() {
    MessageLoop::current()->Quit();
  }

  void PostRunMessageLoopTask(BrowserThread::ID thread) {
    BrowserThread::PostTask(
        thread,
        FROM_HERE,
        base::Bind(&DownloadProtectionServiceTest::RunAllPendingAndQuitUI,
                   base::Unretained(this)));
  }

  void FlushMessageLoop(BrowserThread::ID thread) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DownloadProtectionServiceTest::PostRunMessageLoopTask,
                   base::Unretained(this), thread));
    msg_loop_.Run();
  }

 public:
  void CheckDoneCallback(
      DownloadProtectionService::DownloadCheckResult result) {
    result_ = result;
    has_result_ = true;
    msg_loop_.Quit();
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

 protected:
  scoped_refptr<FakeSafeBrowsingService> sb_service_;
  scoped_refptr<MockSignatureUtil> signature_util_;
  DownloadProtectionService* download_service_;
  MessageLoop msg_loop_;
  DownloadProtectionService::DownloadCheckResult result_;
  bool has_result_;
  scoped_ptr<content::TestBrowserThread> io_thread_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;
  FilePath testdata_path_;
};

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadInvalidUrl) {
  DownloadProtectionService::DownloadInfo info;
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));

  info.local_file = FilePath(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.exe"));
  info.download_url_chain.push_back(GURL("file://www.google.com/"));
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadWhitelistedUrl) {
  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.exe"));
  info.download_url_chain.push_back(GURL("http://www.evil.com/bla.exe"));
  info.download_url_chain.push_back(GURL("http://www.google.com/a.exe"));
  info.referrer_url = GURL("http://www.google.com/");

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(GURL("http://www.google.com/a.exe")))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _)).Times(2);

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));

  // Check that the referrer is matched against the whitelist.
  info.download_url_chain.pop_back();
  info.referrer_url = GURL("http://www.google.com/a.exe");
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadFetchFailed) {
  net::FakeURLFetcherFactory factory;
  // HTTP request will fail.
  factory.SetFakeResponse(
      DownloadProtectionService::GetDownloadRequestUrl(), "", false);

  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.exe"));
  info.download_url_chain.push_back(GURL("http://www.evil.com/a.exe"));
  info.referrer_url = GURL("http://www.google.com/");

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _));

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadSuccess) {
  ClientDownloadResponse response;
  response.set_verdict(ClientDownloadResponse::SAFE);
  net::FakeURLFetcherFactory factory;
  // Empty response means SAFE.
  factory.SetFakeResponse(
      DownloadProtectionService::GetDownloadRequestUrl(),
      response.SerializeAsString(),
      true);

  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.exe"));
  info.download_url_chain.push_back(GURL("http://www.evil.com/a.exe"));
  info.referrer_url = GURL("http://www.google.com/");

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _)).Times(4);

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));

  // Invalid response should be safe too.
  response.Clear();
  factory.SetFakeResponse(
      DownloadProtectionService::GetDownloadRequestUrl(),
      response.SerializePartialAsString(),
      true);

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));

  // If the response is dangerous the result should also be marked as dangerous.
  response.set_verdict(ClientDownloadResponse::DANGEROUS);
  factory.SetFakeResponse(
      DownloadProtectionService::GetDownloadRequestUrl(),
      response.SerializeAsString(),
      true);

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
#if defined(OS_WIN)
  EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
#else
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
#endif

  // If the response is uncommon the result should also be marked as uncommon.
  response.set_verdict(ClientDownloadResponse::UNCOMMON);
  factory.SetFakeResponse(
      DownloadProtectionService::GetDownloadRequestUrl(),
      response.SerializeAsString(),
      true);

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
#if defined(OS_WIN)
  EXPECT_TRUE(IsResult(DownloadProtectionService::UNCOMMON));
#else
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
#endif
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadHTTPS) {
  ClientDownloadResponse response;
  response.set_verdict(ClientDownloadResponse::DANGEROUS);
  net::FakeURLFetcherFactory factory;
  factory.SetFakeResponse(
      DownloadProtectionService::GetDownloadRequestUrl(),
      response.SerializeAsString(),
      true);

  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.exe"));
  info.download_url_chain.push_back(GURL("https://www.evil.com/a.exe"));
  info.referrer_url = GURL("http://www.google.com/");

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _)).Times(1);

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
#if defined(OS_WIN)
  EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
#else
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
#endif
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadZip) {
  ClientDownloadResponse response;
  response.set_verdict(ClientDownloadResponse::SAFE);
  net::FakeURLFetcherFactory factory;
  // Empty response means SAFE.
  factory.SetFakeResponse(
      DownloadProtectionService::GetDownloadRequestUrl(),
      response.SerializeAsString(),
      true);

  base::ScopedTempDir download_dir;
  ASSERT_TRUE(download_dir.CreateUniqueTempDir());

  DownloadProtectionService::DownloadInfo info;
  info.local_file = download_dir.path().Append(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.zip"));
  info.download_url_chain.push_back(GURL("http://www.evil.com/a.zip"));
  info.referrer_url = GURL("http://www.google.com/");

  // Write out a zip archive to the temporary file.  In this case, it
  // only contains a text file.
  base::ScopedTempDir zip_source_dir;
  ASSERT_TRUE(zip_source_dir.CreateUniqueTempDir());
  std::string file_contents = "dummy file";
  ASSERT_EQ(static_cast<int>(file_contents.size()), file_util::WriteFile(
      zip_source_dir.path().Append(FILE_PATH_LITERAL("file.txt")),
      file_contents.data(), file_contents.size()));
  ASSERT_TRUE(zip::Zip(zip_source_dir.path(), info.local_file, false));

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
  Mock::VerifyAndClearExpectations(sb_service_);
  Mock::VerifyAndClearExpectations(signature_util_);

  // Now check with an executable in the zip file as well.
  ASSERT_EQ(static_cast<int>(file_contents.size()), file_util::WriteFile(
      zip_source_dir.path().Append(FILE_PATH_LITERAL("file.exe")),
      file_contents.data(), file_contents.size()));
  ASSERT_TRUE(zip::Zip(zip_source_dir.path(), info.local_file, false));

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
  Mock::VerifyAndClearExpectations(signature_util_);

  // If the response is dangerous the result should also be marked as
  // dangerous.
  response.set_verdict(ClientDownloadResponse::DANGEROUS);
  factory.SetFakeResponse(
      DownloadProtectionService::GetDownloadRequestUrl(),
      response.SerializeAsString(),
      true);

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
#if defined(OS_WIN)
  EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
#else
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
#endif
  Mock::VerifyAndClearExpectations(signature_util_);
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadCorruptZip) {
  base::ScopedTempDir download_dir;
  ASSERT_TRUE(download_dir.CreateUniqueTempDir());

  DownloadProtectionService::DownloadInfo info;
  info.local_file = download_dir.path().Append(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.zip"));
  info.download_url_chain.push_back(GURL("http://www.evil.com/a.zip"));
  info.referrer_url = GURL("http://www.google.com/");

  std::string file_contents = "corrupt zip file";
  ASSERT_EQ(static_cast<int>(file_contents.size()), file_util::WriteFile(
      download_dir.path().Append(FILE_PATH_LITERAL("a.tmp")),
      file_contents.data(), file_contents.size()));

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
  Mock::VerifyAndClearExpectations(sb_service_);
  Mock::VerifyAndClearExpectations(signature_util_);
}

TEST_F(DownloadProtectionServiceTest, CheckClientCrxDownloadSuccess) {
  ClientDownloadResponse response;
  // Even if the server verdict is dangerous we should return SAFE because
  // DownloadProtectionService::IsSupportedDownload() will return false
  // for crx downloads.
  response.set_verdict(ClientDownloadResponse::DANGEROUS);
  net::FakeURLFetcherFactory factory;
  // Empty response means SAFE.
  factory.SetFakeResponse(
      DownloadProtectionService::GetDownloadRequestUrl(),
      response.SerializeAsString(),
      true);

  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.crx"));
  info.download_url_chain.push_back(GURL("http://www.evil.com/a.crx"));
  info.referrer_url = GURL("http://www.google.com/");

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _)).Times(1);

  EXPECT_FALSE(download_service_->IsSupportedDownload(info));
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadValidateRequest) {
  net::TestURLFetcherFactory factory;

  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("bla.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("bla.exe"));
  info.download_url_chain.push_back(GURL("http://www.google.com/"));
  info.download_url_chain.push_back(GURL("http://www.google.com/bla.exe"));
  info.referrer_url = GURL("http://www.google.com/");
  info.sha256_hash = "hash";
  info.total_bytes = 100;
  info.user_initiated = true;
  info.remote_address = "10.11.12.13";

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _))
      .WillOnce(SetCertificateContents("dummy cert data"));
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));

#if !defined(OS_WIN)
  // SendRequest is not called.  Wait for FinishRequest to call our callback.
  msg_loop_.Run();
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  EXPECT_EQ(NULL, fetcher);
#else
  // Run the message loop(s) until SendRequest is called.
  FlushThreadMessageLoops();
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  ClientDownloadRequest request;
  EXPECT_TRUE(request.ParseFromString(fetcher->upload_data()));
  EXPECT_EQ("http://www.google.com/bla.exe", request.url());
  EXPECT_EQ(info.sha256_hash, request.digests().sha256());
  EXPECT_EQ(info.total_bytes, request.length());
  EXPECT_EQ(info.user_initiated, request.user_initiated());
  EXPECT_TRUE(RequestContainsServerIp(request, info.remote_address));
  EXPECT_EQ(2, request.resources_size());
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_REDIRECT,
                                      "http://www.google.com/", ""));
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_URL,
                                      "http://www.google.com/bla.exe",
                                      info.referrer_url.spec()));
  EXPECT_TRUE(request.has_signature());
  ASSERT_EQ(1, request.signature().certificate_chain_size());
  const ClientDownloadRequest_CertificateChain& chain =
      request.signature().certificate_chain(0);
  ASSERT_EQ(1, chain.element_size());
  EXPECT_EQ("dummy cert data", chain.element(0).certificate());

  // Simulate the request finishing.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DownloadProtectionServiceTest::SendURLFetchComplete,
                 base::Unretained(this), fetcher));
  msg_loop_.Run();
#endif
}

// Similar to above, but with an unsigned binary.
TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadValidateRequestNoSignature) {
  net::TestURLFetcherFactory factory;

  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("bla.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("bla.exe"));
  info.download_url_chain.push_back(GURL("http://www.google.com/"));
  info.download_url_chain.push_back(GURL("ftp://www.google.com/bla.exe"));
  info.referrer_url = GURL("http://www.google.com/");
  info.sha256_hash = "hash";
  info.total_bytes = 100;
  info.user_initiated = false;

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _));
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));

#if !defined(OS_WIN)
  // SendRequest is not called.  Wait for FinishRequest to call our callback.
  msg_loop_.Run();
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  EXPECT_EQ(NULL, fetcher);
#else
  // Run the message loop(s) until SendRequest is called.
  FlushThreadMessageLoops();
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  ClientDownloadRequest request;
  EXPECT_TRUE(request.ParseFromString(fetcher->upload_data()));
  EXPECT_EQ("ftp://www.google.com/bla.exe", request.url());
  EXPECT_EQ(info.sha256_hash, request.digests().sha256());
  EXPECT_EQ(info.total_bytes, request.length());
  EXPECT_EQ(info.user_initiated, request.user_initiated());
  EXPECT_EQ(2, request.resources_size());
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_REDIRECT,
                                      "http://www.google.com/", ""));
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_URL,
                                      "ftp://www.google.com/bla.exe",
                                      info.referrer_url.spec()));
  EXPECT_TRUE(request.has_signature());
  EXPECT_EQ(0, request.signature().certificate_chain_size());

  // Simulate the request finishing.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DownloadProtectionServiceTest::SendURLFetchComplete,
                 base::Unretained(this), fetcher));
  msg_loop_.Run();
#endif
}

TEST_F(DownloadProtectionServiceTest, TestCheckDownloadUrl) {
  DownloadProtectionService::DownloadInfo info;
  info.download_url_chain.push_back(GURL("http://www.google.com/"));
  info.download_url_chain.push_back(GURL("http://www.google.com/bla.exe"));
  info.referrer_url = GURL("http://www.google.com/");

  // CheckDownloadURL returns immediately which means the client object callback
  // will never be called.  Nevertheless the callback provided to
  // CheckClientDownload must still be called.
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              CheckDownloadUrl(ContainerEq(info.download_url_chain),
                               NotNull()))
      .WillOnce(Return(true));
  download_service_->CheckDownloadUrl(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
  Mock::VerifyAndClearExpectations(sb_service_);

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              CheckDownloadUrl(ContainerEq(info.download_url_chain),
                               NotNull()))
      .WillOnce(DoAll(CheckDownloadUrlDone(SB_THREAT_TYPE_SAFE),
                      Return(false)));
  download_service_->CheckDownloadUrl(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
  Mock::VerifyAndClearExpectations(sb_service_);

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              CheckDownloadUrl(ContainerEq(info.download_url_chain),
                               NotNull()))
      .WillOnce(DoAll(
          CheckDownloadUrlDone(SB_THREAT_TYPE_URL_MALWARE),
          Return(false)));
  download_service_->CheckDownloadUrl(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
  Mock::VerifyAndClearExpectations(sb_service_);

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              CheckDownloadUrl(ContainerEq(info.download_url_chain),
                               NotNull()))
      .WillOnce(DoAll(
          CheckDownloadUrlDone(SB_THREAT_TYPE_BINARY_MALWARE_URL),
          Return(false)));
  download_service_->CheckDownloadUrl(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::DANGEROUS));
}

TEST_F(DownloadProtectionServiceTest, TestDownloadRequestTimeout) {
  net::TestURLFetcherFactory factory;

  DownloadProtectionService::DownloadInfo info;
  info.download_url_chain.push_back(GURL("http://www.evil.com/bla.exe"));
  info.referrer_url = GURL("http://www.google.com/");
  info.local_file = FilePath(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.exe"));

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _));

  download_service_->download_request_timeout_ms_ = 10;
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));

  // The request should time out because the HTTP request hasn't returned
  // anything yet.
  msg_loop_.Run();
  EXPECT_TRUE(IsResult(DownloadProtectionService::SAFE));
}

TEST_F(DownloadProtectionServiceTest, GetCertificateWhitelistStrings) {
  // We'll pass this cert in as the "issuer", even though it isn't really
  // used to sign the certs below.  GetCertificateWhitelistStirngs doesn't care
  // about this.
  scoped_refptr<net::X509Certificate> issuer_cert(
      ReadTestCertificate("issuer.pem"));
  ASSERT_TRUE(issuer_cert.get());
  std::string cert_base = "cert/" + base::HexEncode(
      issuer_cert->fingerprint().data,
      sizeof(issuer_cert->fingerprint().data));

  scoped_refptr<net::X509Certificate> cert(ReadTestCertificate("test_cn.pem"));
  ASSERT_TRUE(cert.get());
  std::vector<std::string> whitelist_strings;
  GetCertificateWhitelistStrings(*cert, *issuer_cert, &whitelist_strings);
  // This also tests escaping of characters in the certificate attributes.
  EXPECT_THAT(whitelist_strings, ElementsAre(
      cert_base + "/CN=subject%2F%251"));

  cert = ReadTestCertificate("test_cn_o.pem");
  ASSERT_TRUE(cert.get());
  whitelist_strings.clear();
  GetCertificateWhitelistStrings(*cert, *issuer_cert, &whitelist_strings);
  EXPECT_THAT(whitelist_strings, ElementsAre(
      cert_base + "/CN=subject",
      cert_base + "/CN=subject/O=org",
      cert_base + "/O=org"));

  cert = ReadTestCertificate("test_cn_o_ou.pem");
  ASSERT_TRUE(cert.get());
  whitelist_strings.clear();
  GetCertificateWhitelistStrings(*cert, *issuer_cert, &whitelist_strings);
  EXPECT_THAT(whitelist_strings, ElementsAre(
      cert_base + "/CN=subject",
      cert_base + "/CN=subject/O=org",
      cert_base + "/CN=subject/O=org/OU=unit",
      cert_base + "/CN=subject/OU=unit",
      cert_base + "/O=org",
      cert_base + "/O=org/OU=unit",
      cert_base + "/OU=unit"));

  cert = ReadTestCertificate("test_cn_ou.pem");
  ASSERT_TRUE(cert.get());
  whitelist_strings.clear();
  GetCertificateWhitelistStrings(*cert, *issuer_cert, &whitelist_strings);
  EXPECT_THAT(whitelist_strings, ElementsAre(
      cert_base + "/CN=subject",
      cert_base + "/CN=subject/OU=unit",
      cert_base + "/OU=unit"));

  cert = ReadTestCertificate("test_o.pem");
  ASSERT_TRUE(cert.get());
  whitelist_strings.clear();
  GetCertificateWhitelistStrings(*cert, *issuer_cert, &whitelist_strings);
  EXPECT_THAT(whitelist_strings, ElementsAre(cert_base + "/O=org"));

  cert = ReadTestCertificate("test_o_ou.pem");
  ASSERT_TRUE(cert.get());
  whitelist_strings.clear();
  GetCertificateWhitelistStrings(*cert, *issuer_cert, &whitelist_strings);
  EXPECT_THAT(whitelist_strings, ElementsAre(
      cert_base + "/O=org",
      cert_base + "/O=org/OU=unit",
      cert_base + "/OU=unit"));

  cert = ReadTestCertificate("test_ou.pem");
  ASSERT_TRUE(cert.get());
  whitelist_strings.clear();
  GetCertificateWhitelistStrings(*cert, *issuer_cert, &whitelist_strings);
  EXPECT_THAT(whitelist_strings, ElementsAre(cert_base + "/OU=unit"));

  cert = ReadTestCertificate("test_c.pem");
  ASSERT_TRUE(cert.get());
  whitelist_strings.clear();
  GetCertificateWhitelistStrings(*cert, *issuer_cert, &whitelist_strings);
  EXPECT_THAT(whitelist_strings, ElementsAre());
}
}  // namespace safe_browsing
