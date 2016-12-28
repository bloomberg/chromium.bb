// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/crw_pass_kit_downloader.h"

#import <UIKit/UIKit.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "ios/web/public/test/web_test.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#import "testing/gtest_mac.h"
#include "url/gurl.h"

using net::HttpResponseHeaders;
using net::URLRequestStatus;

namespace web {

// Constant URL to use in tests.
const char kTestUrlString[] = "http://www.example.com";

const char kPassKitMimeType[] = "application/vnd.apple.pkpass";

// Constant string which is the expected response from the PassKit Downloader.
const char kExpectedString[] = "test string";

// Test fixture for testing CRWPassKitDownloader.
class CRWPassKitDownloaderTest : public WebTest {
 protected:
  void SetUp() override {
    WebTest::SetUp();
    completion_handler_success_ = false;
    fetcher_factory_.reset(new net::TestURLFetcherFactory());
    downloader_.reset([[CRWPassKitDownloader alloc]
        initWithContextGetter:GetBrowserState()->GetRequestContext()
            completionHandler:^(NSData* data) {
              NSData* expected_data =
                  [NSData dataWithBytes:kExpectedString
                                 length:strlen(kExpectedString)];
              completion_handler_success_ = [data isEqualToData:expected_data];
            }]);
  }

  // Sets up |fetcher|'s request status, HTTP response code, HTTP headers, and
  // MIME type.
  void SetUpFetcher(net::TestURLFetcher* fetcher,
                    URLRequestStatus status,
                    int response_code,
                    const std::string& mime_type) {
    fetcher->set_status(status);
    fetcher->set_response_code(response_code);
    scoped_refptr<HttpResponseHeaders> headers;
    headers = new HttpResponseHeaders("HTTP/1.x 200 OK\0");
    headers->AddHeader("Content-Type: " + mime_type);
    fetcher->set_response_headers(headers);
  }

  // Test fetcher factory from which we access and control the URLFetcher
  // used in CRWPassKitDownloader.
  std::unique_ptr<net::TestURLFetcherFactory> fetcher_factory_;

  // The CRWPassKitDownloader that is being tested.
  base::scoped_nsobject<CRWPassKitDownloader> downloader_;

  // Indicates whether or not the downloader successfully downloaded data. It is
  // set from the completion handler based on whether actual data is equal to
  // expected data.
  bool completion_handler_success_;
};

// Tests case where CRWPassKitDownloader successfully downloads data.
TEST_F(CRWPassKitDownloaderTest, TestDownloadPassKitSuccess) {
  GURL test_url(kTestUrlString);
  [downloader_ downloadPassKitFileWithURL:test_url];

  net::TestURLFetcher* fetcher = fetcher_factory_->GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  ASSERT_EQ(test_url, fetcher->GetOriginalURL());
  SetUpFetcher(fetcher, URLRequestStatus(), 200, kPassKitMimeType);
  fetcher->SetResponseString(kExpectedString);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  EXPECT_TRUE(completion_handler_success_);
}

// Tests case where CRWPassKitDownloader fails download due to a bad HTTP
// response code. |completion_handler_success_| should be false.
TEST_F(CRWPassKitDownloaderTest, TestDownloadPassKitBadErrorCodeFailure) {
  GURL test_url(kTestUrlString);
  [downloader_ downloadPassKitFileWithURL:test_url];

  net::TestURLFetcher* fetcher = fetcher_factory_->GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  ASSERT_EQ(test_url, fetcher->GetOriginalURL());
  SetUpFetcher(fetcher, URLRequestStatus(), 403, kPassKitMimeType);
  fetcher->SetResponseString(kExpectedString);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  EXPECT_FALSE(completion_handler_success_);
}

// Tests case where CRWPassKitDownloader fails download with a status set to
// failure. |completion_handler_success_| should be false.
TEST_F(CRWPassKitDownloaderTest, TestDownloadPassKitStatusFailedFailure) {
  GURL test_url(kTestUrlString);
  [downloader_ downloadPassKitFileWithURL:test_url];

  net::TestURLFetcher* fetcher = fetcher_factory_->GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  ASSERT_EQ(test_url, fetcher->GetOriginalURL());
  SetUpFetcher(fetcher, URLRequestStatus::FromError(net::ERR_ACCESS_DENIED),
               200, kPassKitMimeType);
  fetcher->SetResponseString(kExpectedString);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  EXPECT_FALSE(completion_handler_success_);
}

// Tests case where CRWPassKitDownloader fails download with no response data.
// |completion_handler_success_| should be false.
TEST_F(CRWPassKitDownloaderTest, TestDownloadPassKitNoResponseFailure) {
  GURL test_url(kTestUrlString);
  [downloader_ downloadPassKitFileWithURL:test_url];

  net::TestURLFetcher* fetcher = fetcher_factory_->GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  ASSERT_EQ(test_url, fetcher->GetOriginalURL());
  SetUpFetcher(fetcher, URLRequestStatus(), 200, kPassKitMimeType);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  EXPECT_FALSE(completion_handler_success_);
}

}  // namespace
