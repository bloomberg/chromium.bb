// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_SYSTEM_COOKIE_STORE_H_
#define IOS_NET_COOKIES_SYSTEM_COOKIE_STORE_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"

class GURL;

namespace net {

class CookieCreationTimeManager;

// SystemCookieStore is an abstract class that should be implemented for every
// type of system store (WKHTTPCookieStore, NSHTTPCookieStorage, ..), its main
// purpose is to interact with the system cookie store, and let the caller use
// it directly without caring about the type of the underlying cookie store.
class SystemCookieStore {
 public:
  virtual ~SystemCookieStore();

  // Returns cookies for specific URL without sorting.
  NSArray* GetCookiesForURL(const GURL& url);

  // Returns all cookies for a specific |url| from the internal cookie store.
  // If |manager| is provided, use it to sort cookies, as per RFC6265.
  virtual NSArray* GetCookiesForURL(const GURL& url,
                                    CookieCreationTimeManager* manager) = 0;

  // Returns all cookies from the internal http cookie store without sorting.
  NSArray* GetAllCookies();

  // Returns all cookies from the internal http cookie store.
  // If |manager| is provided, use it to sort cookies, as per RFC6265.
  virtual NSArray* GetAllCookies(CookieCreationTimeManager* manager) = 0;

  // Delete a specific cookie from the internal http cookie store.
  virtual void DeleteCookie(NSHTTPCookie* cookie) = 0;

  // Set a specific cookie to the internal http cookie store.
  virtual void SetCookie(NSHTTPCookie* cookie) = 0;

  // Delete all cookies from the internal http cookie store.
  virtual void ClearStore() = 0;

  // Returns the Cookie Accept policy for the internal cookie store.
  virtual NSHTTPCookieAcceptPolicy GetCookieAcceptPolicy() = 0;

 protected:
  // Compares cookies based on the path lengths and the creation times, as per
  // RFC6265.
  static NSInteger CompareCookies(id a, id b, void* context);
};

}  // namespace net

#endif  // IOS_NET_COOKIES_SYSTEM_COOKIE_STORE_H_
