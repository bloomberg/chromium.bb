// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/history_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test case for NTP tiles.
@interface NTPTilesTest : ChromeTestCase
@end

@implementation NTPTilesTest

// Tests that loading a URL ends up creating an NTP tile.
- (void)testTopSitesTileAfterLoadURL {
  std::map<GURL, std::string> responses;
  GURL URL = web::test::HttpServer::MakeUrl("http://simple_tile.html");
  responses[URL] =
      "<head><title>title1</title></head>"
      "<body>You are here.</body>";
  web::test::SetUpSimpleHttpServer(responses);

  // Clear history and verify that the tile does not exist.
  chrome_test_util::ClearBrowsingHistory();
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  chrome_test_util::OpenNewTab();

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(@"title1")]
      assertWithMatcher:grey_nil()];

  [ChromeEarlGrey loadURL:URL];

  // After loading URL, need to do another action before opening a new tab
  // with the icon present.
  [ChromeEarlGrey goBack];

  chrome_test_util::OpenNewTab();
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(@"title1")]
      assertWithMatcher:grey_notNil()];
}

@end
