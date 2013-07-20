// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_LOCAL_DISCOVERY_SERVICE_DISCOVERY_MESSAGE_HANDLER_H_
#define CHROME_UTILITY_LOCAL_DISCOVERY_SERVICE_DISCOVERY_MESSAGE_HANDLER_H_

#include <map>

#include "base/memory/linked_ptr.h"
#include "chrome/common/local_discovery/service_discovery_client.h"
#include "chrome/utility/utility_message_handler.h"

namespace net {
class MDnsClient;
}

namespace local_discovery {

class ServiceDiscoveryClient;

// Handles messages related to local discovery inside utility process.
class ServiceDiscoveryMessageHandler : public chrome::UtilityMessageHandler {
 public:
  ServiceDiscoveryMessageHandler();
  virtual ~ServiceDiscoveryMessageHandler();

  // UtilityMessageHandler implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  static void PreSandboxStartup();

 private:
  typedef std::map<uint64, linked_ptr<ServiceWatcher> > ServiceWatchers;
  typedef std::map<uint64, linked_ptr<ServiceResolver> > ServiceResolvers;

  // Lazy initializes ServiceDiscoveryClient.
  bool Initialize();

  // IPC message handlers.
  void OnStartWatcher(uint64 id, const std::string& service_type);
  void OnDiscoverServices(uint64 id, bool force_update);
  void OnDestroyWatcher(uint64 id);
  void OnResolveService(uint64 id, const std::string& service_name);
  void OnDestroyResolver(uint64 id);

  // Is called by ServiceWatcher as callback.
  void OnServiceUpdated(uint64 id,
                        ServiceWatcher::UpdateType update,
                        const std::string& name);

  // Is called by ServiceResolver as callback.
  void OnServiceResolved(uint64 id,
                         ServiceResolver::RequestStatus status,
                         const ServiceDescription& description);

  ServiceWatchers service_watchers_;
  ServiceResolvers service_resolvers_;

  scoped_ptr<net::MDnsClient> mdns_client_;
  scoped_ptr<ServiceDiscoveryClient> service_discovery_client_;
};

}  // namespace local_discovery

#endif  // CHROME_UTILITY_LOCAL_DISCOVERY_SERVICE_DISCOVERY_MESSAGE_HANDLER_H_

