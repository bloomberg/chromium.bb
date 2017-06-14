// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>

#import "ios/web_view/test/chrome_web_view_test.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Creates web view for testing.
CWVWebView* CreateWebView() {
  return [[CWVWebView alloc]
      initWithFrame:CGRectMake(0, 0, 50, 50)
      configuration:[CWVWebViewConfiguration defaultConfiguration]];
}

// Creates a new web view and restores its state from |source_web_view|.
CWVWebView* CreateWebViewWithState(CWVWebView* source_web_view) {
  NSMutableData* data = [[NSMutableData alloc] init];
  NSKeyedArchiver* archiver =
      [[NSKeyedArchiver alloc] initForWritingWithMutableData:data];
  [source_web_view encodeRestorableStateWithCoder:archiver];
  [archiver finishEncoding];
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingWithData:data];
  CWVWebView* result = CreateWebView();
  [result decodeRestorableStateWithCoder:unarchiver];
  return result;
}

}  // namespace

namespace ios_web_view {

// Tests encodeRestorableStateWithCoder: and decodeRestorableStateWithCoder:
// methods.
typedef ios_web_view::ChromeWebViewTest ChromeWebViewRestorableStateTest;
TEST_F(ChromeWebViewRestorableStateTest, EncodeDecode) {
  // Create web view that will be used as a source for state restoration.
  CWVWebView* web_view = CreateWebView();

  // Load 2 URLs to create non-default state.
  ASSERT_FALSE(web_view.lastCommittedURL);
  ASSERT_FALSE(web_view.visibleURL);
  ASSERT_FALSE(web_view.canGoBack);
  ASSERT_FALSE(web_view.canGoForward);
  LoadUrl(web_view, [NSURL URLWithString:@"about:newtab"]);
  ASSERT_NSEQ(@"about:newtab", web_view.lastCommittedURL.absoluteString);
  ASSERT_NSEQ(@"about:newtab", web_view.visibleURL.absoluteString);
  LoadUrl(web_view, [NSURL URLWithString:@"about:blank"]);
  ASSERT_NSEQ(@"about:blank", web_view.lastCommittedURL.absoluteString);
  ASSERT_NSEQ(@"about:blank", web_view.visibleURL.absoluteString);
  ASSERT_TRUE(web_view.canGoBack);
  ASSERT_FALSE(web_view.canGoForward);

  // Create second web view and restore its state from the first web view.
  CWVWebView* restored_web_view = CreateWebViewWithState(web_view);

  // Verify that the state has been restored correctly.
  EXPECT_NSEQ(@"about:blank",
              restored_web_view.lastCommittedURL.absoluteString);
  EXPECT_NSEQ(@"about:blank", restored_web_view.visibleURL.absoluteString);
  EXPECT_TRUE(web_view.canGoBack);
  EXPECT_FALSE(web_view.canGoForward);
}

}  // namespace ios_web_view
