// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/cryptauth/cryptauth_api_call_flow.h"

#include "base/strings/string_number_conversions.h"
#include "net/url_request/url_fetcher.h"

namespace proximity_auth {

namespace {

const char kResponseBodyError[] = "Failed to get response body";
const char kRequestFailedError[] = "Request failed";
const char kHttpStatusErrorPrefix[] = "HTTP status: ";

}  // namespace

CryptAuthApiCallFlow::CryptAuthApiCallFlow(const GURL& request_url)
    : request_url_(request_url) {
}

CryptAuthApiCallFlow::~CryptAuthApiCallFlow() {
}

void CryptAuthApiCallFlow::Start(net::URLRequestContextGetter* context,
                                 const std::string& access_token,
                                 const std::string& serialized_request,
                                 const ResultCallback& result_callback,
                                 const ErrorCallback& error_callback) {
  serialized_request_ = serialized_request;
  result_callback_ = result_callback;
  error_callback_ = error_callback;
  OAuth2ApiCallFlow::Start(context, access_token);
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
  if (!source->GetResponseAsString(&serialized_response)) {
    error_callback_.Run(kResponseBodyError);
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

}  // proximity_auth
