// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_helpers.h"

#include "net/base/net_errors.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_IOS)
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/gaia_urls.h"
#endif

namespace policy {

bool OverrideBlacklistForURL(const GURL& url, bool* block, int* reason) {
#if defined(OS_CHROMEOS)
  // On ChromeOS browsing is only allowed once OOBE has completed. Therefore all
  // requests are blocked until this condition is met.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kOobeGuestSession)) {
    if (!url.SchemeIs("chrome") && !url.SchemeIs("chrome-extension")) {
      *reason = net::ERR_BLOCKED_ENROLLMENT_CHECK_PENDING;
      *block = true;
      return true;
    }
  }
  return false;
#elif defined(OS_IOS)
  return false;
#else
  static const char kServiceLoginAuth[] = "/ServiceLoginAuth";

  *block = false;
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
