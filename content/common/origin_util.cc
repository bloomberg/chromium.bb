// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/origin_util.h"

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "content/common/url_schemes.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace content {

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

  if (base::ContainsValue(url::GetSecureSchemes(), url.scheme()))
    return true;

  if (base::ContainsValue(GetSecureOrigins(), url.GetOrigin())) {
    return true;
  }

  return false;
}

bool OriginCanAccessServiceWorkers(const GURL& url) {
  if (url.SchemeIsHTTPOrHTTPS() && IsOriginSecure(url))
    return true;

  if (base::ContainsValue(GetServiceWorkerSchemes(), url.scheme())) {
    return true;
  }

  return false;
}

}  // namespace content
