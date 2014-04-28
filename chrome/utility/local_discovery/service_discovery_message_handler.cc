// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/local_discovery/service_discovery_message_handler.h"

#include <algorithm>

#include "base/lazy_instance.h"
#include "chrome/common/local_discovery/local_discovery_messages.h"
#include "chrome/common/local_discovery/service_discovery_client_impl.h"
#include "content/public/utility/utility_thread.h"
#include "net/socket/socket_descriptor.h"
#include "net/udp/datagram_server_socket.h"

namespace local_discovery {

namespace {

void ClosePlatformSocket(net::SocketDescriptor socket);

// Sets socket factory used by |net::CreatePlatformSocket|. Implemetation
// keeps single socket that will be returned to the first call to
// |net::CreatePlatformSocket| during object lifetime.
class ScopedSocketFactory : public net::PlatformSocketFactory {
 public:
  explicit ScopedSocketFactory(net::SocketDescriptor socket) : socket_(socket) {
    net::PlatformSocketFactory::SetInstance(this);
  }

  virtual ~ScopedSocketFactory() {
    net::PlatformSocketFactory::SetInstance(NULL);
    ClosePlatformSocket(socket_);
    socket_ = net::kInvalidSocket;
  }

  virtual net::SocketDescriptor CreateSocket(int family, int type,
                                             int protocol) OVERRIDE {
    DCHECK_EQ(type, SOCK_DGRAM);
    DCHECK(family == AF_INET || family == AF_INET6);
    net::SocketDescriptor result = net::kInvalidSocket;
    std::swap(result, socket_);
    return result;
  }

 private:
  net::SocketDescriptor socket_;
  DISALLOW_COPY_AND_ASSIGN(ScopedSocketFactory);
};

struct SocketInfo {
  SocketInfo(net::SocketDescriptor socket,
             net::AddressFamily address_family,
             uint32 interface_index)
      : socket(socket),
        address_family(address_family),
        interface_index(interface_index) {
  }
  net::SocketDescriptor socket;
  net::AddressFamily address_family;
  uint32 interface_index;
};

// Returns list of sockets preallocated before.
class PreCreatedMDnsSocketFactory : public net::MDnsSocketFactory {
 public:
  PreCreatedMDnsSocketFactory() {}
  virtual ~PreCreatedMDnsSocketFactory() {
    // Not empty if process exits too fast, before starting mDns code. If
    // happened, destructors may crash accessing destroyed global objects.
    sockets_.weak_clear();
  }

  // net::MDnsSocketFactory implementation:
  virtual void CreateSockets(
      ScopedVector<net::DatagramServerSocket>* sockets) OVERRIDE {
    sockets->swap(sockets_);
    Reset();
  }

  void AddSocket(const SocketInfo& socket_info) {
    // Takes ownership of socket_info.socket;
    ScopedSocketFactory platform_factory(socket_info.socket);
    scoped_ptr<net::DatagramServerSocket> socket(
        net::CreateAndBindMDnsSocket(socket_info.address_family,
                                     socket_info.interface_index));
    if (socket) {
      socket->DetachFromThread();
      sockets_.push_back(socket.release());
    }
  }

  void Reset() {
    sockets_.clear();
  }

 private:
  ScopedVector<net::DatagramServerSocket> sockets_;

  DISALLOW_COPY_AND_ASSIGN(PreCreatedMDnsSocketFactory);
};

base::LazyInstance<PreCreatedMDnsSocketFactory>
    g_local_discovery_socket_factory = LAZY_INSTANCE_INITIALIZER;

#if defined(OS_WIN)

void ClosePlatformSocket(net::SocketDescriptor socket) {
  ::closesocket(socket);
}

void StaticInitializeSocketFactory() {
  net::InterfaceIndexFamilyList interfaces(net::GetMDnsInterfacesToBind());
  for (size_t i = 0; i < interfaces.size(); ++i) {
    DCHECK(interfaces[i].second == net::ADDRESS_FAMILY_IPV4 ||
           interfaces[i].second == net::ADDRESS_FAMILY_IPV6);
    net::SocketDescriptor descriptor =
        net::CreatePlatformSocket(
            net::ConvertAddressFamily(interfaces[i].second), SOCK_DGRAM,
                                      IPPROTO_UDP);
    g_local_discovery_socket_factory.Get().AddSocket(
        SocketInfo(descriptor, interfaces[i].second, interfaces[i].first));
  }
}

#else  // OS_WIN

void ClosePlatformSocket(net::SocketDescriptor socket) {
  ::close(socket);
}

void StaticInitializeSocketFactory() {
}

#endif  // OS_WIN

void SendHostMessageOnUtilityThread(IPC::Message* msg) {
  content::UtilityThread::Get()->Send(msg);
}

std::string WatcherUpdateToString(ServiceWatcher::UpdateType update) {
  switch (update) {
    case ServiceWatcher::UPDATE_ADDED:
      return "UPDATE_ADDED";
    case ServiceWatcher::UPDATE_CHANGED:
      return "UPDATE_CHANGED";
    case ServiceWatcher::UPDATE_REMOVED:
      return "UPDATE_REMOVED";
    case ServiceWatcher::UPDATE_INVALIDATED:
      return "UPDATE_INVALIDATED";
  }
  return "Unknown Update";
}

std::string ResolverStatusToString(ServiceResolver::RequestStatus status) {
  switch (status) {
    case ServiceResolver::STATUS_SUCCESS:
      return "STATUS_SUCESS";
    case ServiceResolver::STATUS_REQUEST_TIMEOUT:
      return "STATUS_REQUEST_TIMEOUT";
    case ServiceResolver::STATUS_KNOWN_NONEXISTENT:
      return "STATUS_KNOWN_NONEXISTENT";
  }
  return "Unknown Status";
}

}  // namespace

ServiceDiscoveryMessageHandler::ServiceDiscoveryMessageHandler() {
}

ServiceDiscoveryMessageHandler::~ServiceDiscoveryMessageHandler() {
  DCHECK(!discovery_thread_);
}

void ServiceDiscoveryMessageHandler::PreSandboxStartup() {
  StaticInitializeSocketFactory();
}

void ServiceDiscoveryMessageHandler::InitializeMdns() {
  if (service_discovery_client_ || mdns_client_)
    return;

  mdns_client_ = net::MDnsClient::CreateDefault();
  bool result =
      mdns_client_->StartListening(g_local_discovery_socket_factory.Pointer());
  // Close unused sockets.
  g_local_discovery_socket_factory.Get().Reset();
  if (!result) {
    VLOG(1) << "Failed to start MDnsClient";
    Send(new LocalDiscoveryHostMsg_Error());
    return;
  }

  service_discovery_client_.reset(
      new local_discovery::ServiceDiscoveryClientImpl(mdns_client_.get()));
}

bool ServiceDiscoveryMessageHandler::InitializeThread() {
  if (discovery_task_runner_)
    return true;
  if (discovery_thread_)
    return false;
  utility_task_runner_ = base::MessageLoop::current()->message_loop_proxy();
  discovery_thread_.reset(new base::Thread("ServiceDiscoveryThread"));
  base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
  if (discovery_thread_->StartWithOptions(thread_options)) {
    discovery_task_runner_ = discovery_thread_->message_loop_proxy();
    discovery_task_runner_->PostTask(FROM_HERE,
        base::Bind(&ServiceDiscoveryMessageHandler::InitializeMdns,
                    base::Unretained(this)));
  }
  return discovery_task_runner_ != NULL;
}

bool ServiceDiscoveryMessageHandler::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceDiscoveryMessageHandler, message)
#if defined(OS_POSIX)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_SetSockets, OnSetSockets)
#endif  // OS_POSIX
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_StartWatcher, OnStartWatcher)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_DiscoverServices, OnDiscoverServices)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_SetActivelyRefreshServices,
                        OnSetActivelyRefreshServices)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_DestroyWatcher, OnDestroyWatcher)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_ResolveService, OnResolveService)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_DestroyResolver, OnDestroyResolver)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_ResolveLocalDomain,
                        OnResolveLocalDomain)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_DestroyLocalDomainResolver,
                        OnDestroyLocalDomainResolver)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_ShutdownLocalDiscovery,
                        ShutdownLocalDiscovery)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ServiceDiscoveryMessageHandler::PostTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  if (!InitializeThread())
    return;
  discovery_task_runner_->PostTask(from_here, task);
}

#if defined(OS_POSIX)
void ServiceDiscoveryMessageHandler::OnSetSockets(
    const std::vector<LocalDiscoveryMsg_SocketInfo>& sockets) {
  for (size_t i = 0; i < sockets.size(); ++i) {
    g_local_discovery_socket_factory.Get().AddSocket(
        SocketInfo(sockets[i].descriptor.fd, sockets[i].address_family,
                   sockets[i].interface_index));
  }
}
#endif  // OS_POSIX

void ServiceDiscoveryMessageHandler::OnStartWatcher(
    uint64 id,
    const std::string& service_type) {
  PostTask(FROM_HERE,
           base::Bind(&ServiceDiscoveryMessageHandler::StartWatcher,
                      base::Unretained(this), id, service_type));
}

void ServiceDiscoveryMessageHandler::OnDiscoverServices(uint64 id,
                                                        bool force_update) {
  PostTask(FROM_HERE,
           base::Bind(&ServiceDiscoveryMessageHandler::DiscoverServices,
                      base::Unretained(this), id, force_update));
}

void ServiceDiscoveryMessageHandler::OnSetActivelyRefreshServices(
    uint64 id, bool actively_refresh_services) {
  PostTask(FROM_HERE,
           base::Bind(
               &ServiceDiscoveryMessageHandler::SetActivelyRefreshServices,
               base::Unretained(this), id, actively_refresh_services));
}

void ServiceDiscoveryMessageHandler::OnDestroyWatcher(uint64 id) {
  PostTask(FROM_HERE,
           base::Bind(&ServiceDiscoveryMessageHandler::DestroyWatcher,
                      base::Unretained(this), id));
}

void ServiceDiscoveryMessageHandler::OnResolveService(
    uint64 id,
    const std::string& service_name) {
  PostTask(FROM_HERE,
           base::Bind(&ServiceDiscoveryMessageHandler::ResolveService,
                      base::Unretained(this), id, service_name));
}

void ServiceDiscoveryMessageHandler::OnDestroyResolver(uint64 id) {
  PostTask(FROM_HERE,
           base::Bind(&ServiceDiscoveryMessageHandler::DestroyResolver,
                      base::Unretained(this), id));
}

void ServiceDiscoveryMessageHandler::OnResolveLocalDomain(
    uint64 id, const std::string& domain,
    net::AddressFamily address_family) {
    PostTask(FROM_HERE,
           base::Bind(&ServiceDiscoveryMessageHandler::ResolveLocalDomain,
                      base::Unretained(this), id, domain, address_family));
}

void ServiceDiscoveryMessageHandler::OnDestroyLocalDomainResolver(uint64 id) {
  PostTask(FROM_HERE,
           base::Bind(
               &ServiceDiscoveryMessageHandler::DestroyLocalDomainResolver,
               base::Unretained(this), id));
}

void ServiceDiscoveryMessageHandler::StartWatcher(
    uint64 id,
    const std::string& service_type) {
  VLOG(1) << "StartWatcher, id=" << id << ", type=" << service_type;
  if (!service_discovery_client_)
    return;
  DCHECK(!ContainsKey(service_watchers_, id));
  scoped_ptr<ServiceWatcher> watcher(
      service_discovery_client_->CreateServiceWatcher(
          service_type,
          base::Bind(&ServiceDiscoveryMessageHandler::OnServiceUpdated,
                     base::Unretained(this), id)));
  watcher->Start();
  service_watchers_[id].reset(watcher.release());
}

void ServiceDiscoveryMessageHandler::DiscoverServices(uint64 id,
                                                      bool force_update) {
  VLOG(1) << "DiscoverServices, id=" << id;
  if (!service_discovery_client_)
    return;
  DCHECK(ContainsKey(service_watchers_, id));
  service_watchers_[id]->DiscoverNewServices(force_update);
}

void ServiceDiscoveryMessageHandler::SetActivelyRefreshServices(
    uint64 id,
    bool actively_refresh_services) {
  VLOG(1) << "ActivelyRefreshServices, id=" << id;
  if (!service_discovery_client_)
    return;
  DCHECK(ContainsKey(service_watchers_, id));
  service_watchers_[id]->SetActivelyRefreshServices(actively_refresh_services);
}

void ServiceDiscoveryMessageHandler::DestroyWatcher(uint64 id) {
  VLOG(1) << "DestoryWatcher, id=" << id;
  if (!service_discovery_client_)
    return;
  service_watchers_.erase(id);
}

void ServiceDiscoveryMessageHandler::ResolveService(
    uint64 id,
    const std::string& service_name) {
  VLOG(1) << "ResolveService, id=" << id << ", name=" << service_name;
  if (!service_discovery_client_)
    return;
  DCHECK(!ContainsKey(service_resolvers_, id));
  scoped_ptr<ServiceResolver> resolver(
      service_discovery_client_->CreateServiceResolver(
          service_name,
          base::Bind(&ServiceDiscoveryMessageHandler::OnServiceResolved,
                     base::Unretained(this), id)));
  resolver->StartResolving();
  service_resolvers_[id].reset(resolver.release());
}

void ServiceDiscoveryMessageHandler::DestroyResolver(uint64 id) {
  VLOG(1) << "DestroyResolver, id=" << id;
  if (!service_discovery_client_)
    return;
  service_resolvers_.erase(id);
}

void ServiceDiscoveryMessageHandler::ResolveLocalDomain(
    uint64 id,
    const std::string& domain,
    net::AddressFamily address_family) {
  VLOG(1) << "ResolveLocalDomain, id=" << id << ", domain=" << domain;
  if (!service_discovery_client_)
    return;
  DCHECK(!ContainsKey(local_domain_resolvers_, id));
  scoped_ptr<LocalDomainResolver> resolver(
      service_discovery_client_->CreateLocalDomainResolver(
          domain, address_family,
          base::Bind(&ServiceDiscoveryMessageHandler::OnLocalDomainResolved,
                     base::Unretained(this), id)));
  resolver->Start();
  local_domain_resolvers_[id].reset(resolver.release());
}

void ServiceDiscoveryMessageHandler::DestroyLocalDomainResolver(uint64 id) {
  VLOG(1) << "DestroyLocalDomainResolver, id=" << id;
  if (!service_discovery_client_)
    return;
  local_domain_resolvers_.erase(id);
}

void ServiceDiscoveryMessageHandler::ShutdownLocalDiscovery() {
  if (!discovery_task_runner_)
    return;

  discovery_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ServiceDiscoveryMessageHandler::ShutdownOnIOThread,
                 base::Unretained(this)));

  // This will wait for message loop to drain, so ShutdownOnIOThread will
  // definitely be called.
  discovery_thread_.reset();
}

void ServiceDiscoveryMessageHandler::ShutdownOnIOThread() {
  VLOG(1) << "ShutdownLocalDiscovery";
  service_watchers_.clear();
  service_resolvers_.clear();
  local_domain_resolvers_.clear();
  service_discovery_client_.reset();
  mdns_client_.reset();
}

void ServiceDiscoveryMessageHandler::OnServiceUpdated(
    uint64 id,
    ServiceWatcher::UpdateType update,
    const std::string& name) {
  VLOG(1) << "OnServiceUpdated, id=" << id
          << ", status=" << WatcherUpdateToString(update) << ", name=" << name;
  DCHECK(service_discovery_client_);

  Send(new LocalDiscoveryHostMsg_WatcherCallback(id, update, name));
}

void ServiceDiscoveryMessageHandler::OnServiceResolved(
    uint64 id,
    ServiceResolver::RequestStatus status,
    const ServiceDescription& description) {
  VLOG(1) << "OnServiceResolved, id=" << id
          << ", status=" << ResolverStatusToString(status)
          << ", name=" << description.service_name;

  DCHECK(service_discovery_client_);
  Send(new LocalDiscoveryHostMsg_ResolverCallback(id, status, description));
}

void ServiceDiscoveryMessageHandler::OnLocalDomainResolved(
    uint64 id,
    bool success,
    const net::IPAddressNumber& address_ipv4,
    const net::IPAddressNumber& address_ipv6) {
  VLOG(1) << "OnLocalDomainResolved, id=" << id
          << ", IPv4=" << (address_ipv4.empty() ? "" :
                           net::IPAddressToString(address_ipv4))
          << ", IPv6=" << (address_ipv6.empty() ? "" :
                           net::IPAddressToString(address_ipv6));

  DCHECK(service_discovery_client_);
  Send(new LocalDiscoveryHostMsg_LocalDomainResolverCallback(
          id, success, address_ipv4, address_ipv6));
}

void ServiceDiscoveryMessageHandler::Send(IPC::Message* msg) {
  utility_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&SendHostMessageOnUtilityThread,
                                            msg));
}

}  // namespace local_discovery
