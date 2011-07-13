// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/download/download_safe_browsing_client.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char* kUrl1 = "http://127.0.0.1/";
const char* kUrl2 = "http://127.0.0.1/two";
const char* kUrl3 = "http://127.0.0.1/three";
const char* kRefUrl = "http://127.0.0.1/referrer";

class MockSafeBrowsingService : public SafeBrowsingService {
 public:
  MockSafeBrowsingService() {}
  virtual ~MockSafeBrowsingService() {}

  MOCK_METHOD6(ReportSafeBrowsingHit,
               void(const GURL&, const GURL&, const GURL&, bool, UrlCheckResult,
                    const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingService);
};

}  // namespace

class DownloadSBClientTest : public testing::Test {
 public:
  DownloadSBClientTest() :
      ui_thread_(BrowserThread::UI, &loop_) {
  }

 private:
  MessageLoop loop_;

  // UI thread to satisfy debug checks in DownloadSBClient.
  BrowserThread ui_thread_;
};

TEST_F(DownloadSBClientTest, UrlHit) {
  std::string expected_post =
      std::string(kUrl1) + "\n" + kUrl2 + "\n" + kUrl3 + "\n";
  scoped_refptr<MockSafeBrowsingService> sb_service(
      new MockSafeBrowsingService);
  EXPECT_CALL(*sb_service.get(),
      ReportSafeBrowsingHit(GURL(kUrl3), GURL(kUrl1), GURL(kRefUrl), true,
                            SafeBrowsingService::BINARY_MALWARE_URL,
                            expected_post));
  std::vector<GURL> url_chain;
  url_chain.push_back(GURL(kUrl1));
  url_chain.push_back(GURL(kUrl2));
  url_chain.push_back(GURL(kUrl3));

  scoped_refptr<DownloadSBClient> client(
      new DownloadSBClient(1, url_chain, GURL(kRefUrl), true));
  client->SetSBService(sb_service.get());

  client->ReportMalware(SafeBrowsingService::BINARY_MALWARE_URL, "");
}

TEST_F(DownloadSBClientTest, DigestHit) {
  std::string hash_data = "\xDE\xAD\xBE\xEF";
  std::string hash_string = "DEADBEEF";
  std::string expected_post =
      hash_string + "\n" + kUrl1 + "\n" + kUrl2 + "\n" + kUrl3 + "\n";
  scoped_refptr<MockSafeBrowsingService> sb_service(
      new MockSafeBrowsingService);
  EXPECT_CALL(*sb_service.get(),
      ReportSafeBrowsingHit(GURL(kUrl3), GURL(kUrl1), GURL(kRefUrl), true,
                            SafeBrowsingService::BINARY_MALWARE_HASH,
                            expected_post));
  std::vector<GURL> url_chain;
  url_chain.push_back(GURL(kUrl1));
  url_chain.push_back(GURL(kUrl2));
  url_chain.push_back(GURL(kUrl3));

  scoped_refptr<DownloadSBClient> client(
      new DownloadSBClient(1, url_chain, GURL(kRefUrl), true));
  client->SetSBService(sb_service.get());

  client->ReportMalware(SafeBrowsingService::BINARY_MALWARE_HASH, hash_data);
}
