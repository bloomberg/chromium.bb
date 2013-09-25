// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MDNS_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MDNS_H_

#include <string>

#include "base/cancelable_callback.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/common/local_discovery/service_discovery_client.h"
#include "net/base/network_change_notifier.h"

namespace local_discovery {

class ServiceDiscoveryHostClient;

class ServiceDiscoveryClientMdns
    : public ServiceDiscoverySharedClient,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  ServiceDiscoveryClientMdns();

  // ServiceDiscoveryClient implementation.
  virtual scoped_ptr<ServiceWatcher> CreateServiceWatcher(
      const std::string& service_type,
      const ServiceWatcher::UpdatedCallback& callback) OVERRIDE;
  virtual scoped_ptr<ServiceResolver> CreateServiceResolver(
      const std::string& service_name,
      const ServiceResolver::ResolveCompleteCallback& callback) OVERRIDE;
  virtual scoped_ptr<LocalDomainResolver> CreateLocalDomainResolver(
      const std::string& domain,
      net::AddressFamily address_family,
      const LocalDomainResolver::IPAddressCallback& callback) OVERRIDE;

  // net::NetworkChangeNotifier::NetworkChangeObserver implementation.
  virtual void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

 private:
  friend class base::RefCounted<ServiceDiscoveryClientMdns>;

  virtual ~ServiceDiscoveryClientMdns();
  void ScheduleStartNewClient();
  void StartNewClient();

  scoped_refptr<ServiceDiscoveryHostClient> host_client_;
  int restart_attempts_;
  base::WeakPtrFactory<ServiceDiscoveryClientMdns> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceDiscoveryClientMdns);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MDNS_H_
