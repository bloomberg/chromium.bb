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

namespace local_discovery {

namespace {
const char kCloudPrintAutomatedClaimURLFormat[] = "%s/confirm?token=%s";
const char kGCDAutomatedClaimURLFormat[] = "%s/registrationTickets/%s";
const char kGCDAutomatedClaimUploadData[] = "{ \"userEmail\": \"me\" }";
const char kGCDKindRegistrationTicket[] = "clouddevices#registrationTicket";
}

PrivetConfirmApiCallFlow::PrivetConfirmApiCallFlow(
    net::URLRequestContextGetter* request_context,
    OAuth2TokenService* token_service,
    const std::string& account_id,
    bool is_cloud_print,
    const GURL& base_url,
    const std::string& token,
    const ResponseCallback& callback)
    : is_cloud_print_(is_cloud_print),
      flow_(request_context,
            token_service,
            account_id,
            GURL(base::StringPrintf((is_cloud_print)
                                        ? kCloudPrintAutomatedClaimURLFormat
                                        : kGCDAutomatedClaimURLFormat,
                                    base_url.spec().c_str(),
                                    token.c_str())),
            this),
      callback_(callback) {}

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

  if (is_cloud_print_) {
    if (!value->GetBoolean(cloud_print::kSuccessValue, &success)) {
      callback_.Run(GCDBaseApiFlow::ERROR_MALFORMED_RESPONSE);
      return;
    }
  } else {
    std::string kind;
    value->GetString(kGCDKeyKind, &kind);
    success = (kind == kGCDKindRegistrationTicket);
  }

  if (success) {
    callback_.Run(GCDBaseApiFlow::SUCCESS);
  } else {
    callback_.Run(GCDBaseApiFlow::ERROR_FROM_SERVER);
  }
}

bool PrivetConfirmApiCallFlow::GCDIsCloudPrint() { return is_cloud_print_; }

net::URLFetcher::RequestType PrivetConfirmApiCallFlow::GetRequestType() {
  return (is_cloud_print_) ? net::URLFetcher::GET : net::URLFetcher::PATCH;
}

void PrivetConfirmApiCallFlow::GetUploadData(std::string* upload_type,
                                             std::string* upload_data) {
  if (is_cloud_print_) {
    *upload_type = "";
    *upload_data = "";
  } else {
    *upload_type = cloud_print::kContentTypeJSON;
    *upload_data = kGCDAutomatedClaimUploadData;
  }
}

}  // namespace local_discovery
