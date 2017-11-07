// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_COOKIES_WK_HTTP_SYSTEM_COOKIE_STORE_H_
#define IOS_WEB_NET_COOKIES_WK_HTTP_SYSTEM_COOKIE_STORE_H_

#import <Foundation/Foundation.h>
#import <WebKit/Webkit.h>

#import "ios/net/cookies/system_cookie_store.h"

#if defined(__IPHONE_11_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0)

namespace web {

// This class is an implementation of SystemCookieStore, WKHTTPSystemCookieStore
// uses WKHTTPCookieStore as the underlying system cookie store.
class API_AVAILABLE(ios(11.0)) WKHTTPSystemCookieStore
    : public net::SystemCookieStore {
 public:
  explicit WKHTTPSystemCookieStore(WKHTTPCookieStore* cookie_store);

  ~WKHTTPSystemCookieStore() override;

  void GetCookiesForURLAsync(
      const GURL& url,
      net::SystemCookieStore::SystemCookieCallbackForCookies callback) override;

  void GetAllCookiesAsync(
      net::SystemCookieStore::SystemCookieCallbackForCookies callback) override;

  void DeleteCookieAsync(
      NSHTTPCookie* cookie,
      net::SystemCookieStore::SystemCookieCallback callback) override;

  void SetCookieAsync(
      NSHTTPCookie* cookie,
      const base::Time* optional_creation_time,
      net::SystemCookieStore::SystemCookieCallback callback) override;

  void ClearStoreAsync(
      net::SystemCookieStore::SystemCookieCallback callback) override;

  NSHTTPCookieAcceptPolicy GetCookieAcceptPolicy() override;

 private:
  // Run |callback| on |cookies| after sorting them  as per RFC6265.
  void RunSystemCookieCallbackForCookies(
      net::SystemCookieStore::SystemCookieCallbackForCookies callback,
      NSArray<NSHTTPCookie*>* cookies);

  WKHTTPCookieStore* cookie_store_;

  DISALLOW_COPY_AND_ASSIGN(WKHTTPSystemCookieStore);
};

}  // namespace web

#endif

#endif  // IOS_WEB_NET_COOKIES_WK_HTTP_SYSTEM_COOKIE_STORE_H_
