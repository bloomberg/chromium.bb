// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookies/wk_http_system_cookie_store.h"

#include "base/bind.h"
#include "base/ios/callback_counter.h"
#include "base/ios/ios_util.h"
#import "base/mac/bind_objc_block.h"
#import "ios/net/cookies/cookie_creation_time_manager.h"
#include "ios/net/cookies/system_cookie_util.h"
#include "ios/web/public/web_thread.h"
#import "net/base/mac/url_conversions.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"

#if defined(__IPHONE_11_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0)

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// private
// Posts |callback| to run on IO Thread, this is needed because
// WKHTTPCookieStore executes callbacks on the main thread, while
// SystemCookieStore should operate on IO thread.
void RunSystemCookieCallbackOnIOThread(base::OnceClosure callback) {
  if (callback.is_null())
    return;
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE, std::move(callback));
}

// private
// Returns wether |cookie| should be included for queries about |url|.
// To include |cookie| for |url|, all these conditions need to be met:
//   1- If the cookie is secure the URL needs to be secure.
//   2- |url| domain need to match the cookie domain.
//   3- |cookie| url path need to be on the path of the given |url|.
bool ShouldIncludeForRequestUrl(NSHTTPCookie* cookie, const GURL& url) {
  // CanonicalCookies already implements cookie selection for URLs, so instead
  // of rewriting the checks here, the function converts the NSHTTPCookie to
  // canonical cookie and provide it with dummy CookieOption, so when iOS starts
  // to support cookieOptions this function can be modified to support that.
  net::CanonicalCookie canonical_cookie =
      net::CanonicalCookieFromSystemCookie(cookie, base::Time());
  net::CookieOptions options;
  options.set_include_httponly();
  return canonical_cookie.IncludeForRequestURL(url, options);
}

WKHTTPSystemCookieStore::WKHTTPSystemCookieStore(
    WKHTTPCookieStore* cookie_store)
    : cookie_store_(cookie_store) {}

WKHTTPSystemCookieStore::~WKHTTPSystemCookieStore() = default;

#pragma mark -
#pragma mark SystemCookieStore methods

void WKHTTPSystemCookieStore::GetCookiesForURLAsync(
    const GURL& url,
    SystemCookieCallbackForCookies callback) {
  __block SystemCookieCallbackForCookies shared_callback = std::move(callback);
  GURL block_url = url;
  [cookie_store_ getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
    NSMutableArray* result = [NSMutableArray array];
    for (NSHTTPCookie* cookie in cookies) {
      if (ShouldIncludeForRequestUrl(cookie, block_url)) {
        [result addObject:cookie];
      }
    }
    RunSystemCookieCallbackForCookies(std::move(shared_callback), result);
  }];
}

void WKHTTPSystemCookieStore::GetAllCookiesAsync(
    SystemCookieCallbackForCookies callback) {
  __block SystemCookieCallbackForCookies shared_callback = std::move(callback);
  [cookie_store_ getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {
    RunSystemCookieCallbackForCookies(std::move(shared_callback), cookies);
  }];
}

void WKHTTPSystemCookieStore::DeleteCookieAsync(NSHTTPCookie* cookie,
                                                SystemCookieCallback callback) {
  __block SystemCookieCallback shared_callback = std::move(callback);
  [cookie_store_ deleteCookie:cookie
            completionHandler:^{
              creation_time_manager_->DeleteCreationTime(cookie);
              RunSystemCookieCallbackOnIOThread(std::move(shared_callback));
            }];
}
void WKHTTPSystemCookieStore::SetCookieAsync(
    NSHTTPCookie* cookie,
    const base::Time* optional_creation_time,
    SystemCookieCallback callback) {
  __block SystemCookieCallback shared_callback = std::move(callback);
  [cookie_store_ setCookie:cookie
         completionHandler:^{
           // Set creation time as soon as possible
           base::Time cookie_time = base::Time::Now();
           if (optional_creation_time && !optional_creation_time->is_null())
             cookie_time = *optional_creation_time;

           creation_time_manager_->SetCreationTime(
               cookie,
               creation_time_manager_->MakeUniqueCreationTime(cookie_time));
           RunSystemCookieCallbackOnIOThread(std::move(shared_callback));
         }];
}

void WKHTTPSystemCookieStore::ClearStoreAsync(SystemCookieCallback callback) {
  __block SystemCookieCallback shared_callback = std::move(callback);
  [cookie_store_ getAllCookies:^(NSArray<NSHTTPCookie*>* cookies) {

    scoped_refptr<CallbackCounter> callback_counter =
        new CallbackCounter(base::BindBlockArc(^{
          creation_time_manager_->Clear();
          RunSystemCookieCallbackOnIOThread(std::move(shared_callback));
        }));
    // Add callback for each cookie
    callback_counter->IncrementCount(cookies.count);
    for (NSHTTPCookie* cookie in cookies) {
      [cookie_store_ deleteCookie:cookie
                completionHandler:^{
                  callback_counter->DecrementCount();
                }];
    }
  }];
}

NSHTTPCookieAcceptPolicy WKHTTPSystemCookieStore::GetCookieAcceptPolicy() {
  // TODO(crbug.com/759226): Make sure there is no other way to return
  // WKHTTPCookieStore Specific cookieAcceptPolicy.
  return [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];
}

#pragma mark private methods

void WKHTTPSystemCookieStore::RunSystemCookieCallbackForCookies(
    net::SystemCookieStore::SystemCookieCallbackForCookies callback,
    NSArray<NSHTTPCookie*>* cookies) {
  if (callback.is_null())
    return;
  NSArray* result = [static_cast<NSArray*>(cookies)
      sortedArrayUsingFunction:CompareCookies
                       context:creation_time_manager_.get()];
  RunSystemCookieCallbackOnIOThread(
      base::BindOnce(std::move(callback), result));
}

}  // namespace web

#endif  // defined(__IPHONE_11_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >=
        //__IPHONE_11_0)
