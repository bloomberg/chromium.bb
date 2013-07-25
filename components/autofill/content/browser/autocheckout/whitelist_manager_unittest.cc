// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "components/autofill/content/browser/autocheckout/whitelist_manager.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const int64 kTestDownloadInterval = 3;  // 3 seconds

// First 4 retry delays are 3, 18, 108, 648 seconds, and following retries
// capped at one hour.
const int64 kBackoffDelaysInMs[] = {
    3000, 18000, 108000, 648000, 3600000, 3600000 };

const char kDownloadWhitelistResponse[] =
    "https://www.merchant1.com/checkout/\n"
    "https://cart.merchant2.com/";

}  // namespace

namespace autofill {
namespace autocheckout {

class WhitelistManagerTest;

class MockAutofillMetrics : public AutofillMetrics {
 public:
  MockAutofillMetrics() {}
  MOCK_CONST_METHOD2(LogAutocheckoutWhitelistDownloadDuration,
      void(const base::TimeDelta& duration,
           AutofillMetrics::AutocheckoutWhitelistDownloadStatus));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillMetrics);
};

class TestWhitelistManager : public WhitelistManager {
 public:
  TestWhitelistManager()
      : WhitelistManager(),
        did_start_download_timer_(false) {}

  virtual void ScheduleDownload(base::TimeDelta interval) OVERRIDE {
    did_start_download_timer_ = false;
    download_interval_ = interval;
    return WhitelistManager::ScheduleDownload(interval);
  }

  virtual void StartDownloadTimer(base::TimeDelta interval) OVERRIDE {
    WhitelistManager::StartDownloadTimer(interval);
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

  const base::TimeDelta& download_interval() const {
    return download_interval_;
  }

  const std::vector<std::string>& url_prefixes() const {
    return WhitelistManager::url_prefixes();
  }

  virtual const AutofillMetrics& GetMetricLogger() const OVERRIDE {
      return mock_metrics_logger_;
  }

 private:
  bool did_start_download_timer_;
  base::TimeDelta download_interval_;

  MockAutofillMetrics mock_metrics_logger_;

  DISALLOW_COPY_AND_ASSIGN(TestWhitelistManager);
};

class WhitelistManagerTest : public testing::Test {
 public:
  WhitelistManagerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

 protected:
  void CreateWhitelistManager() {
    if (!whitelist_manager_.get()) {
      whitelist_manager_.reset(new TestWhitelistManager());
    }
  }

  void DownloadWhitelist(int response_code, const std::string& response) {
    // Create and register factory.
    net::TestURLFetcherFactory factory;

    CreateWhitelistManager();

    AutofillMetrics::AutocheckoutWhitelistDownloadStatus status;
    if (response_code == net::HTTP_OK)
      status = AutofillMetrics::AUTOCHECKOUT_WHITELIST_DOWNLOAD_SUCCEEDED;
    else
      status = AutofillMetrics::AUTOCHECKOUT_WHITELIST_DOWNLOAD_FAILED;
    EXPECT_CALL(
        static_cast<const MockAutofillMetrics&>(
            whitelist_manager_->GetMetricLogger()),
        LogAutocheckoutWhitelistDownloadDuration(testing::_, status)).Times(1);

    whitelist_manager_->TriggerDownload();
    net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

 protected:
  scoped_ptr<TestWhitelistManager> whitelist_manager_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(WhitelistManagerTest, DownloadWhitelist) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalFormFilling);
  DownloadWhitelist(net::HTTP_OK, kDownloadWhitelistResponse);
  ASSERT_EQ(2U, whitelist_manager_->url_prefixes().size());
  EXPECT_EQ("https://www.merchant1.com/checkout/",
            whitelist_manager_->url_prefixes()[0]);
  EXPECT_EQ("https://cart.merchant2.com/",
            whitelist_manager_->url_prefixes()[1]);
}

TEST_F(WhitelistManagerTest, DoNotDownloadWhitelistWhenSwitchIsOff) {
  CreateWhitelistManager();
  whitelist_manager_->ScheduleDownload(
      base::TimeDelta::FromSeconds(kTestDownloadInterval));
  EXPECT_FALSE(whitelist_manager_->did_start_download_timer());
}

TEST_F(WhitelistManagerTest, DoNotDownloadWhitelistIfAlreadyScheduled) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalFormFilling);
  CreateWhitelistManager();
  // First attempt should schedule a download.
  whitelist_manager_->ScheduleDownload(
      base::TimeDelta::FromSeconds(kTestDownloadInterval));
  EXPECT_TRUE(whitelist_manager_->did_start_download_timer());
  // Second attempt should NOT schedule a download while there is already one.
  whitelist_manager_->ScheduleDownload(
      base::TimeDelta::FromSeconds(kTestDownloadInterval));
  EXPECT_FALSE(whitelist_manager_->did_start_download_timer());
  // It should schedule a new download when not in backoff mode.
  whitelist_manager_->StopDownloadTimer();
  whitelist_manager_->ScheduleDownload(
      base::TimeDelta::FromSeconds(kTestDownloadInterval));
  EXPECT_TRUE(whitelist_manager_->did_start_download_timer());
}

TEST_F(WhitelistManagerTest, DownloadWhitelistFailed) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalFormFilling);
  DownloadWhitelist(net::HTTP_INTERNAL_SERVER_ERROR,
                    kDownloadWhitelistResponse);
  EXPECT_EQ(0U, whitelist_manager_->url_prefixes().size());

  whitelist_manager_->StopDownloadTimer();
  DownloadWhitelist(net::HTTP_OK, kDownloadWhitelistResponse);
  EXPECT_EQ(2U, whitelist_manager_->url_prefixes().size());

  whitelist_manager_->StopDownloadTimer();
  DownloadWhitelist(net::HTTP_INTERNAL_SERVER_ERROR,
                    kDownloadWhitelistResponse);
  EXPECT_EQ(2U, whitelist_manager_->url_prefixes().size());
}

TEST_F(WhitelistManagerTest, DownloadWhitelistRetry) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalFormFilling);

  for (size_t i = 0; i < arraysize(kBackoffDelaysInMs); ++i) {
    DownloadWhitelist(net::HTTP_INTERNAL_SERVER_ERROR,
                      kDownloadWhitelistResponse);
    SCOPED_TRACE(base::StringPrintf("Testing retry %" PRIuS
                                    ", expecting delay: %" PRId64,
                                    i,
                                    kBackoffDelaysInMs[i]));
    EXPECT_EQ(
        kBackoffDelaysInMs[i],
        whitelist_manager_->download_interval().InMillisecondsRoundedUp());
  }
}

TEST_F(WhitelistManagerTest, GetMatchedURLPrefix) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalFormFilling);
  DownloadWhitelist(net::HTTP_OK, kDownloadWhitelistResponse);
  EXPECT_EQ(2U, whitelist_manager_->url_prefixes().size());

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

TEST_F(WhitelistManagerTest, BypassWhitelist) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalFormFilling);
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kBypassAutocheckoutWhitelist);
  DownloadWhitelist(net::HTTP_OK, kDownloadWhitelistResponse);
  EXPECT_EQ(2U, whitelist_manager_->url_prefixes().size());

  // Empty url.
  EXPECT_EQ(std::string(),
            whitelist_manager_->GetMatchedURLPrefix(GURL(std::string())));
  // Positive tests.
  EXPECT_EQ("https://www.merchant1.com/checkout/",
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://www.merchant1.com/checkout/")));
  EXPECT_EQ("https://cart.merchant2.com/",
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://cart.merchant2.com/ShippingInfo?a=b&c=d")));
  // Bypass other urls.
  EXPECT_EQ(std::string("https://bypass.me/"),
            whitelist_manager_->GetMatchedURLPrefix(
                GURL("https://bypass.me/")));
}

}  // namespace autocheckout
}  // namespace autofill

