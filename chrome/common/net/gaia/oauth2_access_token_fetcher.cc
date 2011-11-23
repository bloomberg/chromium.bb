// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/gaia/oauth2_access_token_fetcher.h"

#include <algorithm>
#include <string>

#include "base/json/json_reader.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/net/http_return.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using content::URLFetcher;
using content::URLFetcherDelegate;
using net::ResponseCookies;
using net::URLRequestContextGetter;
using net::URLRequestStatus;

namespace {
static const char kGetAccessTokenBodyFormat[] =
    "client_id=%s&"
    "client_secret=%s"
    "grant_type=refresh_token&"
    "refresh_token=%s";

static const char kAccessTokenKey[] = "access_token";

static bool GetStringFromDictionary(const DictionaryValue* dict,
                                    const std::string& key,
                                    std::string* value) {
  Value* json_value;
  if (!dict->Get(key, &json_value))
    return false;
  if (json_value->GetType() != base::Value::TYPE_STRING)
    return false;

  StringValue* json_str_value = static_cast<StringValue*>(json_value);
  json_str_value->GetAsString(value);
  return true;
}

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
  URLFetcher* result = URLFetcher::Create(
      0, url,
      empty_body ? URLFetcher::GET : URLFetcher::POST,
      delegate);

  result->SetRequestContext(getter);
  result->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                       net::LOAD_DO_NOT_SAVE_COOKIES);

  if (!empty_body)
    result->SetUploadData("application/x-www-form-urlencoded", body);

  return result;
}
}  // namespace

OAuth2AccessTokenFetcher::OAuth2AccessTokenFetcher(
    OAuth2AccessTokenConsumer* consumer,
    URLRequestContextGetter* getter,
    const std::string& source)
    : consumer_(consumer),
      getter_(getter),
      source_(source),
      state_(INITIAL) { }

OAuth2AccessTokenFetcher::~OAuth2AccessTokenFetcher() { }

void OAuth2AccessTokenFetcher::CancelRequest() {
  fetcher_.reset();
}

void OAuth2AccessTokenFetcher::Start(const std::string& refresh_token) {
  refresh_token_ = refresh_token;
  StartGetAccessToken();
}

void OAuth2AccessTokenFetcher::StartGetAccessToken() {
  CHECK_EQ(INITIAL, state_);
  state_ = GET_ACCESS_TOKEN_STARTED;
  fetcher_.reset(CreateFetcher(
      getter_,
      MakeGetAccessTokenUrl(),
      MakeGetAccessTokenBody(refresh_token_),
      this));
  fetcher_->Start();  // OnURLFetchComplete will be called.
}

void OAuth2AccessTokenFetcher::EndGetAccessToken(const URLFetcher* source) {
  CHECK_EQ(GET_ACCESS_TOKEN_STARTED, state_);
  state_ = GET_ACCESS_TOKEN_DONE;

  URLRequestStatus status = source->GetStatus();
  if (!status.is_success()) {
    ReportFailure(CreateAuthError(status));
    return;
  }

  if (source->GetResponseCode() != RC_REQUEST_OK) {
    ReportFailure(GoogleServiceAuthError(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    return;
  }

  // The request was successfully fetched and it returned OK.
  // Parse out the access token.
  std::string access_token;
  if (!ParseGetAccessTokenResponse(source, &access_token)) {
    ReportFailure(GoogleServiceAuthError(
        GoogleServiceAuthError::UNEXPECTED_RESPONSE));
    return;
  }

  ReportSuccess(access_token);
}

void OAuth2AccessTokenFetcher::ReportSuccess(const std::string& access_token) {
  consumer_->OnGetTokenSuccess(access_token);
}

void OAuth2AccessTokenFetcher::ReportFailure(GoogleServiceAuthError error) {
  state_ = ERROR_STATE;
  consumer_->OnGetTokenFailure(error);
}

void OAuth2AccessTokenFetcher::OnURLFetchComplete(const URLFetcher* source) {
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
    const std::string& refresh_token) {
  return StringPrintf(
      kGetAccessTokenBodyFormat,
      net::EscapeUrlEncodedData(
          GaiaUrls::GetInstance()->oauth2_chrome_client_id(), true).c_str(),
      net::EscapeUrlEncodedData(
          GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
              true).c_str(),
      net::EscapeUrlEncodedData(refresh_token, true).c_str());
}

// static
bool OAuth2AccessTokenFetcher::ParseGetAccessTokenResponse(
    const URLFetcher* source,
    std::string* access_token) {
  CHECK(source);
  CHECK(access_token);
  std::string data;
  source->GetResponseAsString(&data);
  base::JSONReader reader;
  scoped_ptr<base::Value> value(reader.Read(data, false));
  if (!value.get() || value->GetType() != base::Value::TYPE_DICTIONARY)
    return false;

  DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());
  return GetStringFromDictionary(dict, kAccessTokenKey, access_token);
}
