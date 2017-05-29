// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_auth_fetcher.h"

#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace {
const int kLoadFlagsIgnoreCookies = net::LOAD_DO_NOT_SEND_COOKIES |
                                    net::LOAD_DO_NOT_SAVE_COOKIES;

static bool CookiePartsContains(const std::vector<std::string>& parts,
                                const char* part) {
  for (std::vector<std::string>::const_iterator it = parts.begin();
       it != parts.end(); ++it) {
    if (base::LowerCaseEqualsASCII(*it, part))
      return true;
  }
  return false;
}

// From the JSON string |data|, extract the |access_token| and |expires_in_secs|
// both of which must exist. If the |refresh_token| is non-NULL, then it also
// must exist and is extraced; if it's NULL, then no extraction is attempted.
bool ExtractOAuth2TokenPairResponse(const std::string& data,
                                    std::string* refresh_token,
                                    std::string* access_token,
                                    int* expires_in_secs) {
  DCHECK(access_token);
  DCHECK(expires_in_secs);

  std::unique_ptr<base::Value> value = base::JSONReader::Read(data);
  if (!value.get() || value->GetType() != base::Value::Type::DICTIONARY)
    return false;

  base::DictionaryValue* dict =
        static_cast<base::DictionaryValue*>(value.get());

  if (!dict->GetStringWithoutPathExpansion("access_token", access_token) ||
      !dict->GetIntegerWithoutPathExpansion("expires_in", expires_in_secs)) {
    return false;
  }

  // Refresh token may not be required.
  if (refresh_token) {
    if (!dict->GetStringWithoutPathExpansion("refresh_token", refresh_token))
      return false;
  }
  return true;
}

void GetCookiesFromResponse(const net::HttpResponseHeaders* headers,
                            net::ResponseCookies* cookies) {
  if (!headers)
    return;

  std::string value;
  size_t iter = 0;
  while (headers->EnumerateHeader(&iter, "Set-Cookie", &value)) {
    if (!value.empty())
      cookies->push_back(value);
  }
}

const char kListIdpServiceRequested[] = "list_idp";
const char kGetTokenResponseRequested[] = "get_token";

}  // namespace

// static
const char GaiaAuthFetcher::kIssueAuthTokenFormat[] =
    "SID=%s&"
    "LSID=%s&"
    "service=%s&"
    "Session=%s";
// static
const char GaiaAuthFetcher::kClientLoginToOAuth2URLFormat[] =
    "?scope=%s&client_id=%s";
// static
const char GaiaAuthFetcher::kOAuth2CodeToTokenPairBodyFormat[] =
    "scope=%s&"
    "grant_type=authorization_code&"
    "client_id=%s&"
    "client_secret=%s&"
    "code=%s";
// static
const char GaiaAuthFetcher::kOAuth2CodeToTokenPairDeviceIdParam[] =
    "device_id=%s&device_type=chrome";
// static
const char GaiaAuthFetcher::kOAuth2RevokeTokenBodyFormat[] =
    "token=%s";
// static
const char GaiaAuthFetcher::kGetUserInfoFormat[] =
    "LSID=%s";
// static
const char GaiaAuthFetcher::kMergeSessionFormat[] =
    "?uberauth=%s&"
    "continue=%s&"
    "source=%s";
// static
const char GaiaAuthFetcher::kUberAuthTokenURLFormat[] =
    "?source=%s&"
    "issueuberauth=1";

const char GaiaAuthFetcher::kOAuthLoginFormat[] = "service=%s&source=%s";

// static
const char GaiaAuthFetcher::kAccountDeletedError[] = "AccountDeleted";
// static
const char GaiaAuthFetcher::kAccountDisabledError[] = "AccountDisabled";
// static
const char GaiaAuthFetcher::kBadAuthenticationError[] = "BadAuthentication";
// static
const char GaiaAuthFetcher::kCaptchaError[] = "CaptchaRequired";
// static
const char GaiaAuthFetcher::kServiceUnavailableError[] =
    "ServiceUnavailable";
// static
const char GaiaAuthFetcher::kErrorParam[] = "Error";
// static
const char GaiaAuthFetcher::kErrorUrlParam[] = "Url";
// static
const char GaiaAuthFetcher::kCaptchaUrlParam[] = "CaptchaUrl";
// static
const char GaiaAuthFetcher::kCaptchaTokenParam[] = "CaptchaToken";

// static
const char GaiaAuthFetcher::kSecondFactor[] = "Info=InvalidSecondFactor";
// static
const char GaiaAuthFetcher::kWebLoginRequired[] = "Info=WebLoginRequired";

// static
const char GaiaAuthFetcher::kAuthHeaderFormat[] =
    "Authorization: GoogleLogin auth=%s";
// static
const char GaiaAuthFetcher::kOAuthHeaderFormat[] = "Authorization: OAuth %s";
// static
const char GaiaAuthFetcher::kOAuth2BearerHeaderFormat[] =
    "Authorization: Bearer %s";
// static
const char GaiaAuthFetcher::kDeviceIdHeaderFormat[] = "X-Device-ID: %s";
// static
const char GaiaAuthFetcher::kClientLoginToOAuth2CookiePartSecure[] = "secure";
// static
const char GaiaAuthFetcher::kClientLoginToOAuth2CookiePartHttpOnly[] =
    "httponly";
// static
const char GaiaAuthFetcher::kClientLoginToOAuth2CookiePartCodePrefix[] =
    "oauth_code=";
// static
const int GaiaAuthFetcher::kClientLoginToOAuth2CookiePartCodePrefixLength =
    arraysize(GaiaAuthFetcher::kClientLoginToOAuth2CookiePartCodePrefix) - 1;

GaiaAuthFetcher::GaiaAuthFetcher(GaiaAuthConsumer* consumer,
                                 const std::string& source,
                                 net::URLRequestContextGetter* getter)
    : consumer_(consumer),
      getter_(getter),
      source_(source),
      issue_auth_token_gurl_(GaiaUrls::GetInstance()->issue_auth_token_url()),
      oauth2_token_gurl_(GaiaUrls::GetInstance()->oauth2_token_url()),
      oauth2_revoke_gurl_(GaiaUrls::GetInstance()->oauth2_revoke_url()),
      get_user_info_gurl_(GaiaUrls::GetInstance()->get_user_info_url()),
      merge_session_gurl_(GaiaUrls::GetInstance()->merge_session_url()),
      uberauth_token_gurl_(GaiaUrls::GetInstance()->oauth1_login_url().Resolve(
          base::StringPrintf(kUberAuthTokenURLFormat, source.c_str()))),
      oauth_login_gurl_(GaiaUrls::GetInstance()->oauth1_login_url()),
      list_accounts_gurl_(
          GaiaUrls::GetInstance()->ListAccountsURLWithSource(source)),
      logout_gurl_(GaiaUrls::GetInstance()->LogOutURLWithSource(source)),
      get_check_connection_info_url_(
          GaiaUrls::GetInstance()->GetCheckConnectionInfoURLWithSource(source)),
      oauth2_iframe_url_(GaiaUrls::GetInstance()->oauth2_iframe_url()),
      client_login_to_oauth2_gurl_(
          GaiaUrls::GetInstance()->client_login_to_oauth2_url()) {
}

GaiaAuthFetcher::~GaiaAuthFetcher() {}

bool GaiaAuthFetcher::HasPendingFetch() {
  return fetch_pending_;
}

void GaiaAuthFetcher::SetPendingFetch(bool pending_fetch) {
  fetch_pending_ = pending_fetch;
}

void GaiaAuthFetcher::SetLogoutHeaders(const std::string& headers) {
  logout_headers_ = headers;
}

void GaiaAuthFetcher::CancelRequest() {
  fetcher_.reset();
  fetch_pending_ = false;
}

void GaiaAuthFetcher::CreateAndStartGaiaFetcher(
    const std::string& body,
    const std::string& headers,
    const GURL& gaia_gurl,
    int load_flags,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";
  fetcher_ = net::URLFetcher::Create(
      0, gaia_gurl, body.empty() ? net::URLFetcher::GET : net::URLFetcher::POST,
      this, traffic_annotation);
  fetcher_->SetRequestContext(getter_);
  fetcher_->SetUploadData("application/x-www-form-urlencoded", body);
  gaia::MarkURLFetcherAsGaia(fetcher_.get());

  VLOG(2) << "Gaia fetcher URL: " << gaia_gurl.spec();
  VLOG(2) << "Gaia fetcher headers: " << headers;
  VLOG(2) << "Gaia fetcher body: " << body;

  // The Gaia token exchange requests do not require any cookie-based
  // identification as part of requests.  We suppress sending any cookies to
  // maintain a separation between the user's browsing and Chrome's internal
  // services.  Where such mixing is desired (MergeSession or OAuthLogin), it
  // will be done explicitly.
  fetcher_->SetLoadFlags(load_flags);

  // Fetchers are sometimes cancelled because a network change was detected,
  // especially at startup and after sign-in on ChromeOS. Retrying once should
  // be enough in those cases; let the fetcher retry up to 3 times just in case.
  // http://crbug.com/163710
  fetcher_->SetAutomaticallyRetryOnNetworkChanges(3);

  if (!headers.empty())
    fetcher_->SetExtraRequestHeaders(headers);

  fetch_pending_ = true;
  fetcher_->Start();
}

// static
std::string GaiaAuthFetcher::MakeIssueAuthTokenBody(
    const std::string& sid,
    const std::string& lsid,
    const char* const service) {
  std::string encoded_sid = net::EscapeUrlEncodedData(sid, true);
  std::string encoded_lsid = net::EscapeUrlEncodedData(lsid, true);

  // All tokens should be session tokens except the gaia auth token.
  bool session = true;
  if (!strcmp(service, GaiaConstants::kGaiaService))
    session = false;

  return base::StringPrintf(kIssueAuthTokenFormat,
                            encoded_sid.c_str(),
                            encoded_lsid.c_str(),
                            service,
                            session ? "true" : "false");
}

// static
std::string GaiaAuthFetcher::MakeGetTokenPairBody(
    const std::string& auth_code,
    const std::string& device_id) {
  std::string encoded_scope = net::EscapeUrlEncodedData(
      GaiaConstants::kOAuth1LoginScope, true);
  std::string encoded_client_id = net::EscapeUrlEncodedData(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(), true);
  std::string encoded_client_secret = net::EscapeUrlEncodedData(
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(), true);
  std::string encoded_auth_code = net::EscapeUrlEncodedData(auth_code, true);
  std::string body = base::StringPrintf(
      kOAuth2CodeToTokenPairBodyFormat, encoded_scope.c_str(),
      encoded_client_id.c_str(), encoded_client_secret.c_str(),
      encoded_auth_code.c_str());
  if (!device_id.empty()) {
    body += "&" + base::StringPrintf(kOAuth2CodeToTokenPairDeviceIdParam,
                                     device_id.c_str());
  }
  return body;
}

// static
std::string GaiaAuthFetcher::MakeRevokeTokenBody(
    const std::string& auth_token) {
  return base::StringPrintf(kOAuth2RevokeTokenBodyFormat, auth_token.c_str());
}

// static
std::string GaiaAuthFetcher::MakeGetUserInfoBody(const std::string& lsid) {
  std::string encoded_lsid = net::EscapeUrlEncodedData(lsid, true);
  return base::StringPrintf(kGetUserInfoFormat, encoded_lsid.c_str());
}

// static
std::string GaiaAuthFetcher::MakeMergeSessionQuery(
    const std::string& auth_token,
    const std::string& external_cc_result,
    const std::string& continue_url,
    const std::string& source) {
  std::string encoded_auth_token = net::EscapeUrlEncodedData(auth_token, true);
  std::string encoded_continue_url = net::EscapeUrlEncodedData(continue_url,
                                                               true);
  std::string encoded_source = net::EscapeUrlEncodedData(source, true);
  std::string result = base::StringPrintf(kMergeSessionFormat,
                                          encoded_auth_token.c_str(),
                                          encoded_continue_url.c_str(),
                                          encoded_source.c_str());
  if (!external_cc_result.empty()) {
    base::StringAppendF(&result, "&externalCcResult=%s",
                        net::EscapeUrlEncodedData(
                            external_cc_result, true).c_str());
  }

  return result;
}

// static
std::string GaiaAuthFetcher::MakeGetAuthCodeHeader(
    const std::string& auth_token) {
  return base::StringPrintf(kAuthHeaderFormat, auth_token.c_str());
}

// Helper method that extracts tokens from a successful reply.
// static
void GaiaAuthFetcher::ParseClientLoginResponse(const std::string& data,
                                               std::string* sid,
                                               std::string* lsid,
                                               std::string* token) {
  using std::vector;
  using std::pair;
  using std::string;
  sid->clear();
  lsid->clear();
  token->clear();
  base::StringPairs tokens;
  base::SplitStringIntoKeyValuePairs(data, '=', '\n', &tokens);
  for (base::StringPairs::iterator i = tokens.begin();
      i != tokens.end(); ++i) {
    if (i->first == "SID") {
      sid->assign(i->second);
    } else if (i->first == "LSID") {
      lsid->assign(i->second);
    } else if (i->first == "Auth") {
      token->assign(i->second);
    }
  }
  // If this was a request for uberauth token, then that's all we've got in
  // data.
  if (sid->empty() && lsid->empty() && token->empty())
    token->assign(data);
}

// static
std::string GaiaAuthFetcher::MakeOAuthLoginBody(const std::string& service,
                                                const std::string& source) {
  std::string encoded_service = net::EscapeUrlEncodedData(service, true);
  std::string encoded_source = net::EscapeUrlEncodedData(source, true);
  return base::StringPrintf(kOAuthLoginFormat,
                            encoded_service.c_str(),
                            encoded_source.c_str());
}

// static
std::string GaiaAuthFetcher::MakeListIDPSessionsBody(
    const std::string& scopes,
    const std::string& domain) {
  static const char getTokenResponseBodyFormat[] =
    "action=listSessions&"
    "client_id=%s&"
    "origin=%s&"
    "scope=%s";
  std::string encoded_client_id = net::EscapeUrlEncodedData(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(), true);
  return base::StringPrintf(getTokenResponseBodyFormat,
                            encoded_client_id.c_str(),
                            domain.c_str(),
                            scopes.c_str());
}

std::string GaiaAuthFetcher::MakeGetTokenResponseBody(
    const std::string& scopes,
    const std::string& domain,
    const std::string& login_hint) {
  static const char getTokenResponseBodyFormat[] =
    "action=issueToken&"
    "client_id=%s&"
    "login_hint=%s&"
    "origin=%s&"
    "response_type=token&"
    "scope=%s";
  std::string encoded_client_id = net::EscapeUrlEncodedData(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(), true);
  return base::StringPrintf(getTokenResponseBodyFormat,
                            encoded_client_id.c_str(),
                            login_hint.c_str(),
                            domain.c_str(),
                            scopes.c_str());
}

// static
void GaiaAuthFetcher::ParseClientLoginFailure(const std::string& data,
                                              std::string* error,
                                              std::string* error_url,
                                              std::string* captcha_url,
                                              std::string* captcha_token) {
  using std::vector;
  using std::pair;
  using std::string;

  base::StringPairs tokens;
  base::SplitStringIntoKeyValuePairs(data, '=', '\n', &tokens);
  for (base::StringPairs::iterator i = tokens.begin();
       i != tokens.end(); ++i) {
    if (i->first == kErrorParam) {
      error->assign(i->second);
    } else if (i->first == kErrorUrlParam) {
      error_url->assign(i->second);
    } else if (i->first == kCaptchaUrlParam) {
      captcha_url->assign(i->second);
    } else if (i->first == kCaptchaTokenParam) {
      captcha_token->assign(i->second);
    }
  }
}

// static
bool GaiaAuthFetcher::ParseClientLoginToOAuth2Response(
    const net::ResponseCookies& cookies,
    std::string* auth_code) {
  DCHECK(auth_code);
  net::ResponseCookies::const_iterator iter;
  for (iter = cookies.begin(); iter != cookies.end(); ++iter) {
    if (ParseClientLoginToOAuth2Cookie(*iter, auth_code))
      return true;
  }
  return false;
}

// static
bool GaiaAuthFetcher::ParseClientLoginToOAuth2Cookie(const std::string& cookie,
                                                     std::string* auth_code) {
  std::vector<std::string> parts = base::SplitString(
      cookie, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  // Per documentation, the cookie should have Secure and HttpOnly.
  if (!CookiePartsContains(parts, kClientLoginToOAuth2CookiePartSecure) ||
      !CookiePartsContains(parts, kClientLoginToOAuth2CookiePartHttpOnly)) {
    return false;
  }

  std::vector<std::string>::const_iterator iter;
  for (iter = parts.begin(); iter != parts.end(); ++iter) {
    const std::string& part = *iter;
    if (base::StartsWith(part, kClientLoginToOAuth2CookiePartCodePrefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      auth_code->assign(part.substr(
          kClientLoginToOAuth2CookiePartCodePrefixLength));
      return true;
    }
  }
  return false;
}

// static
bool GaiaAuthFetcher::ParseListIdpSessionsResponse(const std::string& data,
                                                   std::string* login_hint) {
  DCHECK(login_hint);

  std::unique_ptr<base::Value> value = base::JSONReader::Read(data);
  if (!value.get() || value->GetType() != base::Value::Type::DICTIONARY)
    return false;

  base::DictionaryValue* dict =
        static_cast<base::DictionaryValue*>(value.get());

  base::ListValue* sessionsList;
  if (!dict->GetList("sessions", &sessionsList))
    return false;

  // Find the first login_hint present in any session.
  for (base::ListValue::iterator iter = sessionsList->begin();
       iter != sessionsList->end();
       iter++) {
    base::DictionaryValue* sessionDictionary;
    if (!iter->GetAsDictionary(&sessionDictionary))
      continue;

    if (sessionDictionary->GetString("login_hint", login_hint))
      break;
  }

  if (login_hint->empty())
    return false;
  return true;
}


void GaiaAuthFetcher::StartRevokeOAuth2Token(const std::string& auth_token) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  VLOG(1) << "Starting OAuth2 token revocation";
  request_body_ = MakeRevokeTokenBody(auth_token);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_revoke_token", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description: "This request revokes an OAuth 2.0 refresh token."
          trigger:
            "This request is part of Gaia Auth API, and is triggered whenever "
            "an OAuth 2.0 refresh token needs to be revoked."
          data: "The OAuth 2.0 refresh token that should be revoked."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: false
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  CreateAndStartGaiaFetcher(request_body_, std::string(), oauth2_revoke_gurl_,
                            kLoadFlagsIgnoreCookies, traffic_annotation);
}

void GaiaAuthFetcher::StartCookieForOAuthLoginTokenExchange(
    const std::string& session_index) {
  StartCookieForOAuthLoginTokenExchangeWithDeviceId(session_index,
                                                    std::string());
}

void GaiaAuthFetcher::StartCookieForOAuthLoginTokenExchangeWithDeviceId(
    const std::string& session_index,
    const std::string& device_id) {
  StartCookieForOAuthLoginTokenExchange(
      true,
      session_index,
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      device_id);
}

void GaiaAuthFetcher::StartCookieForOAuthLoginTokenExchange(
    bool fetch_token_from_auth_code,
    const std::string& session_index,
    const std::string& client_id,
    const std::string& device_id) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  VLOG(1) << "Starting OAuth login token fetch with cookie jar";

  std::string encoded_scope = net::EscapeUrlEncodedData(
      GaiaConstants::kOAuth1LoginScope, true);
  std::string encoded_client_id = net::EscapeUrlEncodedData(client_id, true);
  std::string query_string =
      base::StringPrintf(kClientLoginToOAuth2URLFormat, encoded_scope.c_str(),
                         encoded_client_id.c_str());
  if (!device_id.empty())
    query_string += "&device_type=chrome";
  if (!session_index.empty())
    query_string += "&authuser=" + session_index;

  std::string device_id_header;
  if (!device_id.empty()) {
    device_id_header =
        base::StringPrintf(kDeviceIdHeaderFormat, device_id.c_str());
  }

  fetch_token_from_auth_code_ = fetch_token_from_auth_code;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_exchange_cookies", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request exchanges the cookies of a Google signed-in user "
            "session for an OAuth 2.0 refresh token."
          trigger:
            "This request is part of Gaia Auth API, and may be triggered at "
            "the end of the Chrome sign-in flow."
          data:
            "The Google console client ID of the Chrome application, the ID of "
            "the device, and the index of the session in the Google "
            "authentication cookies."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: true
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  CreateAndStartGaiaFetcher(std::string(), device_id_header,
                            client_login_to_oauth2_gurl_.Resolve(query_string),
                            net::LOAD_NORMAL, traffic_annotation);
}

void GaiaAuthFetcher::StartAuthCodeForOAuth2TokenExchange(
    const std::string& auth_code) {
  StartAuthCodeForOAuth2TokenExchangeWithDeviceId(auth_code, std::string());
}

void GaiaAuthFetcher::StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
    const std::string& auth_code,
    const std::string& device_id) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  VLOG(1) << "Starting OAuth token pair fetch";
  request_body_ = MakeGetTokenPairBody(auth_code, device_id);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_exchange_device_id", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request exchanges an authorization code for an OAuth 2.0 "
            "refresh token."
          trigger:
            "This request is part of Gaia Auth API, and may be triggered at "
            "the end of the Chrome sign-in flow."
          data:
            "The Google console client ID and client secret of the Chrome "
            "application, the OAuth 2.0 authorization code, and the ID of the "
            "device."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: false
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  CreateAndStartGaiaFetcher(request_body_, std::string(), oauth2_token_gurl_,
                            kLoadFlagsIgnoreCookies, traffic_annotation);
}

void GaiaAuthFetcher::StartGetUserInfo(const std::string& lsid) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  VLOG(1) << "Starting GetUserInfo for lsid=" << lsid;
  request_body_ = MakeGetUserInfoBody(lsid);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_get_user_info", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request fetches user information of a Google account."
          trigger:
            "This fetcher is only used after signing in with a child account."
          data: "The value of the Google authentication LSID cookie."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: false
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  CreateAndStartGaiaFetcher(request_body_, std::string(), get_user_info_gurl_,
                            kLoadFlagsIgnoreCookies, traffic_annotation);
}

void GaiaAuthFetcher::StartMergeSession(const std::string& uber_token,
                                        const std::string& external_cc_result) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  VLOG(1) << "Starting MergeSession with uber_token=" << uber_token;

  // The continue URL is a required parameter of the MergeSession API, but in
  // this case we don't actually need or want to navigate to it.  Setting it to
  // an arbitrary Google URL.
  //
  // In order for the new session to be merged correctly, the server needs to
  // know what sessions already exist in the browser.  The fetcher needs to be
  // created such that it sends the cookies with the request, which is
  // different from all other requests the fetcher can make.
  std::string continue_url("http://www.google.com");
  std::string query = MakeMergeSessionQuery(uber_token, external_cc_result,
                                            continue_url, source_);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_merge_sessions", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request adds an account to the Google authentication cookies."
          trigger:
            "This request is part of Gaia Auth API, and is triggered whenever "
            "a new Google account is added to the browser."
          data:
            "This request includes the user-auth token and sometimes a string "
            "containing the result of connection checks for various Google web "
            "properties."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: true
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  CreateAndStartGaiaFetcher(std::string(), std::string(),
                            merge_session_gurl_.Resolve(query),
                            net::LOAD_NORMAL, traffic_annotation);
}

void GaiaAuthFetcher::StartTokenFetchForUberAuthExchange(
    const std::string& access_token,
    bool is_bound_to_channel_id) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  VLOG(1) << "Starting StartTokenFetchForUberAuthExchange with access_token="
           << access_token;
  std::string authentication_header =
      base::StringPrintf(kOAuthHeaderFormat, access_token.c_str());
  int load_flags =
      is_bound_to_channel_id ? net::LOAD_NORMAL : kLoadFlagsIgnoreCookies;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_fetch_for_uber", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request exchanges an Oauth2 access token for an uber-auth "
            "token. This token may be used to add an account to the Google "
            "authentication cookies."
          trigger:
            "This request is part of Gaia Auth API, and is triggered whenever "
            "a new Google account is added to the browser."
          data: "This request contains an OAuth 2.0 access token. "
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: true
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  CreateAndStartGaiaFetcher(std::string(), authentication_header,
                            uberauth_token_gurl_, load_flags,
                            traffic_annotation);
}

void GaiaAuthFetcher::StartOAuthLogin(const std::string& access_token,
                                      const std::string& service) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  request_body_ = MakeOAuthLoginBody(service, source_);
  std::string authentication_header =
      base::StringPrintf(kOAuth2BearerHeaderFormat, access_token.c_str());
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_login", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request exchanges an OAuthLogin-scoped OAuth 2.0 access "
            "token for a ClientLogin-style service tokens. The response to "
            "this request is the same as the response to a ClientLogin "
            "request, except that captcha challenges are never issued."
          trigger:
            "This request is part of Gaia Auth API, and is triggered after "
            "signing in with a child account."
          data:
            "This request contains an OAuth 2.0 access token and the service "
            "for which a ClientLogin-style should be delivered."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: true
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  CreateAndStartGaiaFetcher(request_body_, authentication_header,
                            oauth_login_gurl_, net::LOAD_NORMAL,
                            traffic_annotation);
}

void GaiaAuthFetcher::StartListAccounts() {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_list_accounts", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request is used to list the accounts in the Google "
            "authentication cookies."
          trigger:
            "This request is part of Gaia Auth API, and is triggered whenever "
            "the list of all available accounts in the Google authentication "
            "cookies is required."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: true
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  CreateAndStartGaiaFetcher(" ",  // To force an HTTP POST.
                            "Origin: https://www.google.com",
                            list_accounts_gurl_, net::LOAD_NORMAL,
                            traffic_annotation);
}

void GaiaAuthFetcher::StartLogOut() {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_log_out", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request is part of the Chrome - Google authentication API "
            "and allows its callers to sign out all Google accounts from the "
            "content area."
          trigger:
            "This request is part of Gaia Auth API, and is triggered whenever "
            "signing out of all Google accounts is required."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: true
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  CreateAndStartGaiaFetcher(std::string(), logout_headers_, logout_gurl_,
                            net::LOAD_NORMAL, traffic_annotation);
}

void GaiaAuthFetcher::StartGetCheckConnectionInfo() {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_check_connection_info", R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request is used to fetch from the Google authentication "
            "server the the list of URLs to check its connection info."
          trigger:
            "This request is part of Gaia Auth API, and is triggered once "
            "after a Google account is added to the browser."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: false
          setting:
            "This feature cannot be disabled in settings, but if the user "
            "signs out of Chrome, this request would not be made."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  CreateAndStartGaiaFetcher(std::string(), std::string(),
                            get_check_connection_info_url_,
                            kLoadFlagsIgnoreCookies, traffic_annotation);
}


// static
GoogleServiceAuthError GaiaAuthFetcher::GenerateAuthError(
    const std::string& data,
    const net::URLRequestStatus& status) {
  if (!status.is_success()) {
    if (status.status() == net::URLRequestStatus::CANCELED) {
      return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
    }
    DLOG(WARNING) << "Could not reach Google Accounts servers: errno "
                  << status.error();
    return GoogleServiceAuthError::FromConnectionError(status.error());
  }

  if (IsSecondFactorSuccess(data))
    return GoogleServiceAuthError(GoogleServiceAuthError::TWO_FACTOR);

  if (IsWebLoginRequiredSuccess(data))
    return GoogleServiceAuthError(GoogleServiceAuthError::WEB_LOGIN_REQUIRED);

  std::string error;
  std::string url;
  std::string captcha_url;
  std::string captcha_token;
  ParseClientLoginFailure(data, &error, &url, &captcha_url, &captcha_token);
  DLOG(WARNING) << "ClientLogin failed with " << error;

  if (error == kCaptchaError) {
    return GoogleServiceAuthError::FromClientLoginCaptchaChallenge(
        captcha_token,
        GURL(GaiaUrls::GetInstance()->captcha_base_url().Resolve(captcha_url)),
        GURL(url));
  }
  if (error == kAccountDeletedError)
    return GoogleServiceAuthError(GoogleServiceAuthError::ACCOUNT_DELETED);
  if (error == kAccountDisabledError)
    return GoogleServiceAuthError(GoogleServiceAuthError::ACCOUNT_DISABLED);
  if (error == kBadAuthenticationError) {
    return GoogleServiceAuthError(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  }
  if (error == kServiceUnavailableError) {
    return GoogleServiceAuthError(
        GoogleServiceAuthError::SERVICE_UNAVAILABLE);
  }

  DLOG(WARNING) << "Incomprehensible response from Google Accounts servers.";
  return GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE);
}

void GaiaAuthFetcher::OnIssueAuthTokenFetched(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    // Only the bare token is returned in the body of this Gaia call
    // without any padding.
    consumer_->OnIssueAuthTokenSuccess(requested_service_, data);
  } else {
    consumer_->OnIssueAuthTokenFailure(requested_service_,
        GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnClientLoginToOAuth2Fetched(
    const std::string& data,
    const net::ResponseCookies& cookies,
    const net::URLRequestStatus& status,
    int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    std::string auth_code;
    if (ParseClientLoginToOAuth2Response(cookies, &auth_code)) {
      if (fetch_token_from_auth_code_)
        StartAuthCodeForOAuth2TokenExchange(auth_code);
      else
        consumer_->OnClientOAuthCode(auth_code);
    } else {
      GoogleServiceAuthError auth_error(
          GoogleServiceAuthError::FromUnexpectedServiceResponse(
              "ClientLogin response cookies didn't contain an auth code"));
      consumer_->OnClientOAuthFailure(auth_error);
    }
  } else {
    GoogleServiceAuthError auth_error(GenerateAuthError(data, status));
    consumer_->OnClientOAuthFailure(auth_error);
  }
}

void GaiaAuthFetcher::OnOAuth2TokenPairFetched(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  std::string refresh_token;
  std::string access_token;
  int expires_in_secs = 0;

  bool success = false;
  if (status.is_success() && response_code == net::HTTP_OK) {
      success = ExtractOAuth2TokenPairResponse(data, &refresh_token,
                                               &access_token, &expires_in_secs);
  }

  if (success) {
    consumer_->OnClientOAuthSuccess(
        GaiaAuthConsumer::ClientOAuthResult(refresh_token, access_token,
                                            expires_in_secs));
  } else {
    consumer_->OnClientOAuthFailure(GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnOAuth2RevokeTokenFetched(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  consumer_->OnOAuth2RevokeTokenCompleted();
}

void GaiaAuthFetcher::OnListAccountsFetched(const std::string& data,
                                            const net::URLRequestStatus& status,
                                            int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    consumer_->OnListAccountsSuccess(data);
  } else {
    consumer_->OnListAccountsFailure(GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnLogOutFetched(const std::string& data,
                                      const net::URLRequestStatus& status,
                                      int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    consumer_->OnLogOutSuccess();
  } else {
    consumer_->OnLogOutFailure(GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnGetUserInfoFetched(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    base::StringPairs tokens;
    UserInfoMap matches;
    base::SplitStringIntoKeyValuePairs(data, '=', '\n', &tokens);
    base::StringPairs::iterator i;
    for (i = tokens.begin(); i != tokens.end(); ++i) {
      matches[i->first] = i->second;
    }
    consumer_->OnGetUserInfoSuccess(matches);
  } else {
    consumer_->OnGetUserInfoFailure(GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnMergeSessionFetched(const std::string& data,
                                            const net::URLRequestStatus& status,
                                            int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    consumer_->OnMergeSessionSuccess(data);
  } else {
    consumer_->OnMergeSessionFailure(GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnUberAuthTokenFetch(const std::string& data,
                                           const net::URLRequestStatus& status,
                                           int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    consumer_->OnUberAuthTokenSuccess(data);
  } else {
    consumer_->OnUberAuthTokenFailure(GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnOAuthLoginFetched(const std::string& data,
                                          const net::URLRequestStatus& status,
                                          int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    VLOG(1) << "ClientLogin successful!";
    std::string sid;
    std::string lsid;
    std::string token;
    ParseClientLoginResponse(data, &sid, &lsid, &token);
    consumer_->OnClientLoginSuccess(
        GaiaAuthConsumer::ClientLoginResult(sid, lsid, token, data));
  } else {
    consumer_->OnClientLoginFailure(GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnGetCheckConnectionInfoFetched(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    consumer_->OnGetCheckConnectionInfoSuccess(data);
  } else {
    consumer_->OnGetCheckConnectionInfoError(GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnListIdpSessionsFetched(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    VLOG(1) << "ListIdpSessions successful!";
    std::string login_hint;
    if (ParseListIdpSessionsResponse(data, &login_hint)) {
      consumer_->OnListIdpSessionsSuccess(login_hint);
    } else {
      GoogleServiceAuthError auth_error(
          GoogleServiceAuthError::FromUnexpectedServiceResponse(
              "List Sessions response didn't contain a login_hint."));
      consumer_->OnListIdpSessionsError(auth_error);
    }
  } else {
    consumer_->OnListIdpSessionsError(GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnGetTokenResponseFetched(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  std::string access_token;
  int expires_in_secs = 0;
  bool success = false;
  if (status.is_success() && response_code == net::HTTP_OK) {
    VLOG(1) << "GetTokenResponse successful!";
    success = ExtractOAuth2TokenPairResponse(data, NULL,
                                             &access_token, &expires_in_secs);
  }

  if (success) {
    consumer_->OnGetTokenResponseSuccess(
        GaiaAuthConsumer::ClientOAuthResult(std::string(), access_token,
                                            expires_in_secs));
  } else {
    consumer_->OnGetTokenResponseError(GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  fetch_pending_ = false;
  // Some of the GAIA requests perform redirects, which results in the final
  // URL of the fetcher not being the original URL requested.  Therefore use
  // the original URL when determining which OnXXX function to call.
  const GURL& url = source->GetOriginalURL();
  const net::URLRequestStatus& status = source->GetStatus();
  int response_code = source->GetResponseCode();
  std::string data;
  source->GetResponseAsString(&data);

// Retrieve the response headers from the request.  Must only be called after
// the OnURLFetchComplete callback has run.
#ifndef NDEBUG
  std::string headers;
  if (source->GetResponseHeaders())
    headers = net::HttpUtil::ConvertHeadersBackToHTTPResponse(
        source->GetResponseHeaders()->raw_headers());
  DVLOG(2) << "Response " << url.spec() << ", code = " << response_code << "\n"
           << headers << "\n";
  DVLOG(2) << "data: " << data << "\n";
#endif

  net::ResponseCookies cookies;
  GetCookiesFromResponse(source->GetResponseHeaders(), &cookies);
  DispatchFetchedRequest(url, data, cookies, status, response_code);
}

void GaiaAuthFetcher::DispatchFetchedRequest(
    const GURL& url,
    const std::string& data,
    const net::ResponseCookies& cookies,
    const net::URLRequestStatus& status,
    int response_code) {
  if (url == issue_auth_token_gurl_) {
    OnIssueAuthTokenFetched(data, status, response_code);
  } else if (base::StartsWith(url.spec(),
                              client_login_to_oauth2_gurl_.spec(),
                              base::CompareCase::SENSITIVE)) {
    OnClientLoginToOAuth2Fetched(data, cookies, status, response_code);
  } else if (url == oauth2_token_gurl_) {
    OnOAuth2TokenPairFetched(data, status, response_code);
  } else if (url == get_user_info_gurl_) {
    OnGetUserInfoFetched(data, status, response_code);
  } else if (base::StartsWith(url.spec(), merge_session_gurl_.spec(),
                              base::CompareCase::SENSITIVE)) {
    OnMergeSessionFetched(data, status, response_code);
  } else if (url == uberauth_token_gurl_) {
    OnUberAuthTokenFetch(data, status, response_code);
  } else if (url == oauth_login_gurl_) {
    OnOAuthLoginFetched(data, status, response_code);
  } else if (url == oauth2_revoke_gurl_) {
    OnOAuth2RevokeTokenFetched(data, status, response_code);
  } else if (url == list_accounts_gurl_) {
    OnListAccountsFetched(data, status, response_code);
  } else if (url == logout_gurl_) {
    OnLogOutFetched(data, status, response_code);
  } else if (url == get_check_connection_info_url_) {
    OnGetCheckConnectionInfoFetched(data, status, response_code);
  } else if (url == oauth2_iframe_url_) {
    if (requested_service_ == kListIdpServiceRequested)
      OnListIdpSessionsFetched(data, status, response_code);
    else if (requested_service_ == kGetTokenResponseRequested)
      OnGetTokenResponseFetched(data, status, response_code);
    else
      NOTREACHED();
  } else {
    NOTREACHED();
  }
}

// static
bool GaiaAuthFetcher::IsSecondFactorSuccess(
    const std::string& alleged_error) {
  return alleged_error.find(kSecondFactor) !=
      std::string::npos;
}

// static
bool GaiaAuthFetcher::IsWebLoginRequiredSuccess(
    const std::string& alleged_error) {
  return alleged_error.find(kWebLoginRequired) !=
      std::string::npos;
}
