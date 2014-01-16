// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_device_lister.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace local_discovery {

namespace {
#if defined(OS_MACOSX)
const int kMacServiceResolvingIntervalSecs = 60;
#endif
}  // namespace

ServiceDiscoveryDeviceLister::ServiceDiscoveryDeviceLister(
    Delegate* delegate,
    ServiceDiscoveryClient* service_discovery_client,
    const std::string& service_type)
    : delegate_(delegate),
      service_discovery_client_(service_discovery_client),
      service_type_(service_type),
      weak_factory_(this) {
}

ServiceDiscoveryDeviceLister::~ServiceDiscoveryDeviceLister() {
}

void ServiceDiscoveryDeviceLister::Start() {
  VLOG(1) << "DeviceListerStart: service_type: " << service_type_;
  CreateServiceWatcher();
}

void ServiceDiscoveryDeviceLister::DiscoverNewDevices(bool force_update) {
  VLOG(1) << "DiscoverNewDevices: service_type: " << service_type_;
  service_watcher_->DiscoverNewServices(force_update);
}

void ServiceDiscoveryDeviceLister::OnServiceUpdated(
    ServiceWatcher::UpdateType update,
    const std::string& service_name) {
  VLOG(1) << "OnServiceUpdated: service_type: " << service_type_
          << ", service_name: " << service_name
          << ", update: " << update;
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
      VLOG(1) << "Adding resolver for service_name: " << service_name;
      scoped_ptr<ServiceResolver> resolver =
          service_discovery_client_->CreateServiceResolver(
          service_name, base::Bind(
              &ServiceDiscoveryDeviceLister::OnResolveComplete,
              weak_factory_.GetWeakPtr(),
              added,
              service_name));

      insert_result.first->second.reset(resolver.release());
      insert_result.first->second->StartResolving();
    } else {
      VLOG(1) << "Resolver already exists, service_name: " << service_name;
    }
  } else {
    delegate_->OnDeviceRemoved(service_name);
  }
}

// TODO(noamsml): Update ServiceDiscoveryClient interface to match this.
void ServiceDiscoveryDeviceLister::OnResolveComplete(
    bool added,
    std::string service_name,
    ServiceResolver::RequestStatus status,
    const ServiceDescription& service_description) {
  VLOG(1) << "OnResolveComplete: service_type: " << service_type_
          << ", service_name: " << service_name
          << ", status: " << status;
  if (status == ServiceResolver::STATUS_SUCCESS) {
    delegate_->OnDeviceChanged(added, service_description);

#if defined(OS_MACOSX)
    // On Mac, the Bonjour service does not seem to ever evict a service if a
    // device is unplugged, so we need to continuously try to resolve the
    // service to detect non-graceful shutdowns.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ServiceDiscoveryDeviceLister::OnServiceUpdated,
                   weak_factory_.GetWeakPtr(),
                   ServiceWatcher::UPDATE_CHANGED,
                   service_description.service_name),
        base::TimeDelta::FromSeconds(kMacServiceResolvingIntervalSecs));
#endif
  } else {
    // TODO(noamsml): Add retry logic.
  }
  resolvers_.erase(service_name);
}

void ServiceDiscoveryDeviceLister::CreateServiceWatcher() {
  service_watcher_ =
      service_discovery_client_->CreateServiceWatcher(
          service_type_,
          base::Bind(&ServiceDiscoveryDeviceLister::OnServiceUpdated,
                     weak_factory_.GetWeakPtr()));
  service_watcher_->Start();
}

}  // namespace local_discovery
