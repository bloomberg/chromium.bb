// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection_service.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "content/browser/browser_thread.h"
#include "content/common/net/url_fetcher.h"
#include "content/test/test_url_fetcher_factory.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::_;

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
}  // namespace

class DownloadProtectionServiceTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ui_thread_.reset(new BrowserThread(BrowserThread::UI, &msg_loop_));
    io_thread_.reset(new BrowserThread(BrowserThread::IO, &msg_loop_));
    sb_service_ = new MockSafeBrowsingService();
    download_service_ = new DownloadProtectionService(sb_service_.get(),
                                                      NULL);
    download_service_->SetEnabled(true);
    msg_loop_.RunAllPending();
  }

  virtual void TearDown() {
    msg_loop_.RunAllPending();
    download_service_ = NULL;
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

 public:
  void CheckDoneCallback(
      DownloadProtectionService::DownloadCheckResult result) {
    result_ = result;
    msg_loop_.Quit();
  }

 protected:
  scoped_refptr<MockSafeBrowsingService> sb_service_;
  scoped_refptr<DownloadProtectionService> download_service_;
  MessageLoop msg_loop_;
  DownloadProtectionService::DownloadCheckResult result_;
  scoped_ptr<BrowserThread> io_thread_;
  scoped_ptr<BrowserThread> ui_thread_;
};

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadInvalidUrl) {
  DownloadProtectionService::DownloadInfo info;
  EXPECT_TRUE(download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this))));
  // Only http is supported for now.
  info.download_url_chain.push_back(GURL("https://www.google.com/"));
  EXPECT_TRUE(download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this))));
  info.download_url_chain[0] = GURL("ftp://www.google.com/");
  EXPECT_TRUE(download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this))));
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadWhitelistedUrl) {
  DownloadProtectionService::DownloadInfo info;
  info.download_url_chain.push_back(GURL("http://www.evil.com/bla.exe"));
  info.download_url_chain.push_back(GURL("http://www.google.com/a.exe"));
  info.referrer_url = GURL("http://www.google.com/");

  EXPECT_CALL(*sb_service_, MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*sb_service_,
              MatchDownloadWhitelistUrl(GURL("http://www.google.com/a.exe")))
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this))));
  msg_loop_.Run();
  EXPECT_EQ(DownloadProtectionService::SAFE, result_);

  // Check that the referrer is matched against the whitelist.
  info.download_url_chain.pop_back();
  info.referrer_url = GURL("http://www.google.com/a.exe");
  EXPECT_FALSE(download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this))));
  msg_loop_.Run();
  EXPECT_EQ(DownloadProtectionService::SAFE, result_);
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadFetchFailed) {
  FakeURLFetcherFactory factory;
  // HTTP request will fail.
  factory.SetFakeResponse(
      DownloadProtectionService::kDownloadRequestUrl, "", false);

  EXPECT_CALL(*sb_service_, MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));

  DownloadProtectionService::DownloadInfo info;
  info.download_url_chain.push_back(GURL("http://www.evil.com/a.exe"));
  info.referrer_url = GURL("http://www.google.com/");
  EXPECT_FALSE(download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this))));
  msg_loop_.Run();
  EXPECT_EQ(DownloadProtectionService::SAFE, result_);
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadSuccess) {
  FakeURLFetcherFactory factory;
  // Empty response means SAFE.
  factory.SetFakeResponse(
      DownloadProtectionService::kDownloadRequestUrl, "", true);

  EXPECT_CALL(*sb_service_, MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));

  DownloadProtectionService::DownloadInfo info;
  info.download_url_chain.push_back(GURL("http://www.evil.com/a.exe"));
  info.referrer_url = GURL("http://www.google.com/");
  EXPECT_FALSE(download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this))));
  msg_loop_.Run();
  EXPECT_EQ(DownloadProtectionService::SAFE, result_);

  // Invalid response should be safe too.
  factory.SetFakeResponse(
      DownloadProtectionService::kDownloadRequestUrl, "bla", true);

  EXPECT_FALSE(download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this))));
  msg_loop_.Run();
  EXPECT_EQ(DownloadProtectionService::SAFE, result_);
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadValidateRequest) {
  TestURLFetcherFactory factory;

  DownloadProtectionService::DownloadInfo info;
  info.download_url_chain.push_back(GURL("http://www.google.com/"));
  info.download_url_chain.push_back(GURL("http://www.google.com/bla.exe"));
  info.referrer_url = GURL("http://www.google.com/");
  info.sha256_hash = "hash";
  info.total_bytes = 100;
  info.user_initiated = false;

  EXPECT_CALL(*sb_service_, MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(download_service_->CheckClientDownload(
      info,
      base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                 base::Unretained(this))));
  msg_loop_.RunAllPending();  // Wait until StartCheckClientDownload is called.

  ASSERT_TRUE(factory.GetFetcherByID(0));
  ClientDownloadRequest request;
  EXPECT_TRUE(request.ParseFromString(
      factory.GetFetcherByID(0)->upload_data()));
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
}
}  // namespace safe_browsing
