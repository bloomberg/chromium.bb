// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/gaia/gaia_oauth_fetcher.h"

#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "chrome/browser/net/gaia/gaia_oauth_consumer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/net/gaia/oauth_request_signer.h"
#include "chrome/common/net/url_util.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/url_fetcher.h"
#include "grit/chromium_strings.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/l10n/l10n_util.h"

static const char kOAuthTokenCookie[] = "oauth_token";

GaiaOAuthFetcher::GaiaOAuthFetcher(GaiaOAuthConsumer* consumer,
                                   net::URLRequestContextGetter* getter,
                                   Profile* profile,
                                   const std::string& service_scope)
    : consumer_(consumer),
      getter_(getter),
      profile_(profile),
      popup_(NULL),
      service_scope_(service_scope),
      fetch_pending_(false),
      auto_fetch_limit_(USER_INFO) {}

GaiaOAuthFetcher::~GaiaOAuthFetcher() {}

bool GaiaOAuthFetcher::HasPendingFetch() const {
  return fetch_pending_;
}

void GaiaOAuthFetcher::CancelRequest() {
  fetcher_.reset();
  fetch_pending_ = false;
}

// static
content::URLFetcher* GaiaOAuthFetcher::CreateGaiaFetcher(
    net::URLRequestContextGetter* getter,
    const GURL& gaia_gurl,
    const std::string& body,
    const std::string& headers,
    bool send_cookies,
    content::URLFetcherDelegate* delegate) {
  bool empty_body = body.empty();
  content::URLFetcher* result = content::URLFetcher::Create(
      0, gaia_gurl,
      empty_body ? content::URLFetcher::GET : content::URLFetcher::POST,
      delegate);
  result->SetRequestContext(getter);

  // The Gaia/OAuth token exchange requests do not require any cookie-based
  // identification as part of requests.  We suppress sending any cookies to
  // maintain a separation between the user's browsing and Chrome's internal
  // services.  Where such mixing is desired (prelogin, autologin
  // or chromeos login), it will be done explicitly.
  if (!send_cookies)
    result->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES);

  if (!empty_body)
    result->SetUploadData("application/x-www-form-urlencoded", body);
  if (!headers.empty())
    result->SetExtraRequestHeaders(headers);

  return result;
}

// static
GURL GaiaOAuthFetcher::MakeGetOAuthTokenUrl(
    const std::string& oauth1_login_scope,
    const std::string& product_name) {
  return GURL(GaiaUrls::GetInstance()->get_oauth_token_url() +
      "?scope=" + oauth1_login_scope +
      "&xoauth_display_name=" +
      OAuthRequestSigner::Encode(product_name));
}

// static
std::string GaiaOAuthFetcher::MakeOAuthLoginBody(
    const char* source,
    const char* service,
    const std::string& oauth1_access_token,
    const std::string& oauth1_access_token_secret) {
  OAuthRequestSigner::Parameters parameters;
  parameters["service"] = service;
  parameters["source"] = source;
  std::string signed_request;
  bool is_signed = OAuthRequestSigner::SignURL(
      GURL(GaiaUrls::GetInstance()->oauth1_login_url()),
      parameters,
      OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
      OAuthRequestSigner::POST_METHOD,
      "anonymous",  // oauth_consumer_key
      "anonymous",  // consumer secret
      oauth1_access_token,  // oauth_token
      oauth1_access_token_secret,  // token secret
      &signed_request);
  DCHECK(is_signed);
  return signed_request;
}

// static
std::string GaiaOAuthFetcher::MakeOAuthGetAccessTokenBody(
    const std::string& oauth1_request_token) {
  OAuthRequestSigner::Parameters empty_parameters;
  std::string signed_request;
  bool is_signed = OAuthRequestSigner::SignURL(
      GURL(GaiaUrls::GetInstance()->oauth_get_access_token_url()),
      empty_parameters,
      OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
      OAuthRequestSigner::POST_METHOD,
      "anonymous",  // oauth_consumer_key
      "anonymous",  // consumer secret
      oauth1_request_token,  // oauth_token
      "",  // token secret
      &signed_request);
  DCHECK(is_signed);
  return signed_request;
}

// static
std::string GaiaOAuthFetcher::MakeOAuthWrapBridgeBody(
    const std::string& oauth1_access_token,
    const std::string& oauth1_access_token_secret,
    const std::string& wrap_token_duration,
    const std::string& oauth2_scope) {
  OAuthRequestSigner::Parameters parameters;
  parameters["wrap_token_duration"] = wrap_token_duration;
  parameters["wrap_scope"] = oauth2_scope;
  std::string signed_request;
  bool is_signed = OAuthRequestSigner::SignURL(
      GURL(GaiaUrls::GetInstance()->oauth_wrap_bridge_url()),
      parameters,
      OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
      OAuthRequestSigner::POST_METHOD,
      "anonymous",  // oauth_consumer_key
      "anonymous",  // consumer secret
      oauth1_access_token,  // oauth_token
      oauth1_access_token_secret,  // token secret
      &signed_request);
  DCHECK(is_signed);
  return signed_request;
}

// Helper method that extracts tokens from a successful reply.
// static
void GaiaOAuthFetcher::ParseGetOAuthTokenResponse(
    const std::string& data,
    std::string* token) {
  using std::vector;
  using std::pair;
  using std::string;

  vector<pair<string, string> > tokens;
  base::SplitStringIntoKeyValuePairs(data, '=', '&', &tokens);
  for (vector<pair<string, string> >::iterator i = tokens.begin();
       i != tokens.end(); ++i) {
    if (i->first == "oauth_token") {
      std::string decoded;
      if (OAuthRequestSigner::Decode(i->second, &decoded))
        token->assign(decoded);
    }
  }
}

// Helper method that extracts tokens from a successful reply.
// static
void GaiaOAuthFetcher::ParseOAuthLoginResponse(
    const std::string& data,
    std::string* sid,
    std::string* lsid,
    std::string* auth) {
  using std::vector;
  using std::pair;
  using std::string;
  vector<pair<string, string> > tokens;
  base::SplitStringIntoKeyValuePairs(data, '=', '\n', &tokens);
  for (vector<pair<string, string> >::iterator i = tokens.begin();
      i != tokens.end(); ++i) {
    if (i->first == "SID") {
      *sid = i->second;
    } else if (i->first == "LSID") {
      *lsid = i->second;
    } else if (i->first == "Auth") {
      *auth = i->second;
    }
  }
}

// Helper method that extracts tokens from a successful reply.
// static
void GaiaOAuthFetcher::ParseOAuthGetAccessTokenResponse(
    const std::string& data,
    std::string* token,
    std::string* secret) {
  using std::vector;
  using std::pair;
  using std::string;

  vector<pair<string, string> > tokens;
  base::SplitStringIntoKeyValuePairs(data, '=', '&', &tokens);
  for (vector<pair<string, string> >::iterator i = tokens.begin();
       i != tokens.end(); ++i) {
    if (i->first == "oauth_token") {
      std::string decoded;
      if (OAuthRequestSigner::Decode(i->second, &decoded))
        token->assign(decoded);
    } else if (i->first == "oauth_token_secret") {
      std::string decoded;
      if (OAuthRequestSigner::Decode(i->second, &decoded))
        secret->assign(decoded);
    }
  }
}

// Helper method that extracts tokens from a successful reply.
// static
void GaiaOAuthFetcher::ParseOAuthWrapBridgeResponse(const std::string& data,
                                                    std::string* token,
                                                    std::string* expires_in) {
  using std::vector;
  using std::pair;
  using std::string;

  vector<pair<string, string> > tokens;
  base::SplitStringIntoKeyValuePairs(data, '=', '&', &tokens);
  for (vector<pair<string, string> >::iterator i = tokens.begin();
       i != tokens.end(); ++i) {
    if (i->first == "wrap_access_token") {
      std::string decoded;
      if (OAuthRequestSigner::Decode(i->second, &decoded))
        token->assign(decoded);
    } else if (i->first == "wrap_access_token_expires_in") {
      std::string decoded;
      if (OAuthRequestSigner::Decode(i->second, &decoded))
        expires_in->assign(decoded);
    }
  }
}

// Helper method that extracts tokens from a successful reply.
// static
void GaiaOAuthFetcher::ParseUserInfoResponse(const std::string& data,
                                             std::string* email_result) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (value->GetType() == base::Value::TYPE_DICTIONARY) {
    Value* email_value;
    DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());
    if (dict->Get("email", &email_value)) {
      if (email_value->GetType() == base::Value::TYPE_STRING) {
        StringValue* email = static_cast<StringValue*>(email_value);
        email->GetAsString(email_result);
      }
    }
  }
}

namespace {
// Based on Browser::OpenURLFromTab
void OpenGetOAuthTokenURL(Browser* browser,
                          const GURL& url,
                          const content::Referrer& referrer,
                          WindowOpenDisposition disposition,
                          content::PageTransition transition) {
  browser::NavigateParams params(
      browser,
      url,
      content::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.source_contents =
      browser->tabstrip_model()->GetTabContentsAt(
          browser->tabstrip_model()->GetWrapperIndex(NULL));
  params.referrer = referrer;
  params.disposition = disposition;
  params.tabstrip_add_types = TabStripModel::ADD_NONE;
  params.window_action = browser::NavigateParams::SHOW_WINDOW;
  params.window_bounds = gfx::Rect(380, 520);
  params.user_gesture = true;
  browser::Navigate(&params);
}
}

void GaiaOAuthFetcher::StartGetOAuthToken() {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";
  DCHECK(!popup_);

  request_type_ = OAUTH1_LOGIN;
  fetch_pending_ = true;
  registrar_.Add(this,
                 chrome::NOTIFICATION_COOKIE_CHANGED,
                 content::Source<Profile>(profile_));

  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  DCHECK(browser);

  OpenGetOAuthTokenURL(browser,
      MakeGetOAuthTokenUrl(GaiaUrls::GetInstance()->oauth1_login_scope(),
                           l10n_util::GetStringUTF8(IDS_PRODUCT_NAME)),
      content::Referrer(GURL("chrome://settings/personal"),
                        WebKit::WebReferrerPolicyDefault),
      NEW_POPUP,
      content::PAGE_TRANSITION_AUTO_BOOKMARK);
  popup_ = BrowserList::GetLastActiveWithProfile(profile_);
  DCHECK(popup_ && popup_ != browser);
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_CLOSING,
                 content::Source<Browser>(popup_));
}

void GaiaOAuthFetcher::StartOAuthLogin(
    const char* source,
    const char* service,
    const std::string& oauth1_access_token,
    const std::string& oauth1_access_token_secret) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  request_type_ = OAUTH1_LOGIN;
  // Must outlive fetcher_.
  request_body_ = MakeOAuthLoginBody(source, service, oauth1_access_token,
                                     oauth1_access_token_secret);
  request_headers_ = "";
  GURL url(GaiaUrls::GetInstance()->oauth1_login_url());
  fetcher_.reset(CreateGaiaFetcher(getter_, url, request_body_,
                                   request_headers_, false, this));
  fetch_pending_ = true;
  fetcher_->Start();
}

void GaiaOAuthFetcher::StartGetOAuthTokenRequest() {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  request_type_ = OAUTH1_REQUEST_TOKEN;
  // Must outlive fetcher_.
  request_body_ = "";
  request_headers_ = "";
  fetcher_.reset(CreateGaiaFetcher(getter_,
      MakeGetOAuthTokenUrl(GaiaUrls::GetInstance()->oauth1_login_scope(),
                           l10n_util::GetStringUTF8(IDS_PRODUCT_NAME)),
      std::string(),
      std::string(),
      true,           // send_cookies
      this));
  fetch_pending_ = true;
  fetcher_->Start();
}

void GaiaOAuthFetcher::StartOAuthGetAccessToken(
    const std::string& oauth1_request_token) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  request_type_ = OAUTH1_ALL_ACCESS_TOKEN;
  // Must outlive fetcher_.
  request_body_ = MakeOAuthGetAccessTokenBody(oauth1_request_token);
  request_headers_ = "";
  GURL url(GaiaUrls::GetInstance()->oauth_get_access_token_url());
  fetcher_.reset(CreateGaiaFetcher(getter_, url, request_body_,
                                   request_headers_, false, this));
  fetch_pending_ = true;
  fetcher_->Start();
}

void GaiaOAuthFetcher::StartOAuthWrapBridge(
    const std::string& oauth1_access_token,
    const std::string& oauth1_access_token_secret,
    const std::string& wrap_token_duration,
    const std::string& service_scope) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  request_type_ = OAUTH2_SERVICE_ACCESS_TOKEN;
  VLOG(1) << "Starting OAuthWrapBridge for: " << service_scope;
  std::string combined_scope = service_scope + " " +
      GaiaUrls::GetInstance()->oauth_wrap_bridge_user_info_scope();
  service_scope_ = service_scope;

  // Must outlive fetcher_.
  request_body_ = MakeOAuthWrapBridgeBody(
      oauth1_access_token,
      oauth1_access_token_secret,
      wrap_token_duration,
      combined_scope);

  request_headers_ = "";
  GURL url(GaiaUrls::GetInstance()->oauth_wrap_bridge_url());
  fetcher_.reset(CreateGaiaFetcher(getter_, url, request_body_,
                                   request_headers_, false, this));
  fetch_pending_ = true;
  fetcher_->Start();
}

void GaiaOAuthFetcher::StartUserInfo(const std::string& oauth2_access_token) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  request_type_ = USER_INFO;
  // Must outlive fetcher_.
  request_body_ = "";
  request_headers_ = "Authorization: OAuth " + oauth2_access_token;
  GURL url(GaiaUrls::GetInstance()->oauth_user_info_url());
  fetcher_.reset(CreateGaiaFetcher(getter_, url, request_body_,
                                   request_headers_, false, this));
  fetch_pending_ = true;
  fetcher_->Start();
}

void GaiaOAuthFetcher::StartOAuthRevokeAccessToken(const std::string& token,
                                                   const std::string& secret) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  request_type_ = OAUTH2_REVOKE_TOKEN;
  // Must outlive fetcher_.
  request_body_ = "";

  OAuthRequestSigner::Parameters empty_parameters;
  std::string auth_header;
  bool is_signed = OAuthRequestSigner::SignAuthHeader(
      GURL(GaiaUrls::GetInstance()->oauth_revoke_token_url()),
      empty_parameters,
      OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
      OAuthRequestSigner::GET_METHOD,
      "anonymous",
      "anonymous",
      token,
      secret,
      &auth_header);
  DCHECK(is_signed);
  request_headers_ = "Authorization: " + auth_header;
  GURL url(GaiaUrls::GetInstance()->oauth_revoke_token_url());
  fetcher_.reset(CreateGaiaFetcher(getter_, url, request_body_,
                                   request_headers_, false, this));
  fetch_pending_ = true;
  fetcher_->Start();
}

void GaiaOAuthFetcher::StartOAuthRevokeWrapToken(const std::string& token) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  // Must outlive fetcher_.
  request_body_ = "";

  request_headers_ = "Authorization: Bearer " + token;
  GURL url(GaiaUrls::GetInstance()->oauth_revoke_token_url());
  fetcher_.reset(CreateGaiaFetcher(getter_, url, request_body_,
                                   request_headers_, false, this));
  fetch_pending_ = true;
  fetcher_->Start();
}

// static
GoogleServiceAuthError GaiaOAuthFetcher::GenerateAuthError(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  if (!status.is_success()) {
    if (status.status() == net::URLRequestStatus::CANCELED) {
      return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
    } else {
      LOG(WARNING) << "Could not reach Google Accounts servers: errno "
                   << status.error();
      return GoogleServiceAuthError::FromConnectionError(status.error());
    }
  } else {
    LOG(WARNING) << "Unrecognized response from Google Accounts servers "
                 << "code " << response_code << " data " << data;
    return GoogleServiceAuthError(
        GoogleServiceAuthError::SERVICE_UNAVAILABLE);
  }

  NOTREACHED();
  return GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE);
}

void GaiaOAuthFetcher::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_COOKIE_CHANGED: {
      OnCookieChanged(content::Source<Profile>(source).ptr(),
                      content::Details<ChromeCookieDetails>(details).ptr());
      break;
    }
    case chrome::NOTIFICATION_BROWSER_CLOSING: {
      OnBrowserClosing(content::Source<Browser>(source).ptr(),
                       *(content::Details<bool>(details)).ptr());
      break;
    }
    default: {
      NOTREACHED();
    }
  }
}

namespace {
  const char* CauseName(net::CookieMonster::Delegate::ChangeCause cause) {
    switch (cause) {
      case net::CookieMonster::Delegate::CHANGE_COOKIE_EXPLICIT:
        return "CHANGE_COOKIE_EXPLICIT";
      case net::CookieMonster::Delegate::CHANGE_COOKIE_OVERWRITE:
        return "CHANGE_COOKIE_OVERWRITE";
      case net::CookieMonster::Delegate::CHANGE_COOKIE_EXPIRED:
        return "CHANGE_COOKIE_EXPIRED";
      case net::CookieMonster::Delegate::CHANGE_COOKIE_EVICTED:
        return "CHANGE_COOKIE_EVICTED";
      case net::CookieMonster::Delegate::CHANGE_COOKIE_EXPIRED_OVERWRITE:
        return "CHANGE_COOKIE_EXPIRED_OVERWRITE";
      default:
        return "<unknown>";
    }
  }
}

void GaiaOAuthFetcher::OnBrowserClosing(Browser* browser,
                                        bool detail) {
  if (browser == popup_) {
    consumer_->OnGetOAuthTokenFailure(
        GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
  }
  popup_ = NULL;
}

void GaiaOAuthFetcher::OnCookieChanged(Profile* profile,
                                       ChromeCookieDetails* cookie_details) {
  const net::CookieMonster::CanonicalCookie* canonical_cookie =
      cookie_details->cookie;
  if (canonical_cookie->Name() == kOAuthTokenCookie) {
    VLOG(1) << "OAuth1 request token cookie changed.";
    fetch_pending_ = false;
    OnGetOAuthTokenFetched(canonical_cookie->Value());
  }
}

void GaiaOAuthFetcher::OnGetOAuthTokenFetched(const std::string& token) {
  VLOG(1) << "OAuth1 request token fetched.";
  if (popup_) {
    Browser* popped_up = popup_;
    popup_ = NULL;
    popped_up->CloseWindow();
  }
  consumer_->OnGetOAuthTokenSuccess(token);
  if (ShouldAutoFetch(OAUTH1_ALL_ACCESS_TOKEN))
    StartOAuthGetAccessToken(token);
}

void GaiaOAuthFetcher::OnGetOAuthTokenUrlFetched(
    const net::ResponseCookies& cookies,
    const net::URLRequestStatus& status,
    int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    for (net::ResponseCookies::const_iterator iter = cookies.begin();
        iter != cookies.end(); ++iter) {
      net::CookieMonster::ParsedCookie cookie(*iter);
      if (cookie.Name() == kOAuthTokenCookie) {
        std::string token = cookie.Value();
        consumer_->OnGetOAuthTokenSuccess(token);
        if (ShouldAutoFetch(OAUTH1_ALL_ACCESS_TOKEN))
          StartOAuthGetAccessToken(token);
        return;
      }
    }
  }
  consumer_->OnGetOAuthTokenFailure(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
}

void GaiaOAuthFetcher::OnOAuthLoginFetched(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    std::string sid;
    std::string lsid;
    std::string auth;
    ParseOAuthLoginResponse(data, &sid, &lsid, &auth);
    // TODO(pastarmovj): Verify if all parameters are required.
    if (!sid.empty() && !lsid.empty() && !auth.empty()) {
      consumer_->OnOAuthLoginSuccess(sid, lsid, auth);
      return;
    }
  }
  // OAuthLogin returns error messages that are identical to ClientLogin,
  // so we use GaiaAuthFetcher::GenerateAuthError to parse the response
  // instead.
  consumer_->OnOAuthLoginFailure(
      GaiaAuthFetcher::GenerateOAuthLoginError(data, status));
}

void GaiaOAuthFetcher::OnOAuthGetAccessTokenFetched(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    VLOG(1) << "OAuth1 access token fetched.";
    std::string secret;
    std::string token;
    ParseOAuthGetAccessTokenResponse(data, &token, &secret);
    // TODO(pastarmovj): Verify if all parameters are required.
    if (!token.empty() && !secret.empty()) {
      consumer_->OnOAuthGetAccessTokenSuccess(token, secret);
      if (ShouldAutoFetch(OAUTH2_SERVICE_ACCESS_TOKEN))
        StartOAuthWrapBridge(
            token, secret, GaiaConstants::kGaiaOAuthDuration, service_scope_);
      return;
    }
  }
  consumer_->OnOAuthGetAccessTokenFailure(GenerateAuthError(data, status,
                                                            response_code));
}

void GaiaOAuthFetcher::OnOAuthWrapBridgeFetched(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    VLOG(1) << "OAuth2 access token fetched.";
    std::string token;
    std::string expires_in;
    ParseOAuthWrapBridgeResponse(data, &token, &expires_in);
    // TODO(pastarmovj): Verify if all parameters are required.
    if (!token.empty() && !expires_in.empty()) {
      consumer_->OnOAuthWrapBridgeSuccess(service_scope_, token, expires_in);
      if (ShouldAutoFetch(USER_INFO))
        StartUserInfo(token);
      return;
    }
  }
  consumer_->OnOAuthWrapBridgeFailure(service_scope_,
                                      GenerateAuthError(data, status,
                                                        response_code));
}

void GaiaOAuthFetcher::OnOAuthRevokeTokenFetched(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    // TODO(pastarmovj): Verify if all parameters are required.
    consumer_->OnOAuthRevokeTokenSuccess();
  } else {
    LOG(ERROR) << "Token revocation failure " << response_code << ": " << data;
    consumer_->OnOAuthRevokeTokenFailure(GenerateAuthError(data, status,
                                                           response_code));
  }
}

void GaiaOAuthFetcher::OnUserInfoFetched(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  if (status.is_success() && response_code == net::HTTP_OK) {
    std::string email;
    ParseUserInfoResponse(data, &email);
    // TODO(pastarmovj): Verify if all parameters are required.
    if (!email.empty()) {
      VLOG(1) << "GAIA user info fetched for " << email << ".";
      consumer_->OnUserInfoSuccess(email);
      return;
    }
  }
  consumer_->OnUserInfoFailure(GenerateAuthError(data, status,
                                                 response_code));
}

void GaiaOAuthFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  // Keep |fetcher_| around to avoid invalidating its |status| (accessed below).
  scoped_ptr<content::URLFetcher> current_fetcher(fetcher_.release());
  fetch_pending_ = false;
  std::string data;
  source->GetResponseAsString(&data);
  net::URLRequestStatus status = source->GetStatus();
  int response_code = source->GetResponseCode();

  switch (request_type_) {
    case OAUTH1_LOGIN:
      OnOAuthLoginFetched(data, status, response_code);
      break;
    case OAUTH1_REQUEST_TOKEN:
      OnGetOAuthTokenUrlFetched(source->GetCookies(), status, response_code);
      break;
    case OAUTH1_ALL_ACCESS_TOKEN:
      OnOAuthGetAccessTokenFetched(data, status, response_code);
      break;
    case OAUTH2_SERVICE_ACCESS_TOKEN:
      OnOAuthWrapBridgeFetched(data, status, response_code);
      break;
    case USER_INFO:
      OnUserInfoFetched(data, status, response_code);
      break;
    case OAUTH2_REVOKE_TOKEN:
      OnOAuthRevokeTokenFetched(data, status, response_code);
      break;
  }
}

bool GaiaOAuthFetcher::ShouldAutoFetch(RequestType fetch_step) {
  return fetch_step <= auto_fetch_limit_;
}
