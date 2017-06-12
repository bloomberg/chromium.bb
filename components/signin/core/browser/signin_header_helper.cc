// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_header_helper.h"

#include <stddef.h>
#include <map>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/google/core/browser/google_util.h"
#include "components/signin/core/browser/chrome_connected_header_helper.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/escape.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

#if !defined(OS_IOS) && !defined(OS_ANDROID)
#include "components/signin/core/browser/dice_header_helper.h"
#endif

namespace signin {

namespace {

// Dictionary of fields in a mirror response header.
typedef std::map<std::string, std::string> MirrorResponseHeaderDictionary;

const char kChromeManageAccountsHeader[] = "X-Chrome-Manage-Accounts";
const char kContinueUrlAttrName[] = "continue_url";
const char kEmailAttrName[] = "email";
const char kIsSameTabAttrName[] = "is_same_tab";
const char kIsSamlAttrName[] = "is_saml";
const char kServiceTypeAttrName[] = "action";

// Determines the service type that has been passed from Gaia in the header.
GAIAServiceType GetGAIAServiceTypeFromHeader(const std::string& header_value) {
  if (header_value == "SIGNOUT")
    return GAIA_SERVICE_TYPE_SIGNOUT;
  else if (header_value == "INCOGNITO")
    return GAIA_SERVICE_TYPE_INCOGNITO;
  else if (header_value == "ADDSESSION")
    return GAIA_SERVICE_TYPE_ADDSESSION;
  else if (header_value == "REAUTH")
    return GAIA_SERVICE_TYPE_REAUTH;
  else if (header_value == "SIGNUP")
    return GAIA_SERVICE_TYPE_SIGNUP;
  else if (header_value == "DEFAULT")
    return GAIA_SERVICE_TYPE_DEFAULT;
  else
    return GAIA_SERVICE_TYPE_NONE;
}

// Parses the mirror response header. Its expected format is
// "key1=value1,key2=value2,...".
MirrorResponseHeaderDictionary ParseMirrorResponseHeader(
    const std::string& header_value) {
  MirrorResponseHeaderDictionary dictionary;
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

}  // namespace

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
    const char* header_name,
    const GURL& redirect_url,
    const std::string& account_id,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask) {
  const GURL& url = redirect_url.is_empty() ? request->url() : redirect_url;
  std::string header_value = BuildRequestHeaderIfPossible(
      true /* is_header_request */, url, account_id, cookie_settings,
      profile_mode_mask);

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

std::string SigninHeaderHelper::BuildRequestHeaderIfPossible(
    bool is_header_request,
    const GURL& url,
    const std::string& account_id,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask) {
  // If signin cookies are not allowed, don't add the header.
  if (!SettingsAllowSigninCookies(cookie_settings))
    return std::string();

  // Check if url is eligible for the header.
  if (!IsUrlEligibleForRequestHeader(url))
    return std::string();

  return BuildRequestHeader(is_header_request, url, account_id,
                            profile_mode_mask);
}

void AppendOrRemoveAccountConsistentyRequestHeader(
    net::URLRequest* request,
    const GURL& redirect_url,
    const std::string& account_id,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask) {
// Dice is not enabled on mobile.
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  DiceHeaderHelper dice_helper;
  dice_helper.AppendOrRemoveRequestHeader(request, kDiceRequestHeader,
                                          redirect_url, account_id,
                                          cookie_settings, profile_mode_mask);
#endif

  ChromeConnectedHeaderHelper chrome_connected_helper;
  chrome_connected_helper.AppendOrRemoveRequestHeader(
      request, kChromeConnectedHeader, redirect_url, account_id,
      cookie_settings, profile_mode_mask);
}

ManageAccountsParams BuildManageAccountsParams(
    const std::string& header_value) {
  ManageAccountsParams params;
  MirrorResponseHeaderDictionary header_dictionary =
      ParseMirrorResponseHeader(header_value);
  MirrorResponseHeaderDictionary::const_iterator it = header_dictionary.begin();
  for (; it != header_dictionary.end(); ++it) {
    const std::string key_name(it->first);
    if (key_name == kServiceTypeAttrName) {
      params.service_type =
          GetGAIAServiceTypeFromHeader(header_dictionary[kServiceTypeAttrName]);
    } else if (key_name == kEmailAttrName) {
      params.email = header_dictionary[kEmailAttrName];
    } else if (key_name == kIsSamlAttrName) {
      params.is_saml = header_dictionary[kIsSamlAttrName] == "true";
    } else if (key_name == kContinueUrlAttrName) {
      params.continue_url = header_dictionary[kContinueUrlAttrName];
    } else if (key_name == kIsSameTabAttrName) {
      params.is_same_tab = header_dictionary[kIsSameTabAttrName] == "true";
    } else {
      DLOG(WARNING) << "Unexpected Gaia header attribute '" << key_name << "'.";
    }
  }
  return params;
}

ManageAccountsParams BuildManageAccountsParamsIfExists(net::URLRequest* request,
                                                       bool is_off_the_record) {
  ManageAccountsParams empty_params;
  empty_params.service_type = GAIA_SERVICE_TYPE_NONE;
  if (!gaia::IsGaiaSignonRealm(request->url().GetOrigin()))
    return empty_params;

  std::string header_value;
  net::HttpResponseHeaders* response_headers = request->response_headers();
  if (!response_headers ||
      !response_headers->GetNormalizedHeader(
          kChromeManageAccountsHeader, &header_value)) {
    return empty_params;
  }

  DCHECK(switches::IsAccountConsistencyMirrorEnabled() && !is_off_the_record);
  return BuildManageAccountsParams(header_value);
}

}  // namespace signin
