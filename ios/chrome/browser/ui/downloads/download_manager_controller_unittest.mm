// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/downloads/download_manager_controller.h"

#import <UIKit/UIKit.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#import "ios/chrome/browser/storekit_launcher.h"
#include "ios/web/public/test/test_web_thread.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

using net::HttpResponseHeaders;
using net::URLRequestStatus;

@interface DownloadManagerController (ExposedForTesting)
- (UIView*)documentContainer;
- (UIView*)progressBar;
- (UIImageView*)documentIcon;
- (UIImageView*)foldIcon;
- (UILabel*)timeLeftLabel;
- (UILabel*)fileTypeLabel;
- (UILabel*)fileNameLabel;
- (UILabel*)errorOrSizeLabel;
- (UIImageView*)errorIcon;
- (UIView*)actionBar;
- (UIButton*)downloadButton;
- (UIButton*)cancelButton;
- (UIButton*)openInButton;
- (UIButton*)googleDriveButton;
- (long long)totalFileSize;
@end

@interface TestStoreKitLauncher : NSObject<StoreKitLauncher>
@end

@implementation TestStoreKitLauncher
- (void)openAppStore:(NSString*)appId {
}
@end

namespace {

const GURL kTestURL = GURL("http://www.example.com/test_download_file.txt");

class DownloadManagerControllerTest : public PlatformTest {
 public:
  DownloadManagerControllerTest()
      : _message_loop(base::MessageLoop::TYPE_UI),
        _ui_thread(web::WebThread::UI, &_message_loop) {}

 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    _request_context_getter =
        new net::TestURLRequestContextGetter(_message_loop.task_runner());

    _fetcher_factory.reset(new net::TestURLFetcherFactory());

    _store_kit_launcher.reset([[TestStoreKitLauncher alloc] init]);

    _controller.reset([[DownloadManagerController alloc]
                 initWithURL:kTestURL
        requestContextGetter:_request_context_getter.get()
            storeKitLauncher:_store_kit_launcher.get()]);
  }

  base::MessageLoop _message_loop;
  web::TestWebThread _ui_thread;
  base::scoped_nsobject<TestStoreKitLauncher> _store_kit_launcher;
  scoped_refptr<net::TestURLRequestContextGetter> _request_context_getter;
  std::unique_ptr<net::TestURLFetcherFactory> _fetcher_factory;
  base::scoped_nsobject<DownloadManagerController> _controller;
};

TEST_F(DownloadManagerControllerTest, TestXibViewConnections) {
  EXPECT_TRUE([_controller documentContainer]);
  EXPECT_TRUE([_controller progressBar]);
  EXPECT_TRUE([_controller documentIcon]);
  EXPECT_TRUE([_controller foldIcon]);
  EXPECT_TRUE([_controller timeLeftLabel]);
  EXPECT_TRUE([_controller fileTypeLabel]);
  EXPECT_TRUE([_controller fileNameLabel]);
  EXPECT_TRUE([_controller errorOrSizeLabel]);
  EXPECT_TRUE([_controller errorIcon]);
  EXPECT_TRUE([_controller actionBar]);
  EXPECT_TRUE([_controller downloadButton]);
  EXPECT_TRUE([_controller cancelButton]);
  EXPECT_TRUE([_controller openInButton]);
  EXPECT_TRUE([_controller googleDriveButton]);
}

TEST_F(DownloadManagerControllerTest, TestStart) {
  [_controller start];
  EXPECT_TRUE(
      [[UIApplication sharedApplication] isNetworkActivityIndicatorVisible]);
  net::TestURLFetcher* fetcher = _fetcher_factory->GetFetcherByID(0);
  EXPECT_TRUE(fetcher != NULL);
  EXPECT_EQ(kTestURL, fetcher->GetOriginalURL());
}

TEST_F(DownloadManagerControllerTest, TestOnHeadFetchCompleteSuccess) {
  [_controller start];
  net::TestURLFetcher* fetcher = _fetcher_factory->GetFetcherByID(0);

  URLRequestStatus success_status(URLRequestStatus::SUCCESS, 0);
  fetcher->set_status(success_status);
  fetcher->set_response_code(200);
  scoped_refptr<HttpResponseHeaders> headers;
  headers = new HttpResponseHeaders("HTTP/1.x 200 OK\0");
  headers->AddHeader("Content-Length: 1000000000");  // ~1GB
  fetcher->set_response_headers(headers);

  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_FALSE(
      [[UIApplication sharedApplication] isNetworkActivityIndicatorVisible]);

  EXPECT_EQ(1000000000, [_controller totalFileSize]);
  NSString* fileSizeText = [NSByteCountFormatter
      stringFromByteCount:1000000000
               countStyle:NSByteCountFormatterCountStyleFile];
  EXPECT_NSEQ(fileSizeText, [_controller errorOrSizeLabel].text);
  EXPECT_FALSE([_controller downloadButton].hidden);
}

TEST_F(DownloadManagerControllerTest, TestOnHeadFetchCompleteFailure) {
  [_controller start];
  net::TestURLFetcher* fetcher = _fetcher_factory->GetFetcherByID(0);

  URLRequestStatus failure_status(URLRequestStatus::FAILED,
                                  net::ERR_DNS_TIMED_OUT);
  fetcher->set_status(failure_status);

  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_FALSE(
      [[UIApplication sharedApplication] isNetworkActivityIndicatorVisible]);

  EXPECT_TRUE([_controller fileTypeLabel].hidden);
  EXPECT_FALSE([_controller downloadButton].hidden);
  EXPECT_FALSE([_controller errorIcon].hidden);
  EXPECT_FALSE([_controller errorOrSizeLabel].hidden);
}

}  // namespace
