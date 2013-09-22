// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/mdns/dns_sd_device_lister.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/common/extensions/api/mdns.h"
#include "net/base/net_util.h"

using local_discovery::ServiceDescription;
using local_discovery::ServiceDiscoverySharedClient;
using local_discovery::ServiceResolver;
using local_discovery::ServiceWatcher;

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
    DnsSdDelegate* delegate,
    const std::string& service_type,
    ServiceDiscoverySharedClient* service_discovery_client)
    : delegate_(delegate),
      service_type_(service_type),
      service_discovery_client_(service_discovery_client) {
}

DnsSdDeviceLister::~DnsSdDeviceLister() {}

void DnsSdDeviceLister::Discover(bool force_update) {
  if (!service_discovery_client_)
    return;

  if (!service_watcher_.get()) {
    service_watcher_ = service_discovery_client_->CreateServiceWatcher(
        service_type_,
        base::Bind(&DnsSdDeviceLister::OnServiceUpdated,
                   base::Unretained(this)));
    service_watcher_->Start();
  }
  service_watcher_->DiscoverNewServices(force_update);
}

void DnsSdDeviceLister::OnServiceUpdated(
    ServiceWatcher::UpdateType update,
    const std::string& service_name) {
  // TODO(justinlin): Consolidate with PrivetDeviceListerImpl.
  if (update != ServiceWatcher::UPDATE_REMOVED) {
    bool added = (update == ServiceWatcher::UPDATE_ADDED);
    std::pair<ServiceResolverMap::iterator, bool> insert_result =
        resolvers_.insert(make_pair(service_name,
                                    linked_ptr<ServiceResolver>(NULL)));

    // If there is already a resolver working on this service, don't add one.
    if (insert_result.second) {
      scoped_ptr<ServiceResolver> resolver =
          service_discovery_client_->CreateServiceResolver(
          service_name, base::Bind(
              &DnsSdDeviceLister::OnResolveComplete,
              base::Unretained(this),
              added));

      insert_result.first->second.reset(resolver.release());
      insert_result.first->second->StartResolving();
    }
  } else {
    delegate_->ServiceRemoved(service_type_, service_name);
  }
}

void DnsSdDeviceLister::OnResolveComplete(
    bool added,
    ServiceResolver::RequestStatus status,
    const ServiceDescription& service_description) {
  // TODO(justinlin): Consolidate with PrivetDeviceListerImpl.
  if (status != ServiceResolver::STATUS_SUCCESS) {
    resolvers_.erase(service_description.service_name);

    // TODO(noamsml): Add retry logic.
    return;
  }

  DnsSdService service;
  FillServiceInfo(service_description, &service);

  resolvers_.erase(service_description.service_name);
  delegate_->ServiceChanged(service_type_, added, service);
}

}  // namespace extensions
