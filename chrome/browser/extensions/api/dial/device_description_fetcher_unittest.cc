// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/dial/device_description_fetcher.h"
#include "chrome/browser/extensions/api/dial/dial_device_data.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {
namespace api {
namespace dial {

class DeviceDescriptionFetcherTest : public testing::Test {
 public:
  DeviceDescriptionFetcherTest() : url_("http://127.0.0.1/description.xml") {}

  void TearDown() override {
    EXPECT_FALSE(error_cb_);
    EXPECT_FALSE(success_cb_);
  }

  void ExpectSuccess(const GURL& expected_app_url,
                     const std::string& expected_description) {
    success_cb_ = base::BindOnce(&DeviceDescriptionFetcherTest::OnSuccess,
                                 base::Unretained(this), expected_app_url,
                                 expected_description);
  }

  void ExpectError(const std::string& expected_message) {
    error_cb_ = base::BindOnce(&DeviceDescriptionFetcherTest::OnError,
                               base::Unretained(this), expected_message);
  }

  net::TestURLFetcher* StartRequest() {
    fetcher_ = base::MakeUnique<DeviceDescriptionFetcher>(
        url_, &profile_, std::move(success_cb_), std::move(error_cb_));
    fetcher_->Start();
    return factory_.GetFetcherByID(
        DeviceDescriptionFetcher::kURLFetcherIDForTest);
  }

 protected:
  const content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  const net::TestURLFetcherFactory factory_;
  const GURL url_;
  base::OnceCallback<void(const DialDeviceDescriptionData&)> success_cb_;
  base::OnceCallback<void(const std::string&)> error_cb_;
  std::unique_ptr<DeviceDescriptionFetcher> fetcher_;

 private:
  void OnSuccess(const GURL& expected_app_url,
                 const std::string& expected_description,
                 const DialDeviceDescriptionData& description) {
    EXPECT_EQ(expected_app_url, description.app_url);
    EXPECT_EQ(expected_description, description.device_description);
  }

  void OnError(const std::string& expected_message,
               const std::string& message) {
    EXPECT_TRUE(message.find(expected_message) == 0);
  }

  DISALLOW_COPY_AND_ASSIGN(DeviceDescriptionFetcherTest);
};

TEST_F(DeviceDescriptionFetcherTest, FetchSuccessful) {
  ExpectSuccess(GURL("http://127.0.0.1/apps"), "<xml>description</xml>");
  net::TestURLFetcher* test_fetcher = StartRequest();

  test_fetcher->set_response_code(net::HTTP_OK);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders("");
  headers->AddHeader("Application-URL: http://127.0.0.1/apps");
  test_fetcher->set_response_headers(headers);
  test_fetcher->SetResponseString("<xml>description</xml>");
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnMissingDescription) {
  ExpectError("HTTP 404:");
  net::TestURLFetcher* test_fetcher = StartRequest();

  test_fetcher->set_response_code(net::HTTP_NOT_FOUND);
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnMissingAppUrl) {
  ExpectError("Missing or empty Application-URL:");
  net::TestURLFetcher* test_fetcher = StartRequest();

  test_fetcher->set_response_code(net::HTTP_OK);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders("");
  test_fetcher->set_response_headers(headers);
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnEmptyAppUrl) {
  ExpectError("Missing or empty Application-URL:");
  net::TestURLFetcher* test_fetcher = StartRequest();

  test_fetcher->set_response_code(net::HTTP_OK);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders("");
  headers->AddHeader("Application-URL:");
  test_fetcher->set_response_headers(headers);
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnInvalidAppUrl) {
  ExpectError("Invalid Application-URL:");
  net::TestURLFetcher* test_fetcher = StartRequest();

  test_fetcher->set_response_code(net::HTTP_OK);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders("");
  headers->AddHeader("Application-URL: http://www.example.com");
  test_fetcher->set_response_headers(headers);
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnEmptyDescription) {
  ExpectError("Missing or empty response");
  net::TestURLFetcher* test_fetcher = StartRequest();

  test_fetcher->set_response_code(net::HTTP_OK);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders("");
  headers->AddHeader("Application-URL: http://127.0.0.1/apps");
  test_fetcher->set_response_headers(headers);
  test_fetcher->SetResponseString("");
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnBadDescription) {
  ExpectError("Invalid response encoding");
  net::TestURLFetcher* test_fetcher = StartRequest();

  test_fetcher->set_response_code(net::HTTP_OK);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders("");
  headers->AddHeader("Application-URL: http://127.0.0.1/apps");
  test_fetcher->set_response_headers(headers);
  test_fetcher->SetResponseString("\xfc\x9c\xbf\x80\xbf\x80");
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

TEST_F(DeviceDescriptionFetcherTest, FetchFailsOnResponseTooLarge) {
  ExpectError("Response too large");
  net::TestURLFetcher* test_fetcher = StartRequest();

  test_fetcher->set_response_code(net::HTTP_OK);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders("");
  headers->AddHeader("Application-URL: http://127.0.0.1/apps");
  test_fetcher->set_response_headers(headers);
  test_fetcher->SetResponseString(std::string(262145, 'd'));
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
}

}  // namespace dial
}  // namespace api
}  // namespace extensions
