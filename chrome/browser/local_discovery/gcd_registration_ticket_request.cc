// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/gcd_registration_ticket_request.h"

#include "chrome/browser/local_discovery/gcd_constants.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"

namespace local_discovery {

namespace {

const char kUploadData[] = "{ \"userEmail\": \"me\" }";
const char kKindRegistrationTicket[] = "clouddevices#registrationTicket";
const char kGCDKeyId[] = "id";
}

GCDRegistrationTicketRequest::GCDRegistrationTicketRequest(
    const ResponseCallback& callback)
    : callback_(callback) {
}

GCDRegistrationTicketRequest::~GCDRegistrationTicketRequest() {
}

void GCDRegistrationTicketRequest::GetUploadData(std::string* upload_type,
                                                 std::string* upload_data) {
  *upload_data = kUploadData;

  // TODO(noamsml): Move this constant to cloud_devices component.
  *upload_type = cloud_print::kContentTypeJSON;
}

net::URLFetcher::RequestType GCDRegistrationTicketRequest::GetRequestType() {
  return net::URLFetcher::POST;
}

void GCDRegistrationTicketRequest::OnGCDAPIFlowError(
    GCDApiFlow::Status status) {
  callback_.Run(std::string());
}

void GCDRegistrationTicketRequest::OnGCDAPIFlowComplete(
    const base::DictionaryValue& value) {
  std::string kind;
  std::string id;
  value.GetString(kGCDKeyKind, &kind);
  if (kind == kKindRegistrationTicket)
    value.GetString(kGCDKeyId, &id);
  callback_.Run(id);
}

GURL GCDRegistrationTicketRequest::GetURL() {
  return cloud_devices::GetCloudDevicesRelativeURL("registrationTickets");
}

}  // namespace local_discovery
