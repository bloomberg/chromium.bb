// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_confirm_api_flow.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/local_discovery/gcd_base_api_flow.h"
#include "chrome/browser/local_discovery/gcd_constants.h"
#include "chrome/browser/local_discovery/privet_constants.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "net/base/url_util.h"

namespace local_discovery {

namespace {

GURL GetConfirmFlowUrl(const std::string& token) {
  return net::AppendQueryParameter(
      cloud_devices::GetCloudPrintRelativeURL("confirm"), "token", token);
}

}  // namespace

PrivetConfirmApiCallFlow::PrivetConfirmApiCallFlow(
    net::URLRequestContextGetter* request_context,
    OAuth2TokenService* token_service,
    const std::string& account_id,
    const std::string& token,
    const ResponseCallback& callback)
    : flow_(request_context,
            token_service,
            account_id,
            this),
      callback_(callback),
      token_(token) {
}

PrivetConfirmApiCallFlow::~PrivetConfirmApiCallFlow() {
}

void PrivetConfirmApiCallFlow::Start() {
  flow_.Start();
}

void PrivetConfirmApiCallFlow::OnGCDAPIFlowError(
    GCDBaseApiFlow* flow,
    GCDBaseApiFlow::Status status) {
  callback_.Run(status);
}

void PrivetConfirmApiCallFlow::OnGCDAPIFlowComplete(
    GCDBaseApiFlow* flow,
    const base::DictionaryValue* value) {
  bool success = false;

  if (!value->GetBoolean(cloud_print::kSuccessValue, &success)) {
    callback_.Run(GCDBaseApiFlow::ERROR_MALFORMED_RESPONSE);
    return;
  }

  if (success) {
    callback_.Run(GCDBaseApiFlow::SUCCESS);
  } else {
    callback_.Run(GCDBaseApiFlow::ERROR_FROM_SERVER);
  }
}

net::URLFetcher::RequestType PrivetConfirmApiCallFlow::GetRequestType() {
  return net::URLFetcher::GET;
}

GURL PrivetConfirmApiCallFlow::GetURL() {
  return GetConfirmFlowUrl(token_);
}

}  // namespace local_discovery
