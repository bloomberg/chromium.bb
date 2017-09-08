// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/cookies/ns_http_system_cookie_store.h"

#import "ios/net/cookies/cookie_creation_time_manager.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

NSHTTPSystemCookieStore::NSHTTPSystemCookieStore()
    : cookie_store_([NSHTTPCookieStorage sharedHTTPCookieStorage]) {}

NSHTTPSystemCookieStore::NSHTTPSystemCookieStore(
    NSHTTPCookieStorage* cookie_store)
    : cookie_store_(cookie_store) {}

NSHTTPSystemCookieStore::~NSHTTPSystemCookieStore() = default;

#pragma mark -
#pragma mark SystemCookieStore methods

NSArray* NSHTTPSystemCookieStore::GetCookiesForURL(
    const GURL& url,
    CookieCreationTimeManager* manager) {
  NSArray* cookies = [cookie_store_ cookiesForURL:NSURLWithGURL(url)];
  if (!manager)
    return cookies;
  // Sort cookies by decreasing path length, then creation time, as per
  // RFC6265.
  return [cookies sortedArrayUsingFunction:CompareCookies context:manager];
}

NSArray* NSHTTPSystemCookieStore::GetAllCookies(
    CookieCreationTimeManager* manager) {
  NSArray* cookies = cookie_store_.cookies;
  if (!manager)
    return cookies;
  // Sort cookies by decreasing path length, then creation time, as per
  // RFC6265.
  return [cookies sortedArrayUsingFunction:CompareCookies context:manager];
}

void NSHTTPSystemCookieStore::DeleteCookie(NSHTTPCookie* cookie) {
  [cookie_store_ deleteCookie:cookie];
}

void NSHTTPSystemCookieStore::SetCookie(NSHTTPCookie* cookie) {
  [cookie_store_ setCookie:cookie];
}

void NSHTTPSystemCookieStore::ClearStore() {
  NSArray* copy = [cookie_store_.cookies copy];
  for (NSHTTPCookie* cookie in copy)
    [cookie_store_ deleteCookie:cookie];
  DCHECK_EQ(0u, cookie_store_.cookies.count);
}

NSHTTPCookieAcceptPolicy NSHTTPSystemCookieStore::GetCookieAcceptPolicy() {
  return [cookie_store_ cookieAcceptPolicy];
}

}  // namespace net
