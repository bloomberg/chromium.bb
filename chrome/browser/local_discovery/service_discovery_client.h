// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_util.h"

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
    UPDATE_REMOVED
  };

  class Delegate {
   public:
    virtual ~Delegate() {}

    // A service has been added or removed for a certain service name.
    virtual void OnServiceUpdated(UpdateType update,
                                  const std::string& service_name) = 0;
  };

  // Listening will automatically stop when the destructor is called.
  virtual ~ServiceWatcher() {}

  // Start the service type watcher.
  virtual bool Start() = 0;

  // Get all known services names of this watcher's type. Return them in
  // |services|.
  virtual void GetAvailableServices(
      std::vector<std::string>* services) const = 0;

  // Read services from the cache, alerting the delegate to any service that
  // is not yet known.
  virtual void ReadCachedServices() = 0;

  // Probe for services of this type.
  virtual void DiscoverNewServices(bool force_update) = 0;

  virtual std::string GetServiceType() const = 0;
};

// Represents a service on the network and allows users to access the service's
// address and metadata. See |ServiceDiscoveryClient::CreateServiceResolver|.
class ServiceResolver {
 public:
  enum RequestStatus {
    STATUS_SUCCESS = 0,
    STATUS_REQUEST_TIMEOUT = 1,
    STATUS_KNOWN_NONEXISTENT = 2
  };

  // A callback called once the service has been resolved.
  typedef base::Callback<void(RequestStatus, const ServiceDescription&)>
      ResolveCompleteCallback;

  // Listening will automatically stop when the destructor is called.
  virtual ~ServiceResolver() {}

  // Start the service reader.
  virtual bool StartResolving() = 0;

  // Check whether the resolver is currently resolving. Can be called multiple
  // times.
  virtual bool IsResolving() const = 0;

  // Check wheteher the resolver has resolved the service already.
  virtual bool HasResolved() const = 0;

  virtual std::string GetName() const = 0;

  virtual const ServiceDescription& GetServiceDescription() const = 0;
};

class ServiceDiscoveryClient {
 public:
  virtual ~ServiceDiscoveryClient() {}

  // Create a service watcher object listening for DNS-SD service announcements
  // on service type |service_type|.
  virtual scoped_ptr<ServiceWatcher> CreateServiceWatcher(
      const std::string& service_type,
      ServiceWatcher::Delegate* delegate) = 0;

  // Create a service resolver object for getting detailed service information
  // for the service called |service_name|.
  virtual scoped_ptr<ServiceResolver> CreateServiceResolver(
      const std::string& service_name,
      const ServiceResolver::ResolveCompleteCallback& callback) = 0;

  // Lazily create and return static instance for ServiceDiscoveryClient.
  static ServiceDiscoveryClient* GetInstance();

  // Set the global instance (for testing). MUST be called before the first call
  // to GetInstance.
  static void SetInstance(ServiceDiscoveryClient* instance);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_H_
