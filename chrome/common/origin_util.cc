// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/origin_util.h"

#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "net/base/net_util.h"
#include "url/gurl.h"

bool IsOriginSecure(const GURL& url) {
  if (url.SchemeUsesTLS() || url.SchemeIsFile())
    return true;

  if (url.SchemeIsFileSystem() && url.inner_url() &&
      IsOriginSecure(*url.inner_url())) {
    return true;
  }

  std::string hostname = url.HostNoBrackets();
  if (net::IsLocalhost(hostname))
    return true;

  std::string scheme = url.scheme();
  if (scheme == content::kChromeUIScheme ||
      scheme == extensions::kExtensionScheme ||
      scheme == extensions::kExtensionResourceScheme) {
    return true;
  }

  return false;
}
