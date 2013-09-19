// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/local_discovery/cloud_print_base_api_flow.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"

namespace local_discovery {

namespace {
const char kCloudPrintOAuthHeaderFormat[] = "Authorization: Bearer %s";
const char kXSRFURLParameterKey[] = "xsrf";
const char kUserURLParameterKey[] = "user";
}

CloudPrintBaseApiFlow::CloudPrintBaseApiFlow(
    net::URLRequestContextGetter* request_context,
    OAuth2TokenService* token_service,
    const std::string& account_id,
    const GURL& automated_claim_url,
    Delegate* delegate)
    : request_context_(request_context),
      token_service_(token_service),
      account_id_(account_id),
      user_index_(kAccountIndexUseOAuth2),
      url_(automated_claim_url),
      delegate_(delegate) {
}

CloudPrintBaseApiFlow::CloudPrintBaseApiFlow(
    net::URLRequestContextGetter* request_context,
    int  user_index,
    const std::string& xsrf_token,
    const GURL& automated_claim_url,
    Delegate* delegate)
    : request_context_(request_context),
      token_service_(NULL),
      account_id_(""),
      user_index_(user_index),
      xsrf_token_(xsrf_token),
      url_(automated_claim_url),
      delegate_(delegate) {
}

CloudPrintBaseApiFlow::CloudPrintBaseApiFlow(
    net::URLRequestContextGetter* request_context,
    int  user_index,
    const GURL& automated_claim_url,
    Delegate* delegate)
    : request_context_(request_context),
      token_service_(NULL),
      account_id_(""),
      user_index_(user_index),
      url_(automated_claim_url),
      delegate_(delegate) {
}

CloudPrintBaseApiFlow::~CloudPrintBaseApiFlow() {
}

void CloudPrintBaseApiFlow::Start() {
  if (UseOAuth2()) {
    OAuth2TokenService::ScopeSet oauth_scopes;
    oauth_scopes.insert(cloud_print::kCloudPrintAuth);
    oauth_request_ = token_service_->StartRequest(account_id_,
                                                  oauth_scopes,
                                                  this);
  } else {
    GURL cookie_url = url_;

    if (!xsrf_token_.empty()) {
      cookie_url = net::AppendQueryParameter(cookie_url,
                                             kXSRFURLParameterKey,
                                             xsrf_token_);
    }

    cookie_url = net::AppendQueryParameter(cookie_url,
                                           kUserURLParameterKey,
                                           base::IntToString(user_index_));

    CreateRequest(cookie_url);

    url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);

    url_fetcher_->Start();
  }
}

void CloudPrintBaseApiFlow::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  CreateRequest(url_);

  std::string authorization_header =
      base::StringPrintf(kCloudPrintOAuthHeaderFormat, access_token.c_str());

  url_fetcher_->AddExtraRequestHeader(authorization_header);
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES);
  url_fetcher_->Start();
}

void CloudPrintBaseApiFlow::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  delegate_->OnCloudPrintAPIFlowError(this, ERROR_TOKEN);
}

void CloudPrintBaseApiFlow::CreateRequest(const GURL& url) {
  url_fetcher_.reset(net::URLFetcher::Create(url,
                                             net::URLFetcher::GET,
                                             this));

  url_fetcher_->SetRequestContext(request_context_.get());

  url_fetcher_->AddExtraRequestHeader(
      cloud_print::kChromeCloudPrintProxyHeader);
}

void CloudPrintBaseApiFlow::OnURLFetchComplete(
    const net::URLFetcher* source) {
  // TODO(noamsml): Error logging.

  // TODO(noamsml): Extract this and PrivetURLFetcher::OnURLFetchComplete into
  // one helper method.
  std::string response_str;

  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS ||
      !source->GetResponseAsString(&response_str)) {
    delegate_->OnCloudPrintAPIFlowError(this, ERROR_NETWORK);
    return;
  }

  if (source->GetResponseCode() != net::HTTP_OK) {
    delegate_->OnCloudPrintAPIFlowError(this, ERROR_HTTP_CODE);
    return;
  }

  base::JSONReader reader;
  scoped_ptr<const base::Value> value(reader.Read(response_str));
  const base::DictionaryValue* dictionary_value = NULL;

  if (!value || !value->GetAsDictionary(&dictionary_value)) {
    delegate_->OnCloudPrintAPIFlowError(this, ERROR_MALFORMED_RESPONSE);
    return;
  }

  delegate_->OnCloudPrintAPIFlowComplete(this, dictionary_value);
}

}  // namespace local_discovery
