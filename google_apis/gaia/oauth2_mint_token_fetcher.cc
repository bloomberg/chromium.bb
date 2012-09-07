// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_mint_token_fetcher.h"

#include <algorithm>
#include <string>

#include "base/json/json_reader.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using net::URLFetcher;
using net::URLFetcherDelegate;
using net::ResponseCookies;
using net::URLRequestContextGetter;
using net::URLRequestStatus;

namespace {
static const char kAuthorizationHeaderFormat[] =
    "Authorization: Bearer %s";
static const char kOAuth2IssueTokenBodyFormat[] =
    "force=true"
    "&response_type=token"
    "&scope=%s"
    "&client_id=%s"
    "&origin=%s";
static const char kAccessTokenKey[] = "token";

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
                                 const std::string& headers,
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

  if (!empty_body)
    result->SetUploadData("application/x-www-form-urlencoded", body);
  if (!headers.empty())
    result->SetExtraRequestHeaders(headers);

  return result;
}
}  // namespace

OAuth2MintTokenFetcher::OAuth2MintTokenFetcher(
    OAuth2MintTokenConsumer* consumer,
    URLRequestContextGetter* getter,
    const std::string& source)
    : consumer_(consumer),
      getter_(getter),
      source_(source),
      state_(INITIAL) { }

OAuth2MintTokenFetcher::~OAuth2MintTokenFetcher() { }

void OAuth2MintTokenFetcher::CancelRequest() {
  fetcher_.reset();
}

void OAuth2MintTokenFetcher::Start(const std::string& oauth_login_access_token,
                                   const std::string& client_id,
                                   const std::vector<std::string>& scopes,
                                   const std::string& origin) {
  oauth_login_access_token_ = oauth_login_access_token;
  client_id_ = client_id;
  scopes_ = scopes;
  origin_ = origin;
  StartMintToken();
}

void OAuth2MintTokenFetcher::StartMintToken() {
  CHECK_EQ(INITIAL, state_);
  state_ = MINT_TOKEN_STARTED;
  fetcher_.reset(CreateFetcher(
      getter_,
      MakeMintTokenUrl(),
      MakeMintTokenHeader(oauth_login_access_token_),
      MakeMintTokenBody(client_id_, scopes_, origin_),
      this));
  fetcher_->Start();  // OnURLFetchComplete will be called.
}

void OAuth2MintTokenFetcher::EndMintToken(const net::URLFetcher* source) {
  CHECK_EQ(MINT_TOKEN_STARTED, state_);
  state_ = MINT_TOKEN_DONE;

  URLRequestStatus status = source->GetStatus();
  if (!status.is_success()) {
    OnMintTokenFailure(CreateAuthError(status));
    return;
  }

  if (source->GetResponseCode() != net::HTTP_OK) {
    OnMintTokenFailure(GoogleServiceAuthError(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    return;
  }

  // The request was successfully fetched and it returned OK.
  // Parse out the access token.
  std::string access_token;
  ParseMintTokenResponse(source, &access_token);
  OnMintTokenSuccess(access_token);
}

void OAuth2MintTokenFetcher::OnMintTokenSuccess(
    const std::string& access_token) {
  consumer_->OnMintTokenSuccess(access_token);
}

void OAuth2MintTokenFetcher::OnMintTokenFailure(
    const GoogleServiceAuthError& error) {
  state_ = ERROR_STATE;
  consumer_->OnMintTokenFailure(error);
}

void OAuth2MintTokenFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  CHECK(source);
  CHECK_EQ(MINT_TOKEN_STARTED, state_);
  EndMintToken(source);
}

// static
GURL OAuth2MintTokenFetcher::MakeMintTokenUrl() {
  return GURL(GaiaUrls::GetInstance()->oauth2_issue_token_url());
}

// static
std::string OAuth2MintTokenFetcher::MakeMintTokenHeader(
    const std::string& access_token) {
  return StringPrintf(kAuthorizationHeaderFormat, access_token.c_str());
}

// static
std::string OAuth2MintTokenFetcher::MakeMintTokenBody(
    const std::string& client_id,
    const std::vector<std::string>& scopes,
    const std::string& origin) {
  return StringPrintf(
      kOAuth2IssueTokenBodyFormat,
      net::EscapeUrlEncodedData(JoinString(scopes, ','), true).c_str(),
      net::EscapeUrlEncodedData(client_id, true).c_str(),
      net::EscapeUrlEncodedData(origin, true).c_str());
}

// static
bool OAuth2MintTokenFetcher::ParseMintTokenResponse(
    const net::URLFetcher* source,
    std::string* access_token) {
  CHECK(source);
  CHECK(access_token);
  std::string data;
  source->GetResponseAsString(&data);
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value.get() || value->GetType() != base::Value::TYPE_DICTIONARY)
    return false;

  DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());
  return dict->GetString(kAccessTokenKey, access_token);
}
