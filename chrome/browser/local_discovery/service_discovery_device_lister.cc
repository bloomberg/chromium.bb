// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_device_lister.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"

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

void ServiceDiscoveryDeviceLister::DiscoverNewDevices() {
  VLOG(1) << "DiscoverNewDevices: service_type: " << service_type_;
  service_watcher_->DiscoverNewServices();
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

  if (update == ServiceWatcher::UPDATE_REMOVED) {
    delegate_->OnDeviceRemoved(service_name);
    return;
  }

  // If there is already a resolver working on this service, don't add one.
  if (base::ContainsKey(resolvers_, service_name)) {
    VLOG(1) << "Resolver already exists, service_name: " << service_name;
    return;
  }

  VLOG(1) << "Adding resolver for service_name: " << service_name;
  bool added = (update == ServiceWatcher::UPDATE_ADDED);
  std::unique_ptr<ServiceResolver> resolver =
      service_discovery_client_->CreateServiceResolver(
          service_name,
          base::Bind(&ServiceDiscoveryDeviceLister::OnResolveComplete,
                     weak_factory_.GetWeakPtr(), added, service_name));
  resolver->StartResolving();
  resolvers_[service_name] = std::move(resolver);
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
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ServiceDiscoveryDeviceLister::OnServiceUpdated,
                   weak_factory_.GetWeakPtr(), ServiceWatcher::UPDATE_CHANGED,
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
