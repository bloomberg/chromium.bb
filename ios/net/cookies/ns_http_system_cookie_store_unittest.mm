// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/cookies/ns_http_system_cookie_store.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#import "ios/net/cookies/cookie_store_ios_test_util.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

namespace {
// Helper callbacks to be passed to SetCookieAsync/GetCookiesAsync.
class SystemCookiesCallback {
 public:
  SystemCookiesCallback()
      : did_run_with_cookies_(false), did_run_with_no_cookies_(false) {}
  // Returns if the callback has been run.
  bool did_run_with_cookies() { return did_run_with_cookies_; }
  bool did_run_with_no_cookies() { return did_run_with_no_cookies_; }

  // Returns the paremeter of the callback.
  NSArray<NSHTTPCookie*>* cookies() { return cookies_; }

  void RunWithCookies(NSArray<NSHTTPCookie*>* cookies) {
    ASSERT_FALSE(did_run_with_cookies_);
    did_run_with_cookies_ = true;
    cookies_ = cookies;
  }

  void RunWithNoCookies() {
    ASSERT_FALSE(did_run_with_no_cookies_);
    did_run_with_no_cookies_ = true;
  }

 private:
  bool did_run_with_cookies_;
  bool did_run_with_no_cookies_;
  NSArray<NSHTTPCookie*>* cookies_;
};
}  // namespace

// Test fixture to exercise net::NSHTTPSystemCookieStore created with
// |[NSHTTPCookieStorage sharedHTTPCookieStorage]|.
class NSHTTPSystemCookieStoreTest : public PlatformTest {
 public:
  NSHTTPSystemCookieStoreTest()
      : scoped_cookie_store_ios_client_(
            base::MakeUnique<TestCookieStoreIOSClient>()),
        shared_store_([NSHTTPCookieStorage sharedHTTPCookieStorage]),
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

  bool IsCookieSet(NSHTTPCookie* system_cookie, NSURL* url) {
    // Verify that cookie is set in system storage.
    NSHTTPCookie* result_cookie = nil;

    for (NSHTTPCookie* cookie in [shared_store_ cookiesForURL:url]) {
      if ([cookie.name isEqualToString:system_cookie.name]) {
        result_cookie = cookie;
        break;
      }
    }
    return [result_cookie.value isEqualToString:system_cookie.value];
  }

 protected:
  base::MessageLoop loop;
  ScopedTestingCookieStoreIOSClient scoped_cookie_store_ios_client_;
  NSHTTPCookieStorage* shared_store_;
  std::unique_ptr<net::NSHTTPSystemCookieStore> store_;
  NSURL* test_cookie_url1_;
  NSURL* test_cookie_url2_;
  NSURL* test_cookie_url3_;
};

TEST_F(NSHTTPSystemCookieStoreTest, GetCookieAcceptPolicy) {
  EXPECT_EQ(shared_store_.cookieAcceptPolicy, store_->GetCookieAcceptPolicy());
  shared_store_.cookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
  EXPECT_EQ(shared_store_.cookieAcceptPolicy, store_->GetCookieAcceptPolicy());
  shared_store_.cookieAcceptPolicy = NSHTTPCookieAcceptPolicyAlways;
  EXPECT_EQ(shared_store_.cookieAcceptPolicy, store_->GetCookieAcceptPolicy());
}

TEST_F(NSHTTPSystemCookieStoreTest, GetCookiesAsync) {
  NSMutableDictionary* input_cookies = [[NSMutableDictionary alloc] init];
  NSHTTPCookie* system_cookie = CreateCookie(@"a=b", test_cookie_url1_);
  [input_cookies setValue:system_cookie forKey:@"a"];
  store_->SetCookieAsync(system_cookie, /*optional_creation_time=*/nullptr,
                         SystemCookieStore::SystemCookieCallback());
  system_cookie = CreateCookie(@"x=d", test_cookie_url2_);
  [input_cookies setValue:system_cookie forKey:@"x"];
  store_->SetCookieAsync(system_cookie, /*optional_creation_time=*/nullptr,
                         SystemCookieStore::SystemCookieCallback());
  system_cookie = CreateCookie(@"l=m", test_cookie_url3_);
  [input_cookies setValue:system_cookie forKey:@"l"];
  store_->SetCookieAsync(system_cookie, /*optional_creation_time=*/nullptr,
                         SystemCookieStore::SystemCookieCallback());
  base::RunLoop().RunUntilIdle();

  // Test GetCookieForURLAsync.
  NSHTTPCookie* input_cookie = [input_cookies valueForKey:@"a"];
  SystemCookiesCallback callback_for_url;
  store_->GetCookiesForURLAsync(
      GURLWithNSURL(test_cookie_url1_),
      base::BindOnce(&SystemCookiesCallback::RunWithCookies,
                     base::Unretained(&callback_for_url)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_for_url.did_run_with_cookies());
  EXPECT_EQ(1u, callback_for_url.cookies().count);
  NSHTTPCookie* result_cookie = callback_for_url.cookies()[0];
  EXPECT_TRUE([input_cookie.name isEqualToString:result_cookie.name]);
  EXPECT_TRUE([input_cookie.value isEqualToString:result_cookie.value]);

  // Test GetAllCookies
  SystemCookiesCallback callback_all_cookies;
  store_->GetAllCookiesAsync(
      base::BindOnce(&SystemCookiesCallback::RunWithCookies,
                     base::Unretained(&callback_all_cookies)));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_all_cookies.did_run_with_cookies());
  NSArray<NSHTTPCookie*>* result_cookies = callback_all_cookies.cookies();
  EXPECT_EQ(3u, result_cookies.count);
  for (NSHTTPCookie* cookie in result_cookies) {
    NSHTTPCookie* existing_cookie = [input_cookies valueForKey:cookie.name];
    EXPECT_TRUE(existing_cookie);
    EXPECT_TRUE([existing_cookie.name isEqualToString:cookie.name]);
    EXPECT_TRUE([existing_cookie.value isEqualToString:cookie.value]);
    EXPECT_TRUE([existing_cookie.domain isEqualToString:cookie.domain]);
  }
}

TEST_F(NSHTTPSystemCookieStoreTest, SetCookieAsync) {
  NSHTTPCookie* system_cookie = CreateCookie(@"a=b", test_cookie_url1_);
  SystemCookiesCallback callback;
  store_->SetCookieAsync(
      system_cookie, /*optional_creation_time=*/nullptr,
      base::BindOnce(&SystemCookiesCallback::RunWithNoCookies,
                     base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  // verify callback.
  EXPECT_TRUE(callback.did_run_with_no_cookies());
  EXPECT_TRUE(IsCookieSet(system_cookie, test_cookie_url1_));
}

TEST_F(NSHTTPSystemCookieStoreTest, DeleteCookiesAsync) {
  NSHTTPCookie* system_cookie1 = CreateCookie(@"a=b", test_cookie_url1_);
  store_->SetCookieAsync(system_cookie1, /*optional_creation_time=*/nullptr,
                         SystemCookieStore::SystemCookieCallback());
  NSHTTPCookie* system_cookie2 = CreateCookie(@"x=d", test_cookie_url2_);
  store_->SetCookieAsync(system_cookie2, /*optional_creation_time=*/nullptr,
                         SystemCookieStore::SystemCookieCallback());
  NSHTTPCookie* system_cookie3 = CreateCookie(@"l=m", test_cookie_url3_);
  store_->SetCookieAsync(system_cookie3, /*optional_creation_time=*/nullptr,
                         SystemCookieStore::SystemCookieCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, shared_store_.cookies.count);

  SystemCookiesCallback callback;
  EXPECT_EQ(1u, [shared_store_ cookiesForURL:test_cookie_url2_].count);
  store_->DeleteCookieAsync(
      system_cookie2, base::BindOnce(&SystemCookiesCallback::RunWithNoCookies,
                                     base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback.did_run_with_no_cookies());
  EXPECT_EQ(0u, [shared_store_ cookiesForURL:test_cookie_url2_].count);
  EXPECT_EQ(2u, shared_store_.cookies.count);

  store_->DeleteCookieAsync(system_cookie1,
                            SystemCookieStore::SystemCookieCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, [shared_store_ cookiesForURL:test_cookie_url1_].count);
  EXPECT_EQ(1u, shared_store_.cookies.count);

  store_->DeleteCookieAsync(system_cookie3,
                            SystemCookieStore::SystemCookieCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, shared_store_.cookies.count);
}

TEST_F(NSHTTPSystemCookieStoreTest, ClearCookiesAsync) {
  store_->SetCookieAsync(CreateCookie(@"a=b", test_cookie_url1_),
                         /*optional_creation_time=*/nullptr,
                         SystemCookieStore::SystemCookieCallback());
  store_->SetCookieAsync(CreateCookie(@"x=d", test_cookie_url2_),
                         /*optional_creation_time=*/nullptr,
                         SystemCookieStore::SystemCookieCallback());

  SystemCookiesCallback callback;
  EXPECT_EQ(2u, shared_store_.cookies.count);
  store_->ClearStoreAsync(base::BindOnce(
      &SystemCookiesCallback::RunWithNoCookies, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback.did_run_with_no_cookies());
  EXPECT_EQ(0u, shared_store_.cookies.count);
}

}  // namespace net
