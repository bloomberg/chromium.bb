// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/url_scheme_util.h"

#import <Foundation/Foundation.h>

#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

bool UrlHasWebScheme(const GURL& url) {
  return url.SchemeIs(url::kHttpScheme) ||
         url.SchemeIs(url::kHttpsScheme) ||
         url.SchemeIs(url::kDataScheme);
}

bool UrlHasWebScheme(NSURL* url) {
  NSString* scheme = [url scheme];
  if (![scheme length])
    return false;

  // Use the GURL implementation, but with a scheme-only URL to avoid
  // unnecessary parsing in GURL construction.
  NSString* schemeURLString = [scheme stringByAppendingString:@":"];
  GURL gurl([schemeURLString UTF8String]);
  return UrlHasWebScheme(gurl);
}

}  // namespace web
