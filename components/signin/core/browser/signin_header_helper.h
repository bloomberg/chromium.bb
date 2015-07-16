// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_HEADER_HELPER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_HEADER_HELPER_H_

#include <string>

namespace content_settings {
class CookieSettings;
}

namespace net {
class URLRequest;
}

class GURL;

namespace signin {

// Profile mode flags.
enum ProfileMode {
  PROFILE_MODE_DEFAULT = 0,
  // Incognito mode disabled by enterprise policy or by parental controls.
  PROFILE_MODE_INCOGNITO_DISABLED = 1 << 0,
  // Adding account disabled in the Android-for-EDU mode.
  PROFILE_MODE_ADD_ACCOUNT_DISABLED = 1 << 1
};

// Returns true if signin cookies are allowed.
bool SettingsAllowSigninCookies(
    content_settings::CookieSettings* cookie_settings);

// Adds X-Chrome-Connected header to all Gaia requests from a connected profile,
// with the exception of requests from gaia webview.
// Returns true if the account management header was added to the request.
bool AppendMirrorRequestHeaderIfPossible(
    net::URLRequest* request,
    const GURL& redirect_url,
    const std::string& account_id,
    content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_HEADER_HELPER_H_
