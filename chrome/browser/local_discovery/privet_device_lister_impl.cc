// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_device_lister_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/local_discovery/privet_constants.h"

namespace local_discovery {

PrivetDeviceListerImpl::PrivetDeviceListerImpl(
    ServiceDiscoveryClient* service_discovery_client,
    PrivetDeviceLister::Delegate* delegate)
    : delegate_(delegate),
      service_discovery_client_(service_discovery_client),
      service_type_(kPrivetDefaultDeviceType) {
}

PrivetDeviceListerImpl::PrivetDeviceListerImpl(
    ServiceDiscoveryClient* service_discovery_client,
    PrivetDeviceLister::Delegate* delegate,
    std::string subtype) : delegate_(delegate),
                           service_discovery_client_(service_discovery_client),
                           service_type_(
                               base::StringPrintf(kPrivetSubtypeTemplate,
                                                  subtype.c_str())) {
}

PrivetDeviceListerImpl::~PrivetDeviceListerImpl() {
}

void PrivetDeviceListerImpl::Start() {
  CreateServiceWatcher();
}

void PrivetDeviceListerImpl::DiscoverNewDevices(bool force_update) {
  service_watcher_->DiscoverNewServices(force_update);
}

void PrivetDeviceListerImpl::OnServiceUpdated(
    ServiceWatcher::UpdateType update,
    const std::string& service_name) {
  if (update == ServiceWatcher::UPDATE_INVALIDATED) {
    resolvers_.clear();
    CreateServiceWatcher();

    delegate_->DeviceCacheFlushed();
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
              &PrivetDeviceListerImpl::OnResolveComplete,
              base::Unretained(this),
              added));

      insert_result.first->second.reset(resolver.release());
      insert_result.first->second->StartResolving();
    }
  } else {
    delegate_->DeviceRemoved(service_name);
  }
}

void PrivetDeviceListerImpl::OnResolveComplete(
    bool added,
    ServiceResolver::RequestStatus status,
    const ServiceDescription& service_description) {
  if (status != ServiceResolver::STATUS_SUCCESS) {
    resolvers_.erase(service_description.service_name);

    // TODO(noamsml): Add retry logic.
    return;
  }

  DeviceDescription device_description;
  FillDeviceDescription(service_description, &device_description);

  std::string service_name = service_description.service_name;
  resolvers_.erase(service_name);
  delegate_->DeviceChanged(added, service_name, device_description);
}

void PrivetDeviceListerImpl::FillDeviceDescription(
    const ServiceDescription& service_description,
    DeviceDescription* device_description) {
  device_description->address = service_description.address;
  device_description->ip_address = service_description.ip_address;
  device_description->last_seen = service_description.last_seen;

  for (std::vector<std::string>::const_iterator i =
           service_description.metadata.begin();
       i < service_description.metadata.end();
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

DeviceDescription::ConnectionState
PrivetDeviceListerImpl::ConnectionStateFromString(const std::string& str) {
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

void PrivetDeviceListerImpl::CreateServiceWatcher() {
  service_watcher_ =
      service_discovery_client_->CreateServiceWatcher(
          service_type_,
          base::Bind(&PrivetDeviceListerImpl::OnServiceUpdated,
                     base::Unretained(this)));
  service_watcher_->Start();

}

}  // namespace local_discovery
