// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/device_description.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/local_discovery/privet_constants.h"
#include "chrome/common/local_discovery/service_discovery_client.h"

namespace local_discovery {

namespace {

std::string GetValueByName(const std::vector<std::string>& metadata,
                           const std::string& name) {
  std::string prefix(name + "=");
  for (const std::string& record : metadata) {
    if (StartsWithASCII(record, prefix, false)) {
      return record.substr(prefix.size());
    }
  }
  return std::string();
}

}  // namespace

DeviceDescription::DeviceDescription() : version(0) {
}

DeviceDescription::DeviceDescription(
    const ServiceDescription& service_description) {
  address = service_description.address;

  const std::vector<std::string>& metadata = service_description.metadata;
  if (!base::StringToInt(GetValueByName(metadata, kPrivetTxtKeyVersion),
                         &version)) {
    version = 0;
  }
  name = GetValueByName(metadata, kPrivetTxtKeyName);
  description = GetValueByName(metadata, kPrivetTxtKeyDescription);
  if (version >= 3) {
    type = GetValueByName(metadata, kPrivetTxtKeyDevicesClass);
    id = GetValueByName(metadata, kPrivetTxtKeyGcdID);
  } else {
    type = GetValueByName(metadata, kPrivetTxtKeyType);
    id = GetValueByName(metadata, kPrivetTxtKeyID);
  }
}

DeviceDescription::~DeviceDescription() {
}

}  // namespace local_discovery
