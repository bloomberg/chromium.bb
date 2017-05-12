// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_fetcher_win.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace {

class ChromeCleanerFetcherTest : public ::testing::Test {
 public:
  void SetUp() override {
    FetchChromeCleaner(base::Bind(&ChromeCleanerFetcherTest::FetchedCallback,
                                  base::Unretained(this)));

    // Get the TestURLFetcher instance used by FetchChromeCleaner.
    fetcher_ = factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher_);
  }

  void FetchedCallback(base::FilePath downloaded_path, int http_response_code) {
    callback_called_ = true;
    downloaded_path_ = downloaded_path;
    http_response_code_ = http_response_code;
  }

 protected:
  // TestURLFetcher requires a MessageLoop and an IO thread to release
  // URLRequestContextGetter in URLFetcher::Core.
  content::TestBrowserThreadBundle thread_bundle_;
  // TestURLFetcherFactory automatically sets itself as URLFetcher's factory
  net::TestURLFetcherFactory factory_;

  // The TestURLFetcher instance used by the FetchChromeCleaner.
  net::TestURLFetcher* fetcher_ = nullptr;
  // Variables set by FetchedCallback().
  bool callback_called_ = false;
  base::FilePath downloaded_path_;
  int http_response_code_ = -1;
};

TEST_F(ChromeCleanerFetcherTest, FetchSuccess) {
  EXPECT_EQ(GURL(fetcher_->GetOriginalURL()), GURL(GetSRTDownloadURL()));

  // Set up the fetcher to return success.
  fetcher_->set_status(net::URLRequestStatus{});
  fetcher_->set_response_code(net::HTTP_OK);
  // We need to save the file path where the response will be saved for later
  // because ChromeCleanerFetcher takes ownership of the file after
  // OnURLFetchComplete() has been called and we can't call
  // GetResponseAsFilePath() after that..
  base::FilePath response_file;
  fetcher_->GetResponseAsFilePath(/*take_ownership=*/false, &response_file);

  // After this call, the ChromeCleanerFetcher instance will delete itself.
  fetcher_->delegate()->OnURLFetchComplete(fetcher_);

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(downloaded_path_, response_file);
  EXPECT_EQ(http_response_code_, net::HTTP_OK);
}

TEST_F(ChromeCleanerFetcherTest, FetchFailure) {
  // Set up the fetcher to return failure.
  fetcher_->set_status(net::URLRequestStatus::FromError(net::ERR_FAILED));
  fetcher_->set_response_code(net::HTTP_NOT_FOUND);

  // After this call, the ChromeCleanerFetcher instance will delete itself.
  fetcher_->delegate()->OnURLFetchComplete(fetcher_);

  EXPECT_TRUE(callback_called_);
  EXPECT_TRUE(downloaded_path_.empty());
  EXPECT_EQ(http_response_code_, net::HTTP_NOT_FOUND);
}

}  // namespace
}  // namespace safe_browsing
