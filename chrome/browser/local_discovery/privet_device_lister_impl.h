// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_DEVICE_LISTER_IMPL_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_DEVICE_LISTER_IMPL_H_

#include <string>

#include "chrome/browser/local_discovery/privet_device_lister.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"

namespace local_discovery {

class ServiceDiscoveryClient;

class PrivetDeviceListerImpl : public PrivetDeviceLister,
                               public ServiceDiscoveryDeviceLister::Delegate {
 public:
  PrivetDeviceListerImpl(
      ServiceDiscoveryClient* service_discovery_client,
      PrivetDeviceLister::Delegate* delegate,
      const std::string& subtype);

  PrivetDeviceListerImpl(
      ServiceDiscoveryClient* service_discovery_client,
      PrivetDeviceLister::Delegate* delegate);

  virtual ~PrivetDeviceListerImpl();

  virtual void Start() OVERRIDE;
  virtual void DiscoverNewDevices(bool force_update) OVERRIDE;

 protected:
  virtual void OnDeviceChanged(
      bool added,
      const ServiceDescription& service_description) OVERRIDE;
  virtual void OnDeviceRemoved(const std::string& service_name) OVERRIDE;
  virtual void OnDeviceCacheFlushed() OVERRIDE;

 private:
  PrivetDeviceLister::Delegate* delegate_;
  ServiceDiscoveryDeviceLister device_lister_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_DEVICE_LISTER_IMPL_H_
