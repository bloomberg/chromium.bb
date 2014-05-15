// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MDNS_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MDNS_H_

#include <set>
#include <string>

#include "base/cancelable_callback.h"
#include "base/observer_list.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/common/local_discovery/service_discovery_client.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/mdns_client.h"

namespace local_discovery {

// Implementation of ServiceDiscoverySharedClient with front-end of UI thread
// and networking code on IO thread.
class ServiceDiscoveryClientMdns
    : public ServiceDiscoverySharedClient,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  class Proxy;
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
  virtual ~ServiceDiscoveryClientMdns();

  void ScheduleStartNewClient();
  void StartNewClient();
  void OnInterfaceListReady(const net::InterfaceIndexFamilyList& interfaces);
  void OnMdnsInitialized(bool success);
  void ReportSuccess();
  void InvalidateWeakPtrs();
  void OnBeforeMdnsDestroy();
  void DestroyMdns();

  bool PostToMdnsThread(const base::Closure& task);

  ObserverList<Proxy, true> proxies_;

  scoped_refptr<base::SequencedTaskRunner> mdns_runner_;

  // Access only on |mdns_runner_| thread.
  scoped_ptr<net::MDnsClient> mdns_;

  // Access only on |mdns_runner_| thread.
  scoped_ptr<ServiceDiscoveryClient> client_;

  // Counter of restart attempts we have made after network change.
  int restart_attempts_;

  // If false delay tasks until initialization is posted to |mdns_runner_|
  // thread.
  bool need_dalay_mdns_tasks_;

  // Delayed |mdns_runner_| tasks.
  std::vector<base::Closure> delayed_tasks_;

  base::WeakPtrFactory<ServiceDiscoveryClientMdns> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceDiscoveryClientMdns);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MDNS_H_
