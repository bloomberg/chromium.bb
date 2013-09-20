// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/local_discovery/cloud_print_base_api_flow.h"
#include "chrome/browser/local_discovery/privet_confirm_api_flow.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"

namespace local_discovery {

PrivetConfirmApiCallFlow::PrivetConfirmApiCallFlow(
    net::URLRequestContextGetter* request_context,
    OAuth2TokenService* token_service,
    const std::string& account_id,
    const GURL& automated_claim_url,
    const ResponseCallback& callback)
    : flow_(request_context,
            token_service,
            account_id,
            automated_claim_url,
            this),
      callback_(callback) {
}

PrivetConfirmApiCallFlow::PrivetConfirmApiCallFlow(
    net::URLRequestContextGetter* request_context,
    int  user_index,
    const std::string& xsrf_token,
    const GURL& automated_claim_url,
    const ResponseCallback& callback)
    : flow_(request_context,
            user_index,
            xsrf_token,
            automated_claim_url,
            this),
      callback_(callback) {
}

PrivetConfirmApiCallFlow::~PrivetConfirmApiCallFlow() {
}

void PrivetConfirmApiCallFlow::Start() {
  flow_.Start();
}

void PrivetConfirmApiCallFlow::OnCloudPrintAPIFlowError(
    CloudPrintBaseApiFlow* flow,
    CloudPrintBaseApiFlow::Status status) {
  callback_.Run(status);
}

void PrivetConfirmApiCallFlow::OnCloudPrintAPIFlowComplete(
    CloudPrintBaseApiFlow* flow,
    const base::DictionaryValue* value) {
  bool success = false;

  if (!value->GetBoolean(cloud_print::kSuccessValue, &success)) {
    callback_.Run(CloudPrintBaseApiFlow::ERROR_MALFORMED_RESPONSE);
    return;
  }

  if (success) {
    callback_.Run(CloudPrintBaseApiFlow::SUCCESS);
  } else {
    callback_.Run(CloudPrintBaseApiFlow::ERROR_FROM_SERVER);
  }
}

}  // namespace local_discovery
