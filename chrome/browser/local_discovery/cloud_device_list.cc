// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/cloud_device_list.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "chrome/browser/local_discovery/gcd_constants.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"

namespace local_discovery {

namespace {
const char kKindDevicesList[] = "clouddevices#devicesListResponse";
}

CloudDeviceList::CloudDeviceList(CloudDeviceListDelegate* delegate)
    : delegate_(delegate) {
}

CloudDeviceList::~CloudDeviceList() {
}

void CloudDeviceList::OnGCDAPIFlowError(GCDApiFlow::Status status) {
  delegate_->OnDeviceListUnavailable();
}

void CloudDeviceList::OnGCDAPIFlowComplete(const base::DictionaryValue& value) {
  std::string kind;
  value.GetString(kGCDKeyKind, &kind);

  const base::ListValue* devices = NULL;
  if (kind != kKindDevicesList || !value.GetList("devices", &devices)) {
    delegate_->OnDeviceListUnavailable();
    return;
  }

  std::vector<CloudDeviceListDelegate::Device> result;
  for (base::ListValue::const_iterator i = devices->begin();
       i != devices->end(); i++) {
    base::DictionaryValue* device;
    CloudDeviceListDelegate::Device details;

    if (!(*i)->GetAsDictionary(&device))
      continue;

    if (!FillDeviceDetails(*device, &details))
      continue;

    result.push_back(details);
  }

  delegate_->OnDeviceListReady(result);
}

GURL CloudDeviceList::GetURL() {
  return cloud_devices::GetCloudDevicesRelativeURL("devices");
}

bool CloudDeviceList::FillDeviceDetails(
    const base::DictionaryValue& device_value,
    CloudDeviceListDelegate::Device* details) {
  if (!device_value.GetString("id", &details->id))
    return false;

  if (!device_value.GetString("displayName", &details->display_name) &&
      !device_value.GetString("systemName", &details->display_name)) {
    return false;
  }

  if (!device_value.GetString("deviceKind", &details->type))
    return false;

  // Non-essential.
  device_value.GetString("description", &details->description);

  return true;
}

}  // namespace local_discovery
