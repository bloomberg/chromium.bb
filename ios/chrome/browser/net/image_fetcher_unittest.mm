// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/image_fetcher.h"

#import <UIKit/UIKit.h>

#include "base/ios/ios_util.h"
#include "base/mac/scoped_block.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

static unsigned char kJPGImage[] = {
    255,216,255,224,0,16,74,70,73,70,0,1,1,1,0,72,0,72,0,0,255,254,0,19,67,
    114,101,97,116,101,100,32,119,105,116,104,32,71,73,77,80,255,219,0,67,
    0,5,3,4,4,4,3,5,4,4,4,5,5,5,6,7,12,8,7,7,7,7,15,11,11,9,12,17,15,18,18,
    17,15,17,17,19,22,28,23,19,20,26,21,17,17,24,33,24,26,29,29,31,31,31,
    19,23,34,36,34,30,36,28,30,31,30,255,219,0,67,1,5,5,5,7,6,7,14,8,8,14,
    30,20,17,20,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,
    30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,
    30,30,30,30,30,30,30,30,255,192,0,17,8,0,1,0,1,3,1,34,0,2,17,1,3,17,1,
    255,196,0,21,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,255,196,0,20,16,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,196,0,20,1,1,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,255,196,0,20,17,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,218,
    0,12,3,1,0,2,17,3,17,0,63,0,178,192,7,255,217
};

static unsigned char kPNGImage[] = {
    137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,1,0,0,0,
    0,55,110,249,36,0,0,0,2,98,75,71,68,0,1,221,138,19,164,0,0,0,9,112,72,
    89,115,0,0,11,18,0,0,11,18,1,210,221,126,252,0,0,0,9,118,112,65,103,0,
    0,0,1,0,0,0,1,0,199,149,95,237,0,0,0,10,73,68,65,84,8,215,99,104,0,0,
    0,130,0,129,221,67,106,244,0,0,0,25,116,69,88,116,99,111,109,109,101,
    110,116,0,67,114,101,97,116,101,100,32,119,105,116,104,32,71,73,77,80,
    231,175,64,203,0,0,0,37,116,69,88,116,100,97,116,101,58,99,114,101,97,
    116,101,0,50,48,49,49,45,48,54,45,50,50,84,49,54,58,49,54,58,52,54,43,
    48,50,58,48,48,31,248,231,223,0,0,0,37,116,69,88,116,100,97,116,101,58,
    109,111,100,105,102,121,0,50,48,49,49,45,48,54,45,50,50,84,49,54,58,49,
    54,58,52,54,43,48,50,58,48,48,110,165,95,99,0,0,0,17,116,69,88,116,106,
    112,101,103,58,99,111,108,111,114,115,112,97,99,101,0,50,44,117,85,159,
    0,0,0,32,116,69,88,116,106,112,101,103,58,115,97,109,112,108,105,110,
    103,45,102,97,99,116,111,114,0,50,120,50,44,49,120,49,44,49,120,49,73,
    250,166,180,0,0,0,0,73,69,78,68,174,66,96,130
};

static unsigned char kWEBPImage[] = {
    82,73,70,70,74,0,0,0,87,69,66,80,86,80,56,88,10,0,0,0,16,0,0,0,0,0,0,0,0,0,
    65,76,80,72,12,0,0,0,1,7,16,17,253,15,68,68,255,3,0,0,86,80,56,32,24,0,0,0,
    48,1,0,157,1,42,1,0,1,0,3,0,52,37,164,0,3,112,0,254,251,253,80,0
};

static const char kTestUrl[] = "http://www.img.com";

static const char kWEBPHeaderResponse[] =
    "HTTP/1.1 200 OK\0Content-type: image/webp\0\0";

}  // namespace

// TODO(ios): remove the static_cast<UIImage*>(nil) once all the bots have
// Xcode 6.0 or later installed, http://crbug.com/440857

class ImageFetcherTest : public PlatformTest {
 protected:
  ImageFetcherTest()
      : pool_(new base::SequencedWorkerPool(1, "TestPool")),
        image_fetcher_(new ImageFetcher(pool_)),
        result_(nil),
        called_(false) {
    callback_.reset(
        [^(const GURL& original_url, int http_response_code, NSData* data) {
            result_ = [UIImage imageWithData:data];
            called_ = true;
        } copy]);
    image_fetcher_->SetRequestContextGetter(
        new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get()));
  }

  ~ImageFetcherTest() override { pool_->Shutdown(); }

  net::TestURLFetcher* SetupFetcher() {
    image_fetcher_->StartDownload(GURL(kTestUrl), callback_);
    EXPECT_EQ(static_cast<UIImage*>(nil), result_);
    EXPECT_EQ(false, called_);
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    DCHECK(fetcher);
    DCHECK(fetcher->delegate());
    return fetcher;
  }

  base::MessageLoop loop_;
  base::mac::ScopedBlock<ImageFetchedCallback> callback_;
  net::TestURLFetcherFactory factory_;
  scoped_refptr<base::SequencedWorkerPool> pool_;
  std::unique_ptr<ImageFetcher> image_fetcher_;
  UIImage* result_;
  bool called_;
};

TEST_F(ImageFetcherTest, TestError) {
  net::TestURLFetcher* fetcher = SetupFetcher();
  fetcher->set_response_code(404);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(static_cast<UIImage*>(nil), result_);
  EXPECT_TRUE(called_);
}

TEST_F(ImageFetcherTest, TestJpg) {
  net::TestURLFetcher* fetcher = SetupFetcher();
  fetcher->set_response_code(200);
  fetcher->SetResponseString(std::string((char*)kJPGImage, sizeof(kJPGImage)));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_NE(static_cast<UIImage*>(nil), result_);
  EXPECT_TRUE(called_);
}

TEST_F(ImageFetcherTest, TestPng) {
  net::TestURLFetcher* fetcher = SetupFetcher();
  fetcher->set_response_code(200);
  fetcher->SetResponseString(std::string((char*)kPNGImage, sizeof(kPNGImage)));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_NE(static_cast<UIImage*>(nil), result_);
  EXPECT_TRUE(called_);
}

TEST_F(ImageFetcherTest, TestGoodWebP) {
// TODO(droger): This test fails on iOS 9 x64 devices. http://crbug.com/523235
#if defined(OS_IOS) && defined(ARCH_CPU_ARM64) && !TARGET_IPHONE_SIMULATOR
  if (base::ios::IsRunningOnIOS9OrLater())
    return;
#endif
  net::TestURLFetcher* fetcher = SetupFetcher();
  fetcher->set_response_code(200);
  fetcher->SetResponseString(
      std::string((char*)kWEBPImage, sizeof(kWEBPImage)));
  scoped_refptr<net::HttpResponseHeaders> headers(new net::HttpResponseHeaders(
      std::string(kWEBPHeaderResponse, arraysize(kWEBPHeaderResponse))));
  fetcher->set_response_headers(headers);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  pool_->FlushForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(static_cast<UIImage*>(nil), result_);
  EXPECT_TRUE(called_);
}

TEST_F(ImageFetcherTest, TestBadWebP) {
  net::TestURLFetcher* fetcher = SetupFetcher();
  fetcher->set_response_code(200);
  fetcher->SetResponseString("This is not a valid WebP image");
  scoped_refptr<net::HttpResponseHeaders> headers(new net::HttpResponseHeaders(
      std::string(kWEBPHeaderResponse, arraysize(kWEBPHeaderResponse))));
  fetcher->set_response_headers(headers);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  pool_->FlushForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(static_cast<UIImage*>(nil), result_);
  EXPECT_TRUE(called_);
}

TEST_F(ImageFetcherTest, DeleteDuringWebPDecoding) {
  net::TestURLFetcher* fetcher = SetupFetcher();
  fetcher->set_response_code(200);
  fetcher->SetResponseString(
      std::string((char*)kWEBPImage, sizeof(kWEBPImage)));
  scoped_refptr<net::HttpResponseHeaders> headers(new net::HttpResponseHeaders(
      std::string(kWEBPHeaderResponse, arraysize(kWEBPHeaderResponse))));
  fetcher->set_response_headers(headers);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  // Delete the image fetcher, and check that the callback is not called.
  image_fetcher_.reset();
  pool_->FlushForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(static_cast<UIImage*>(nil), result_);
  EXPECT_FALSE(called_);
}

TEST_F(ImageFetcherTest, TestCallbacksNotCalledDuringDeletion) {
  image_fetcher_->StartDownload(GURL(kTestUrl), callback_);
  image_fetcher_.reset();
  EXPECT_FALSE(called_);
}

