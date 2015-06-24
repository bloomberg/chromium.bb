// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/clipboard_recent_content_ios.h"

#import <UIKit/UIKit.h>

#include "base/ios/ios_util.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace test {
class ClipboardRecentContentIOSTestHelper : public ClipboardRecentContentIOS {
 public:
  // By default, set that the device booted 10 days ago.
  ClipboardRecentContentIOSTestHelper()
      : ClipboardRecentContentIOS(base::TimeDelta::FromDays(10)) {}
  ClipboardRecentContentIOSTestHelper(base::TimeDelta t)
      : ClipboardRecentContentIOS(t) {}

  ~ClipboardRecentContentIOSTestHelper() override {}
  void SetStoredPasteboardChangeDate(NSDate* changeDate) {
    lastPasteboardChangeDate_.reset([changeDate copy]);
    SaveToUserDefaults();
  }
  void SetStoredPasteboardChangeCount(NSInteger newChangeCount) {
    lastPasteboardChangeCount_ = newChangeCount;
    SaveToUserDefaults();
  }
};
}  // namespace test

namespace {
void SetPasteboardContent(const char* data) {
  [[UIPasteboard generalPasteboard]
               setValue:[NSString stringWithUTF8String:data]
      forPasteboardType:@"public.plain-text"];
}
const char* kUnrecognizedURL = "ftp://foo/";
const char* kRecognizedURL = "http://bar/";
const char* kAppSpecificURL = "test://qux/";
const char* kAppSpecificScheme = "test";
NSTimeInterval kSevenHours = 60 * 60 * 7;
}  // namespace

class ClipboardRecentContentIOSTest : public ::testing::Test {
 protected:
  void SetUp() override {
    clipboard_content_.reset(new test::ClipboardRecentContentIOSTestHelper());
  }
  void TearDown() override {}

  void SimulateDeviceRestart() {
    // TODO(jif): Simulates the fact that on iOS7, the pasteboard's changeCount
    // is reset. http://crbug.com/503609
    clipboard_content_.reset(new test::ClipboardRecentContentIOSTestHelper(
        base::TimeDelta::FromSeconds(0)));
  }

 protected:
  scoped_ptr<test::ClipboardRecentContentIOSTestHelper> clipboard_content_;
};

TEST_F(ClipboardRecentContentIOSTest, SchemeFiltering) {
  GURL gurl;

  // Test unrecognized URL.
  SetPasteboardContent(kUnrecognizedURL);
  EXPECT_FALSE(clipboard_content_->GetRecentURLFromClipboard(&gurl));

  // Test recognized URL.
  SetPasteboardContent(kRecognizedURL);
  EXPECT_TRUE(clipboard_content_->GetRecentURLFromClipboard(&gurl));
  EXPECT_STREQ(kRecognizedURL, gurl.spec().c_str());

  // Test URL with app specific scheme, before and after configuration.
  SetPasteboardContent(kAppSpecificURL);
  EXPECT_FALSE(clipboard_content_->GetRecentURLFromClipboard(&gurl));
  clipboard_content_->set_application_scheme(kAppSpecificScheme);
  SetPasteboardContent(kAppSpecificURL);
  EXPECT_TRUE(clipboard_content_->GetRecentURLFromClipboard(&gurl));
  EXPECT_STREQ(kAppSpecificURL, gurl.spec().c_str());
}

TEST_F(ClipboardRecentContentIOSTest, PasteboardURLObsolescence) {
  GURL gurl;
  SetPasteboardContent(kRecognizedURL);

  // Test that recent pasteboard data is provided.
  EXPECT_TRUE(clipboard_content_->GetRecentURLFromClipboard(&gurl));
  EXPECT_STREQ(kRecognizedURL, gurl.spec().c_str());

  // Test that old pasteboard data is not provided.
  clipboard_content_->SetStoredPasteboardChangeDate(
      [NSDate dateWithTimeIntervalSinceNow:-kSevenHours]);
  EXPECT_FALSE(clipboard_content_->GetRecentURLFromClipboard(&gurl));

  // Tests that if chrome is relaunched, old pasteboard data is still
  // not provided.
  clipboard_content_.reset(new test::ClipboardRecentContentIOSTestHelper());
  EXPECT_FALSE(clipboard_content_->GetRecentURLFromClipboard(&gurl));


  SimulateDeviceRestart();
  if (base::ios::IsRunningOnIOS8OrLater()) {
    // Tests that if the device is restarted, old pasteboard data is still
    // not provided.
    EXPECT_FALSE(clipboard_content_->GetRecentURLFromClipboard(&gurl));
  } else {
    // TODO(jif): Simulates the fact that on iOS7, the pasteboard's changeCount
    // is reset. http://crbug.com/503609
  }

}

TEST_F(ClipboardRecentContentIOSTest, SupressedPasteboard) {
  GURL gurl;
  SetPasteboardContent(kRecognizedURL);

  // Test that recent pasteboard data is provided.
  EXPECT_TRUE(clipboard_content_->GetRecentURLFromClipboard(&gurl));

  // Suppress the content of the pasteboard.
  clipboard_content_->SuppressClipboardContent();

  // Check that the pasteboard content is suppressed.
  EXPECT_FALSE(clipboard_content_->GetRecentURLFromClipboard(&gurl));

  // Create a new clipboard content to test persistence.
  clipboard_content_.reset(new test::ClipboardRecentContentIOSTestHelper());

  // Check that the pasteboard content is still suppressed.
  EXPECT_FALSE(clipboard_content_->GetRecentURLFromClipboard(&gurl));

  // Check that the even if the device is restarted, pasteboard content is
  // still suppressed.
  SimulateDeviceRestart();
  EXPECT_FALSE(clipboard_content_->GetRecentURLFromClipboard(&gurl));

  // Check that if the pasteboard changes, the new content is not
  // supressed anymore.
  SetPasteboardContent(kRecognizedURL);
  EXPECT_TRUE(clipboard_content_->GetRecentURLFromClipboard(&gurl));
}
