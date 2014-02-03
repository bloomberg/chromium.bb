// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_DEVICE_RESOLVER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_DEVICE_RESOLVER_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/local_discovery/device_description.h"
#include "chrome/common/local_discovery/service_discovery_client.h"

namespace local_discovery {

class PrivetDeviceResolver {
 public:
  typedef base::Callback<void(bool /*success*/,
                              const DeviceDescription& /*description*/)>
      ResultCallback;

  PrivetDeviceResolver(
      ServiceDiscoveryClient* service_discovery_client,
      const std::string& service_name,
      const ResultCallback& callback);
  ~PrivetDeviceResolver();

  void Start();

 private:
  void OnServiceResolved(
      ServiceResolver::RequestStatus request_status,
      const ServiceDescription& service_description);


  ServiceDiscoveryClient* service_discovery_client_;
  scoped_ptr<ServiceResolver> service_resolver_;
  std::string service_name_;
  ResultCallback callback_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_DEVICE_RESOLVER_H_
