// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_H_

#include <string>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"

namespace local_discovery {

// Implementation of ServiceDiscoveryClient that uses the Bonjour SDK.
// https://developer.apple.com/library/mac/documentation/Networking/Conceptual/
// NSNetServiceProgGuide/Articles/BrowsingForServices.html
class ServiceDiscoveryClientMac : public ServiceDiscoverySharedClient {
 public:
  ServiceDiscoveryClientMac();

 private:
  virtual ~ServiceDiscoveryClientMac();

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

  DISALLOW_COPY_AND_ASSIGN(ServiceDiscoveryClientMac);
};

class ServiceWatcherImplMac : public ServiceWatcher {
 public:
  ServiceWatcherImplMac(const std::string& service_type,
                        const ServiceWatcher::UpdatedCallback& callback);

  void OnServicesUpdate(ServiceWatcher::UpdateType update,
                        const std::string& service);

 private:
  virtual ~ServiceWatcherImplMac();

  virtual void Start() OVERRIDE;
  virtual void DiscoverNewServices(bool force_update) OVERRIDE;
  virtual std::string GetServiceType() const OVERRIDE;

  std::string service_type_;

  ServiceWatcher::UpdatedCallback callback_;
  bool started_;
  base::scoped_nsobject<id> delegate_;
  base::scoped_nsobject<NSNetServiceBrowser> browser_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWatcherImplMac);
};

class ServiceResolverImplMac : public ServiceResolver {
 public:
  ServiceResolverImplMac(
      const std::string& service_name,
      const ServiceResolver::ResolveCompleteCallback& callback);

  void OnResolveUpdate(RequestStatus);

  // Testing methods.
  void SetServiceForTesting(
      base::scoped_nsobject<NSNetService> service);

 private:
  virtual ~ServiceResolverImplMac();

  virtual void StartResolving() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  const std::string service_name_;
  ServiceResolver::ResolveCompleteCallback callback_;
  bool has_resolved_;
  base::scoped_nsobject<id> delegate_;
  base::scoped_nsobject<NSNetService> service_;
  ServiceDescription service_description_;

  DISALLOW_COPY_AND_ASSIGN(ServiceResolverImplMac);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_H_
