// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_header_helper.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/google/core/browser/google_util.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace {

const char kChromeConnectedHeader[] = "X-Chrome-Connected";
const char kEnableAccountConsistencyAttrName[] = "enable_account_consistency";
const char kGaiaIdAttrName[] = "id";
const char kProfileModeAttrName[] = "mode";

bool IsDriveOrigin(const GURL& url) {
  if (!url.SchemeIsCryptographic())
    return false;

  const GURL kGoogleDriveURL("https://drive.google.com");
  const GURL kGoogleDocsURL("https://docs.google.com");
  return url == kGoogleDriveURL || url == kGoogleDocsURL;
}

}  // namespace

namespace signin {

bool SettingsAllowSigninCookies(
    content_settings::CookieSettings* cookie_settings) {
  GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  GURL google_url = GaiaUrls::GetInstance()->google_url();
  return cookie_settings &&
         cookie_settings->IsSettingCookieAllowed(gaia_url, gaia_url) &&
         cookie_settings->IsSettingCookieAllowed(google_url, google_url);
}

bool AppendMirrorRequestHeaderIfPossible(
    net::URLRequest* request,
    const GURL& redirect_url,
    const std::string& account_id,
    content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask) {
  if (account_id.empty())
    return false;

  // If signin cookies are not allowed, don't add the header.
  if (SettingsAllowSigninCookies(cookie_settings)) {
    return false;
  }

  // Only set the header for Drive and Gaia always, and other Google properties
  // if account consistency is enabled.
  // Vasquette, which is integrated with most Google properties, needs the
  // header to redirect certain user actions to Chrome native UI. Drive and Gaia
  // need the header to tell if the current user is connected. The drive path is
  // a temporary workaround until the more generic chrome.principals API is
  // available.
  const GURL& url = redirect_url.is_empty() ? request->url() : redirect_url;
  GURL origin(url.GetOrigin());
  bool is_enable_account_consistency = switches::IsEnableAccountConsistency();
  bool is_google_url = is_enable_account_consistency &&
                       (google_util::IsGoogleDomainUrl(
                            url, google_util::ALLOW_SUBDOMAIN,
                            google_util::DISALLOW_NON_STANDARD_PORTS) ||
                        google_util::IsYoutubeDomainUrl(
                            url, google_util::ALLOW_SUBDOMAIN,
                            google_util::DISALLOW_NON_STANDARD_PORTS));
  if (!is_google_url && !IsDriveOrigin(origin) &&
      !gaia::IsGaiaSignonRealm(origin)) {
    return false;
  }

  std::string header_value(base::StringPrintf(
      "%s=%s,%s=%s,%s=%s", kGaiaIdAttrName, account_id.c_str(),
      kProfileModeAttrName, base::IntToString(profile_mode_mask).c_str(),
      kEnableAccountConsistencyAttrName,
      is_enable_account_consistency ? "true" : "false"));
  request->SetExtraRequestHeaderByName(kChromeConnectedHeader, header_value,
                                       false);
  return true;
}

}  // namespace signin
