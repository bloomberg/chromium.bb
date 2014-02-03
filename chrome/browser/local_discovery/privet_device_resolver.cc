// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/local_discovery/privet_device_resolver.h"

namespace local_discovery {

PrivetDeviceResolver::PrivetDeviceResolver(
    ServiceDiscoveryClient* service_discovery_client,
    const std::string& service_name,
    const ResultCallback& callback)
    : service_discovery_client_(service_discovery_client),
      service_name_(service_name), callback_(callback) {
}

PrivetDeviceResolver::~PrivetDeviceResolver() {
}

void PrivetDeviceResolver::Start() {
  service_resolver_ = service_discovery_client_->CreateServiceResolver(
      service_name_,
      base::Bind(&PrivetDeviceResolver::OnServiceResolved,
                 base::Unretained(this)));
  service_resolver_->StartResolving();
}

void PrivetDeviceResolver::OnServiceResolved(
    ServiceResolver::RequestStatus request_status,
    const ServiceDescription& service_description) {
  DeviceDescription device_description;
  if (request_status != ServiceResolver::STATUS_SUCCESS) {
    callback_.Run(false, device_description);
    return;
  }

  device_description.FillFromServiceDescription(service_description);
  callback_.Run(true, device_description);
}

}  // namespace local_discovery
