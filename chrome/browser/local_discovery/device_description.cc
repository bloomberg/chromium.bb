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

DeviceDescription::ConnectionState
ConnectionStateFromString(const std::string& str) {
  if (LowerCaseEqualsASCII(str, kPrivetConnectionStatusOnline)) {
    return DeviceDescription::ONLINE;
  } else if (LowerCaseEqualsASCII(str, kPrivetConnectionStatusOffline)) {
    return DeviceDescription::OFFLINE;
  } else if (LowerCaseEqualsASCII(str, kPrivetConnectionStatusConnecting)) {
    return DeviceDescription::CONNECTING;
  } else if (LowerCaseEqualsASCII(str, kPrivetConnectionStatusNotConfigured)) {
    return DeviceDescription::NOT_CONFIGURED;
  }

  return DeviceDescription::UNKNOWN;
}

}  // namespace

DeviceDescription::DeviceDescription()
    : version(0),
      connection_state(UNKNOWN) {
}

DeviceDescription::~DeviceDescription() {
}

void DeviceDescription::FillFromServiceDescription(
    const ServiceDescription& service_description) {
  address = service_description.address;
  ip_address = service_description.ip_address;
  last_seen = service_description.last_seen;

  for (std::vector<std::string>::const_iterator i =
           service_description.metadata.begin();
       i != service_description.metadata.end();
       i++) {
    size_t equals_pos = i->find_first_of('=');
    if (equals_pos == std::string::npos)
      continue;  // We do not parse non key-value TXT records

    std::string key = i->substr(0, equals_pos);
    std::string value = i->substr(equals_pos + 1);

    if (LowerCaseEqualsASCII(key, kPrivetTxtKeyVersion)) {
      if (!base::StringToInt(value, &version))
        continue;  // Unknown version.
    } else if (LowerCaseEqualsASCII(key, kPrivetTxtKeyName)) {
      name = value;
    } else if (LowerCaseEqualsASCII(key, kPrivetTxtKeyDescription)) {
      description = value;
    } else if (LowerCaseEqualsASCII(key, kPrivetTxtKeyURL)) {
      url = value;
    } else if (LowerCaseEqualsASCII(key, kPrivetTxtKeyType)) {
      type = value;
    } else if (LowerCaseEqualsASCII(key, kPrivetTxtKeyID)) {
      id = value;
    } else if (LowerCaseEqualsASCII(key, kPrivetTxtKeyConnectionState)) {
      connection_state = ConnectionStateFromString(value);
    }
  }
}


}  // namespace local_discovery
