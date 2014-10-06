// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/cryptauth/cryptauth_api_call_flow.h"

#include "base/strings/string_number_conversions.h"
#include "net/url_request/url_fetcher.h"

namespace proximity_auth {

namespace {

const char kNoResponseBodyError[] = "No response body";
const char kRequestFailedError[] = "Request failed";
const char kMintAccessTokenError[] = "Could not mint access token";
const char kHttpStatusErrorPrefix[] = "HTTP status: ";

// Returns the OAuth2 scopes for authorization of CryptAuth API calls.
std::vector<std::string> GetScopes() {
  return std::vector<std::string>(1,
                                  "https://www.googleapis.com/auth/cryptauth");
}

}  // namespace

CryptAuthApiCallFlow::CryptAuthApiCallFlow(
    net::URLRequestContextGetter* context,
    const std::string& refresh_token,
    const std::string& access_token,
    const GURL& request_url)
    : OAuth2ApiCallFlow(context, refresh_token, access_token, GetScopes()),
      request_url_(request_url) {
}

CryptAuthApiCallFlow::~CryptAuthApiCallFlow() {
}

void CryptAuthApiCallFlow::Start(const std::string& serialized_request,
                                 ResultCallback result_callback,
                                 ErrorCallback error_callback) {
  serialized_request_ = serialized_request;
  result_callback_ = result_callback;
  error_callback_ = error_callback;
  OAuth2ApiCallFlow::Start();
}

GURL CryptAuthApiCallFlow::CreateApiCallUrl() {
  return request_url_;
}

std::string CryptAuthApiCallFlow::CreateApiCallBody() {
  return serialized_request_;
}

std::string CryptAuthApiCallFlow::CreateApiCallBodyContentType() {
  return "application/x-protobuf";
}

void CryptAuthApiCallFlow::ProcessApiCallSuccess(
    const net::URLFetcher* source) {
  std::string serialized_response;
  if (!source->GetResponseAsString(&serialized_response) ||
      serialized_response.empty()) {
    error_callback_.Run(kNoResponseBodyError);
    return;
  }
  result_callback_.Run(serialized_response);
}

void CryptAuthApiCallFlow::ProcessApiCallFailure(
    const net::URLFetcher* source) {
  std::string error_message;
  if (source->GetStatus().status() == net::URLRequestStatus::SUCCESS) {
    error_message =
        kHttpStatusErrorPrefix + base::IntToString(source->GetResponseCode());
  } else {
    error_message = kRequestFailedError;
  }
  error_callback_.Run(error_message);
}

void CryptAuthApiCallFlow::ProcessNewAccessToken(
    const std::string& access_token) {
  // OAuth2ApiCallFlow does not call this function currently, so we do not
  // handle this case.
  NOTIMPLEMENTED();
}

void CryptAuthApiCallFlow::ProcessMintAccessTokenFailure(
    const GoogleServiceAuthError& error) {
  error_callback_.Run(kMintAccessTokenError);
}

}  // proximity_auth
