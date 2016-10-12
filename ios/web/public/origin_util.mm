// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/origin_util.h"

#import <WebKit/WebKit.h>

#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

bool IsOriginSecure(const GURL& url) {
  if (url.SchemeIsCryptographic() || url.SchemeIsFile())
    return true;

  if (url.SchemeIsFileSystem() && url.inner_url() &&
      IsOriginSecure(*url.inner_url())) {
    return true;
  }

  std::string hostname = url.HostNoBrackets();
  if (net::IsLocalhost(hostname))
    return true;

  return false;
}

GURL GURLOriginWithWKSecurityOrigin(WKSecurityOrigin* origin) {
  if (!origin)
    return GURL();

  url::SchemeHostPort origin_tuple(base::SysNSStringToUTF8(origin.protocol),
                                   base::SysNSStringToUTF8(origin.host),
                                   base::checked_cast<uint16_t>(origin.port));
  return origin_tuple.GetURL();
}

}  // namespace web
