// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/url_utils.h"

#include "build/build_config.h"
#include "content/common/url_schemes.h"
#include "content/public/common/url_constants.h"

namespace content {

bool HasWebUIScheme(const GURL& url) {
  return url.SchemeIs(kChromeDevToolsScheme) ||
         url.SchemeIs(kChromeUIScheme);
}

bool IsSavableURL(const GURL& url) {
  for (auto& scheme : GetSavableSchemes()) {
    if (url.SchemeIs(scheme))
      return true;
  }
  return false;
}

}  // namespace content
