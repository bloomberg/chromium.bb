// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/gcd_base_api_flow.h"

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/local_discovery/gcd_constants.h"
#include "chrome/browser/local_discovery/gcd_url.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"

namespace local_discovery {

namespace {
const char kCloudPrintOAuthHeaderFormat[] = "Authorization: Bearer %s";
}

net::URLFetcher::RequestType GCDBaseApiFlow::Delegate::GetRequestType() {
  return net::URLFetcher::GET;
}

void GCDBaseApiFlow::Delegate::GetUploadData(std::string* upload_type,
                                             std::string* upload_data) {
  *upload_type = std::string();
  *upload_data = std::string();
}

GCDBaseApiFlow::GCDBaseApiFlow(net::URLRequestContextGetter* request_context,
                               OAuth2TokenService* token_service,
                               const std::string& account_id,
                               const GURL& automated_claim_url,
                               Delegate* delegate)
    : OAuth2TokenService::Consumer("cloud_print"),
      request_context_(request_context),
      token_service_(token_service),
      account_id_(account_id),
      url_(automated_claim_url),
      delegate_(delegate) {}

GCDBaseApiFlow::~GCDBaseApiFlow() {}

void GCDBaseApiFlow::Start() {
  OAuth2TokenService::ScopeSet oauth_scopes;
  if (delegate_->GCDIsCloudPrint()) {
    oauth_scopes.insert(cloud_print::kCloudPrintAuth);
  } else {
    oauth_scopes.insert(kGCDOAuthScope);
  }
  oauth_request_ =
      token_service_->StartRequest(account_id_, oauth_scopes, this);
}

void GCDBaseApiFlow::OnGetTokenSuccess(
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

void GCDBaseApiFlow::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  delegate_->OnGCDAPIFlowError(this, ERROR_TOKEN);
}

void GCDBaseApiFlow::CreateRequest(const GURL& url) {
  net::URLFetcher::RequestType request_type = delegate_->GetRequestType();

  url_fetcher_.reset(net::URLFetcher::Create(url, request_type, this));

  if (request_type != net::URLFetcher::GET) {
    std::string upload_type;
    std::string upload_data;
    delegate_->GetUploadData(&upload_type, &upload_data);
    url_fetcher_->SetUploadData(upload_type, upload_data);
  }

  url_fetcher_->SetRequestContext(request_context_.get());

  if (delegate_->GCDIsCloudPrint()) {
    url_fetcher_->AddExtraRequestHeader(
        cloud_print::kChromeCloudPrintProxyHeader);
  }
}

void GCDBaseApiFlow::OnURLFetchComplete(const net::URLFetcher* source) {
  // TODO(noamsml): Error logging.

  // TODO(noamsml): Extract this and PrivetURLFetcher::OnURLFetchComplete into
  // one helper method.
  std::string response_str;

  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS ||
      !source->GetResponseAsString(&response_str)) {
    delegate_->OnGCDAPIFlowError(this, ERROR_NETWORK);
    return;
  }

  if (source->GetResponseCode() != net::HTTP_OK) {
    delegate_->OnGCDAPIFlowError(this, ERROR_HTTP_CODE);
    return;
  }

  base::JSONReader reader;
  scoped_ptr<const base::Value> value(reader.Read(response_str));
  const base::DictionaryValue* dictionary_value = NULL;

  if (!value || !value->GetAsDictionary(&dictionary_value)) {
    delegate_->OnGCDAPIFlowError(this, ERROR_MALFORMED_RESPONSE);
    return;
  }

  delegate_->OnGCDAPIFlowComplete(this, dictionary_value);
}

}  // namespace local_discovery
