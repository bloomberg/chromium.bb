// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/invalid_url_tab_helper.h"

#import <Foundation/Foundation.h>

#import "ios/net/protocol_handler_util.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "net/base/net_errors.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class InvalidUrlTabHelperTest : public PlatformTest {
 protected:
  InvalidUrlTabHelperTest() {
    InvalidUrlTabHelper::CreateForWebState(&web_state_);
  }

  // Returns PolicyDecision for URL request with given |spec| and |transition|.
  web::WebStatePolicyDecider::PolicyDecision GetPolicy(
      NSString* spec,
      ui::PageTransition transition) {
    NSURL* url = [NSURL URLWithString:spec];
    NSURLRequest* request = [[NSURLRequest alloc] initWithURL:url];
    web::WebStatePolicyDecider::RequestInfo info(transition,
                                                 /*target_frame_is_main=*/true,
                                                 /*has_user_gesture=*/false);
    return web_state_.ShouldAllowRequest(request, info);
  }

  web::TestWebState web_state_;
};

// Tests that navigation is allowed for https url link.
TEST_F(InvalidUrlTabHelperTest, HttpsUrl) {
  auto policy = GetPolicy(@"https://foo.test", ui::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(policy.ShouldAllowNavigation());
}

// Tests that navigation is allowed for https url links if url length is under
// allowed limit.
TEST_F(InvalidUrlTabHelperTest, HttpsUrlUnderLengthLimit) {
  NSString* spec = [@"https://" stringByPaddingToLength:url::kMaxURLChars
                                             withString:@"0"
                                        startingAtIndex:0];
  auto policy = GetPolicy(spec, ui::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(policy.ShouldAllowNavigation());
}

// Tests that navigation is cancelled for https url links if url length is above
// allowed limit.
TEST_F(InvalidUrlTabHelperTest, HttpsUrlAboveLengthLimit) {
  NSString* spec = [@"https://" stringByPaddingToLength:url::kMaxURLChars + 1
                                             withString:@"0"
                                        startingAtIndex:0];
  auto policy = GetPolicy(spec, ui::PAGE_TRANSITION_LINK);
  EXPECT_FALSE(policy.ShouldAllowNavigation());
  EXPECT_FALSE(policy.ShouldDisplayError());
}

// Tests that navigation is allowed for valid data url link.
TEST_F(InvalidUrlTabHelperTest, ValidDataUrlLink) {
  auto policy = GetPolicy(@"data:text/plain;charset=utf-8,test",
                          ui::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(policy.ShouldAllowNavigation());
}

// Tests that navigation is sillently cancelled for invalid data url link.
TEST_F(InvalidUrlTabHelperTest, InvalidDataUrlLink) {
  auto policy = GetPolicy(@"data://", ui::PAGE_TRANSITION_LINK);
  EXPECT_FALSE(policy.ShouldAllowNavigation());
  EXPECT_FALSE(policy.ShouldDisplayError());
}

// Tests that navigation is cancelled with error for invalid data: url bookmark.
TEST_F(InvalidUrlTabHelperTest, InvalidDataUrlBookmark) {
  auto policy = GetPolicy(@"data://", ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  EXPECT_FALSE(policy.ShouldAllowNavigation());
  EXPECT_NSEQ(net::kNSErrorDomain, policy.GetDisplayError().domain);
  EXPECT_EQ(net::ERR_INVALID_URL, policy.GetDisplayError().code);
}

// Tests that navigation is cancelled with error for invalid data: url typed.
TEST_F(InvalidUrlTabHelperTest, InvalidDataUrlTyped) {
  auto policy = GetPolicy(@"data://", ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(policy.ShouldAllowNavigation());
  EXPECT_NSEQ(net::kNSErrorDomain, policy.GetDisplayError().domain);
  EXPECT_EQ(net::ERR_INVALID_URL, policy.GetDisplayError().code);
}

// Tests that navigation is cancelled with error for invalid data: url
// generated.
TEST_F(InvalidUrlTabHelperTest, InvalidDataUrlGenerated) {
  auto policy = GetPolicy(@"data://", ui::PAGE_TRANSITION_GENERATED);
  EXPECT_FALSE(policy.ShouldAllowNavigation());
  EXPECT_NSEQ(net::kNSErrorDomain, policy.GetDisplayError().domain);
  EXPECT_EQ(net::ERR_INVALID_URL, policy.GetDisplayError().code);
}
