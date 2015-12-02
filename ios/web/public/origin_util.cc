// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/origin_util.h"

#include "net/base/net_util.h"
#include "url/gurl.h"

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

}  // namespace web
