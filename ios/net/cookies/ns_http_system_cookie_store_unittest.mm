// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/cookies/ns_http_system_cookie_store.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/memory/ptr_util.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

// Test fixture to exercise net::NSHTTPSystemCookieStore created with
// |[NSHTTPCookieStorage sharedHTTPCookieStorage]|.
class NSHTTPSystemCookieStoreTest : public PlatformTest {
 public:
  NSHTTPSystemCookieStoreTest()
      : shared_store_([NSHTTPCookieStorage sharedHTTPCookieStorage]),
        store_(base::MakeUnique<net::NSHTTPSystemCookieStore>(shared_store_)),
        test_cookie_url1_([NSURL URLWithString:@"http://foo.google.com/bar"]),
        test_cookie_url2_([NSURL URLWithString:@"http://bar.xyz.abc"]),
        test_cookie_url3_([NSURL URLWithString:@"http://123.foo.bar"]) {
    ClearCookies();
  }

  void ClearCookies() {
    [shared_store_ setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyAlways];
    for (NSHTTPCookie* cookie in shared_store_.cookies)
      [shared_store_ deleteCookie:cookie];
    EXPECT_EQ(0u, shared_store_.cookies.count);
  }

  NSHTTPCookie* CreateCookie(NSString* cookie_line, NSURL* url) {
    return [[NSHTTPCookie
        cookiesWithResponseHeaderFields:@{@"Set-Cookie" : cookie_line}
                                 forURL:url] objectAtIndex:0];
  }

 protected:
  NSHTTPCookieStorage* shared_store_;
  std::unique_ptr<net::NSHTTPSystemCookieStore> store_;
  NSURL* test_cookie_url1_;
  NSURL* test_cookie_url2_;
  NSURL* test_cookie_url3_;
};

TEST_F(NSHTTPSystemCookieStoreTest, SetCookie) {
  NSHTTPCookie* system_cookie = CreateCookie(@"a=b", test_cookie_url1_);
  // Verify that cookie is not set in system storage.
  for (NSHTTPCookie* cookie in
       [shared_store_ cookiesForURL:test_cookie_url1_]) {
    EXPECT_FALSE([cookie.name isEqualToString:system_cookie.name]);
  }
  store_->SetCookie(system_cookie);

  // Verify that cookie is set in system storage.
  NSHTTPCookie* result_cookie = nil;

  for (NSHTTPCookie* cookie in
       [shared_store_ cookiesForURL:test_cookie_url1_]) {
    if ([cookie.name isEqualToString:system_cookie.name]) {
      result_cookie = cookie;
      break;
    }
  }
  EXPECT_TRUE([result_cookie.value isEqualToString:system_cookie.value]);
}

TEST_F(NSHTTPSystemCookieStoreTest, ClearCookies) {
  [shared_store_ setCookie:CreateCookie(@"a=b", test_cookie_url1_)];
  [shared_store_ setCookie:CreateCookie(@"x=d", test_cookie_url2_)];

  EXPECT_EQ(2u, shared_store_.cookies.count);
  store_->ClearStore();
  EXPECT_EQ(0u, shared_store_.cookies.count);
}

TEST_F(NSHTTPSystemCookieStoreTest, GetCookies) {
  NSMutableDictionary* input_cookies = [[NSMutableDictionary alloc] init];
  NSHTTPCookie* system_cookie = CreateCookie(@"a=b", test_cookie_url1_);
  [input_cookies setValue:system_cookie forKey:@"a"];
  [shared_store_ setCookie:system_cookie];

  system_cookie = CreateCookie(@"x=d", test_cookie_url2_);
  [input_cookies setValue:system_cookie forKey:@"x"];
  [shared_store_ setCookie:system_cookie];

  system_cookie = CreateCookie(@"l=m", test_cookie_url3_);
  [input_cookies setValue:system_cookie forKey:@"l"];
  [shared_store_ setCookie:system_cookie];

  EXPECT_EQ(3u, shared_store_.cookies.count);

  // Test GetAllCookies
  NSArray* cookies = store_->GetAllCookies(/*manager=*/nullptr);
  EXPECT_EQ(3u, cookies.count);
  for (NSHTTPCookie* cookie in cookies) {
    NSHTTPCookie* existing_cookie = [input_cookies valueForKey:cookie.name];
    EXPECT_TRUE(existing_cookie);
    EXPECT_TRUE([existing_cookie.name isEqualToString:cookie.name]);
    EXPECT_TRUE([existing_cookie.value isEqualToString:cookie.value]);
    EXPECT_TRUE([existing_cookie.domain isEqualToString:cookie.domain]);
  }

  // Test GetCookiesForURL
  NSHTTPCookie* input_cookie = [input_cookies valueForKey:@"a"];
  NSArray* cookies_for_url = store_->GetCookiesForURL(
      GURLWithNSURL(test_cookie_url1_), /*manager=*/nullptr);
  EXPECT_EQ(1u, cookies_for_url.count);
  NSHTTPCookie* output_cookie = [cookies_for_url objectAtIndex:0];
  EXPECT_TRUE([input_cookie.name isEqualToString:output_cookie.name]);
  EXPECT_TRUE([input_cookie.value isEqualToString:output_cookie.value]);
  EXPECT_TRUE([input_cookie.domain isEqualToString:output_cookie.domain]);
}

TEST_F(NSHTTPSystemCookieStoreTest, DeleteCookies) {
  NSHTTPCookie* system_cookie1 = CreateCookie(@"a=b", test_cookie_url1_);
  [shared_store_ setCookie:system_cookie1];

  NSHTTPCookie* system_cookie2 = CreateCookie(@"x=d", test_cookie_url2_);
  store_->SetCookie(system_cookie2);

  NSHTTPCookie* system_cookie3 = CreateCookie(@"l=m", test_cookie_url3_);
  [shared_store_ setCookie:system_cookie3];

  EXPECT_EQ(3u, shared_store_.cookies.count);

  store_->DeleteCookie(system_cookie2);
  EXPECT_EQ(0u, [shared_store_ cookiesForURL:test_cookie_url2_].count);
  EXPECT_EQ(2u, shared_store_.cookies.count);

  store_->DeleteCookie(system_cookie1);
  EXPECT_EQ(0u, [shared_store_ cookiesForURL:test_cookie_url1_].count);
  EXPECT_EQ(1u, shared_store_.cookies.count);

  store_->DeleteCookie(system_cookie3);
  EXPECT_EQ(0u, shared_store_.cookies.count);
}

TEST_F(NSHTTPSystemCookieStoreTest, GetCookieAcceptPolicy) {
  EXPECT_EQ(shared_store_.cookieAcceptPolicy, store_->GetCookieAcceptPolicy());
  shared_store_.cookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
  EXPECT_EQ(shared_store_.cookieAcceptPolicy, store_->GetCookieAcceptPolicy());
  shared_store_.cookieAcceptPolicy = NSHTTPCookieAcceptPolicyAlways;
  EXPECT_EQ(shared_store_.cookieAcceptPolicy, store_->GetCookieAcceptPolicy());
}

}  // namespace net
