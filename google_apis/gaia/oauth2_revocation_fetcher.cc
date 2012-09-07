// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_revocation_fetcher.h"

#include <algorithm>
#include <string>
#include <vector>

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

using net::ResponseCookies;
using net::URLFetcher;
using net::URLFetcherDelegate;
using net::URLRequestContextGetter;
using net::URLRequestStatus;

namespace {
static const char kOAuth2RevokeTokenURL[] =
    "https://www.googleapis.com/oauth2/v2/RevokeToken";

static const char kAuthorizationHeaderFormat[] =
    "Authorization: Bearer %s";

static const char kRevocationBodyFormat[] =
    "client_id=%s&origin=%s";

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
                                 const std::string& header,
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
  if (!header.empty())
    result->SetExtraRequestHeaders(header);

  if (!empty_body)
    result->SetUploadData("application/x-www-form-urlencoded", body);

  return result;
}
}  // namespace

OAuth2RevocationFetcher::OAuth2RevocationFetcher(
    OAuth2RevocationConsumer* consumer,
    URLRequestContextGetter* getter)
    : consumer_(consumer),
      getter_(getter),
      state_(INITIAL) { }

OAuth2RevocationFetcher::~OAuth2RevocationFetcher() { }

void OAuth2RevocationFetcher::CancelRequest() {
  fetcher_.reset();
}

void OAuth2RevocationFetcher::Start(const std::string& access_token,
                                    const std::string& client_id,
                                    const std::string& origin) {
  access_token_ = access_token;
  client_id_ = client_id;
  origin_ = origin;
  StartRevocation();
}

void OAuth2RevocationFetcher::StartRevocation() {
  CHECK_EQ(INITIAL, state_);
  state_ = REVOCATION_STARTED;
  fetcher_.reset(CreateFetcher(
      getter_,
      MakeRevocationUrl(),
      MakeRevocationHeader(access_token_),
      MakeRevocationBody(client_id_, origin_),
      this));
  fetcher_->Start();  // OnURLFetchComplete will be called.
}

void OAuth2RevocationFetcher::EndRevocation(const net::URLFetcher* source) {
  CHECK_EQ(REVOCATION_STARTED, state_);
  state_ = REVOCATION_DONE;

  URLRequestStatus status = source->GetStatus();
  if (!status.is_success()) {
    OnRevocationFailure(CreateAuthError(status));
    return;
  }

  if (source->GetResponseCode() != net::HTTP_NO_CONTENT) {
    OnRevocationFailure(GoogleServiceAuthError(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    return;
  }

  OnRevocationSuccess();
}

void OAuth2RevocationFetcher::OnRevocationSuccess() {
  consumer_->OnRevocationSuccess();
}

void OAuth2RevocationFetcher::OnRevocationFailure(
    const GoogleServiceAuthError& error) {
  state_ = ERROR_STATE;
  consumer_->OnRevocationFailure(error);
}

void OAuth2RevocationFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  CHECK(source);
  EndRevocation(source);
}

// static
GURL OAuth2RevocationFetcher::MakeRevocationUrl() {
  return GURL(kOAuth2RevokeTokenURL);
}

// static
std::string OAuth2RevocationFetcher::MakeRevocationHeader(
    const std::string& access_token) {
  return StringPrintf(kAuthorizationHeaderFormat, access_token.c_str());
}

// static
std::string OAuth2RevocationFetcher::MakeRevocationBody(
    const std::string& client_id,
    const std::string& origin) {
  std::string enc_client_id = net::EscapeUrlEncodedData(client_id, true);
  std::string enc_origin = net::EscapeUrlEncodedData(origin, true);
  return StringPrintf(
      kRevocationBodyFormat,
      enc_client_id.c_str(),
      enc_origin.c_str());
}
