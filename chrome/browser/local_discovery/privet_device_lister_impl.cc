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
  device_description.FillFromServiceDescription(service_description);

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
