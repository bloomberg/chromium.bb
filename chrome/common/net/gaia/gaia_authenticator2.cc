// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/gaia/gaia_authenticator2.h"

#include <string>

#include "base/string_split.h"
#include "base/string_util.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/net/http_return.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"
#include "third_party/libjingle/source/talk/base/urlencode.h"

// TODO(chron): Add sourceless version of this formatter.
// static
const char GaiaAuthenticator2::kClientLoginFormat[] =
    "Email=%s&"
    "Passwd=%s&"
    "PersistentCookie=%s&"
    "accountType=%s&"
    "source=%s&"
    "service=%s";
// static
const char GaiaAuthenticator2::kClientLoginCaptchaFormat[] =
    "Email=%s&"
    "Passwd=%s&"
    "PersistentCookie=%s&"
    "accountType=%s&"
    "source=%s&"
    "service=%s&"
    "logintoken=%s&"
    "logincaptcha=%s";
// static
const char GaiaAuthenticator2::kIssueAuthTokenFormat[] =
    "SID=%s&"
    "LSID=%s&"
    "service=%s&"
    "Session=true";

// static
const char GaiaAuthenticator2::kAccountDeletedError[] = "AccountDeleted";
// static
const char GaiaAuthenticator2::kAccountDisabledError[] = "AccountDisabled";
// static
const char GaiaAuthenticator2::kCaptchaError[] = "CaptchaRequired";
// static
const char GaiaAuthenticator2::kServiceUnavailableError[] =
    "ServiceUnavailable";
// static
const char GaiaAuthenticator2::kErrorParam[] = "Error";
// static
const char GaiaAuthenticator2::kErrorUrlParam[] = "Url";
// static
const char GaiaAuthenticator2::kCaptchaUrlParam[] = "CaptchaUrl";
// static
const char GaiaAuthenticator2::kCaptchaTokenParam[] = "CaptchaToken";
// static
const char GaiaAuthenticator2::kCaptchaUrlPrefix[] =
    "http://www.google.com/accounts/";

// static
const char GaiaAuthenticator2::kCookiePersistence[] = "true";
// static
const char GaiaAuthenticator2::kAccountType[] = "HOSTED_OR_GOOGLE";
// static
const char GaiaAuthenticator2::kSecondFactor[] = "Info=InvalidSecondFactor";

// TODO(chron): These urls are also in auth_response_handler.h.
// The URLs for different calls in the Google Accounts programmatic login API.
const char GaiaAuthenticator2::kClientLoginUrl[] =
    "https://www.google.com/accounts/ClientLogin";
const char GaiaAuthenticator2::kIssueAuthTokenUrl[] =
    "https://www.google.com/accounts/IssueAuthToken";

GaiaAuthenticator2::GaiaAuthenticator2(GaiaAuthConsumer* consumer,
                                       const std::string& source,
                                       URLRequestContextGetter* getter)
    : consumer_(consumer),
      getter_(getter),
      source_(source),
      client_login_gurl_(kClientLoginUrl),
      issue_auth_token_gurl_(kIssueAuthTokenUrl),
      fetch_pending_(false) {}

GaiaAuthenticator2::~GaiaAuthenticator2() {}

bool GaiaAuthenticator2::HasPendingFetch() {
  return fetch_pending_;
}

void GaiaAuthenticator2::CancelRequest() {
  fetcher_.reset();
  fetch_pending_ = false;
}

// static
URLFetcher* GaiaAuthenticator2::CreateGaiaFetcher(
    URLRequestContextGetter* getter,
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
std::string GaiaAuthenticator2::MakeClientLoginBody(
    const std::string& username,
    const std::string& password,
    const std::string& source,
    const char* service,
    const std::string& login_token,
    const std::string& login_captcha) {

  if (login_token.empty() || login_captcha.empty()) {
    return StringPrintf(kClientLoginFormat,
                        UrlEncodeString(username).c_str(),
                        UrlEncodeString(password).c_str(),
                        kCookiePersistence,
                        kAccountType,
                        source.c_str(),
                        service);
  }

  return StringPrintf(kClientLoginCaptchaFormat,
                      UrlEncodeString(username).c_str(),
                      UrlEncodeString(password).c_str(),
                      kCookiePersistence,
                      kAccountType,
                      source.c_str(),
                      service,
                      UrlEncodeString(login_token).c_str(),
                      UrlEncodeString(login_captcha).c_str());

}

// static
std::string GaiaAuthenticator2::MakeIssueAuthTokenBody(
    const std::string& sid,
    const std::string& lsid,
    const char* const service) {

  return StringPrintf(kIssueAuthTokenFormat,
                      UrlEncodeString(sid).c_str(),
                      UrlEncodeString(lsid).c_str(),
                      service);
}

// Helper method that extracts tokens from a successful reply.
// static
void GaiaAuthenticator2::ParseClientLoginResponse(const std::string& data,
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
void GaiaAuthenticator2::ParseClientLoginFailure(const std::string& data,
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

void GaiaAuthenticator2::StartClientLogin(const std::string& username,
                                          const std::string& password,
                                          const char* const service,
                                          const std::string& login_token,
                                          const std::string& login_captcha) {

  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  // This class is thread agnostic, so be sure to call this only on the
  // same thread each time.
  LOG(INFO) << "Starting new ClientLogin fetch for:" << username;

  // Must outlive fetcher_.
  request_body_ = MakeClientLoginBody(username,
                                      password,
                                      source_,
                                      service,
                                      login_token,
                                      login_captcha);
  fetcher_.reset(CreateGaiaFetcher(getter_,
                                   request_body_,
                                   client_login_gurl_,
                                   this));
  fetch_pending_ = true;
  fetcher_->Start();
}

void GaiaAuthenticator2::StartIssueAuthToken(const std::string& sid,
                                             const std::string& lsid,
                                             const char* const service) {

  DCHECK(!fetch_pending_) << "Tried to fetch two things at once!";

  LOG(INFO) << "Starting IssueAuthToken for: " << service;
  requested_service_ = service;
  request_body_ = MakeIssueAuthTokenBody(sid, lsid, service);
  fetcher_.reset(CreateGaiaFetcher(getter_,
                                   request_body_,
                                   issue_auth_token_gurl_,
                                   this));
  fetch_pending_ = true;
  fetcher_->Start();
}

// static
GoogleServiceAuthError GaiaAuthenticator2::GenerateAuthError(
    const std::string& data,
    const URLRequestStatus& status) {

  if (!status.is_success()) {
    if (status.status() == URLRequestStatus::CANCELED) {
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

    if (error == kCaptchaError) {
      GURL image_url(kCaptchaUrlPrefix + captcha_url);
      GURL unlock_url(url);
      return GoogleServiceAuthError::FromCaptchaChallenge(
          captcha_token, image_url, unlock_url);
    } else if (error == kAccountDeletedError) {
      return GoogleServiceAuthError(GoogleServiceAuthError::ACCOUNT_DELETED);
    } else if (error == kAccountDisabledError) {
      return GoogleServiceAuthError(GoogleServiceAuthError::ACCOUNT_DISABLED);
    } else if (error == kServiceUnavailableError) {
      return GoogleServiceAuthError(
          GoogleServiceAuthError::SERVICE_UNAVAILABLE);
    }

    return GoogleServiceAuthError(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  }

  NOTREACHED();
}

void GaiaAuthenticator2::OnClientLoginFetched(const std::string& data,
                                              const URLRequestStatus& status,
                                              int response_code) {

  if (status.is_success() && response_code == RC_REQUEST_OK) {
    LOG(INFO) << "ClientLogin successful!";
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

void GaiaAuthenticator2::OnIssueAuthTokenFetched(
    const std::string& data,
    const URLRequestStatus& status,
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

void GaiaAuthenticator2::OnURLFetchComplete(const URLFetcher* source,
                                            const GURL& url,
                                            const URLRequestStatus& status,
                                            int response_code,
                                            const ResponseCookies& cookies,
                                            const std::string& data) {
  fetch_pending_ = false;
  if (url == client_login_gurl_) {
    OnClientLoginFetched(data, status, response_code);
  } else if (url == issue_auth_token_gurl_) {
    OnIssueAuthTokenFetched(data, status, response_code);
  } else {
    NOTREACHED();
  }
}

// static
bool GaiaAuthenticator2::IsSecondFactorSuccess(
    const std::string& alleged_error) {
  return alleged_error.find(kSecondFactor) !=
      std::string::npos;
}
