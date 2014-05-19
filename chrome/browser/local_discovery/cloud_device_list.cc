// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/cloud_device_list.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"

namespace local_discovery {

CloudDeviceList::CloudDeviceList(net::URLRequestContextGetter* request_context,
                                 OAuth2TokenService* token_service,
                                 const std::string& account_id,
                                 CloudDeviceListDelegate* delegate)
    : request_context_(request_context),
      delegate_(delegate),
      api_flow_(request_context_,
                token_service,
                account_id,
                cloud_devices::GetCloudDevicesRelativeURL("devices"),
                this) {
}

CloudDeviceList::~CloudDeviceList() {
}

void CloudDeviceList::Start() {
  api_flow_.Start();
}

void CloudDeviceList::OnGCDAPIFlowError(GCDBaseApiFlow* flow,
                                        GCDBaseApiFlow::Status status) {
  delegate_->OnDeviceListUnavailable();
}

void CloudDeviceList::OnGCDAPIFlowComplete(GCDBaseApiFlow* flow,
                                           const base::DictionaryValue* value) {
  const base::ListValue* devices;

  if (!value->GetList("devices", &devices)) {
    delegate_->OnDeviceListUnavailable();
    return;
  }

  for (base::ListValue::const_iterator i = devices->begin();
       i != devices->end(); i++) {
    base::DictionaryValue* device;
    CloudDeviceListDelegate::Device details;

    if (!(*i)->GetAsDictionary(&device))
      continue;

    if (!FillDeviceDetails(device, &details))
      continue;

    device_list_.push_back(details);
  }

  delegate_->OnDeviceListReady();
}

bool CloudDeviceList::GCDIsCloudPrint() {
  return false;
}

bool CloudDeviceList::FillDeviceDetails(
    const base::DictionaryValue* device_value,
    CloudDeviceListDelegate::Device* details) {
  if (!device_value->GetString("id", &details->id))
    return false;

  if (!device_value->GetString("displayName", &details->display_name) &&
      !device_value->GetString("systemName", &details->display_name)) {
    return false;
  }

  if (!device_value->GetString("deviceKind", &details->type))
    return false;

  // Non-essential.
  device_value->GetString("description", &details->description);

  return true;
}

}  // namespace local_discovery
