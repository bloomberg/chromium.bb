// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/local_discovery/privet_confirm_api_flow.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"

namespace local_discovery {

namespace {
const char kCloudPrintOAuthHeaderFormat[] = "Authorization: Bearer %s";
const char kCookieURLFormat[] = "%s&xsrf=%s&user=%d";
}

PrivetConfirmApiCallFlow::PrivetConfirmApiCallFlow(
    net::URLRequestContextGetter* request_context,
    OAuth2TokenService* token_service,
    const GURL& automated_claim_url,
    const ResponseCallback& callback)
    : request_context_(request_context),
      token_service_(token_service),
      automated_claim_url_(automated_claim_url),
      callback_(callback) {
}

PrivetConfirmApiCallFlow::PrivetConfirmApiCallFlow(
    net::URLRequestContextGetter* request_context,
    int  user_index,
    const std::string& xsrf_token,
    const GURL& automated_claim_url,
    const ResponseCallback& callback)
    : request_context_(request_context),
      token_service_(NULL),
      user_index_(user_index),
      xsrf_token_(xsrf_token),
      automated_claim_url_(automated_claim_url),
      callback_(callback) {
}

PrivetConfirmApiCallFlow::~PrivetConfirmApiCallFlow() {
}

void PrivetConfirmApiCallFlow::Start() {
  if (UseOAuth2()) {
    OAuth2TokenService::ScopeSet oauth_scopes;
    oauth_scopes.insert(cloud_print::kCloudPrintAuth);
    oauth_request_ = token_service_->StartRequest(oauth_scopes, this);
  } else {
    GURL cookie_url(
        base::StringPrintf(kCookieURLFormat,
                           automated_claim_url_.spec().c_str(),
                           xsrf_token_.c_str(),
                           user_index_));

    CreateRequest(cookie_url);

    url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);

    url_fetcher_->Start();
  }
}

void PrivetConfirmApiCallFlow::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  CreateRequest(automated_claim_url_);

  std::string authorization_header =
      base::StringPrintf(kCloudPrintOAuthHeaderFormat, access_token.c_str());

  url_fetcher_->AddExtraRequestHeader(authorization_header);
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES);
  url_fetcher_->Start();
}

void PrivetConfirmApiCallFlow::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  callback_.Run(ERROR_TOKEN);
}

void PrivetConfirmApiCallFlow::CreateRequest(const GURL& url) {
  url_fetcher_.reset(net::URLFetcher::Create(url,
                                             net::URLFetcher::GET,
                                             this));

  url_fetcher_->SetRequestContext(request_context_.get());

  url_fetcher_->AddExtraRequestHeader(
      cloud_print::kChromeCloudPrintProxyHeader);
}

void PrivetConfirmApiCallFlow::OnURLFetchComplete(
    const net::URLFetcher* source) {
  // TODO(noamsml): Error logging.

  // TODO(noamsml): Extract this and PrivetURLFetcher::OnURLFetchComplete into
  // one helper method.
  std::string response_str;

  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS ||
      !source->GetResponseAsString(&response_str)) {
    callback_.Run(ERROR_NETWORK);
    return;
  }

  if (source->GetResponseCode() != net::HTTP_OK) {
    callback_.Run(ERROR_HTTP_CODE);
    return;
  }

  base::JSONReader reader;
  scoped_ptr<const base::Value> value(reader.Read(response_str));
  const base::DictionaryValue* dictionary_value;
  bool success = false;

  if (!value.get() || !value->GetAsDictionary(&dictionary_value)
      || !dictionary_value->GetBoolean(cloud_print::kSuccessValue, &success)) {
    callback_.Run(ERROR_MALFORMED_RESPONSE);
    return;
  }

  if (success) {
    callback_.Run(SUCCESS);
  } else {
    callback_.Run(ERROR_FROM_SERVER);
  }
}

}  // namespace local_discovery
