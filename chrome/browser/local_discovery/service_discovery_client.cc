// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_client.h"

#include "chrome/browser/local_discovery/service_discovery_client_impl.h"

namespace local_discovery {
static ServiceDiscoveryClient* g_instance = NULL;

ServiceDescription::ServiceDescription() {
}

ServiceDescription::~ServiceDescription() {
}

std::string ServiceDescription::instance_name() const {
  // TODO(noamsml): Once we have escaping working, get this to
  // parse escaped domains.
  size_t first_period = service_name.find_first_of('.');
  return service_name.substr(0, first_period);
}

std::string ServiceDescription::service_type() const {
  // TODO(noamsml): Once we have escaping working, get this to
  // parse escaped domains.
  size_t first_period = service_name.find_first_of('.');
  if (first_period == std::string::npos)
    return "";
  return service_name.substr(first_period+1);
}


ServiceDiscoveryClient* ServiceDiscoveryClient::GetInstance() {
  return g_instance ? g_instance : ServiceDiscoveryClientImpl::GetInstance();
}

void ServiceDiscoveryClient::SetInstance(ServiceDiscoveryClient* instance) {
  g_instance = instance;
}

}  // namespace local_discovery
