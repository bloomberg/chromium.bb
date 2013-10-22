// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/mdns/dns_sd_device_lister.h"

#include "chrome/common/extensions/api/mdns.h"

using local_discovery::ServiceDescription;

namespace extensions {

namespace {

void FillServiceInfo(const ServiceDescription& service_description,
                     DnsSdService* service) {
  service->service_name = service_description.service_name;
  service->service_host_port = service_description.address.ToString();
  if (!service_description.ip_address.empty()) {
    service->ip_address = net::IPAddressToString(
        service_description.ip_address);
  }
  service->service_data = service_description.metadata;

  DVLOG(1) << "Found " << service->service_name << ", "
           << service->service_host_port << ", "
           << service->ip_address;
}

}  // namespace

DnsSdDeviceLister::DnsSdDeviceLister(
    local_discovery::ServiceDiscoveryClient* service_discovery_client,
    DnsSdDelegate* delegate,
    const std::string& service_type)
    : delegate_(delegate),
      device_lister_(this, service_discovery_client, service_type),
      started_(false) {
}

DnsSdDeviceLister::~DnsSdDeviceLister() {
}

void DnsSdDeviceLister::Discover(bool force_update) {
  if (!started_) {
    device_lister_.Start();
    started_ = true;
  }
  device_lister_.DiscoverNewDevices(force_update);
}

void DnsSdDeviceLister::OnDeviceChanged(
    bool added,
    const ServiceDescription& service_description) {
  DnsSdService service;
  FillServiceInfo(service_description, &service);
  delegate_->ServiceChanged(device_lister_.service_type(), added, service);
}

void DnsSdDeviceLister::OnDeviceRemoved(const std::string& service_name) {
  delegate_->ServiceRemoved(device_lister_.service_type(), service_name);
}

void DnsSdDeviceLister::OnDeviceCacheFlushed() {
  delegate_->ServicesFlushed(device_lister_.service_type());
  device_lister_.DiscoverNewDevices(false);
}

}  // namespace extensions
