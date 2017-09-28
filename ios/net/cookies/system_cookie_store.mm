// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/cookies/system_cookie_store.h"

#include "base/time/time.h"
#import "ios/net/cookies/cookie_creation_time_manager.h"
#include "ios/net/ios_net_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

SystemCookieStore::~SystemCookieStore() = default;

NSArray* SystemCookieStore::GetCookiesForURL(const GURL& url) {
  return GetCookiesForURL(url, /* manager = */ nullptr);
}

NSArray* SystemCookieStore::GetAllCookies() {
  return GetAllCookies(/* manager = */ nullptr);
}

// protected static
NSInteger SystemCookieStore::CompareCookies(id a, id b, void* context) {
  NSHTTPCookie* cookie_a = static_cast<NSHTTPCookie*>(a);
  NSHTTPCookie* cookie_b = static_cast<NSHTTPCookie*>(b);
  // Compare path lengths first.
  NSUInteger path_length_a = cookie_a.path.length;
  NSUInteger path_length_b = cookie_b.path.length;
  if (path_length_a < path_length_b)
    return NSOrderedDescending;
  if (path_length_b < path_length_a)
    return NSOrderedAscending;

  // Compare creation times.
  CookieCreationTimeManager* manager =
      static_cast<CookieCreationTimeManager*>(context);
  base::Time created_a = manager->GetCreationTime(cookie_a);
  base::Time created_b = manager->GetCreationTime(cookie_b);
#if !BUILDFLAG(CRONET_BUILD)
  // CookieCreationTimeManager is returning creation times that are null.
  // Since in Cronet, the cookie store is recreated on startup, let's suppress
  // this warning for now.
  DLOG_IF(ERROR, created_a.is_null() || created_b.is_null())
      << "Cookie without creation date";
#endif
  if (created_a < created_b)
    return NSOrderedAscending;
  if (created_a > created_b)
    return NSOrderedDescending;

  return NSOrderedSame;
}

}  // namespace net
