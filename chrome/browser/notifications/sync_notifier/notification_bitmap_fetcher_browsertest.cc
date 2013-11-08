// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/notification_bitmap_fetcher.h"

#include "base/compiler_specific.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"

namespace {
const bool kAsyncCall = true;
const bool kSyncCall = false;
}  // namespace

namespace notifier {

// Class to catch events from the NotificationBitmapFetcher for testing.
class NotificationBitmapFetcherTestDelegate
    : public NotificationBitmapFetcherDelegate {
 public:
  explicit NotificationBitmapFetcherTestDelegate(bool async)
      : called_(false), success_(false), async_(async) {}

  virtual ~NotificationBitmapFetcherTestDelegate() {
    EXPECT_TRUE(called_);
  }

  // Method inherited from NotificationBitmapFetcherDelegate.
  virtual void OnFetchComplete(const GURL url,
                               const SkBitmap* bitmap) OVERRIDE {
    called_ = true;
    url_ = url;
    if (NULL != bitmap) {
      success_ = true;
      bitmap->deepCopyTo(&bitmap_, bitmap->getConfig());
    }
    // For async calls, we need to quit the message loop so the test can
    // continue.
    if (async_) {
      base::MessageLoop::current()->Quit();
    }
  }

  GURL url() const { return url_; }
  bool success() const { return success_; }
  const SkBitmap& bitmap() const { return bitmap_; }

 private:
  bool called_;
  GURL url_;
  bool success_;
  bool async_;
  SkBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(NotificationBitmapFetcherTestDelegate);
};

class NotificationBitmapFetcherBrowserTest : public InProcessBrowserTest {
 public:
  virtual void SetUp() OVERRIDE {
    url_fetcher_factory_.reset(new net::FakeURLFetcherFactory(NULL));
    InProcessBrowserTest::SetUp();
  }

 protected:
  scoped_ptr<net::FakeURLFetcherFactory> url_fetcher_factory_;
};

#if defined(OS_WIN)
#define MAYBE_StartTest DISABLED_StartTest
#else
#define MAYBE_StartTest StartTest
#endif

// WARNING:  These tests work with --single_process, but not
// --single-process.  The reason is that the sandbox does not get created
// for us by the test process if --single-process is used.

IN_PROC_BROWSER_TEST_F(NotificationBitmapFetcherBrowserTest,
                       MAYBE_StartTest) {
  GURL url("http://example.com/this-should-work");

  // Put some realistic looking bitmap data into the url_fetcher.
  SkBitmap image;

  // Put a real bitmap into "image".  2x2 bitmap of green 32 bit pixels.
  image.setConfig(SkBitmap::kARGB_8888_Config, 2, 2);
  image.allocPixels();
  image.eraseColor(SK_ColorGREEN);

  // Encode the bits as a PNG.
  std::vector<unsigned char> compressed;
  ASSERT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(image, true, &compressed));

  // Copy the bits into the string, and put them into the FakeURLFetcher.
  std::string image_string(compressed.begin(), compressed.end());

  // Set up a delegate to wait for the callback.
  NotificationBitmapFetcherTestDelegate delegate(kAsyncCall);

  NotificationBitmapFetcher fetcher(url, &delegate);

  url_fetcher_factory_->SetFakeResponse(url, image_string, net::HTTP_OK,
                                        net::URLRequestStatus::SUCCESS);

  // We expect that the image decoder will get called and return
  // an image in a callback to OnImageDecoded().
  fetcher.Start(browser()->profile());

  // Blocks until test delegate is notified via a callback.
  content::RunMessageLoop();

  ASSERT_TRUE(delegate.success());

  // Make sure we get back the bitmap we expect.
  const SkBitmap& found_image = delegate.bitmap();
  EXPECT_TRUE(gfx::BitmapsAreEqual(image, found_image));
}

IN_PROC_BROWSER_TEST_F(NotificationBitmapFetcherBrowserTest,
                       OnImageDecodedTest) {
  GURL url("http://example.com/this-should-work-as-well");
  SkBitmap image;

  // Put a real bitmap into "image".  2x2 bitmap of green 16 bit pixels.
  image.setConfig(SkBitmap::kARGB_8888_Config, 2, 2);
  image.allocPixels();
  image.eraseColor(SK_ColorGREEN);

  NotificationBitmapFetcherTestDelegate delegate(kSyncCall);

  NotificationBitmapFetcher fetcher(url, &delegate);

  fetcher.OnImageDecoded(NULL, image);

  // Ensure image is marked as succeeded.
  EXPECT_TRUE(delegate.success());

  // Test that the image is what we expect.
  EXPECT_TRUE(gfx::BitmapsAreEqual(image, delegate.bitmap()));
}

IN_PROC_BROWSER_TEST_F(NotificationBitmapFetcherBrowserTest,
                       OnURLFetchFailureTest) {
  GURL url("http://example.com/this-should-be-fetch-failure");

  // We intentionally put no data into the bitmap to simulate a failure.

  // Set up a delegate to wait for the callback.
  NotificationBitmapFetcherTestDelegate delegate(kAsyncCall);

  NotificationBitmapFetcher fetcher(url, &delegate);

  url_fetcher_factory_->SetFakeResponse(url,
                                        std::string(),
                                        net::HTTP_INTERNAL_SERVER_ERROR,
                                        net::URLRequestStatus::FAILED);

  fetcher.Start(browser()->profile());

  // Blocks until test delegate is notified via a callback.
  content::RunMessageLoop();

  EXPECT_FALSE(delegate.success());
}

// Flaky on Win XP Debug: crbug.com/316488
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_HandleImageFailedTest DISABLED_HandleImageFailedTest
#else
#define MAYBE_HandleImageFailedTest HandleImageFailedTest
#endif

IN_PROC_BROWSER_TEST_F(NotificationBitmapFetcherBrowserTest,
                       MAYBE_HandleImageFailedTest) {
  GURL url("http://example.com/this-should-be-a-decode-failure");
  NotificationBitmapFetcherTestDelegate delegate(kAsyncCall);
  NotificationBitmapFetcher fetcher(url, &delegate);
  url_fetcher_factory_->SetFakeResponse(
      url, std::string("Not a real bitmap"),
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  fetcher.Start(browser()->profile());

  // Blocks until test delegate is notified via a callback.
  content::RunMessageLoop();

  EXPECT_FALSE(delegate.success());
}

}  // namespace notifier
