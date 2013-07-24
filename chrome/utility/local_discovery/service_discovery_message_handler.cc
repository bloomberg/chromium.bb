// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/local_discovery/service_discovery_message_handler.h"

#include <algorithm>

#include "base/command_line.h"
#include "chrome/common/local_discovery/local_discovery_messages.h"
#include "chrome/utility/local_discovery/service_discovery_client_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/utility/utility_thread.h"

#if defined(OS_WIN)

#include "base/lazy_instance.h"
#include "net/base/winsock_init.h"
#include "net/base/winsock_util.h"

#endif  // OS_WIN

namespace local_discovery {

namespace {

bool NeedsSockets() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoSandbox) &&
         CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kUtilityProcessEnableMDns);
}

#if defined(OS_WIN)

class SocketFactory : public net::PlatformSocketFactory {
 public:
  SocketFactory()
      : socket_v4_(NULL),
        socket_v6_(NULL) {
    net::EnsureWinsockInit();
    socket_v4_ = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0,
                           WSA_FLAG_OVERLAPPED);
    socket_v6_ = WSASocket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, NULL, 0,
                           WSA_FLAG_OVERLAPPED);
  }

  void Reset() {
    if (socket_v4_ != INVALID_SOCKET) {
      closesocket(socket_v4_);
      socket_v4_ = INVALID_SOCKET;
    }
    if (socket_v6_ != INVALID_SOCKET) {
      closesocket(socket_v6_);
      socket_v6_ = INVALID_SOCKET;
    }
  }

  virtual ~SocketFactory() {
    Reset();
  }

  virtual SOCKET CreateSocket(int family, int type, int protocol) OVERRIDE {
    SOCKET result = INVALID_SOCKET;
    if (type != SOCK_DGRAM && protocol != IPPROTO_UDP) {
      NOTREACHED();
    } else if (family == AF_INET) {
      std::swap(result, socket_v4_);
    } else if (family == AF_INET6) {
      std::swap(result, socket_v6_);
    }
    return result;
  }

  SOCKET socket_v4_;
  SOCKET socket_v6_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SocketFactory);
};

base::LazyInstance<SocketFactory>
    g_local_discovery_socket_factory = LAZY_INSTANCE_INITIALIZER;

class ScopedSocketFactorySetter {
 public:
  ScopedSocketFactorySetter() {
    if (NeedsSockets()) {
      net::PlatformSocketFactory::SetInstance(
          &g_local_discovery_socket_factory.Get());
    }
  }

  ~ScopedSocketFactorySetter() {
    if (NeedsSockets()) {
      net::PlatformSocketFactory::SetInstance(NULL);
      g_local_discovery_socket_factory.Get().Reset();
    }
  }

  static void Initialize() {
    if (NeedsSockets()) {
      g_local_discovery_socket_factory.Get();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedSocketFactorySetter);
};

#else  // OS_WIN

class ScopedSocketFactorySetter {
 public:
  ScopedSocketFactorySetter() {}

  static void Initialize() {
    // TODO(vitalybuka) : implement socket access from sandbox for other
    // platforms.
    DCHECK(!NeedsSockets());
  }
};

#endif  // OS_WIN

void SendServiceResolved(uint64 id, ServiceResolver::RequestStatus status,
                         const ServiceDescription& description) {
  content::UtilityThread::Get()->Send(
      new LocalDiscoveryHostMsg_ResolverCallback(id, status, description));
}

void SendServiceUpdated(uint64 id, ServiceWatcher::UpdateType update,
                        const std::string& name) {
  content::UtilityThread::Get()->Send(
      new LocalDiscoveryHostMsg_WatcherCallback(id, update, name));
}

void SendLocalDomainResolved(uint64 id, bool success,
                             const net::IPAddressNumber& address) {
  content::UtilityThread::Get()->Send(
      new LocalDiscoveryHostMsg_LocalDomainResolverCallback(
          id, success, address));
}

}  // namespace

ServiceDiscoveryMessageHandler::ServiceDiscoveryMessageHandler() {
}

ServiceDiscoveryMessageHandler::~ServiceDiscoveryMessageHandler() {
  discovery_thread_.reset();
}

void ServiceDiscoveryMessageHandler::PreSandboxStartup() {
  ScopedSocketFactorySetter::Initialize();
}

void ServiceDiscoveryMessageHandler::InitializeMdns() {
  if (service_discovery_client_ || mdns_client_)
    return;

  mdns_client_ = net::MDnsClient::CreateDefault();
  {
    // Temporarily redirect network code to use pre-created sockets.
    ScopedSocketFactorySetter socket_factory_setter;
    if (!mdns_client_->StartListening())
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
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_StartWatcher, OnStartWatcher)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_DiscoverServices, OnDiscoverServices)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_DestroyWatcher, OnDestroyWatcher)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_ResolveService, OnResolveService)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_DestroyResolver, OnDestroyResolver)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_ResolveLocalDomain,
                        OnResolveLocalDomain)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_DestroyLocalDomainResolver,
                        OnDestroyLocalDomainResolver)
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
  if (!service_discovery_client_)
    return;
  DCHECK(ContainsKey(service_watchers_, id));
  service_watchers_[id]->DiscoverNewServices(force_update);
}

void ServiceDiscoveryMessageHandler::DestroyWatcher(uint64 id) {
  if (!service_discovery_client_)
    return;
  DCHECK(ContainsKey(service_watchers_, id));
  service_watchers_.erase(id);
}

void ServiceDiscoveryMessageHandler::ResolveService(
    uint64 id,
    const std::string& service_name) {
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
  if (!service_discovery_client_)
    return;
  DCHECK(ContainsKey(service_resolvers_, id));
  service_resolvers_.erase(id);
}

void ServiceDiscoveryMessageHandler::ResolveLocalDomain(
    uint64 id,
    const std::string& domain,
    net::AddressFamily address_family) {
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
  if (!service_discovery_client_)
    return;
  DCHECK(ContainsKey(local_domain_resolvers_, id));
  local_domain_resolvers_.erase(id);
}

void ServiceDiscoveryMessageHandler::OnServiceUpdated(
    uint64 id,
    ServiceWatcher::UpdateType update,
    const std::string& name) {
  DCHECK(service_discovery_client_);
  utility_task_runner_->PostTask(FROM_HERE,
      base::Bind(&SendServiceUpdated, id, update, name));
}

void ServiceDiscoveryMessageHandler::OnServiceResolved(
    uint64 id,
    ServiceResolver::RequestStatus status,
    const ServiceDescription& description) {
  DCHECK(service_discovery_client_);
  utility_task_runner_->PostTask(FROM_HERE,
      base::Bind(&SendServiceResolved, id, status, description));
}

void ServiceDiscoveryMessageHandler::OnLocalDomainResolved(
    uint64 id,
    bool success,
    const net::IPAddressNumber& address) {
  DCHECK(service_discovery_client_);
  utility_task_runner_->PostTask(FROM_HERE, base::Bind(&SendLocalDomainResolved,
                                                       id, success, address));
}


}  // namespace local_discovery
