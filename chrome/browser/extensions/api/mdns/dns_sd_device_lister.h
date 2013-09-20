// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MDNS_DNS_SD_DEVICE_LISTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_MDNS_DNS_SD_DEVICE_LISTER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/mdns/dns_sd_delegate.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"

namespace local_discovery {
class ServiceDiscoverySharedClient;
}  // local_discovery

namespace extensions {

// Manages a watcher for a specific MDNS/DNS-SD service type and notifies
// a delegate of changes to watched services.
class DnsSdDeviceLister {
 public:
  DnsSdDeviceLister(
      DnsSdDelegate* delegate,
      const std::string& service_type,
      local_discovery::ServiceDiscoverySharedClient* service_discovery_client);
  virtual ~DnsSdDeviceLister();

  // Requests that the service watcher issue an immediate query for services.
  // force_update will first clear the service cache. This must be called
  // to instantiate the service watcher and start discovery.
  virtual void Discover(bool force_update);

 protected:
  // Invoked when the watcher notifies us of a service change.
  void OnServiceUpdated(
    local_discovery::ServiceWatcher::UpdateType update,
    const std::string& service_name);

  void OnResolveComplete(
    bool added,
    local_discovery::ServiceResolver::RequestStatus status,
    const local_discovery::ServiceDescription& description);

 private:
  typedef std::map<std::string, linked_ptr<local_discovery::ServiceResolver> >
       ServiceResolverMap;

  // The delegate to notify of changes to services.
  DnsSdDelegate* const delegate_;

  // The service type for this watcher.
  const std::string service_type_;

  // The instance of the service discovery client.
  scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
      service_discovery_client_;

  // The instance of the service watcher.
  scoped_ptr<local_discovery::ServiceWatcher> service_watcher_;

  ServiceResolverMap resolvers_;

  DISALLOW_COPY_AND_ASSIGN(DnsSdDeviceLister);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MDNS_DNS_SD_DEVICE_LISTER_H_
