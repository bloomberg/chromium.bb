// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/webrtc/send_command_request.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "net/base/url_util.h"

using local_discovery::GCDApiFlow;
using local_discovery::GCDApiFlowRequest;

namespace {

const char kContentTypeJSON[] = "application/json";
const char kCommandTimeoutMs[] = "20000";

}  // namespace

SendCommandRequest::SendCommandRequest(const base::DictionaryValue* request,
                                       Delegate* delegate)
    : delegate_(delegate) {
  base::JSONWriter::Write(*request, &upload_data_);
  DCHECK(delegate_);
}

net::URLFetcher::RequestType SendCommandRequest::GetRequestType() {
  return net::URLFetcher::POST;
}

void SendCommandRequest::GetUploadData(std::string* upload_type,
                                       std::string* upload_data) {
  *upload_type = kContentTypeJSON;
  *upload_data = upload_data_;
}

void SendCommandRequest::OnGCDAPIFlowError(GCDApiFlow::Status status) {
  delegate_->OnCommandFailed();
}

void SendCommandRequest::OnGCDAPIFlowComplete(
    const base::DictionaryValue& value) {
  delegate_->OnCommandSucceeded(value);
}

GURL SendCommandRequest::GetURL() {
  GURL url = cloud_devices::GetCloudDevicesRelativeURL("commands");
  url = net::AppendQueryParameter(url, "expireInMs", kCommandTimeoutMs);
  url = net::AppendQueryParameter(url, "responseAwaitMs", kCommandTimeoutMs);
  return url;
}
