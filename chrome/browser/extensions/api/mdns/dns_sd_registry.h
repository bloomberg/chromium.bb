// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MDNS_DNS_SD_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_MDNS_DNS_SD_REGISTRY_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/extensions/api/mdns/dns_sd_delegate.h"

namespace local_discovery {
class ServiceDiscoverySharedClient;
class ServiceDiscoveryClient;
}

namespace extensions {

class DnsSdDeviceLister;
class ServiceTypeData;

// Registry class for keeping track of discovered network services over DNS-SD.
class DnsSdRegistry : public DnsSdDelegate {
 public:
  typedef std::vector<DnsSdService> DnsSdServiceList;

  class DnsSdObserver {
   public:
    virtual void OnDnsSdEvent(const std::string& service_type,
                              const DnsSdServiceList& services) = 0;

   protected:
    virtual ~DnsSdObserver() {}
  };

  DnsSdRegistry();
  explicit DnsSdRegistry(local_discovery::ServiceDiscoverySharedClient* client);
  virtual ~DnsSdRegistry();

  // Observer registration for parties interested in discovery events.
  virtual void AddObserver(DnsSdObserver* observer);
  virtual void RemoveObserver(DnsSdObserver* observer);

  // DNS-SD-related discovery functionality.
  virtual void RegisterDnsSdListener(std::string service_type);
  virtual void UnregisterDnsSdListener(std::string service_type);

 protected:
  // Data class for managing all the resources and information related to a
  // particular service type.
  class ServiceTypeData {
   public:
    explicit ServiceTypeData(scoped_ptr<DnsSdDeviceLister> lister);
    virtual ~ServiceTypeData();

    // Notify the data class of listeners so that it can be reference counted.
    void ListenerAdded();
    // Returns true if the last listener was removed.
    bool ListenerRemoved();
    int GetListenerCount();

    // Methods for adding, updating or removing services for this service type.
    bool UpdateService(bool added, const DnsSdService& service);
    bool RemoveService(const std::string& service_name);
    bool ClearServices();

    const DnsSdRegistry::DnsSdServiceList& GetServiceList();

   private:
    int ref_count;
    scoped_ptr<DnsSdDeviceLister> lister_;
    DnsSdRegistry::DnsSdServiceList service_list_;
    DISALLOW_COPY_AND_ASSIGN(ServiceTypeData);
  };

  // Maps service types to associated data such as listers and service lists.
  typedef std::map<std::string, linked_ptr<ServiceTypeData> >
      DnsSdServiceTypeDataMap;

  virtual DnsSdDeviceLister* CreateDnsSdDeviceLister(
      DnsSdDelegate* delegate,
      const std::string& service_type,
      local_discovery::ServiceDiscoverySharedClient* discovery_client);

  // DnsSdDelegate implementation:
  virtual void ServiceChanged(const std::string& service_type,
                              bool added,
                              const DnsSdService& service) OVERRIDE;
  virtual void ServiceRemoved(const std::string& service_type,
                              const std::string& service_name) OVERRIDE;
  virtual void ServicesFlushed(const std::string& service_type) OVERRIDE;

  DnsSdServiceTypeDataMap service_data_map_;

 private:
  void DispatchApiEvent(const std::string& service_type);
  bool IsRegistered(const std::string& service_type);

  scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
      service_discovery_client_;
  ObserverList<DnsSdObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(DnsSdRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MDNS_DNS_SD_REGISTRY_H_
