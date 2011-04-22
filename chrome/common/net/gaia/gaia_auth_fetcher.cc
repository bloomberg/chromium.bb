// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/gaia/gaia_auth_fetcher.h"

#include <string>
#include <utility>
#include <vector>

#include "base/string_split.h"
#include "base/string_util.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/net/http_return.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "third_party/libjingle/source/talk/base/urlencode.h"

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
const char GaiaAuthFetcher::kCaptchaUrlPrefix[] =
    "http://www.google.com/accounts/";

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

// TODO(chron): These urls are also in auth_response_handler.h.
// The URLs for different calls in the Google Accounts programmatic login API.
const char GaiaAuthFetcher::kClientLoginUrl[] =
    "https://www.google.com/accounts/ClientLogin";
const char GaiaAuthFetcher::kIssueAuthTokenUrl[] =
    "https://www.google.com/accounts/IssueAuthToken";
const char GaiaAuthFetcher::kGetUserInfoUrl[] =
    "https://www.google.com/accounts/GetUserInfo";

GaiaAuthFetcher::GaiaAuthFetcher(GaiaAuthConsumer* consumer,
                                       const std::string& source,
                                       net::URLRequestContextGetter* getter)
    : consumer_(consumer),
      getter_(getter),
      source_(source),
      client_login_gurl_(kClientLoginUrl),
      issue_auth_token_gurl_(kIssueAuthTokenUrl),
      get_user_info_gurl_(kGetUserInfoUrl),
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
URLFetcher* GaiaAuthFetcher::CreateGaiaFetcher(
    net::URLRequestContextGetter* getter,
    const std::string& body,
    const GURL& gaia_gurl,
    URLFetcher::Delegate* delegate) {

  URLFetcher* to_return =
      URLFetcher::Create(0,
                         gaia_gurl,
                         URLFetcher::POST,
                         delegate);
  to_return->set_request_context(getter);
  to_return->set_load_flags(net::LOAD_DO_NOT_SEND_COOKIES);
  to_return->set_upload_data("application/x-www-form-urlencoded", body);
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
  std::string encoded_username = UrlEncodeString(username);
  std::string encoded_password = UrlEncodeString(password);
  std::string encoded_login_token = UrlEncodeString(login_token);
  std::string encoded_login_captcha = UrlEncodeString(login_captcha);

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
  std::string encoded_sid = UrlEncodeString(sid);
  std::string encoded_lsid = UrlEncodeString(lsid);

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
  std::string encoded_lsid = UrlEncodeString(lsid);
  return base::StringPrintf(kGetUserInfoFormat, encoded_lsid.c_str());
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
  VLOG(1) << "Starting new ClientLogin fetch for:" << username;

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
                                   this));
  fetch_pending_ = true;
  fetcher_->Start();
}

void GaiaAuthFetcher::StartIssueAuthToken(const std::string& sid,
                                          const std::string& lsid,
                                          const char* const service) {

  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  VLOG(1) << "Starting IssueAuthToken for: " << service;
  requested_service_ = service;
  request_body_ = MakeIssueAuthTokenBody(sid, lsid, service);
  fetcher_.reset(CreateGaiaFetcher(getter_,
                                   request_body_,
                                   issue_auth_token_gurl_,
                                   this));
  fetch_pending_ = true;
  fetcher_->Start();
}

void GaiaAuthFetcher::StartGetUserInfo(const std::string& lsid,
                                       const std::string& info_key) {
  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  VLOG(1) << "Starting GetUserInfo for lsid=" << lsid;
  request_body_ = MakeGetUserInfoBody(lsid);
  fetcher_.reset(CreateGaiaFetcher(getter_,
                                   request_body_,
                                   get_user_info_gurl_,
                                   this));
  fetch_pending_ = true;
  requested_info_key_ = info_key;
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
      LOG(WARNING) << "Could not reach Google Accounts servers: errno "
          << status.os_error();
      return GoogleServiceAuthError::FromConnectionError(status.os_error());
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
    LOG(WARNING) << "ClientLogin failed with " << error;

    if (error == kCaptchaError) {
      GURL image_url(kCaptchaUrlPrefix + captcha_url);
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

    LOG(WARNING) << "Incomprehensible response from Google Accounts servers.";
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

void GaiaAuthFetcher::OnURLFetchComplete(const URLFetcher* source,
                                         const GURL& url,
                                         const net::URLRequestStatus& status,
                                         int response_code,
                                         const ResponseCookies& cookies,
                                         const std::string& data) {
  fetch_pending_ = false;
  if (url == client_login_gurl_) {
    OnClientLoginFetched(data, status, response_code);
  } else if (url == issue_auth_token_gurl_) {
    OnIssueAuthTokenFetched(data, status, response_code);
  } else if (url == get_user_info_gurl_) {
    OnGetUserInfoFetched(data, status, response_code);
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
