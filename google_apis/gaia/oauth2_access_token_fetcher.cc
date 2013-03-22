// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_access_token_fetcher.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using net::ResponseCookies;
using net::URLFetcher;
using net::URLFetcherDelegate;
using net::URLRequestContextGetter;
using net::URLRequestStatus;

namespace {
static const char kGetAccessTokenBodyFormat[] =
    "client_id=%s&"
    "client_secret=%s&"
    "grant_type=refresh_token&"
    "refresh_token=%s";

static const char kGetAccessTokenBodyWithScopeFormat[] =
    "client_id=%s&"
    "client_secret=%s&"
    "grant_type=refresh_token&"
    "refresh_token=%s&"
    "scope=%s";

static const char kAccessTokenKey[] = "access_token";
static const char kExpiresInKey[] = "expires_in";

static GoogleServiceAuthError CreateAuthError(URLRequestStatus status) {
  CHECK(!status.is_success());
  if (status.status() == URLRequestStatus::CANCELED) {
    return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
  } else {
    DLOG(WARNING) << "Could not reach Google Accounts servers: errno "
                  << status.error();
    return GoogleServiceAuthError::FromConnectionError(status.error());
  }
}

static URLFetcher* CreateFetcher(URLRequestContextGetter* getter,
                                 const GURL& url,
                                 const std::string& body,
                                 URLFetcherDelegate* delegate) {
  bool empty_body = body.empty();
  URLFetcher* result = net::URLFetcher::Create(
      0, url,
      empty_body ? URLFetcher::GET : URLFetcher::POST,
      delegate);

  result->SetRequestContext(getter);
  result->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                       net::LOAD_DO_NOT_SAVE_COOKIES);
  // Fetchers are sometimes cancelled because a network change was detected,
  // especially at startup and after sign-in on ChromeOS. Retrying once should
  // be enough in those cases; let the fetcher retry up to 3 times just in case.
  // http://crbug.com/163710
  result->SetAutomaticallyRetryOnNetworkChanges(3);

  if (!empty_body)
    result->SetUploadData("application/x-www-form-urlencoded", body);

  return result;
}
}  // namespace

OAuth2AccessTokenFetcher::OAuth2AccessTokenFetcher(
    OAuth2AccessTokenConsumer* consumer,
    URLRequestContextGetter* getter)
    : consumer_(consumer),
      getter_(getter),
      state_(INITIAL) { }

OAuth2AccessTokenFetcher::~OAuth2AccessTokenFetcher() { }

void OAuth2AccessTokenFetcher::CancelRequest() {
  fetcher_.reset();
}

void OAuth2AccessTokenFetcher::Start(const std::string& client_id,
                                     const std::string& client_secret,
                                     const std::string& refresh_token,
                                     const std::vector<std::string>& scopes) {
  client_id_ = client_id;
  client_secret_ = client_secret;
  refresh_token_ = refresh_token;
  scopes_ = scopes;
  StartGetAccessToken();
}

void OAuth2AccessTokenFetcher::StartGetAccessToken() {
  CHECK_EQ(INITIAL, state_);
  state_ = GET_ACCESS_TOKEN_STARTED;
  fetcher_.reset(CreateFetcher(
      getter_,
      MakeGetAccessTokenUrl(),
      MakeGetAccessTokenBody(
          client_id_, client_secret_, refresh_token_, scopes_),
      this));
  fetcher_->Start();  // OnURLFetchComplete will be called.
}

void OAuth2AccessTokenFetcher::EndGetAccessToken(
    const net::URLFetcher* source) {
  CHECK_EQ(GET_ACCESS_TOKEN_STARTED, state_);
  state_ = GET_ACCESS_TOKEN_DONE;

  URLRequestStatus status = source->GetStatus();
  if (!status.is_success()) {
    OnGetTokenFailure(CreateAuthError(status));
    return;
  }

  // HTTP_FORBIDDEN (403) is treated as temporary error, because it may be
  // '403 Rate Limit Exeeded.'
  if (source->GetResponseCode() == net::HTTP_FORBIDDEN) {
    OnGetTokenFailure(GoogleServiceAuthError(
        GoogleServiceAuthError::SERVICE_UNAVAILABLE));
    return;
  }

  // The other errors are treated as permanent error.
  if (source->GetResponseCode() != net::HTTP_OK) {
    OnGetTokenFailure(GoogleServiceAuthError(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    return;
  }

  // The request was successfully fetched and it returned OK.
  // Parse out the access token and the expiration time.
  std::string access_token;
  int expires_in;
  if (!ParseGetAccessTokenResponse(source, &access_token, &expires_in)) {
    DLOG(WARNING) << "Response doesn't match expected format";
    OnGetTokenFailure(
        GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
    return;
  }
  // The token will expire in |expires_in| seconds. Take a 10% error margin to
  // prevent reusing a token too close to its expiration date.
  OnGetTokenSuccess(
      access_token,
      base::Time::Now() + base::TimeDelta::FromSeconds(9 * expires_in / 10));
}

void OAuth2AccessTokenFetcher::OnGetTokenSuccess(
    const std::string& access_token,
    const base::Time& expiration_time) {
  consumer_->OnGetTokenSuccess(access_token, expiration_time);
}

void OAuth2AccessTokenFetcher::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  state_ = ERROR_STATE;
  consumer_->OnGetTokenFailure(error);
}

void OAuth2AccessTokenFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  CHECK(source);
  CHECK(state_ == GET_ACCESS_TOKEN_STARTED);
  EndGetAccessToken(source);
}

// static
GURL OAuth2AccessTokenFetcher::MakeGetAccessTokenUrl() {
  return GURL(GaiaUrls::GetInstance()->oauth2_token_url());
}

// static
std::string OAuth2AccessTokenFetcher::MakeGetAccessTokenBody(
    const std::string& client_id,
    const std::string& client_secret,
    const std::string& refresh_token,
    const std::vector<std::string>& scopes) {
  std::string enc_client_id = net::EscapeUrlEncodedData(client_id, true);
  std::string enc_client_secret =
      net::EscapeUrlEncodedData(client_secret, true);
  std::string enc_refresh_token =
      net::EscapeUrlEncodedData(refresh_token, true);
  if (scopes.empty()) {
    return base::StringPrintf(
        kGetAccessTokenBodyFormat,
        enc_client_id.c_str(),
        enc_client_secret.c_str(),
        enc_refresh_token.c_str());
  } else {
    std::string scopes_string = JoinString(scopes, ' ');
    return base::StringPrintf(
        kGetAccessTokenBodyWithScopeFormat,
        enc_client_id.c_str(),
        enc_client_secret.c_str(),
        enc_refresh_token.c_str(),
        net::EscapeUrlEncodedData(scopes_string, true).c_str());
  }
}

// static
bool OAuth2AccessTokenFetcher::ParseGetAccessTokenResponse(
    const net::URLFetcher* source,
    std::string* access_token,
    int* expires_in) {
  CHECK(source);
  CHECK(access_token);
  std::string data;
  source->GetResponseAsString(&data);
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value.get() || value->GetType() != base::Value::TYPE_DICTIONARY)
    return false;

  DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());
  return dict->GetString(kAccessTokenKey, access_token) &&
      dict->GetInteger(kExpiresInKey, expires_in);
}
