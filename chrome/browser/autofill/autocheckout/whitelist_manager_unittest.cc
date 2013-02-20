// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/autocheckout/whitelist_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const size_t kTestDownloadInterval = 3;  // 3 seconds

const char kDownloadWhitelistResponse[] =
    "https://www.merchant1.com/checkout/\n"
    "https://cart.merchant2.com/";

}  // namespace

namespace autofill {
namespace autocheckout {

class WhitelistManagerTest;

class TestWhitelistManager : public WhitelistManager {
 public:
  explicit TestWhitelistManager(net::URLRequestContextGetter* context_getter)
      : WhitelistManager(context_getter),
        did_start_download_timer_(false) {}

  virtual void ScheduleDownload(size_t interval_seconds) OVERRIDE {
    did_start_download_timer_ = false;
    return WhitelistManager::ScheduleDownload(interval_seconds);
  }

  virtual void StartDownloadTimer(size_t interval_seconds) OVERRIDE {
    WhitelistManager::StartDownloadTimer(interval_seconds);
    did_start_download_timer_ = true;
  }

  bool did_start_download_timer() const {
    return did_start_download_timer_;
  }

  void TriggerDownload() {
    WhitelistManager::TriggerDownload();
  }

  void StopDownloadTimer() {
    WhitelistManager::StopDownloadTimer();
  }

  const std::vector<std::string>& url_prefixes() const {
    return WhitelistManager::url_prefixes();
  }

 private:
  bool did_start_download_timer_;

  DISALLOW_COPY_AND_ASSIGN(TestWhitelistManager);
};

class WhitelistManagerTest : public testing::Test {
 public:
  WhitelistManagerTest() : io_thread_(content::BrowserThread::IO) {}

  virtual void SetUp() {
    io_thread_.StartIOThread();
    profile_.CreateRequestContext();
  }

  virtual void TearDown() {
    profile_.ResetRequestContext();
    io_thread_.Stop();
  }

 protected:
  void CreateWhitelistManager() {
    if (!whitelist_manager_.get()) {
      whitelist_manager_.reset(new TestWhitelistManager(
          profile_.GetRequestContext()));
    }
  }

  void DownloadWhitelist(int response_code, const std::string& response) {
    // Create and register factory.
    net::TestURLFetcherFactory factory;

    CreateWhitelistManager();

    whitelist_manager_->TriggerDownload();
    net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void ResetBackOff() {
    whitelist_manager_->StopDownloadTimer();
  }

  const std::vector<std::string>& get_url_prefixes() const {
    return whitelist_manager_->url_prefixes();
  }

 protected:
  TestingProfile profile_;
  scoped_ptr<TestWhitelistManager> whitelist_manager_;

 private:
  MessageLoopForIO message_loop_;
  // The profile's request context must be released on the IO thread.
  content::TestBrowserThread io_thread_;
};

TEST_F(WhitelistManagerTest, DownloadWhitelist) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalFormFilling);
  DownloadWhitelist(net::HTTP_OK, kDownloadWhitelistResponse);
  ASSERT_EQ(2U, get_url_prefixes().size());
  EXPECT_EQ("https://www.merchant1.com/checkout/",
            get_url_prefixes()[0]);
  EXPECT_EQ("https://cart.merchant2.com/",
            get_url_prefixes()[1]);
}

TEST_F(WhitelistManagerTest, DoNotDownloadWhitelistWhenSwitchIsOff) {
  CreateWhitelistManager();
  whitelist_manager_->ScheduleDownload(kTestDownloadInterval);
  EXPECT_FALSE(whitelist_manager_->did_start_download_timer());
}

TEST_F(WhitelistManagerTest, DoNotDownloadWhitelistWhenBackOff) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalFormFilling);
  CreateWhitelistManager();
  // First attempt should schedule a download.
  whitelist_manager_->ScheduleDownload(kTestDownloadInterval);
  EXPECT_TRUE(whitelist_manager_->did_start_download_timer());
  // Second attempt should NOT schedule a download while there is already one.
  whitelist_manager_->ScheduleDownload(kTestDownloadInterval);
  EXPECT_FALSE(whitelist_manager_->did_start_download_timer());
  // It should schedule a new download when not in backoff mode.
  ResetBackOff();
  whitelist_manager_->ScheduleDownload(kTestDownloadInterval);
  EXPECT_TRUE(whitelist_manager_->did_start_download_timer());
}

TEST_F(WhitelistManagerTest, DownloadWhitelistFailed) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalFormFilling);
  DownloadWhitelist(net::HTTP_INTERNAL_SERVER_ERROR,
                    kDownloadWhitelistResponse);
  EXPECT_EQ(0U, get_url_prefixes().size());

  ResetBackOff();
  DownloadWhitelist(net::HTTP_OK, kDownloadWhitelistResponse);
  EXPECT_EQ(2U, get_url_prefixes().size());

  ResetBackOff();
  DownloadWhitelist(net::HTTP_INTERNAL_SERVER_ERROR,
                    kDownloadWhitelistResponse);
  EXPECT_EQ(2U, get_url_prefixes().size());
}

TEST_F(WhitelistManagerTest, GetMatchedURLPrefix) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalFormFilling);
  DownloadWhitelist(net::HTTP_OK, kDownloadWhitelistResponse);
  EXPECT_EQ(2U, get_url_prefixes().size());

  // Empty url.
  EXPECT_EQ(std::string(),
            whitelist_manager_->GetMatchedURLPrefix(GURL(std::string())));
  EXPECT_EQ(std::string(),
            whitelist_manager_->GetMatchedURLPrefix(GURL()));

  // Positive tests.
  EXPECT_EQ("https://www.merchant1.com/checkout/",
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://www.merchant1.com/checkout/")));
  EXPECT_EQ("https://www.merchant1.com/checkout/",
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://www.merchant1.com/checkout/Shipping")));
  EXPECT_EQ("https://www.merchant1.com/checkout/",
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://www.merchant1.com/checkout/?a=b&c=d")));
  EXPECT_EQ("https://cart.merchant2.com/",
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://cart.merchant2.com/")));
  EXPECT_EQ("https://cart.merchant2.com/",
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://cart.merchant2.com/ShippingInfo")));
  EXPECT_EQ("https://cart.merchant2.com/",
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://cart.merchant2.com/ShippingInfo?a=b&c=d")));

  // Negative tests.
  EXPECT_EQ(std::string(),
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://www.merchant1.com/checkout")));
  EXPECT_EQ(std::string(),
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://www.merchant1.com/")));
  EXPECT_EQ(std::string(),
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://www.merchant1.com/Building")));
  EXPECT_EQ(std::string(),
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://www.merchant2.com/cart")));
  EXPECT_EQ(std::string(),
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("a random string")));

  // Test different cases in schema, host and path.
  EXPECT_EQ(std::string(),
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("http://www.merchant1.com/checkout/")));
  EXPECT_EQ(std::string(),
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("www.merchant1.com/checkout/")));
  EXPECT_EQ("https://www.merchant1.com/checkout/",
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://www.Merchant1.com/checkout/")));
  EXPECT_EQ(std::string(),
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://www.merchant1.com/CheckOut/")));
}

}  // namespace autocheckout
}  // namespace autofill

