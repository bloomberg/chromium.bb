// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/gaia/gaia_authenticator2.h"

#include <string>

#include "base/string_split.h"
#include "base/string_util.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
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
const char GaiaAuthenticator2::kCookiePersistence[] = "true";
// static
const char GaiaAuthenticator2::kAccountType[] = "HOSTED_OR_GOOGLE";
// static
const char GaiaAuthenticator2::kChromeOSSource[] = "chromeos";
// static
// Service name for Gaia Contacts API. API is used to get user's image.
const char GaiaAuthenticator2::kContactsService[] = "cp";
// static
const char GaiaAuthenticator2::kSecondFactor[] = "Info=InvalidSecondFactor";

// TODO(chron): These urls are also in auth_response_handler.h.
// The URLs for different calls in the Google Accounts programmatic login API.
const char GaiaAuthenticator2::kClientLoginUrl[] =
    "https://www.google.com/accounts/ClientLogin";
const char GaiaAuthenticator2::kIssueAuthTokenUrl[] =
    "https://www.google.com/accounts/IssueAuthToken";
// TODO(chron): Fix this URL not to hardcode source
// TODO(cmasone): make sure that using an http:// URL in the "continue"
// parameter here doesn't open the system up to attack long-term.
const char GaiaAuthenticator2::kTokenAuthUrl[] =
    "https://www.google.com/accounts/TokenAuth?"
        "continue=http://www.google.com/webhp&source=chromeos&auth=";

GaiaAuthenticator2::GaiaAuthenticator2(GaiaAuthConsumer* consumer,
                                       const std::string& source,
                                       URLRequestContextGetter* getter)
    : consumer_(consumer),
      getter_(getter),
      source_(source),
      client_login_gurl_(kClientLoginUrl),
      fetch_pending_(false){}

GaiaAuthenticator2::~GaiaAuthenticator2() {}

bool GaiaAuthenticator2::HasPendingFetch() {
  return fetch_pending_;
}

void GaiaAuthenticator2::CancelRequest() {
  fetcher_.reset();
  fetch_pending_ = false;
}

// static
URLFetcher* GaiaAuthenticator2::CreateClientLoginFetcher(
    URLRequestContextGetter* getter,
    const std::string& body,
    const GURL& client_login_gurl,
    URLFetcher::Delegate* delegate) {

  URLFetcher* to_return =
      URLFetcher::Create(0,
                         client_login_gurl,
                         URLFetcher::POST,
                         delegate);
  to_return->set_request_context(getter);
  to_return->set_load_flags(net::LOAD_DO_NOT_SEND_COOKIES);
  to_return->set_upload_data("application/x-www-form-urlencoded", body);
  return to_return;
}

std::string GaiaAuthenticator2::GenerateRequestBody(
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

// Helper method that extracts tokens from a successful reply, and saves them
// in the right fields.
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

void GaiaAuthenticator2::StartClientLogin(const std::string& username,
                                          const std::string& password,
                                          const char* service,
                                          const std::string& login_token,
                                          const std::string& login_captcha) {

  // This class is thread agnostic, so be sure to call this only on the
  // same thread each time.
  LOG(INFO) << "Starting new ClientLogin fetch.";

  // Must outlive fetcher_.
  request_body_ = GenerateRequestBody(username,
                                      password,
                                      source_,
                                      service,
                                      login_token,
                                      login_captcha);

  fetcher_.reset(CreateClientLoginFetcher(getter_,
                                          request_body_,
                                          client_login_gurl_,
                                          this));
  fetch_pending_ = true;
  fetcher_->Start();
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
    return;
  }

  GaiaAuthConsumer::ClientLoginError error;
  error.data = data;

  if (!status.is_success()) {
    if (status.status() == URLRequestStatus::CANCELED) {
      error.code = GaiaAuthConsumer::REQUEST_CANCELED;
    } else {
      error.code = GaiaAuthConsumer::NETWORK_ERROR;
      error.network_error = status.os_error();
      LOG(WARNING) << "Could not reach Google Accounts servers: errno "
        << status.os_error();
    }
  } else {
    if (IsSecondFactorSuccess(data)) {
      error.code = GaiaAuthConsumer::TWO_FACTOR;
    } else {
      error.code = GaiaAuthConsumer::PERMISSION_DENIED;
    }
  }

  consumer_->OnClientLoginFailure(error);
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
    return;
  }
  NOTREACHED();
}

// static
bool GaiaAuthenticator2::IsSecondFactorSuccess(
    const std::string& alleged_error) {
  return alleged_error.find(kSecondFactor) !=
      std::string::npos;
}
