// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_confirm_api_flow.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"
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
    const std::string& token,
    const ResponseCallback& callback)
    : callback_(callback), token_(token) {
}

PrivetConfirmApiCallFlow::~PrivetConfirmApiCallFlow() {
}

void PrivetConfirmApiCallFlow::OnGCDAPIFlowError(GCDApiFlow::Status status) {
  callback_.Run(status);
}

void PrivetConfirmApiCallFlow::OnGCDAPIFlowComplete(
    const base::DictionaryValue& value) {
  bool success = false;

  if (!value.GetBoolean(cloud_print::kSuccessValue, &success)) {
    callback_.Run(GCDApiFlow::ERROR_MALFORMED_RESPONSE);
    return;
  }

  if (success) {
    callback_.Run(GCDApiFlow::SUCCESS);
  } else {
    callback_.Run(GCDApiFlow::ERROR_FROM_SERVER);
  }
}

net::URLFetcher::RequestType PrivetConfirmApiCallFlow::GetRequestType() {
  return net::URLFetcher::GET;
}

GURL PrivetConfirmApiCallFlow::GetURL() {
  return GetConfirmFlowUrl(token_);
}

}  // namespace local_discovery
