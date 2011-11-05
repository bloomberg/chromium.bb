// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection_service.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/signature_util.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "content/browser/download/download_item.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "content/test/test_browser_thread.h"
#include "content/test/test_url_fetcher_factory.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::_;
using content::BrowserThread;

namespace safe_browsing {
namespace {
class MockSafeBrowsingService : public SafeBrowsingService {
 public:
  MockSafeBrowsingService() {}
  virtual ~MockSafeBrowsingService() {}

  MOCK_METHOD1(MatchDownloadWhitelistUrl, bool(const GURL&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingService);
};

class MockSignatureUtil : public SignatureUtil {
 public:
  MockSignatureUtil() {}

  MOCK_METHOD2(CheckSignature,
               void(const FilePath&, ClientDownloadRequest_SignatureInfo*));

 private:
  virtual ~MockSignatureUtil() {}
  DISALLOW_COPY_AND_ASSIGN(MockSignatureUtil);
};
}  // namespace

ACTION_P(SetCertificateContents, contents) {
  arg1->set_certificate_contents(contents);
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
    file_thread_.reset(new content::TestBrowserThread(BrowserThread::FILE));
    ASSERT_TRUE(file_thread_->Start());
    sb_service_ = new MockSafeBrowsingService();
    signature_util_ = new MockSignatureUtil();
    download_service_ = sb_service_->download_protection_service();
    download_service_->signature_util_ = signature_util_;
    download_service_->SetEnabled(true);
    msg_loop_.RunAllPending();
  }

  virtual void TearDown() {
    // Flush all of the thread message loops to ensure that there are no
    // tasks currently running.
    FlushThreadMessageLoops();
    sb_service_ = NULL;
    io_thread_.reset();
    file_thread_.reset();
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

  // Flushes any pending tasks in the message loops of all threads.
  void FlushThreadMessageLoops() {
    FlushMessageLoop(BrowserThread::FILE);
    FlushMessageLoop(BrowserThread::IO);
    msg_loop_.RunAllPending();
  }

 private:
  // Helper functions for FlushThreadMessageLoops.
  void RunAllPendingAndQuitUI() {
    MessageLoop::current()->RunAllPending();
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
    msg_loop_.Quit();
  }

  void SendURLFetchComplete(TestURLFetcher* fetcher) {
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

 protected:
  scoped_refptr<MockSafeBrowsingService> sb_service_;
  scoped_refptr<MockSignatureUtil> signature_util_;
  DownloadProtectionService* download_service_;
  MessageLoop msg_loop_;
  DownloadProtectionService::DownloadCheckResult result_;
  scoped_ptr<content::TestBrowserThread> io_thread_;
  scoped_ptr<content::TestBrowserThread> file_thread_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;
};

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadInvalidUrl) {
  DownloadProtectionService::DownloadInfo info;
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_EQ(DownloadProtectionService::SAFE, result_);

  // Only http is supported for now.
  info.local_file = FilePath(FILE_PATH_LITERAL("a.exe"));
  info.download_url_chain.push_back(GURL("https://www.google.com/"));
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_EQ(DownloadProtectionService::SAFE, result_);

  info.download_url_chain[0] = GURL("ftp://www.google.com/");
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_EQ(DownloadProtectionService::SAFE, result_);
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadWhitelistedUrl) {
  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("a.exe"));
  info.download_url_chain.push_back(GURL("http://www.evil.com/bla.exe"));
  info.download_url_chain.push_back(GURL("http://www.google.com/a.exe"));
  info.referrer_url = GURL("http://www.google.com/");

  EXPECT_CALL(*sb_service_, MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*sb_service_,
              MatchDownloadWhitelistUrl(GURL("http://www.google.com/a.exe")))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _)).Times(2);

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_EQ(DownloadProtectionService::SAFE, result_);

  // Check that the referrer is matched against the whitelist.
  info.download_url_chain.pop_back();
  info.referrer_url = GURL("http://www.google.com/a.exe");
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_EQ(DownloadProtectionService::SAFE, result_);
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadFetchFailed) {
  FakeURLFetcherFactory factory;
  // HTTP request will fail.
  factory.SetFakeResponse(
      DownloadProtectionService::kDownloadRequestUrl, "", false);

  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("a.exe"));
  info.download_url_chain.push_back(GURL("http://www.evil.com/a.exe"));
  info.referrer_url = GURL("http://www.google.com/");

  EXPECT_CALL(*sb_service_, MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _));

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_EQ(DownloadProtectionService::SAFE, result_);
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadSuccess) {
  ClientDownloadResponse response;
  response.set_verdict(ClientDownloadResponse::SAFE);
  FakeURLFetcherFactory factory;
  // Empty response means SAFE.
  factory.SetFakeResponse(
      DownloadProtectionService::kDownloadRequestUrl,
      response.SerializeAsString(),
      true);

  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("a.exe"));
  info.download_url_chain.push_back(GURL("http://www.evil.com/a.exe"));
  info.referrer_url = GURL("http://www.google.com/");

  EXPECT_CALL(*sb_service_, MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _)).Times(3);

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_EQ(DownloadProtectionService::SAFE, result_);

  // Invalid response should be safe too.
  response.Clear();
  factory.SetFakeResponse(
      DownloadProtectionService::kDownloadRequestUrl,
      response.SerializePartialAsString(),
      true);

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_EQ(DownloadProtectionService::SAFE, result_);

  // If the response is dangerous the result should also be marked as dangerous.
  response.set_verdict(ClientDownloadResponse::DANGEROUS);
  factory.SetFakeResponse(
      DownloadProtectionService::kDownloadRequestUrl,
      response.SerializeAsString(),
      true);

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  EXPECT_EQ(DownloadProtectionService::DANGEROUS, result_);
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadValidateRequest) {
  TestURLFetcherFactory factory;

  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("bla.exe"));
  info.download_url_chain.push_back(GURL("http://www.google.com/"));
  info.download_url_chain.push_back(GURL("http://www.google.com/bla.exe"));
  info.referrer_url = GURL("http://www.google.com/");
  info.sha256_hash = "hash";
  info.total_bytes = 100;
  info.user_initiated = false;

  EXPECT_CALL(*sb_service_, MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _))
      .WillOnce(SetCertificateContents("dummy cert data"));

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  // Run the message loop(s) until SendRequest is called.
  FlushThreadMessageLoops();

  TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  ClientDownloadRequest request;
  EXPECT_TRUE(request.ParseFromString(fetcher->upload_data()));
  EXPECT_EQ("http://www.google.com/bla.exe", request.url());
  EXPECT_EQ(info.sha256_hash, request.digests().sha256());
  EXPECT_EQ(info.total_bytes, request.length());
  EXPECT_EQ(info.user_initiated, request.user_initiated());
  EXPECT_EQ(2, request.resources_size());
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_REDIRECT,
                                      "http://www.google.com/", ""));
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_URL,
                                      "http://www.google.com/bla.exe",
                                      info.referrer_url.spec()));
  EXPECT_TRUE(request.has_signature());
  EXPECT_TRUE(request.signature().has_certificate_contents());
  EXPECT_EQ("dummy cert data", request.signature().certificate_contents());

  // Simulate the request finishing.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DownloadProtectionServiceTest::SendURLFetchComplete,
                 base::Unretained(this), fetcher));
  msg_loop_.Run();
}

// Similar to above, but with an unsigned binary.
TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadValidateRequestNoSignature) {
  TestURLFetcherFactory factory;

  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("bla.exe"));
  info.download_url_chain.push_back(GURL("http://www.google.com/"));
  info.download_url_chain.push_back(GURL("http://www.google.com/bla.exe"));
  info.referrer_url = GURL("http://www.google.com/");
  info.sha256_hash = "hash";
  info.total_bytes = 100;
  info.user_initiated = false;

  EXPECT_CALL(*sb_service_, MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _));

  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  // Run the message loop(s) until SendRequest is called.
  FlushThreadMessageLoops();

  TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  ClientDownloadRequest request;
  EXPECT_TRUE(request.ParseFromString(fetcher->upload_data()));
  EXPECT_EQ("http://www.google.com/bla.exe", request.url());
  EXPECT_EQ(info.sha256_hash, request.digests().sha256());
  EXPECT_EQ(info.total_bytes, request.length());
  EXPECT_EQ(info.user_initiated, request.user_initiated());
  EXPECT_EQ(2, request.resources_size());
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_REDIRECT,
                                      "http://www.google.com/", ""));
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_URL,
                                      "http://www.google.com/bla.exe",
                                      info.referrer_url.spec()));
  EXPECT_TRUE(request.has_signature());
  EXPECT_FALSE(request.signature().has_certificate_contents());

  // Simulate the request finishing.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DownloadProtectionServiceTest::SendURLFetchComplete,
                 base::Unretained(this), fetcher));
  msg_loop_.Run();
}
}  // namespace safe_browsing
