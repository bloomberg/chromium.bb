// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_header_helper.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/google/core/browser/google_util.h"
#include "components/signin/core/browser/chrome_connected_header_helper.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"

#if !defined(OS_IOS) && !defined(OS_ANDROID)
#include "components/signin/core/browser/dice_header_helper.h"
#endif

namespace signin {

extern const char kChromeConnectedHeader[] = "X-Chrome-Connected";
extern const char kDiceRequestHeader[] = "X-Chrome-ID-Consistency-Request";

ManageAccountsParams::ManageAccountsParams()
    : service_type(GAIA_SERVICE_TYPE_NONE),
      email(""),
      is_saml(false),
      continue_url(""),
      is_same_tab(false) {
#if !defined(OS_IOS)
  child_id = 0;
  route_id = 0;
#endif  // !defined(OS_IOS)
}

ManageAccountsParams::ManageAccountsParams(const ManageAccountsParams& other) =
    default;

DiceResponseParams::DiceResponseParams() : user_intention(DiceAction::NONE) {}

DiceResponseParams::~DiceResponseParams() {}

DiceResponseParams::DiceResponseParams(const DiceResponseParams& other) =
    default;

bool SettingsAllowSigninCookies(
    const content_settings::CookieSettings* cookie_settings) {
  GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  GURL google_url = GaiaUrls::GetInstance()->google_url();
  return cookie_settings &&
         cookie_settings->IsCookieAccessAllowed(gaia_url, gaia_url) &&
         cookie_settings->IsCookieAccessAllowed(google_url, google_url);
}

std::string BuildMirrorRequestCookieIfPossible(
    const GURL& url,
    const std::string& account_id,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask) {
  return signin::ChromeConnectedHeaderHelper::BuildRequestCookieIfPossible(
      url, account_id, cookie_settings, profile_mode_mask);
}

bool SigninHeaderHelper::AppendOrRemoveRequestHeader(
    net::URLRequest* request,
    const GURL& redirect_url,
    const char* header_name,
    const std::string& header_value) {
  if (header_value.empty()) {
    // If the request is being redirected, and it has the account consistency
    // header, and current url is a Google URL, and the redirected one is not,
    // remove the header.
    if (!redirect_url.is_empty() &&
        request->extra_request_headers().HasHeader(header_name) &&
        IsUrlEligibleForRequestHeader(request->url()) &&
        !IsUrlEligibleForRequestHeader(redirect_url)) {
      request->RemoveRequestHeaderByName(header_name);
    }
    return false;
  }
  request->SetExtraRequestHeaderByName(header_name, header_value, false);
  return true;
}

// static
SigninHeaderHelper::ResponseHeaderDictionary
SigninHeaderHelper::ParseAccountConsistencyResponseHeader(
    const std::string& header_value) {
  ResponseHeaderDictionary dictionary;
  for (const base::StringPiece& field :
       base::SplitStringPiece(header_value, ",", base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    size_t delim = field.find_first_of('=');
    if (delim == std::string::npos) {
      DLOG(WARNING) << "Unexpected Gaia header field '" << field << "'.";
      continue;
    }
    dictionary[field.substr(0, delim).as_string()] = net::UnescapeURLComponent(
        field.substr(delim + 1).as_string(),
        net::UnescapeRule::PATH_SEPARATORS |
            net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
  }
  return dictionary;
}

bool SigninHeaderHelper::ShouldBuildRequestHeader(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings) {
  // If signin cookies are not allowed, don't add the header.
  if (!SettingsAllowSigninCookies(cookie_settings))
    return false;

  // Check if url is eligible for the header.
  if (!IsUrlEligibleForRequestHeader(url))
    return false;

  return true;
}

void AppendOrRemoveAccountConsistentyRequestHeader(
    net::URLRequest* request,
    const GURL& redirect_url,
    const std::string& account_id,
    bool sync_enabled,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask) {
  const GURL& url = redirect_url.is_empty() ? request->url() : redirect_url;
// Dice is not enabled on mobile.
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  DiceHeaderHelper dice_helper;
  std::string dice_header_value;
  if (dice_helper.ShouldBuildRequestHeader(url, cookie_settings)) {
    dice_header_value =
        dice_helper.BuildRequestHeader(account_id, sync_enabled);
  }
  dice_helper.AppendOrRemoveRequestHeader(
      request, redirect_url, kDiceRequestHeader, dice_header_value);
#endif

  ChromeConnectedHeaderHelper chrome_connected_helper;
  std::string chrome_connected_header_value;
  if (chrome_connected_helper.ShouldBuildRequestHeader(url, cookie_settings)) {
    chrome_connected_header_value = chrome_connected_helper.BuildRequestHeader(
        true /* is_header_request */, url, account_id, profile_mode_mask);
  }
  chrome_connected_helper.AppendOrRemoveRequestHeader(
      request, redirect_url, kChromeConnectedHeader,
      chrome_connected_header_value);
}

ManageAccountsParams BuildManageAccountsParams(
    const std::string& header_value) {
  return ChromeConnectedHeaderHelper::BuildManageAccountsParams(header_value);
}

#if !defined(OS_IOS) && !defined(OS_ANDROID)
DiceResponseParams BuildDiceResponseParams(const std::string& header_value) {
  return DiceHeaderHelper::BuildDiceResponseParams(header_value);
}
#endif

}  // namespace signin
