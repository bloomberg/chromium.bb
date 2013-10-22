// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_device_lister.h"

#include <utility>
#include <vector>

#include "base/bind.h"

namespace local_discovery {

ServiceDiscoveryDeviceLister::ServiceDiscoveryDeviceLister(
    Delegate* delegate,
    ServiceDiscoveryClient* service_discovery_client,
    const std::string& service_type)
    : delegate_(delegate),
      service_discovery_client_(service_discovery_client),
      service_type_(service_type) {
}

ServiceDiscoveryDeviceLister::~ServiceDiscoveryDeviceLister() {
}

void ServiceDiscoveryDeviceLister::Start() {
  CreateServiceWatcher();
}

void ServiceDiscoveryDeviceLister::DiscoverNewDevices(bool force_update) {
  service_watcher_->DiscoverNewServices(force_update);
}

void ServiceDiscoveryDeviceLister::OnServiceUpdated(
    ServiceWatcher::UpdateType update,
    const std::string& service_name) {
  if (update == ServiceWatcher::UPDATE_INVALIDATED) {
    resolvers_.clear();
    CreateServiceWatcher();

    delegate_->OnDeviceCacheFlushed();
    return;
  }

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
              &ServiceDiscoveryDeviceLister::OnResolveComplete,
              base::Unretained(this),
              added));

      insert_result.first->second.reset(resolver.release());
      insert_result.first->second->StartResolving();
    }
  } else {
    delegate_->OnDeviceRemoved(service_name);
  }
}

void ServiceDiscoveryDeviceLister::OnResolveComplete(
    bool added,
    ServiceResolver::RequestStatus status,
    const ServiceDescription& service_description) {
  if (status != ServiceResolver::STATUS_SUCCESS) {
    resolvers_.erase(service_description.service_name);

    // TODO(noamsml): Add retry logic.
    return;
  }

  delegate_->OnDeviceChanged(added, service_description);
  resolvers_.erase(service_description.service_name);
}

void ServiceDiscoveryDeviceLister::CreateServiceWatcher() {
  service_watcher_ =
      service_discovery_client_->CreateServiceWatcher(
          service_type_,
          base::Bind(&ServiceDiscoveryDeviceLister::OnServiceUpdated,
                     base::Unretained(this)));
  service_watcher_->Start();
}

}  // namespace local_discovery
