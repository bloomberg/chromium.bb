// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_device_lister_impl.h"

#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/local_discovery/privet_constants.h"

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

void FillDeviceDescription(const ServiceDescription& service_description,
                           DeviceDescription* device_description) {
  device_description->address = service_description.address;
  device_description->ip_address = service_description.ip_address;
  device_description->last_seen = service_description.last_seen;

  for (std::vector<std::string>::const_iterator i =
           service_description.metadata.begin();
       i != service_description.metadata.end();
       i++) {
    size_t equals_pos = i->find_first_of('=');
    if (equals_pos == std::string::npos)
      continue;  // We do not parse non key-value TXT records

    std::string key = i->substr(0, equals_pos);
    std::string value = i->substr(equals_pos + 1);

    if (LowerCaseEqualsASCII(key, kPrivetTxtKeyName)) {
      device_description->name = value;
    } else if (LowerCaseEqualsASCII(key, kPrivetTxtKeyDescription)) {
      device_description->description = value;
    } else if (LowerCaseEqualsASCII(key, kPrivetTxtKeyURL)) {
      device_description->url = value;
    } else if (LowerCaseEqualsASCII(key, kPrivetTxtKeyType)) {
      device_description->type = value;
    } else if (LowerCaseEqualsASCII(key, kPrivetTxtKeyID)) {
      device_description->id = value;
    } else if (LowerCaseEqualsASCII(key, kPrivetTxtKeyConnectionState)) {
      device_description->connection_state = ConnectionStateFromString(value);
    }
  }
}

}  // namespace

PrivetDeviceListerImpl::PrivetDeviceListerImpl(
    ServiceDiscoveryClient* service_discovery_client,
    PrivetDeviceLister::Delegate* delegate)
    : delegate_(delegate),
      device_lister_(this, service_discovery_client, kPrivetDefaultDeviceType) {
}

PrivetDeviceListerImpl::PrivetDeviceListerImpl(
    ServiceDiscoveryClient* service_discovery_client,
    PrivetDeviceLister::Delegate* delegate,
    const std::string& subtype)
        : delegate_(delegate),
          device_lister_(
              this,
              service_discovery_client,
              base::StringPrintf(kPrivetSubtypeTemplate, subtype.c_str())) {
}

PrivetDeviceListerImpl::~PrivetDeviceListerImpl() {
}

void PrivetDeviceListerImpl::Start() {
  device_lister_.Start();
}

void PrivetDeviceListerImpl::DiscoverNewDevices(bool force_update) {
  device_lister_.DiscoverNewDevices(force_update);
}

void PrivetDeviceListerImpl::OnDeviceChanged(
    bool added, const ServiceDescription& service_description) {
  if (!delegate_)
    return;

  DeviceDescription device_description;
  FillDeviceDescription(service_description, &device_description);

  delegate_->DeviceChanged(
      added, service_description.service_name, device_description);
}

void PrivetDeviceListerImpl::OnDeviceRemoved(const std::string& service_name) {
  if (delegate_)
    delegate_->DeviceRemoved(service_name);
}

void PrivetDeviceListerImpl::OnDeviceCacheFlushed() {
  if (delegate_)
    delegate_->DeviceCacheFlushed();
}

}  // namespace local_discovery
