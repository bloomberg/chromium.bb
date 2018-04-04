// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/itunes_links/itunes_links_handler_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/observer_list.h"
#import "ios/chrome/browser/store_kit/store_kit_tab_helper.h"
#import "ios/chrome/test/fakes/fake_store_kit_launcher.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/web_state/web_state_policy_decider.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ITunesLinksHandlerTabHelperTest : public PlatformTest {
 protected:
  ITunesLinksHandlerTabHelperTest()
      : fake_launcher_([[FakeStoreKitLauncher alloc] init]) {
    StoreKitTabHelper::CreateForWebState(&web_state_);
    std::unique_ptr<web::TestNavigationManager> test_navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    navigation_manager_ = test_navigation_manager.get();
    web_state_.SetNavigationManager(std::move(test_navigation_manager));
    ITunesLinksHandlerTabHelper::CreateForWebState(&web_state_);
    StoreKitTabHelper::FromWebState(&web_state_)->SetLauncher(fake_launcher_);
  }

  // Tries to finish navigation with the given |url_string| and returns true if
  // store kit was launched.
  bool VerifyStoreKitLaunched(const std::string& url_string) {
    fake_launcher_.launchedProductID = nil;
    fake_launcher_.launchedProductParams = nil;
    web::FakeNavigationContext context;
    context.SetUrl(GURL(url_string));
    web_state_.OnNavigationFinished(&context);
    return fake_launcher_.launchedProductID != nil ||
           fake_launcher_.launchedProductParams != nil;
  }

  // Checks that given the pending item URL & the request URL if
  // ITunesHandlerPolicyDecider will allow the request.
  bool ShouldAllowRequest(NSString* url_string,
                          std::string const& pending_item_url) {
    std::unique_ptr<web::NavigationItem> pending_item;
    if (!pending_item_url.empty()) {
      pending_item = web::NavigationItem::Create();
      pending_item->SetURL(GURL(pending_item_url));
    }
    navigation_manager_->SetPendingItem(pending_item.get());

    return web_state_.ShouldAllowRequest(
        [NSURLRequest requestWithURL:[NSURL URLWithString:url_string]],
        ui::PageTransition::PAGE_TRANSITION_LINK);
  }

  // Checks that given the pending item URL & the request URL if
  // ITunesHandlerPolicyDecider will allow the request.
  bool ShouldAllowResponse(NSString* url_string, bool main_frame) {
    NSURLResponse* response =
        [[NSURLResponse alloc] initWithURL:[NSURL URLWithString:url_string]
                                  MIMEType:@"text/html"
                     expectedContentLength:0
                          textEncodingName:nil];
    return web_state_.ShouldAllowResponse(response, main_frame);
  }

  web::TestNavigationManager* navigation_manager_;
  FakeStoreKitLauncher* fake_launcher_;
  web::TestWebState web_state_;
};

// Verifies that navigating to URLs that are not product hosted on the iTunes
// AppStore does not launch storekit.
TEST_F(ITunesLinksHandlerTabHelperTest, NonMatchingUrlsDoesntLaunchStoreKit) {
  EXPECT_FALSE(VerifyStoreKitLaunched(""));
  EXPECT_FALSE(VerifyStoreKitLaunched("foobar"));
  EXPECT_FALSE(VerifyStoreKitLaunched("foo://bar"));
  EXPECT_FALSE(VerifyStoreKitLaunched("http://foo"));
  EXPECT_FALSE(VerifyStoreKitLaunched("http://foo?bar#qux"));
  EXPECT_FALSE(VerifyStoreKitLaunched("http://foo.bar/qux"));
  EXPECT_FALSE(VerifyStoreKitLaunched("http://itunes.apple.com"));
  EXPECT_FALSE(VerifyStoreKitLaunched("http://itunes.apple.com/id"));
  EXPECT_FALSE(VerifyStoreKitLaunched("http://itunes.apple.com/12345"));
  EXPECT_FALSE(VerifyStoreKitLaunched("itms-apps://itunes.apple.com/id123"));
}

// Verifies that navigating to URLs for a product hosted on iTunes AppStore
// launches storekit.
TEST_F(ITunesLinksHandlerTabHelperTest, MatchingUrlsLaunchesStoreKit) {
  EXPECT_TRUE(VerifyStoreKitLaunched("http://itunes.apple.com/id123"));
  NSString* product_id = @"id";
  NSString* af_tkn = @"at";
  NSDictionary* expected_params = @{product_id : @"123"};

  EXPECT_NSEQ(expected_params, fake_launcher_.launchedProductParams);

  EXPECT_TRUE(VerifyStoreKitLaunched("http://itunes.apple.com/bar/id123?"));
  EXPECT_NSEQ(expected_params, fake_launcher_.launchedProductParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      "http://foo.itunes.apple.com/bar/id123?qux&baz#foo"));
  expected_params = @{product_id : @"123", @"qux" : @"", @"baz" : @""};
  EXPECT_NSEQ(expected_params, fake_launcher_.launchedProductParams);

  EXPECT_TRUE(
      VerifyStoreKitLaunched("http://itunes.apple.com/bar/id243?at=12312"));
  expected_params = @{product_id : @"243", af_tkn : @"12312"};
  EXPECT_NSEQ(expected_params, fake_launcher_.launchedProductParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      "http://itunes.apple.com/bar/idabc?at=213&ct=123"));
  expected_params = @{product_id : @"abc", af_tkn : @"213", @"ct" : @"123"};
  EXPECT_NSEQ(expected_params, fake_launcher_.launchedProductParams);

  EXPECT_TRUE(VerifyStoreKitLaunched(
      "http://itunes.apple.com/bar/id123?at=2&uo=4#foo"));
  expected_params = @{product_id : @"123", af_tkn : @"2", @"uo" : @"4"};
  EXPECT_NSEQ(expected_params, fake_launcher_.launchedProductParams);
}

// Verifies that ItunesLinkHandlerPolicyDecider don't allow redirects to Apple
// appstore when the original request link is http itunes product URL.
TEST_F(ITunesLinksHandlerTabHelperTest, TestPolicyDeciderShouldAllowRequest) {
  EXPECT_FALSE(ShouldAllowRequest(
      /*url_string=*/@"itms://itunes.apple.com/12345",
      /*pending_item_url=*/"http://itunes.apple.com/bar/id123"));
  EXPECT_TRUE(
      ShouldAllowRequest(/*url_string=*/@"itms://itunes.apple.com/12345",
                         /*pending_item_url=*/"http://foo.bar"));
  EXPECT_TRUE(
      ShouldAllowRequest(/*url_string=*/@"http://itunes.apple.com/12345",
                         /*pending_item_url=*/""));
  EXPECT_TRUE(
      ShouldAllowRequest(/*url_string=*/@"http://itunes.apple.com/bar/id123",
                         /*pending_item_url=*/""));
  EXPECT_TRUE(ShouldAllowRequest(
      /*url_string=*/@"http://itunes.apple.com/12345",
      /*pending_item_url=*/"https://foo.bar"));
}

// Verifies that ItunesLinkHandlerPolicyDecider block response from http itunes
// product URL.
TEST_F(ITunesLinksHandlerTabHelperTest, TestPolicyDeciderShouldAllowResponse) {
  EXPECT_TRUE(ShouldAllowResponse(/*url_string=*/@"http://itunes.apple.com",
                                  /*main_frame=*/true));
  EXPECT_FALSE(ShouldAllowResponse(
      /*url_string=*/@"http://itunes.apple.com/id1234", /*main_frame=*/true));
  // If the response is for subframe, it should be allowed.
  EXPECT_TRUE(ShouldAllowResponse(
      /*url_string=*/@"http://itunes.apple.com/id1234", /*main_frame=*/false));

  EXPECT_TRUE(ShouldAllowResponse(
      /*url_string=*/@"http://itunes.apple.com/12345", /*main_frame=*/true));
  EXPECT_FALSE(ShouldAllowResponse(
      /*url_string=*/@"http://itunes.apple.com/bar/id123?qux",
      /*main_frame=*/true));

  EXPECT_TRUE(
      ShouldAllowResponse(/*url_string=*/@"itms-apps://itunes.apple.com/id123",
                          /*main_frame=*/true));
  EXPECT_TRUE(ShouldAllowResponse(/*url_string=*/@"http://foo.bar/qux",
                                  /*main_frame=*/true));
}
