// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/mdns/dns_sd_registry.h"

#include "base/stl_util.h"
#include "chrome/browser/extensions/api/mdns/dns_sd_device_lister.h"
#include "chrome/browser/local_discovery/service_discovery_host_client.h"

using local_discovery::ServiceDiscoverySharedClient;

namespace extensions {

namespace {
// Predicate to test if two discovered services have the same service_name.
class IsSameServiceName {
 public:
  explicit IsSameServiceName(const DnsSdService& service) : service_(service) {}
  bool operator()(const DnsSdService& other) const {
    return service_.service_name == other.service_name;
  }

 private:
  const DnsSdService& service_;
};

}  // namespace


DnsSdRegistry::ServiceTypeData::ServiceTypeData(
    scoped_ptr<DnsSdDeviceLister> lister)
    : ref_count(1), lister_(lister.Pass()) {}

DnsSdRegistry::ServiceTypeData::~ServiceTypeData() {}

void DnsSdRegistry::ServiceTypeData::ListenerAdded() {
  ref_count++;
};

bool DnsSdRegistry::ServiceTypeData::ListenerRemoved() {
  return --ref_count == 0;
};

int DnsSdRegistry::ServiceTypeData::GetListenerCount() {
  return ref_count;
}

bool DnsSdRegistry::ServiceTypeData::UpdateService(
      bool added, const DnsSdService& service) {
  if (added) {
    service_list_.push_back(service);
  } else {
    DnsSdRegistry::DnsSdServiceList::iterator it =
        std::find_if(service_list_.begin(),
                     service_list_.end(),
                     IsSameServiceName(service));
    if (it == service_list_.end())
      return false;
    *it = service;
  }
  return true;
};

bool DnsSdRegistry::ServiceTypeData::RemoveService(
    const std::string& service_name) {
  for (DnsSdRegistry::DnsSdServiceList::iterator it = service_list_.begin();
       it != service_list_.end(); ++it) {
    if ((*it).service_name == service_name) {
      service_list_.erase(it);
      return true;
    }
  }
  return false;
};

const DnsSdRegistry::DnsSdServiceList&
DnsSdRegistry::ServiceTypeData::GetServiceList() {
  return service_list_;
}

DnsSdRegistry::DnsSdRegistry() {
#if defined(ENABLED_MDNS)
  service_discovery_client_ = ServiceDiscoverySharedClient::GetInstance();
#endif
}

DnsSdRegistry::DnsSdRegistry(ServiceDiscoverySharedClient* client) {
  service_discovery_client_ = client;
}

DnsSdRegistry::~DnsSdRegistry() {
}

void DnsSdRegistry::AddObserver(DnsSdObserver* observer) {
  observers_.AddObserver(observer);
}

void DnsSdRegistry::RemoveObserver(DnsSdObserver* observer) {
  observers_.RemoveObserver(observer);
}

DnsSdDeviceLister* DnsSdRegistry::CreateDnsSdDeviceLister(
    DnsSdDelegate* delegate,
    const std::string& service_type,
    local_discovery::ServiceDiscoverySharedClient* discovery_client) {
  return new DnsSdDeviceLister(delegate, service_type, discovery_client);
}

void DnsSdRegistry::RegisterDnsSdListener(std::string service_type) {
  if (service_type.empty())
    return;

  if (service_data_map_.find(service_type) != service_data_map_.end()) {
    service_data_map_[service_type].get()->ListenerAdded();
    DispatchApiEvent(service_type);
    return;
  }

  scoped_ptr<DnsSdDeviceLister> dns_sd_device_lister(CreateDnsSdDeviceLister(
      this, service_type, service_discovery_client_));
  dns_sd_device_lister->Discover(false);
  linked_ptr<ServiceTypeData> service_type_data(
      new ServiceTypeData(dns_sd_device_lister.Pass()));
  service_data_map_[service_type] = service_type_data;
  DispatchApiEvent(service_type);
}

void DnsSdRegistry::UnregisterDnsSdListener(std::string service_type) {
  DnsSdRegistry::DnsSdServiceTypeDataMap::iterator it =
      service_data_map_.find(service_type);
  if (it == service_data_map_.end())
    return;

  if (service_data_map_[service_type].get()->ListenerRemoved())
    service_data_map_.erase(it);
}

void DnsSdRegistry::ServiceChanged(const std::string& service_type,
                                   bool added,
                                   const DnsSdService& service) {
  if (service_data_map_.find(service_type) == service_data_map_.end())
    return;

  if (service_data_map_[service_type].get()->UpdateService(added, service)) {
    DispatchApiEvent(service_type);
  } else {
    DVLOG(1) << "Failed to find existing service to update: "
             << service.service_name;
  }
}

void DnsSdRegistry::ServiceRemoved(const std::string& service_type,
                                   const std::string& service_name) {
  if (service_data_map_.find(service_type) == service_data_map_.end())
    return;

  if (service_data_map_[service_type].get()->RemoveService(service_name)) {
    DispatchApiEvent(service_type);
  } else {
    DVLOG(1) << "Failed to remove service: " << service_name;
  }
}

void DnsSdRegistry::DispatchApiEvent(const std::string& service_type) {
  FOR_EACH_OBSERVER(DnsSdObserver, observers_, OnDnsSdEvent(
      service_type, service_data_map_[service_type].get()->GetServiceList()));
}

}  // namespace extensions
