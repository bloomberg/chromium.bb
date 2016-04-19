// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

const bool kAsyncCall = true;
const bool kSyncCall = false;

namespace chrome {

// Class to catch events from the BitmapFetcher for testing.
class BitmapFetcherTestDelegate : public BitmapFetcherDelegate {
 public:
  explicit BitmapFetcherTestDelegate(bool async) : called_(false),
                                                   success_(false),
                                                   async_(async) {}

  ~BitmapFetcherTestDelegate() override { EXPECT_TRUE(called_); }

  // Method inherited from BitmapFetcherDelegate.
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override {
    called_ = true;
    url_ = url;
    if (bitmap) {
      success_ = true;
      bitmap->deepCopyTo(&bitmap_);
    }
    // For async calls, we need to quit the run loop so the test can continue.
    if (async_)
      run_loop_.Quit();
  }

  // Waits until OnFetchComplete() is called. Should only be used for
  // async tests.
  void Wait() {
    ASSERT_TRUE(async_);
    run_loop_.Run();
  }

  GURL url() const { return url_; }
  bool success() const { return success_; }
  const SkBitmap& bitmap() const { return bitmap_; }

 private:
  base::RunLoop run_loop_;
  bool called_;
  GURL url_;
  bool success_;
  bool async_;
  SkBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(BitmapFetcherTestDelegate);
};

class BitmapFetcherBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    url_fetcher_factory_.reset(
        new net::FakeURLFetcherFactory(&url_fetcher_impl_factory_));
    InProcessBrowserTest::SetUp();
  }

 protected:
  net::URLFetcherImplFactory url_fetcher_impl_factory_;
  std::unique_ptr<net::FakeURLFetcherFactory> url_fetcher_factory_;
};

// WARNING:  These tests work with --single_process, but not
// --single-process.  The reason is that the sandbox does not get created
// for us by the test process if --single-process is used.

IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest, StartTest) {
  GURL url("http://example.com/this-should-work");

  // Put some realistic looking bitmap data into the url_fetcher.
  SkBitmap image;

  // Put a real bitmap into "image".  2x2 bitmap of green 32 bit pixels.
  image.allocN32Pixels(2, 2);
  image.eraseColor(SK_ColorGREEN);

  // Encode the bits as a PNG.
  std::vector<unsigned char> compressed;
  ASSERT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(image, true, &compressed));

  // Copy the bits into the string, and put them into the FakeURLFetcher.
  std::string image_string(compressed.begin(), compressed.end());

  // Set up a delegate to wait for the callback.
  BitmapFetcherTestDelegate delegate(kAsyncCall);

  BitmapFetcher fetcher(url, &delegate);

  url_fetcher_factory_->SetFakeResponse(
      url, image_string, net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  // We expect that the image decoder will get called and return
  // an image in a callback to OnImageDecoded().
  fetcher.Init(
      browser()->profile()->GetRequestContext(),
      std::string(),
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      net::LOAD_NORMAL);
  fetcher.Start();

  // Blocks until test delegate is notified via a callback.
  delegate.Wait();

  ASSERT_TRUE(delegate.success());

  // Make sure we get back the bitmap we expect.
  const SkBitmap& found_image = delegate.bitmap();
  EXPECT_TRUE(gfx::BitmapsAreEqual(image, found_image));
}

IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest, OnImageDecodedTest) {
  GURL url("http://example.com/this-should-work-as-well");
  SkBitmap image;

  // Put a real bitmap into "image".  2x2 bitmap of green 16 bit pixels.
  image.allocN32Pixels(2, 2);
  image.eraseColor(SK_ColorGREEN);

  BitmapFetcherTestDelegate delegate(kSyncCall);

  BitmapFetcher fetcher(url, &delegate);

  fetcher.OnImageDecoded(image);

  // Ensure image is marked as succeeded.
  EXPECT_TRUE(delegate.success());

  // Test that the image is what we expect.
  EXPECT_TRUE(gfx::BitmapsAreEqual(image, delegate.bitmap()));
}

IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest, OnURLFetchFailureTest) {
  GURL url("http://example.com/this-should-be-fetch-failure");

  // We intentionally put no data into the bitmap to simulate a failure.

  // Set up a delegate to wait for the callback.
  BitmapFetcherTestDelegate delegate(kAsyncCall);

  BitmapFetcher fetcher(url, &delegate);

  url_fetcher_factory_->SetFakeResponse(url,
                                        std::string(),
                                        net::HTTP_INTERNAL_SERVER_ERROR,
                                        net::URLRequestStatus::FAILED);

  fetcher.Init(
      browser()->profile()->GetRequestContext(),
      std::string(),
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      net::LOAD_NORMAL);
  fetcher.Start();

  // Blocks until test delegate is notified via a callback.
  delegate.Wait();

  EXPECT_FALSE(delegate.success());
}

IN_PROC_BROWSER_TEST_F(BitmapFetcherBrowserTest, HandleImageFailedTest) {
  GURL url("http://example.com/this-should-be-a-decode-failure");
  BitmapFetcherTestDelegate delegate(kAsyncCall);
  BitmapFetcher fetcher(url, &delegate);
  url_fetcher_factory_->SetFakeResponse(url,
                                        std::string("Not a real bitmap"),
                                        net::HTTP_OK,
                                        net::URLRequestStatus::SUCCESS);

  fetcher.Init(
      browser()->profile()->GetRequestContext(),
      std::string(),
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      net::LOAD_NORMAL);
  fetcher.Start();

  // Blocks until test delegate is notified via a callback.
  delegate.Wait();

  EXPECT_FALSE(delegate.success());
}

}  // namespace chrome
