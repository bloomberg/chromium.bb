// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_helpers.h"

#include "url/gurl.h"

#if !defined(OS_CHROMEOS) && !defined(OS_IOS)
#include "chrome/browser/signin/signin_manager.h"
#include "google_apis/gaia/gaia_urls.h"
#endif

namespace policy {

bool SkipBlacklistForURL(const GURL& url) {
#if defined(OS_CHROMEOS) || defined(OS_IOS)
  return false;
#else
  static const char kServiceLoginAuth[] = "/ServiceLoginAuth";

  // Whitelist all the signin flow URLs flagged by the SigninManager.
  if (SigninManager::IsWebBasedSigninFlowURL(url))
    return true;

  // Additionally whitelist /ServiceLoginAuth.
  if (url.GetOrigin() != GaiaUrls::GetInstance()->gaia_url().GetOrigin())
    return false;

  return url.path() == kServiceLoginAuth;
#endif
}

}  // namespace policy
