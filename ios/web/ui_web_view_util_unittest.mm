// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/ui_web_view_util.h"

#include "testing/gtest_mac.h"

namespace {
// Returns user agent string registered for UIWebView.
NSString* GetUserAgent() {
  return [[NSUserDefaults standardUserDefaults] stringForKey:@"UserAgent"];
}
}  // namespace

namespace web {

// Tests web::RegisterUserAgentForUIWebView function that it correctly registers
// arbitrary user agent.
TEST(UIWebViewUtilTest, RegisterUserAgentForUIWebView) {
  web::RegisterUserAgentForUIWebView(@"UA");
  EXPECT_NSEQ(@"UA", GetUserAgent());
}

}  // namespace web
