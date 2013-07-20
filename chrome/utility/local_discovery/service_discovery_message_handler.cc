// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/local_discovery/service_discovery_message_handler.h"

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

}  // namespace

namespace local_discovery {

ServiceDiscoveryMessageHandler::ServiceDiscoveryMessageHandler() {
}

ServiceDiscoveryMessageHandler::~ServiceDiscoveryMessageHandler() {
}

void ServiceDiscoveryMessageHandler::PreSandboxStartup() {
  ScopedSocketFactorySetter::Initialize();
}

bool ServiceDiscoveryMessageHandler::Initialize() {
  if (service_discovery_client_)
    return true;

  if (mdns_client_)  // We tried but failed before.
    return false;

  mdns_client_ = net::MDnsClient::CreateDefault();
  {
    // Temporarily redirect network code to use pre-created sockets.
    ScopedSocketFactorySetter socket_factory_setter;
    if (!mdns_client_->StartListening())
      return false;
  }

  service_discovery_client_.reset(
      new local_discovery::ServiceDiscoveryClientImpl(mdns_client_.get()));
  return true;
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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ServiceDiscoveryMessageHandler::OnStartWatcher(
    uint64 id,
    const std::string& service_type) {
  if (!Initialize())
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

void ServiceDiscoveryMessageHandler::OnDiscoverServices(uint64 id,
                                                        bool force_update) {
  if (!service_discovery_client_)
    return;
  DCHECK(ContainsKey(service_watchers_, id));
  service_watchers_[id]->DiscoverNewServices(force_update);
}

void ServiceDiscoveryMessageHandler::OnDestroyWatcher(uint64 id) {
  if (!service_discovery_client_)
    return;
  DCHECK(ContainsKey(service_watchers_, id));
  service_watchers_.erase(id);
}

void ServiceDiscoveryMessageHandler::OnResolveService(
    uint64 id,
    const std::string& service_name) {
  if (!Initialize())
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

void ServiceDiscoveryMessageHandler::OnDestroyResolver(uint64 id) {
  if (!service_discovery_client_)
    return;
  DCHECK(ContainsKey(service_resolvers_, id));
  service_resolvers_.erase(id);
}

void ServiceDiscoveryMessageHandler::OnServiceUpdated(
    uint64 id,
    ServiceWatcher::UpdateType update,
    const std::string& name) {
  DCHECK(service_discovery_client_);
  content::UtilityThread::Get()->Send(
      new LocalDiscoveryHostMsg_WatcherCallback(id, update, name));
}

void ServiceDiscoveryMessageHandler::OnServiceResolved(
    uint64 id,
    ServiceResolver::RequestStatus status,
    const ServiceDescription& description) {
  DCHECK(service_discovery_client_);
  content::UtilityThread::Get()->Send(
      new LocalDiscoveryHostMsg_ResolverCallback(id, status, description));
}

}  // namespace local_discovery

