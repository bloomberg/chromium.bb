// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/notification_bitmap_fetcher.h"

#include "base/compiler_specific.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
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
      : success_(false), async_(async) {}

  virtual ~NotificationBitmapFetcherTestDelegate() {}

  // Method inherited from NotificationBitmapFetcherDelegate.
  virtual void OnFetchComplete(const GURL url,
                               const SkBitmap* bitmap) OVERRIDE {
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
  GURL url_;
  bool success_;
  bool async_;
  SkBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(NotificationBitmapFetcherTestDelegate);
};

typedef InProcessBrowserTest NotificationBitmapFetcherBrowserTest;

// WARNING:  These tests work with --single_process, but not
// --single-process.  The reason is that the sandbox does not get created
// for us by the test process if --single-process is used.

IN_PROC_BROWSER_TEST_F(NotificationBitmapFetcherBrowserTest,
                       StartTest) {
  GURL url("http://localhost");

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

  scoped_ptr<net::URLFetcher> url_fetcher(new net::FakeURLFetcher(
      url, &fetcher, image_string, true));
  fetcher.SetURLFetcherForTest(url_fetcher.Pass());

  // We expect that the image decoder will get called and return
  // an image in a callback to OnImageDecoded().
  fetcher.Start(NULL);

  // Blocks until test delegate is notified via a callback.
  content::RunMessageLoop();

  ASSERT_TRUE(delegate.success());

  // Make sure we get back the bitmap we expect.
  const SkBitmap& found_image = delegate.bitmap();
  EXPECT_TRUE(gfx::BitmapsAreEqual(image, found_image));
}

IN_PROC_BROWSER_TEST_F(NotificationBitmapFetcherBrowserTest,
                       OnImageDecodedTest) {
  GURL url("http://localhost");
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
  GURL url("http://localhost");

  // We intentionally put no data into the bitmap to simulate a failure.

  // Set up a delegate to wait for the callback.
  NotificationBitmapFetcherTestDelegate delegate(kAsyncCall);

  NotificationBitmapFetcher fetcher(url, &delegate);

  scoped_ptr<net::URLFetcher> url_fetcher(new net::FakeURLFetcher(
      url, &fetcher, std::string(), false));
  fetcher.SetURLFetcherForTest(url_fetcher.Pass());

  fetcher.Start(NULL);

  // Blocks until test delegate is notified via a callback.
  content::RunMessageLoop();

  EXPECT_FALSE(delegate.success());
}

IN_PROC_BROWSER_TEST_F(NotificationBitmapFetcherBrowserTest,
                       HandleImageFailedTest) {
  GURL url("http://localhost");
  NotificationBitmapFetcherTestDelegate delegate(kAsyncCall);
  NotificationBitmapFetcher fetcher(url, &delegate);
  scoped_ptr<net::URLFetcher> url_fetcher(new net::FakeURLFetcher(
      url, &fetcher, std::string("Not a real bitmap"), true));
  fetcher.SetURLFetcherForTest(url_fetcher.Pass());

  fetcher.Start(NULL);

  // Blocks until test delegate is notified via a callback.
  content::RunMessageLoop();

  EXPECT_FALSE(delegate.success());
}

}  // namespace notifier
