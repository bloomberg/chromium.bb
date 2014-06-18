// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_LOCAL_DISCOVERY_SERVICE_DISCOVERY_MESSAGE_HANDLER_H_
#define CHROME_UTILITY_LOCAL_DISCOVERY_SERVICE_DISCOVERY_MESSAGE_HANDLER_H_

#include <map>
#include <string>

#include "base/memory/linked_ptr.h"
#include "chrome/common/local_discovery/service_discovery_client.h"
#include "chrome/utility/utility_message_handler.h"

struct LocalDiscoveryMsg_SocketInfo;

namespace net {
class MDnsClient;
}

namespace base {
struct FileDescriptor;
class TaskRunner;
class Thread;
}

namespace tracked_objects {
class Location;
}

namespace local_discovery {

class ServiceDiscoveryClient;

// Handles messages related to local discovery inside utility process.
class ServiceDiscoveryMessageHandler : public UtilityMessageHandler {
 public:
  ServiceDiscoveryMessageHandler();
  virtual ~ServiceDiscoveryMessageHandler();

  // UtilityMessageHandler implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  static void PreSandboxStartup();

 private:
  typedef std::map<uint64, linked_ptr<ServiceWatcher> > ServiceWatchers;
  typedef std::map<uint64, linked_ptr<ServiceResolver> > ServiceResolvers;
  typedef std::map<uint64, linked_ptr<LocalDomainResolver> >
      LocalDomainResolvers;

  // Lazy initializes ServiceDiscoveryClient.
  bool InitializeThread();
  void PostTask(const tracked_objects::Location& from_here,
                const base::Closure& task);

  // IPC message handlers.
#if defined(OS_POSIX)
  void OnSetSockets(const std::vector<LocalDiscoveryMsg_SocketInfo>& sockets);
#endif  // OS_POSIX
  void OnStartWatcher(uint64 id, const std::string& service_type);
  void OnDiscoverServices(uint64 id, bool force_update);
  void OnSetActivelyRefreshServices(uint64 id, bool actively_refresh_services);
  void OnDestroyWatcher(uint64 id);
  void OnResolveService(uint64 id, const std::string& service_name);
  void OnDestroyResolver(uint64 id);
  void OnResolveLocalDomain(uint64 id, const std::string& domain,
                            net::AddressFamily address_family);
  void OnDestroyLocalDomainResolver(uint64 id);

  void InitializeMdns();
  void StartWatcher(uint64 id, const std::string& service_type);
  void DiscoverServices(uint64 id, bool force_update);
  void SetActivelyRefreshServices(uint64 id, bool actively_refresh_services);
  void DestroyWatcher(uint64 id);
  void ResolveService(uint64 id, const std::string& service_name);
  void DestroyResolver(uint64 id);
  void ResolveLocalDomain(uint64 id, const std::string& domain,
                          net::AddressFamily address_family);
  void DestroyLocalDomainResolver(uint64 id);

  void ShutdownLocalDiscovery();
  void ShutdownOnIOThread();

  // Is called by ServiceWatcher as callback.
  void OnServiceUpdated(uint64 id,
                        ServiceWatcher::UpdateType update,
                        const std::string& name);

  // Is called by ServiceResolver as callback.
  void OnServiceResolved(uint64 id,
                         ServiceResolver::RequestStatus status,
                         const ServiceDescription& description);

  // Is called by LocalDomainResolver as callback.
  void OnLocalDomainResolved(uint64 id,
                             bool success,
                             const net::IPAddressNumber& address_ipv4,
                             const net::IPAddressNumber& address_ipv6);

  void Send(IPC::Message* msg);

  ServiceWatchers service_watchers_;
  ServiceResolvers service_resolvers_;
  LocalDomainResolvers local_domain_resolvers_;

  scoped_ptr<net::MDnsClient> mdns_client_;
  scoped_ptr<ServiceDiscoveryClient> service_discovery_client_;

  scoped_refptr<base::TaskRunner> utility_task_runner_;
  scoped_refptr<base::TaskRunner> discovery_task_runner_;
  scoped_ptr<base::Thread> discovery_thread_;
};

}  // namespace local_discovery

#endif  // CHROME_UTILITY_LOCAL_DISCOVERY_SERVICE_DISCOVERY_MESSAGE_HANDLER_H_
