// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_DEVICE_LISTER_IMPL_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_DEVICE_LISTER_IMPL_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/local_discovery/privet_device_lister.h"
#include "chrome/common/local_discovery/service_discovery_client.h"

namespace local_discovery {

class PrivetDeviceListerImpl : public PrivetDeviceLister {
 public:
  PrivetDeviceListerImpl(
      ServiceDiscoveryClient* service_discovery_client,
      PrivetDeviceLister::Delegate* delegate,
      std::string subtype);

  PrivetDeviceListerImpl(
      ServiceDiscoveryClient* service_discovery_client,
      PrivetDeviceLister::Delegate* delegate);

  virtual ~PrivetDeviceListerImpl();

  virtual void Start() OVERRIDE;

  virtual void DiscoverNewDevices(bool force_update) OVERRIDE;

 private:
  typedef std::map<std::string, linked_ptr<ServiceResolver> >
     ServiceResolverMap;

  void OnServiceUpdated(ServiceWatcher::UpdateType update,
                        const std::string& service_name);

  void OnResolveComplete(
      bool added,
      ServiceResolver::RequestStatus status,
      const ServiceDescription& description);

  void FillDeviceDescription(const ServiceDescription& service_description,
                             DeviceDescription* device_description);

  DeviceDescription::ConnectionState ConnectionStateFromString(
      const std::string& str);

  // Create or recreate the service watcher
  void CreateServiceWatcher();

  PrivetDeviceLister::Delegate* delegate_;

  ServiceDiscoveryClient* service_discovery_client_;
  scoped_ptr<ServiceWatcher> service_watcher_;
  ServiceResolverMap resolvers_;
  std::string service_type_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_DEVICE_LISTER_IMPL_H_
