// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/ui_web_view_util.h"

#include "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/test_web_client.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {

// Returns user agent string registered for UIWebView.
NSString* GetUserAgent() {
  return [[NSUserDefaults standardUserDefaults] stringForKey:@"UserAgent"];
}

class UIWebViewUtilTest : public PlatformTest {
 public:
  UIWebViewUtilTest() : web_client_(make_scoped_ptr(new web::TestWebClient)) {}

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    test_web_client()->SetUserAgent("DesktopUA", true);
    test_web_client()->SetUserAgent("RegularUA", false);
  }

  web::TestWebClient* test_web_client() {
    return static_cast<web::TestWebClient*>(web_client_.Get());
  }

 private:
  // WebClient that returns test user agent.
  web::ScopedTestingWebClient web_client_;
};

// Tests registration of a non-desktop user agent.
TEST_F(UIWebViewUtilTest, BuildAndRegisterNonDesktopUserAgentForUIWebView) {
  web::BuildAndRegisterUserAgentForUIWebView(@"1231546541321", NO);
  EXPECT_NSEQ(@"RegularUA (1231546541321)", GetUserAgent());
}

// Tests registration of a desktop user agent.
TEST_F(UIWebViewUtilTest, BuildAndRegisterDesktopUserAgentForUIWebView) {
  web::BuildAndRegisterUserAgentForUIWebView(@"1231546541321", YES);
  EXPECT_NSEQ(@"DesktopUA (1231546541321)", GetUserAgent());
}

// Tests web::RegisterUserAgentForUIWebView function that it correctly registers
// arbitrary user agent.
TEST_F(UIWebViewUtilTest, RegisterUserAgentForUIWebView) {
  web::RegisterUserAgentForUIWebView(@"UA");
  EXPECT_NSEQ(@"UA", GetUserAgent());
}

}  // namespace
