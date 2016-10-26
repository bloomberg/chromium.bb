// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_base_test_case.h"

#import <EarlGrey/EarlGrey.h>

#import "ios/web/public/test/http_server.h"
#import "ios/web/shell/test/earl_grey/shell_matchers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::test::HttpServer;
using web::webViewContainingText;

@implementation ShellBaseTestCase

// Set up called once for the class.
+ (void)setUp {
  [super setUp];
  [[EarlGrey selectElementWithMatcher:webViewContainingText("Chromium")]
      assertWithMatcher:grey_notNil()];
  HttpServer::GetSharedInstance().StartOrDie();
}

// Tear down called once for the class.
+ (void)tearDown {
  HttpServer::GetSharedInstance().Stop();
  [super tearDown];
}

// Tear down called after each test.
- (void)tearDown {
  HttpServer::GetSharedInstance().RemoveAllResponseProviders();
  [super tearDown];
}

@end
