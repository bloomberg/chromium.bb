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

using ::testing::ContainerEq;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::_;
using content::BrowserThread;

namespace safe_browsing {
namespace {
class MockSafeBrowsingService : public SafeBrowsingService {
 public:
  MockSafeBrowsingService() {}
  virtual ~MockSafeBrowsingService() {}

  MOCK_METHOD1(MatchDownloadWhitelistUrl, bool(const GURL&));
  MOCK_METHOD2(CheckDownloadUrl, bool(const std::vector<GURL>& url_chain,
                                      Client* client));
  MOCK_METHOD2(CheckDownloadHash, bool(const std::string&, Client* client));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingService);
};

class MockSignatureUtil : public SignatureUtil {
 public:
  MockSignatureUtil() {}
  virtual ~MockSignatureUtil() {}
  MOCK_METHOD2(CheckSignature,
               void(const FilePath&, ClientDownloadRequest_SignatureInfo*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSignatureUtil);
};
}  // namespace

ACTION_P(SetCertificateContents, contents) {
  arg1->add_certificate_chain()->add_element()->set_certificate(contents);
}

ACTION(TrustSignature) {
  arg1->set_trusted(true);
}

// We can't call OnSafeBrowsingResult directly because SafeBrowsingCheck does
// not have any copy constructor which means it can't be stored in a callback
// easily.  Note: check will be deleted automatically when the callback is
// deleted.
void OnSafeBrowsingResult(SafeBrowsingService::SafeBrowsingCheck* check) {
  check->client->OnSafeBrowsingResult(*check);
}

ACTION_P(CheckDownloadUrlDone, result) {
  SafeBrowsingService::SafeBrowsingCheck* check =
      new SafeBrowsingService::SafeBrowsingCheck();
  check->urls = arg0;
  check->is_download = true;
  check->result = result;
  check->client = arg1;
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&OnSafeBrowsingResult,
                                     base::Owned(check)));
}

ACTION_P(CheckDownloadHashDone, result) {
  SafeBrowsingService::SafeBrowsingCheck* check =
      new SafeBrowsingService::SafeBrowsingCheck();
  check->full_hash.reset(new SBFullHash());
  CHECK_EQ(arg0.size(), sizeof(check->full_hash->full_hash));
  memcpy(check->full_hash->full_hash, arg0.data(), arg0.size());
  check->result = result;
  check->client = arg1;
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
    file_thread_.reset(new content::TestBrowserThread(BrowserThread::FILE));
    ASSERT_TRUE(file_thread_->Start());
    sb_service_ = new StrictMock<MockSafeBrowsingService>();
    signature_util_ = new StrictMock<MockSignatureUtil>();
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
    result_.reset(new DownloadProtectionService::DownloadCheckResult(result));
    msg_loop_.Quit();
  }

  void SendURLFetchComplete(TestURLFetcher* fetcher) {
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void ExpectResult(DownloadProtectionService::DownloadCheckResult expected) {
    ASSERT_TRUE(result_.get());
    EXPECT_EQ(expected, *result_);
  }

 protected:
  scoped_refptr<MockSafeBrowsingService> sb_service_;
  scoped_refptr<MockSignatureUtil> signature_util_;
  DownloadProtectionService* download_service_;
  MessageLoop msg_loop_;
  scoped_ptr<DownloadProtectionService::DownloadCheckResult> result_;
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
  ExpectResult(DownloadProtectionService::SAFE);

  // Only https is not supported for now for privacy reasons.
  info.local_file = FilePath(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.exe"));
  info.download_url_chain.push_back(GURL("https://www.google.com/"));
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  ExpectResult(DownloadProtectionService::SAFE);
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadWhitelistedUrl) {
  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.exe"));
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
  ExpectResult(DownloadProtectionService::SAFE);

  // Check that the referrer is matched against the whitelist.
  info.download_url_chain.pop_back();
  info.referrer_url = GURL("http://www.google.com/a.exe");
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  ExpectResult(DownloadProtectionService::SAFE);
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadFetchFailed) {
  FakeURLFetcherFactory factory;
  // HTTP request will fail.
  factory.SetFakeResponse(
      DownloadProtectionService::kDownloadRequestUrl, "", false);

  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.exe"));
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
  ExpectResult(DownloadProtectionService::SAFE);
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
  info.local_file = FilePath(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.exe"));
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
  ExpectResult(DownloadProtectionService::SAFE);

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
  ExpectResult(DownloadProtectionService::SAFE);

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
  ExpectResult(DownloadProtectionService::DANGEROUS);
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadValidateRequest) {
  TestURLFetcherFactory factory;

  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("bla.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("bla.exe"));
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
}

// Similar to above, but with an unsigned binary.
TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadValidateRequestNoSignature) {
  TestURLFetcherFactory factory;

  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("bla.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("bla.exe"));
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
  EXPECT_EQ(0, request.signature().certificate_chain_size());

  // Simulate the request finishing.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DownloadProtectionServiceTest::SendURLFetchComplete,
                 base::Unretained(this), fetcher));
  msg_loop_.Run();
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadDigestList) {
  DownloadProtectionService::DownloadInfo info;
  info.local_file = FilePath(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.exe"));

  // HTTPs URLs never result in a server ping for privacy reasons.  However,
  // we do lookup the bad binary digest list.
  info.download_url_chain.push_back(GURL("https://www.evil.com/a.exe"));
  info.referrer_url = GURL("http://www.google.com/");
  info.sha256_hash = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

  // CheckDownloadHash returns immediately which means the hash is not
  // malicious.
  EXPECT_CALL(*sb_service_,
              CheckDownloadHash(info.sha256_hash, NotNull()))
      .WillOnce(Return(true));
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  ExpectResult(DownloadProtectionService::SAFE);
  Mock::VerifyAndClearExpectations(sb_service_);

  // The hash does not match the bad binary digest list.
  EXPECT_CALL(*sb_service_,
              CheckDownloadHash(info.sha256_hash, NotNull()))
      .WillOnce(DoAll(CheckDownloadHashDone(SafeBrowsingService::SAFE),
                      Return(false)));
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  ExpectResult(DownloadProtectionService::SAFE);
  Mock::VerifyAndClearExpectations(sb_service_);

  // The hash matches the bad binary URL list but not the bad binary digest
  // list.  This is an artificial example to make sure we really check the
  // result value in the code.
  EXPECT_CALL(*sb_service_,
              CheckDownloadHash(info.sha256_hash, NotNull()))
      .WillOnce(DoAll(
          CheckDownloadHashDone(SafeBrowsingService::BINARY_MALWARE_URL),
          Return(false)));
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  ExpectResult(DownloadProtectionService::SAFE);
  Mock::VerifyAndClearExpectations(sb_service_);

  // A match is found with the bad binary digest list.  We currently do not
  // warn based on the digest list.  Hence we should always return SAFE.
  EXPECT_CALL(*sb_service_,
              CheckDownloadHash(info.sha256_hash, NotNull()))
      .WillOnce(DoAll(
          CheckDownloadHashDone(SafeBrowsingService::BINARY_MALWARE_HASH),
          Return(false)));
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  ExpectResult(DownloadProtectionService::SAFE);
  Mock::VerifyAndClearExpectations(sb_service_);

  // If the download is not an executable we do not send a server ping but we
  // still want to lookup the binary digest list.
  info.target_file = FilePath(FILE_PATH_LITERAL("a.pdf"));
  info.download_url_chain[0] = GURL("http://www.evil.com/a.pdf");
  EXPECT_CALL(*sb_service_,
              CheckDownloadHash(info.sha256_hash, NotNull()))
      .WillOnce(DoAll(
          CheckDownloadHashDone(SafeBrowsingService::BINARY_MALWARE_HASH),
          Return(false)));
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  ExpectResult(DownloadProtectionService::SAFE);
  Mock::VerifyAndClearExpectations(sb_service_);

  // If the URL or the referrer matches the download whitelist we cannot send
  // a server ping for privacy reasons but we still match the digest against
  // the bad binary digest list.
  info.target_file = FilePath(FILE_PATH_LITERAL("a.exe"));
  info.download_url_chain[0] = GURL("http://www.evil.com/a.exe");
  EXPECT_CALL(*sb_service_, MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _));
  EXPECT_CALL(*sb_service_,
              CheckDownloadHash(info.sha256_hash, NotNull()))
      .WillOnce(DoAll(
          CheckDownloadHashDone(SafeBrowsingService::BINARY_MALWARE_HASH),
          Return(false)));
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  ExpectResult(DownloadProtectionService::SAFE);
  Mock::VerifyAndClearExpectations(sb_service_);

  // If the binary is a trusted executable we will not ping the server but
  // we will still lookup the digest list.
  EXPECT_CALL(*sb_service_, MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*signature_util_, CheckSignature(info.local_file, _))
      .WillOnce(TrustSignature());
  EXPECT_CALL(*sb_service_,
              CheckDownloadHash(info.sha256_hash, NotNull()))
      .WillOnce(DoAll(CheckDownloadHashDone(SafeBrowsingService::SAFE),
                      Return(false)));
  download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  ExpectResult(DownloadProtectionService::SAFE);
}

TEST_F(DownloadProtectionServiceTest, TestCheckDownloadUrl) {
  DownloadProtectionService::DownloadInfo info;
  info.download_url_chain.push_back(GURL("http://www.google.com/"));
  info.download_url_chain.push_back(GURL("http://www.google.com/bla.exe"));
  info.referrer_url = GURL("http://www.google.com/");

  // CheckDownloadURL returns immediately which means the client object callback
  // will never be called.  Nevertheless the callback provided to
  // CheckClientDownload must still be called.
  EXPECT_CALL(*sb_service_,
              CheckDownloadUrl(ContainerEq(info.download_url_chain),
                               NotNull()))
      .WillOnce(Return(true));
  download_service_->CheckDownloadUrl(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  ExpectResult(DownloadProtectionService::SAFE);
  Mock::VerifyAndClearExpectations(sb_service_);

  EXPECT_CALL(*sb_service_,
              CheckDownloadUrl(ContainerEq(info.download_url_chain),
                               NotNull()))
      .WillOnce(DoAll(CheckDownloadUrlDone(SafeBrowsingService::SAFE),
                      Return(false)));
  download_service_->CheckDownloadUrl(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  ExpectResult(DownloadProtectionService::SAFE);
  Mock::VerifyAndClearExpectations(sb_service_);

  EXPECT_CALL(*sb_service_,
              CheckDownloadUrl(ContainerEq(info.download_url_chain),
                               NotNull()))
      .WillOnce(DoAll(
          CheckDownloadUrlDone(SafeBrowsingService::URL_MALWARE),
          Return(false)));
  download_service_->CheckDownloadUrl(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  ExpectResult(DownloadProtectionService::SAFE);
  Mock::VerifyAndClearExpectations(sb_service_);

  EXPECT_CALL(*sb_service_,
              CheckDownloadUrl(ContainerEq(info.download_url_chain),
                               NotNull()))
      .WillOnce(DoAll(
          CheckDownloadUrlDone(SafeBrowsingService::BINARY_MALWARE_URL),
          Return(false)));
  download_service_->CheckDownloadUrl(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this)));
  msg_loop_.Run();
  ExpectResult(DownloadProtectionService::DANGEROUS);
}

TEST_F(DownloadProtectionServiceTest, TestDownloadRequestTimeout) {
  TestURLFetcherFactory factory;

  DownloadProtectionService::DownloadInfo info;
  info.download_url_chain.push_back(GURL("http://www.evil.com/bla.exe"));
  info.referrer_url = GURL("http://www.google.com/");
  info.local_file = FilePath(FILE_PATH_LITERAL("a.tmp"));
  info.target_file = FilePath(FILE_PATH_LITERAL("a.exe"));

  EXPECT_CALL(*sb_service_, MatchDownloadWhitelistUrl(_))
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
  ExpectResult(DownloadProtectionService::SAFE);
}
}  // namespace safe_browsing
