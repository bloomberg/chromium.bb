// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/gcd_api_flow.h"

#include "chrome/browser/local_discovery/gcd_api_flow_impl.h"
#include "chrome/browser/local_discovery/gcd_constants.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"

namespace local_discovery {

GCDApiFlow::Request::Request() {
}

GCDApiFlow::Request::~Request() {
}

net::URLFetcher::RequestType GCDApiFlow::Request::GetRequestType() {
  return net::URLFetcher::GET;
}

void GCDApiFlow::Request::GetUploadData(std::string* upload_type,
                                        std::string* upload_data) {
  *upload_type = std::string();
  *upload_data = std::string();
}

scoped_ptr<GCDApiFlow> GCDApiFlow::Create(
    net::URLRequestContextGetter* request_context,
    OAuth2TokenService* token_service,
    const std::string& account_id) {
  return scoped_ptr<GCDApiFlow>(
      new GCDApiFlowImpl(request_context, token_service, account_id));
}

GCDApiFlow::GCDApiFlow() {
}

GCDApiFlow::~GCDApiFlow() {
}

GCDApiFlowRequest::GCDApiFlowRequest() {
}

GCDApiFlowRequest::~GCDApiFlowRequest() {
}

std::string GCDApiFlowRequest::GetOAuthScope() {
  return cloud_devices::kCloudDevicesAuthScope;
}

std::vector<std::string> GCDApiFlowRequest::GetExtraRequestHeaders() {
  return std::vector<std::string>();
}

CloudPrintApiFlowRequest::CloudPrintApiFlowRequest() {
}

CloudPrintApiFlowRequest::~CloudPrintApiFlowRequest() {
}

std::string CloudPrintApiFlowRequest::GetOAuthScope() {
  return cloud_devices::kCloudPrintAuthScope;
}

std::vector<std::string> CloudPrintApiFlowRequest::GetExtraRequestHeaders() {
  return std::vector<std::string>(1, cloud_print::kChromeCloudPrintProxyHeader);
}

}  // namespace local_discovery
