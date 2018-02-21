// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/media/router/discovery/dial/dial_app_info_fetcher.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media_router {

class TestDialAppInfoFetcher : public DialAppInfoFetcher {
 public:
  TestDialAppInfoFetcher(
      const GURL& app_url,
      base::OnceCallback<void(const std::string&)> success_cb,
      base::OnceCallback<void(int, const std::string&)> error_cb,
      network::TestURLLoaderFactory* factory)
      : DialAppInfoFetcher(app_url, std::move(success_cb), std::move(error_cb)),
        factory_(factory) {}
  ~TestDialAppInfoFetcher() override = default;

  void StartDownload() override {
    loader_->DownloadToString(
        factory_,
        base::BindOnce(&DialAppInfoFetcher::ProcessResponse,
                       base::Unretained(this)),
        256 * 1024);
  }

 private:
  network::TestURLLoaderFactory* const factory_;
};

class DialAppInfoFetcherTest : public testing::Test {
 public:
  DialAppInfoFetcherTest() : url_("http://127.0.0.1/app/Youtube") {}

  void ExpectSuccess(const std::string& expected_app_info) {
    EXPECT_CALL(*this, OnSuccess(expected_app_info));
  }

  void ExpectError(const std::string& expected_error) {
    expected_error_ = expected_error;
    EXPECT_CALL(*this, DoOnError());
  }

  void StartRequest() {
    fetcher_ = std::make_unique<TestDialAppInfoFetcher>(
        url_,
        base::BindOnce(&DialAppInfoFetcherTest::OnSuccess,
                       base::Unretained(this)),
        base::BindOnce(&DialAppInfoFetcherTest::OnError,
                       base::Unretained(this)),
        &loader_factory_);
    fetcher_->Start();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::ScopedTaskEnvironment environment_;
  network::TestURLLoaderFactory loader_factory_;
  const GURL url_;
  std::string expected_error_;
  std::unique_ptr<TestDialAppInfoFetcher> fetcher_;

 private:
  MOCK_METHOD1(OnSuccess, void(const std::string&));
  MOCK_METHOD0(DoOnError, void());

  void OnError(int response_code, const std::string& message) {
    EXPECT_TRUE(message.find(expected_error_) == 0);
    DoOnError();
  }

  DISALLOW_COPY_AND_ASSIGN(DialAppInfoFetcherTest);
};

TEST_F(DialAppInfoFetcherTest, FetchSuccessful) {
  std::string body("<xml>appInfo</xml>");
  ExpectSuccess(body);
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  loader_factory_.AddResponse(url_, network::ResourceResponseHead(), body,
                              status);
  StartRequest();
}

TEST_F(DialAppInfoFetcherTest, FetchFailsOnMissingAppInfo) {
  ExpectError("HTTP 404:");

  loader_factory_.AddResponse(
      url_, network::ResourceResponseHead(), "",
      network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  StartRequest();
}

TEST_F(DialAppInfoFetcherTest, FetchFailsOnEmptyAppInfo) {
  ExpectError("Missing or empty response");

  loader_factory_.AddResponse(url_, network::ResourceResponseHead(), "",
                              network::URLLoaderCompletionStatus());
  StartRequest();
}

TEST_F(DialAppInfoFetcherTest, FetchFailsOnBadAppInfo) {
  ExpectError("Invalid response encoding");
  std::string body("\xfc\x9c\xbf\x80\xbf\x80");
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = body.size();
  loader_factory_.AddResponse(url_, network::ResourceResponseHead(), body,
                              status);
  StartRequest();
}

}  // namespace media_router
