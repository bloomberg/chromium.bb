// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_DEVICE_LISTER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_DEVICE_LISTER_H_

#include <map>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/local_discovery/service_discovery_client.h"

namespace local_discovery {

class ServiceDiscoveryDeviceLister {
 public:
  class Delegate {
   public:
    virtual void OnDeviceChanged(
        bool added,
        const ServiceDescription& service_description) = 0;
    virtual void OnDeviceRemoved(const std::string& service_name) = 0;
    virtual void OnDeviceCacheFlushed() = 0;
  };

  ServiceDiscoveryDeviceLister(Delegate* delegate,
                               ServiceDiscoveryClient* service_discovery_client,
                               const std::string& service_type);
  virtual ~ServiceDiscoveryDeviceLister();

  void Start();
  void DiscoverNewDevices(bool force_update);

  std::string service_type() { return service_type_; }

 private:
  typedef std::map<std::string, linked_ptr<ServiceResolver> >
     ServiceResolverMap;

  void OnServiceUpdated(ServiceWatcher::UpdateType update,
                        const std::string& service_name);

  void OnResolveComplete(
      bool added,
      std::string service_name,
      ServiceResolver::RequestStatus status,
      const ServiceDescription& description);

  // Create or recreate the service watcher
  void CreateServiceWatcher();

  Delegate* const delegate_;
  ServiceDiscoveryClient* const service_discovery_client_;
  const std::string service_type_;

  scoped_ptr<ServiceWatcher> service_watcher_;
  ServiceResolverMap resolvers_;

  base::WeakPtrFactory<ServiceDiscoveryDeviceLister> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceDiscoveryDeviceLister);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_DEVICE_LISTER_H_
