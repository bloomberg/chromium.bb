// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_H_
#define CHROME_COMMON_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "net/base/address_family.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_util.h"

namespace net {
class MDnsClient;
}

namespace local_discovery {

struct ServiceDescription {
 public:
  ServiceDescription();
  ~ServiceDescription();

  // Convenience function to get useful parts of the service name. A service
  // name follows the format <instance_name>.<service_type>.
  std::string instance_name() const;
  std::string service_type() const;

  // The name of the service.
  std::string service_name;
  // The address (in host/port format) for the service (from SRV record).
  net::HostPortPair address;
  // The metadata (from TXT record) of the service.
  std::vector<std::string> metadata;
  // IP address of the service, if available from cache. May be empty.
  net::IPAddressNumber ip_address;
  // Last time the service was seen.
  base::Time last_seen;
};

// Lets users browse the network for services of interest or listen for changes
// in the services they are interested in. See
// |ServiceDiscoveryClient::CreateServiceWatcher|.
class ServiceWatcher {
 public:
  enum UpdateType {
    UPDATE_ADDED,
    UPDATE_CHANGED,
    UPDATE_REMOVED,
    UPDATE_INVALIDATED,
    UPDATE_TYPE_LAST = UPDATE_INVALIDATED
  };

  // Called when a service has been added or removed for a certain service name.
  typedef base::Callback<void(UpdateType, const std::string&)> UpdatedCallback;

  // Listening will automatically stop when the destructor is called.
  virtual ~ServiceWatcher() {}

  // Start the service type watcher.
  virtual void Start() = 0;

  // Probe for services of this type.
  virtual void DiscoverNewServices(bool force_update) = 0;

  virtual void SetActivelyRefreshServices(bool actively_refresh_services) = 0;

  virtual std::string GetServiceType() const = 0;
};

// Represents a service on the network and allows users to access the service's
// address and metadata. See |ServiceDiscoveryClient::CreateServiceResolver|.
class ServiceResolver {
 public:
  enum RequestStatus {
    STATUS_SUCCESS,
    STATUS_REQUEST_TIMEOUT,
    STATUS_KNOWN_NONEXISTENT,
    REQUEST_STATUS_LAST = STATUS_KNOWN_NONEXISTENT
  };

  // A callback called once the service has been resolved.
  typedef base::Callback<void(RequestStatus, const ServiceDescription&)>
      ResolveCompleteCallback;

  // Listening will automatically stop when the destructor is called.
  virtual ~ServiceResolver() {}

  // Start the service reader.
  virtual void StartResolving() = 0;

  virtual std::string GetName() const = 0;
};

class LocalDomainResolver {
 public:
  typedef base::Callback<void(bool /*success*/,
                              const net::IPAddressNumber& /*address_ipv4*/,
                              const net::IPAddressNumber& /*address_ipv6*/)>
      IPAddressCallback;

  virtual ~LocalDomainResolver() {}

  virtual void Start() = 0;
};

class ServiceDiscoveryClient {
 public:
  virtual ~ServiceDiscoveryClient() {}

  // Create a service watcher object listening for DNS-SD service announcements
  // on service type |service_type|.
  virtual scoped_ptr<ServiceWatcher> CreateServiceWatcher(
      const std::string& service_type,
      const ServiceWatcher::UpdatedCallback& callback) = 0;

  // Create a service resolver object for getting detailed service information
  // for the service called |service_name|.
  virtual scoped_ptr<ServiceResolver> CreateServiceResolver(
      const std::string& service_name,
      const ServiceResolver::ResolveCompleteCallback& callback) = 0;

  // Create a resolver for local domain, both ipv4 or ipv6.
  virtual scoped_ptr<LocalDomainResolver> CreateLocalDomainResolver(
      const std::string& domain,
      net::AddressFamily address_family,
      const LocalDomainResolver::IPAddressCallback& callback) = 0;
};

}  // namespace local_discovery

#endif  // CHROME_COMMON_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_H_
