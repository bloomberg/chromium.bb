// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/gaia/gaia_auth_fetcher.h"

#include <string>
#include <utility>
#include <vector>

#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/net/http_return.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

// TODO(chron): Add sourceless version of this formatter.
// static
const char GaiaAuthFetcher::kClientLoginFormat[] =
    "Email=%s&"
    "Passwd=%s&"
    "PersistentCookie=%s&"
    "accountType=%s&"
    "source=%s&"
    "service=%s";
// static
const char GaiaAuthFetcher::kClientLoginCaptchaFormat[] =
    "Email=%s&"
    "Passwd=%s&"
    "PersistentCookie=%s&"
    "accountType=%s&"
    "source=%s&"
    "service=%s&"
    "logintoken=%s&"
    "logincaptcha=%s";
// static
const char GaiaAuthFetcher::kIssueAuthTokenFormat[] =
    "SID=%s&"
    "LSID=%s&"
    "service=%s&"
    "Session=%s";
// static
const char GaiaAuthFetcher::kGetUserInfoFormat[] =
    "LSID=%s";
// static
const char GaiaAuthFetcher::kTokenAuthFormat[] =
    "auth=%s&"
    "continue=%s&"
    "source=%s";
// static
const char GaiaAuthFetcher::kMergeSessionFormat[] =
    "uberauth=%s&"
    "continue=%s&"
    "source=%s";

// static
const char GaiaAuthFetcher::kAccountDeletedError[] = "AccountDeleted";
const char GaiaAuthFetcher::kAccountDeletedErrorCode[] = "adel";
// static
const char GaiaAuthFetcher::kAccountDisabledError[] = "AccountDisabled";
const char GaiaAuthFetcher::kAccountDisabledErrorCode[] = "adis";
// static
const char GaiaAuthFetcher::kBadAuthenticationError[] = "BadAuthentication";
const char GaiaAuthFetcher::kBadAuthenticationErrorCode[] = "badauth";
// static
const char GaiaAuthFetcher::kCaptchaError[] = "CaptchaRequired";
const char GaiaAuthFetcher::kCaptchaErrorCode[] = "cr";
// static
const char GaiaAuthFetcher::kServiceUnavailableError[] =
    "ServiceUnavailable";
const char GaiaAuthFetcher::kServiceUnavailableErrorCode[] =
    "ire";
// static
const char GaiaAuthFetcher::kErrorParam[] = "Error";
// static
const char GaiaAuthFetcher::kErrorUrlParam[] = "Url";
// static
const char GaiaAuthFetcher::kCaptchaUrlParam[] = "CaptchaUrl";
// static
const char GaiaAuthFetcher::kCaptchaTokenParam[] = "CaptchaToken";

// static
const char GaiaAuthFetcher::kCookiePersistence[] = "true";
// static
// TODO(johnnyg): When hosted accounts are supported by sync,
// we can always use "HOSTED_OR_GOOGLE"
const char GaiaAuthFetcher::kAccountTypeHostedOrGoogle[] =
    "HOSTED_OR_GOOGLE";
const char GaiaAuthFetcher::kAccountTypeGoogle[] =
    "GOOGLE";

// static
const char GaiaAuthFetcher::kSecondFactor[] = "Info=InvalidSecondFactor";

GaiaAuthFetcher::GaiaAuthFetcher(GaiaAuthConsumer* consumer,
                                       const std::string& source,
                                       net::URLRequestContextGetter* getter)
    : consumer_(consumer),
      getter_(getter),
      source_(source),
      client_login_gurl_(GaiaUrls::GetInstance()->client_login_url()),
      issue_auth_token_gurl_(GaiaUrls::GetInstance()->issue_auth_token_url()),
      get_user_info_gurl_(GaiaUrls::GetInstance()->get_user_info_url()),
      token_auth_gurl_(GaiaUrls::GetInstance()->token_auth_url()),
      merge_session_gurl_(GaiaUrls::GetInstance()->merge_session_url()),
      fetch_pending_(false) {}

GaiaAuthFetcher::~GaiaAuthFetcher() {}

bool GaiaAuthFetcher::HasPendingFetch() {
  return fetch_pending_;
}

void GaiaAuthFetcher::CancelRequest() {
  fetcher_.reset();
  fetch_pending_ = false;
}

// static
content::URLFetcher* GaiaAuthFetcher::CreateGaiaFetcher(
    net::URLRequestContextGetter* getter,
    const std::string& body,
    const GURL& gaia_gurl,
    bool send_cookies,
    content::URLFetcherDelegate* delegate) {
  content::URLFetcher* to_return = content::URLFetcher::Create(
      0, gaia_gurl, content::URLFetcher::POST, delegate);
  to_return->SetRequestContext(getter);
  to_return->SetUploadData("application/x-www-form-urlencoded", body);

  // The Gaia token exchange requests do not require any cookie-based
  // identification as part of requests.  We suppress sending any cookies to
  // maintain a separation between the user's browsing and Chrome's internal
  // services.  Where such mixing is desired (MergeSession), it will be done
  // explicitly.
  if (!send_cookies)
    to_return->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES);

  return to_return;
}

// static
std::string GaiaAuthFetcher::MakeClientLoginBody(
    const std::string& username,
    const std::string& password,
    const std::string& source,
    const char* service,
    const std::string& login_token,
    const std::string& login_captcha,
    HostedAccountsSetting allow_hosted_accounts) {
  std::string encoded_username = net::EscapeUrlEncodedData(username, true);
  std::string encoded_password = net::EscapeUrlEncodedData(password, true);
  std::string encoded_login_token = net::EscapeUrlEncodedData(login_token,
                                                              true);
  std::string encoded_login_captcha = net::EscapeUrlEncodedData(login_captcha,
                                                                true);

  const char* account_type = allow_hosted_accounts == HostedAccountsAllowed ?
      kAccountTypeHostedOrGoogle :
      kAccountTypeGoogle;

  if (login_token.empty() || login_captcha.empty()) {
    return base::StringPrintf(kClientLoginFormat,
                              encoded_username.c_str(),
                              encoded_password.c_str(),
                              kCookiePersistence,
                              account_type,
                              source.c_str(),
                              service);
  }

  return base::StringPrintf(kClientLoginCaptchaFormat,
                            encoded_username.c_str(),
                            encoded_password.c_str(),
                            kCookiePersistence,
                            account_type,
                            source.c_str(),
                            service,
                            encoded_login_token.c_str(),
                            encoded_login_captcha.c_str());
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
std::string GaiaAuthFetcher::MakeGetUserInfoBody(const std::string& lsid) {
  std::string encoded_lsid = net::EscapeUrlEncodedData(lsid, true);
  return base::StringPrintf(kGetUserInfoFormat, encoded_lsid.c_str());
}

// static
std::string GaiaAuthFetcher::MakeTokenAuthBody(const std::string& auth_token,
                                               const std::string& continue_url,
                                               const std::string& source) {
  std::string encoded_auth_token = net::EscapeUrlEncodedData(auth_token, true);
  std::string encoded_continue_url = net::EscapeUrlEncodedData(continue_url,
                                                               true);
  std::string encoded_source = net::EscapeUrlEncodedData(source, true);
  return base::StringPrintf(kTokenAuthFormat,
                            encoded_auth_token.c_str(),
                            encoded_continue_url.c_str(),
                            encoded_source.c_str());
}

// static
std::string GaiaAuthFetcher::MakeMergeSessionBody(
    const std::string& auth_token,
    const std::string& continue_url,
    const std::string& source) {
  std::string encoded_auth_token = net::EscapeUrlEncodedData(auth_token, true);
  std::string encoded_continue_url = net::EscapeUrlEncodedData(continue_url,
                                                               true);
  std::string encoded_source = net::EscapeUrlEncodedData(source, true);
  return base::StringPrintf(kMergeSessionFormat,
                            encoded_auth_token.c_str(),
                            encoded_continue_url.c_str(),
                            encoded_source.c_str());
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

  vector<pair<string, string> > tokens;
  base::SplitStringIntoKeyValuePairs(data, '=', '\n', &tokens);
  for (vector<pair<string, string> >::iterator i = tokens.begin();
      i != tokens.end(); ++i) {
    if (i->first == "SID") {
      sid->assign(i->second);
    } else if (i->first == "LSID") {
      lsid->assign(i->second);
    } else if (i->first == "Auth") {
      token->assign(i->second);
    }
  }
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

  vector<pair<string, string> > tokens;
  base::SplitStringIntoKeyValuePairs(data, '=', '\n', &tokens);
  for (vector<pair<string, string> >::iterator i = tokens.begin();
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

void GaiaAuthFetcher::StartClientLogin(
    const std::string& username,
    const std::string& password,
    const char* const service,
    const std::string& login_token,
    const std::string& login_captcha,
    HostedAccountsSetting allow_hosted_accounts) {

  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  // This class is thread agnostic, so be sure to call this only on the
  // same thread each time.
  DVLOG(1) << "Starting new ClientLogin fetch for:" << username;

  // Must outlive fetcher_.
  request_body_ = MakeClientLoginBody(username,
                                      password,
                                      source_,
                                      service,
                                      login_token,
                                      login_captcha,
                                      allow_hosted_accounts);
  fetcher_.reset(CreateGaiaFetcher(getter_,
                                   request_body_,
                                   client_login_gurl_,
                                   false,
                                   this));
  fetch_pending_ = true;
  fetcher_->Start();
}

void GaiaAuthFetcher::StartIssueAuthToken(const std::string& sid,
                                          const std::string& lsid,
                                          const char* const service) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  DVLOG(1) << "Starting IssueAuthToken for: " << service;
  requested_service_ = service;
  request_body_ = MakeIssueAuthTokenBody(sid, lsid, service);
  fetcher_.reset(CreateGaiaFetcher(getter_,
                                   request_body_,
                                   issue_auth_token_gurl_,
                                   false,
                                   this));
  fetch_pending_ = true;
  fetcher_->Start();
}

void GaiaAuthFetcher::StartGetUserInfo(const std::string& lsid,
                                       const std::string& info_key) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  DVLOG(1) << "Starting GetUserInfo for lsid=" << lsid;
  request_body_ = MakeGetUserInfoBody(lsid);
  fetcher_.reset(CreateGaiaFetcher(getter_,
                                   request_body_,
                                   get_user_info_gurl_,
                                   false,
                                   this));
  fetch_pending_ = true;
  requested_info_key_ = info_key;
  fetcher_->Start();
}

void GaiaAuthFetcher::StartTokenAuth(const std::string& auth_token) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  DVLOG(1) << "Starting TokenAuth with auth_token=" << auth_token;

  // The continue URL is a required parameter of the TokenAuth API, but in this
  // case we don't actually need or want to navigate to it.  Setting it to
  // an arbitrary Google URL.
  std::string continue_url("http://www.google.com");
  request_body_ = MakeTokenAuthBody(auth_token, continue_url, source_);
  fetcher_.reset(CreateGaiaFetcher(getter_,
                                   request_body_,
                                   token_auth_gurl_,
                                   false,
                                   this));
  fetch_pending_ = true;
  fetcher_->Start();
}

void GaiaAuthFetcher::StartMergeSession(const std::string& auth_token) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  DVLOG(1) << "Starting MergeSession with auth_token=" << auth_token;

  // The continue URL is a required parameter of the MergeSession API, but in
  // this case we don't actually need or want to navigate to it.  Setting it to
  // an arbitrary Google URL.
  //
  // In order for the new session to be merged correctly, the server needs to
  // know what sessions already exist in the browser.  The fetcher needs to be
  // created such that it sends the cookies with the request, which is
  // different from all other requests the fetcher can make.
  std::string continue_url("http://www.google.com");
  request_body_ = MakeMergeSessionBody(auth_token, continue_url, source_);
  fetcher_.reset(CreateGaiaFetcher(getter_,
                                   request_body_,
                                   merge_session_gurl_,
                                   true,
                                   this));
  fetch_pending_ = true;
  fetcher_->Start();
}

// static
GoogleServiceAuthError GaiaAuthFetcher::GenerateAuthError(
    const std::string& data,
    const net::URLRequestStatus& status) {
  if (!status.is_success()) {
    if (status.status() == net::URLRequestStatus::CANCELED) {
      return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
    } else {
      DLOG(WARNING) << "Could not reach Google Accounts servers: errno "
          << status.error();
      return GoogleServiceAuthError::FromConnectionError(status.error());
    }
  } else {
    if (IsSecondFactorSuccess(data)) {
      return GoogleServiceAuthError(GoogleServiceAuthError::TWO_FACTOR);
    }

    std::string error;
    std::string url;
    std::string captcha_url;
    std::string captcha_token;
    ParseClientLoginFailure(data, &error, &url, &captcha_url, &captcha_token);
    DLOG(WARNING) << "ClientLogin failed with " << error;

    if (error == kCaptchaError) {
      GURL image_url(
          GaiaUrls::GetInstance()->captcha_url_prefix() + captcha_url);
      GURL unlock_url(url);
      return GoogleServiceAuthError::FromCaptchaChallenge(
          captcha_token, image_url, unlock_url);
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
    return GoogleServiceAuthError(
        GoogleServiceAuthError::SERVICE_UNAVAILABLE);
  }

  NOTREACHED();
  return GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE);
}

// static
GoogleServiceAuthError GaiaAuthFetcher::GenerateOAuthLoginError(
    const std::string& data,
    const net::URLRequestStatus& status) {
  if (!status.is_success()) {
    if (status.status() == net::URLRequestStatus::CANCELED) {
      return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
    } else {
      DLOG(WARNING) << "Could not reach Google Accounts servers: errno "
          << status.error();
      return GoogleServiceAuthError::FromConnectionError(status.error());
    }
  } else {
    if (IsSecondFactorSuccess(data)) {
      return GoogleServiceAuthError(GoogleServiceAuthError::TWO_FACTOR);
    }

    std::string error;
    std::string url;
    std::string captcha_url;
    std::string captcha_token;
    ParseClientLoginFailure(data, &error, &url, &captcha_url, &captcha_token);
    LOG(WARNING) << "OAuthLogin failed with " << error;

    if (error == kCaptchaErrorCode) {
      GURL image_url(
          GaiaUrls::GetInstance()->captcha_url_prefix() + captcha_url);
      GURL unlock_url(url);
      return GoogleServiceAuthError::FromCaptchaChallenge(
          captcha_token, image_url, unlock_url);
    }
    if (error == kAccountDeletedErrorCode)
      return GoogleServiceAuthError(GoogleServiceAuthError::ACCOUNT_DELETED);
    if (error == kAccountDisabledErrorCode)
      return GoogleServiceAuthError(GoogleServiceAuthError::ACCOUNT_DISABLED);
    if (error == kBadAuthenticationErrorCode) {
      return GoogleServiceAuthError(
          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
    }
    if (error == kServiceUnavailableErrorCode) {
      return GoogleServiceAuthError(
          GoogleServiceAuthError::SERVICE_UNAVAILABLE);
    }

    DLOG(WARNING) << "Incomprehensible response from Google Accounts servers.";
    return GoogleServiceAuthError(
        GoogleServiceAuthError::SERVICE_UNAVAILABLE);
  }

  NOTREACHED();
  return GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE);
}

void GaiaAuthFetcher::OnClientLoginFetched(const std::string& data,
                                           const net::URLRequestStatus& status,
                                           int response_code) {
  if (status.is_success() && response_code == RC_REQUEST_OK) {
    DVLOG(1) << "ClientLogin successful!";
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

void GaiaAuthFetcher::OnIssueAuthTokenFetched(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  if (status.is_success() && response_code == RC_REQUEST_OK) {
    // Only the bare token is returned in the body of this Gaia call
    // without any padding.
    consumer_->OnIssueAuthTokenSuccess(requested_service_, data);
  } else {
    consumer_->OnIssueAuthTokenFailure(requested_service_,
        GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnGetUserInfoFetched(
    const std::string& data,
    const net::URLRequestStatus& status,
    int response_code) {
  using std::vector;
  using std::string;
  using std::pair;

  if (status.is_success() && response_code == RC_REQUEST_OK) {
    vector<pair<string, string> > tokens;
    base::SplitStringIntoKeyValuePairs(data, '=', '\n', &tokens);
    for (vector<pair<string, string> >::iterator i = tokens.begin();
         i != tokens.end(); ++i) {
      if (i->first == requested_info_key_) {
        consumer_->OnGetUserInfoSuccess(i->first, i->second);
        return;
      }
    }
    consumer_->OnGetUserInfoKeyNotFound(requested_info_key_);
  } else {
    consumer_->OnGetUserInfoFailure(GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnTokenAuthFetched(const std::string& data,
                                         const net::URLRequestStatus& status,
                                         int response_code) {
  if (status.is_success() && response_code == RC_REQUEST_OK) {
    consumer_->OnTokenAuthSuccess(data);
  } else {
    consumer_->OnTokenAuthFailure(GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnMergeSessionFetched(const std::string& data,
                                            const net::URLRequestStatus& status,
                                            int response_code) {
  if (status.is_success() && response_code == RC_REQUEST_OK) {
    consumer_->OnMergeSessionSuccess(data);
  } else {
    consumer_->OnMergeSessionFailure(GenerateAuthError(data, status));
  }
}

void GaiaAuthFetcher::OnURLFetchComplete(const content::URLFetcher* source) {
  fetch_pending_ = false;
  const GURL& url = source->GetURL();
  const net::URLRequestStatus& status = source->GetStatus();
  int response_code = source->GetResponseCode();
  std::string data;
  source->GetResponseAsString(&data);
  if (url == client_login_gurl_) {
    OnClientLoginFetched(data, status, response_code);
  } else if (url == issue_auth_token_gurl_) {
    OnIssueAuthTokenFetched(data, status, response_code);
  } else if (url == get_user_info_gurl_) {
    OnGetUserInfoFetched(data, status, response_code);
  } else if (url == token_auth_gurl_) {
    OnTokenAuthFetched(data, status, response_code);
  } else if (url == merge_session_gurl_ ||
      (source && source->GetOriginalURL() == merge_session_gurl_)) {
    // MergeSession may redirect, so check the original URL of the fetcher.
    OnMergeSessionFetched(data, status, response_code);
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
