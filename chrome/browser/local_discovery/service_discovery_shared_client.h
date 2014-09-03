// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_SHARED_CLIENT_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_SHARED_CLIENT_H_

#include "chrome/common/local_discovery/service_discovery_client.h"

namespace local_discovery {

class ServiceDiscoverySharedClient
    : public base::RefCounted<ServiceDiscoverySharedClient>,
      public ServiceDiscoveryClient {
 public:
  static scoped_refptr<ServiceDiscoverySharedClient> GetInstance();

  typedef base::Callback<void(
      const scoped_refptr<ServiceDiscoverySharedClient>&)> GetInstanceCallback;
  static void GetInstanceWithoutAlert(const GetInstanceCallback& callback);

 protected:
  ServiceDiscoverySharedClient();
  virtual ~ServiceDiscoverySharedClient();

 private:
  friend class base::RefCounted<ServiceDiscoverySharedClient>;

  DISALLOW_COPY_AND_ASSIGN(ServiceDiscoverySharedClient);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_SHARED_CLIENT_H_
