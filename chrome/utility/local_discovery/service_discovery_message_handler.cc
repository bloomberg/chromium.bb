// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/local_discovery/service_discovery_message_handler.h"

#include <algorithm>

#include "base/lazy_instance.h"
#include "chrome/common/local_discovery/local_discovery_messages.h"
#include "chrome/utility/local_discovery/service_discovery_client_impl.h"
#include "content/public/utility/utility_thread.h"
#include "net/socket/socket_descriptor.h"

namespace local_discovery {

namespace {

void ClosePlatformSocket(net::SocketDescriptor socket);

class SocketFactory : public net::PlatformSocketFactory {
 public:
  SocketFactory()
      : socket_v4_(net::kInvalidSocket),
        socket_v6_(net::kInvalidSocket) {
  }

  void SetSockets(net::SocketDescriptor socket_v4,
                  net::SocketDescriptor socket_v6) {
    Reset();
    socket_v4_ = socket_v4;
    socket_v6_ = socket_v6;
  }

  void Reset() {
    if (socket_v4_ != net::kInvalidSocket) {
      ClosePlatformSocket(socket_v4_);
      socket_v4_ = net::kInvalidSocket;
    }
    if (socket_v6_ != net::kInvalidSocket) {
      ClosePlatformSocket(socket_v6_);
      socket_v6_ = net::kInvalidSocket;
    }
  }

  virtual ~SocketFactory() {
    Reset();
  }

 protected:
  virtual net::SocketDescriptor CreateSocket(int family, int type,
                                             int protocol) OVERRIDE {
    net::SocketDescriptor result = net::kInvalidSocket;
    if (type != SOCK_DGRAM) {
      NOTREACHED();
    } else if (family == AF_INET) {
      std::swap(result, socket_v4_);
    } else if (family == AF_INET6) {
      std::swap(result, socket_v6_);
    }
    return result;
  }

 private:
  net::SocketDescriptor socket_v4_;
  net::SocketDescriptor socket_v6_;

  DISALLOW_COPY_AND_ASSIGN(SocketFactory);
};

base::LazyInstance<SocketFactory>
    g_local_discovery_socket_factory = LAZY_INSTANCE_INITIALIZER;

class ScopedSocketFactorySetter {
 public:
  ScopedSocketFactorySetter() {
    net::PlatformSocketFactory::SetInstance(
        &g_local_discovery_socket_factory.Get());
  }

  ~ScopedSocketFactorySetter() {
    net::PlatformSocketFactory::SetInstance(NULL);
    g_local_discovery_socket_factory.Get().Reset();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedSocketFactorySetter);
};

#if defined(OS_WIN)

void ClosePlatformSocket(net::SocketDescriptor socket) {
  ::closesocket(socket);
}

void StaticInitializeSocketFactory() {
  g_local_discovery_socket_factory.Get().SetSockets(
      net::CreatePlatformSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP),
      net::CreatePlatformSocket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP));
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
  {
    // Temporarily redirect network code to use pre-created sockets.
    ScopedSocketFactorySetter socket_factory_setter;
    if (!mdns_client_->StartListening()) {
      Send(new LocalDiscoveryHostMsg_Error());
      return;
    }
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
    const base::FileDescriptor& socket_v4,
    const base::FileDescriptor& socket_v6) {
  g_local_discovery_socket_factory.Get().SetSockets(socket_v4.fd, socket_v6.fd);
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
  VLOG(1) << "StartWatcher with id " << id;
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
  VLOG(1) << "DiscoverServices with id " << id;
  if (!service_discovery_client_)
    return;
  DCHECK(ContainsKey(service_watchers_, id));
  service_watchers_[id]->DiscoverNewServices(force_update);
}

void ServiceDiscoveryMessageHandler::DestroyWatcher(uint64 id) {
  VLOG(1) << "DestoryWatcher with id " << id;
  if (!service_discovery_client_)
    return;
  service_watchers_.erase(id);
}

void ServiceDiscoveryMessageHandler::ResolveService(
    uint64 id,
    const std::string& service_name) {
  VLOG(1) << "ResolveService with id " << id;
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
  VLOG(1) << "DestroyResolver with id " << id;
  if (!service_discovery_client_)
    return;
  service_resolvers_.erase(id);
}

void ServiceDiscoveryMessageHandler::ResolveLocalDomain(
    uint64 id,
    const std::string& domain,
    net::AddressFamily address_family) {
  VLOG(1) << "ResolveLocalDomain with id " << id;
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
  VLOG(1) << "DestroyLocalDomainResolver with id " << id;
  if (!service_discovery_client_)
    return;
  local_domain_resolvers_.erase(id);
}

void ServiceDiscoveryMessageHandler::ShutdownLocalDiscovery() {
  VLOG(1) << "ShutdownLocalDiscovery";
  discovery_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ServiceDiscoveryMessageHandler::ShutdownOnIOThread,
                 base::Unretained(this)));

  // This will wait for message loop to drain, so ShutdownOnIOThread will
  // definitely be called.
  discovery_thread_.reset();
}

void ServiceDiscoveryMessageHandler::ShutdownOnIOThread() {
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
  VLOG(1) << "OnServiceUpdated with id " << id << WatcherUpdateToString(update);
  DCHECK(service_discovery_client_);

  Send(new LocalDiscoveryHostMsg_WatcherCallback(id, update, name));
}

void ServiceDiscoveryMessageHandler::OnServiceResolved(
    uint64 id,
    ServiceResolver::RequestStatus status,
    const ServiceDescription& description) {
  VLOG(1) << "OnServiceResolved with id " << id << " and status "
          << ResolverStatusToString(status);

  DCHECK(service_discovery_client_);
  Send(new LocalDiscoveryHostMsg_ResolverCallback(id, status, description));
}

void ServiceDiscoveryMessageHandler::OnLocalDomainResolved(
    uint64 id,
    bool success,
    const net::IPAddressNumber& address_ipv4,
    const net::IPAddressNumber& address_ipv6) {
  VLOG(1) << "OnLocalDomainResolved with id=" << id
          << ", IPv4=" << net::IPAddressToString(address_ipv4)
          << ", IPv6=" << net::IPAddressToString(address_ipv6);

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
