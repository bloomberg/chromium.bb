// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_host_client.h"

#include "chrome/common/local_discovery/local_discovery_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "net/dns/mdns_client.h"
#include "net/socket/socket_descriptor.h"

#if defined(OS_POSIX)
#include <netinet/in.h>
#include "base/file_descriptor_posix.h"
#endif  // OS_POSIX

namespace local_discovery {

using content::BrowserThread;
using content::UtilityProcessHost;

namespace {

#if defined(OS_POSIX)
SocketInfoList GetSocketsOnFileThread() {
  net::InterfaceIndexFamilyList interfaces(net::GetMDnsInterfacesToBind());
  SocketInfoList sockets;
  for (size_t i = 0; i < interfaces.size(); ++i) {
    DCHECK(interfaces[i].second == net::ADDRESS_FAMILY_IPV4 ||
           interfaces[i].second == net::ADDRESS_FAMILY_IPV6);
    base::FileDescriptor socket_descriptor(
        net::CreatePlatformSocket(
            net::ConvertAddressFamily(interfaces[i].second), SOCK_DGRAM,
                                      IPPROTO_UDP),
        true);
    LOG_IF(ERROR, socket_descriptor.fd == net::kInvalidSocket)
        << "Can't create socket, family=" << interfaces[i].second;
    if (socket_descriptor.fd != net::kInvalidSocket) {
      LocalDiscoveryMsg_SocketInfo socket;
      socket.descriptor = socket_descriptor;
      socket.interface_index = interfaces[i].first;
      socket.address_family = interfaces[i].second;
      sockets.push_back(socket);
    }
  }

  return sockets;
}
#endif  // OS_POSIX

}  // namespace

class ServiceDiscoveryHostClient::ServiceWatcherProxy : public ServiceWatcher {
 public:
  ServiceWatcherProxy(ServiceDiscoveryHostClient* host,
                      const std::string& service_type,
                      const ServiceWatcher::UpdatedCallback& callback)
      : host_(host),
        service_type_(service_type),
        id_(host_->RegisterWatcherCallback(callback)),
        started_(false) {
  }

  virtual ~ServiceWatcherProxy() {
    DVLOG(1) << "~ServiceWatcherProxy with id " << id_;
    host_->UnregisterWatcherCallback(id_);
    if (started_)
      host_->Send(new LocalDiscoveryMsg_DestroyWatcher(id_));
  }

  virtual void Start() OVERRIDE {
    DVLOG(1) << "ServiceWatcher::Start with id " << id_;
    DCHECK(!started_);
    host_->Send(new LocalDiscoveryMsg_StartWatcher(id_, service_type_));
    started_ = true;
  }

  virtual void DiscoverNewServices(bool force_update) OVERRIDE {
    DVLOG(1) << "ServiceWatcher::DiscoverNewServices with id " << id_;
    DCHECK(started_);
    host_->Send(new LocalDiscoveryMsg_DiscoverServices(id_, force_update));
  }

  virtual void SetActivelyRefreshServices(
      bool actively_refresh_services) OVERRIDE {
    DVLOG(1) << "ServiceWatcher::SetActivelyRefreshServices with id " << id_;
    DCHECK(started_);
    host_->Send(new LocalDiscoveryMsg_SetActivelyRefreshServices(
        id_, actively_refresh_services));
  }

  virtual std::string GetServiceType() const OVERRIDE {
    return service_type_;
  }

 private:
  scoped_refptr<ServiceDiscoveryHostClient> host_;
  const std::string service_type_;
  const uint64 id_;
  bool started_;
};

class ServiceDiscoveryHostClient::ServiceResolverProxy
    : public ServiceResolver {
 public:
  ServiceResolverProxy(ServiceDiscoveryHostClient* host,
                       const std::string& service_name,
                       const ServiceResolver::ResolveCompleteCallback& callback)
      : host_(host),
        service_name_(service_name),
        id_(host->RegisterResolverCallback(callback)),
        started_(false) {
  }

  virtual ~ServiceResolverProxy() {
    DVLOG(1) << "~ServiceResolverProxy with id " << id_;
    host_->UnregisterResolverCallback(id_);
    if (started_)
      host_->Send(new LocalDiscoveryMsg_DestroyResolver(id_));
  }

  virtual void StartResolving() OVERRIDE {
    DVLOG(1) << "ServiceResolverProxy::StartResolving with id " << id_;
    DCHECK(!started_);
    host_->Send(new LocalDiscoveryMsg_ResolveService(id_, service_name_));
    started_ = true;
  }

  virtual std::string GetName() const OVERRIDE {
    return service_name_;
  }

 private:
  scoped_refptr<ServiceDiscoveryHostClient> host_;
  const std::string service_name_;
  const uint64 id_;
  bool started_;
};

class ServiceDiscoveryHostClient::LocalDomainResolverProxy
    : public LocalDomainResolver {
 public:
  LocalDomainResolverProxy(ServiceDiscoveryHostClient* host,
                       const std::string& domain,
                       net::AddressFamily address_family,
                       const LocalDomainResolver::IPAddressCallback& callback)
      : host_(host),
        domain_(domain),
        address_family_(address_family),
        id_(host->RegisterLocalDomainResolverCallback(callback)),
        started_(false) {
  }

  virtual ~LocalDomainResolverProxy() {
    DVLOG(1) << "~LocalDomainResolverProxy with id " << id_;
    host_->UnregisterLocalDomainResolverCallback(id_);
    if (started_)
      host_->Send(new LocalDiscoveryMsg_DestroyLocalDomainResolver(id_));
  }

  virtual void Start() OVERRIDE {
    DVLOG(1) << "LocalDomainResolverProxy::Start with id " << id_;
    DCHECK(!started_);
    host_->Send(new LocalDiscoveryMsg_ResolveLocalDomain(id_, domain_,
                                                         address_family_));
    started_ = true;
  }

 private:
  scoped_refptr<ServiceDiscoveryHostClient> host_;
  std::string domain_;
  net::AddressFamily address_family_;
  const uint64 id_;
  bool started_;
};

ServiceDiscoveryHostClient::ServiceDiscoveryHostClient() : current_id_(0) {
  callback_runner_ = base::MessageLoop::current()->message_loop_proxy();
  io_runner_ = BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

ServiceDiscoveryHostClient::~ServiceDiscoveryHostClient() {
  DCHECK(service_watcher_callbacks_.empty());
  DCHECK(service_resolver_callbacks_.empty());
  DCHECK(domain_resolver_callbacks_.empty());
}

scoped_ptr<ServiceWatcher> ServiceDiscoveryHostClient::CreateServiceWatcher(
    const std::string& service_type,
    const ServiceWatcher::UpdatedCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return scoped_ptr<ServiceWatcher>(
      new ServiceWatcherProxy(this, service_type, callback));
}

scoped_ptr<ServiceResolver> ServiceDiscoveryHostClient::CreateServiceResolver(
    const std::string& service_name,
    const ServiceResolver::ResolveCompleteCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return scoped_ptr<ServiceResolver>(
      new ServiceResolverProxy(this, service_name, callback));
}

scoped_ptr<LocalDomainResolver>
ServiceDiscoveryHostClient::CreateLocalDomainResolver(
    const std::string& domain,
    net::AddressFamily address_family,
    const LocalDomainResolver::IPAddressCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return scoped_ptr<LocalDomainResolver>(new LocalDomainResolverProxy(
      this, domain, address_family, callback));
}

uint64 ServiceDiscoveryHostClient::RegisterWatcherCallback(
    const ServiceWatcher::UpdatedCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!ContainsKey(service_watcher_callbacks_, current_id_ + 1));
  service_watcher_callbacks_[++current_id_] = callback;
  return current_id_;
}

uint64 ServiceDiscoveryHostClient::RegisterResolverCallback(
    const ServiceResolver::ResolveCompleteCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!ContainsKey(service_resolver_callbacks_, current_id_ + 1));
  service_resolver_callbacks_[++current_id_] = callback;
  return current_id_;
}

uint64 ServiceDiscoveryHostClient::RegisterLocalDomainResolverCallback(
    const LocalDomainResolver::IPAddressCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!ContainsKey(domain_resolver_callbacks_, current_id_ + 1));
  domain_resolver_callbacks_[++current_id_] = callback;
  return current_id_;
}

void ServiceDiscoveryHostClient::UnregisterWatcherCallback(uint64 id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  service_watcher_callbacks_.erase(id);
}

void ServiceDiscoveryHostClient::UnregisterResolverCallback(uint64 id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  service_resolver_callbacks_.erase(id);
}

void ServiceDiscoveryHostClient::UnregisterLocalDomainResolverCallback(
    uint64 id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  domain_resolver_callbacks_.erase(id);
}

void ServiceDiscoveryHostClient::Start(
    const base::Closure& error_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!utility_host_);
  DCHECK(error_callback_.is_null());
  error_callback_ = error_callback;
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ServiceDiscoveryHostClient::StartOnIOThread, this));
}

void ServiceDiscoveryHostClient::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ServiceDiscoveryHostClient::ShutdownOnIOThread, this));
}

#if defined(OS_POSIX)

void ServiceDiscoveryHostClient::StartOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!utility_host_);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&GetSocketsOnFileThread),
      base::Bind(&ServiceDiscoveryHostClient::OnSocketsReady, this));
}

void ServiceDiscoveryHostClient::OnSocketsReady(const SocketInfoList& sockets) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!utility_host_);
  utility_host_ = UtilityProcessHost::Create(
      this, base::MessageLoopProxy::current().get())->AsWeakPtr();
  if (!utility_host_)
    return;
  utility_host_->EnableMDns();
  utility_host_->StartBatchMode();
  if (sockets.empty()) {
    ShutdownOnIOThread();
    return;
  }
  utility_host_->Send(new LocalDiscoveryMsg_SetSockets(sockets));
  // Send messages for requests made during network enumeration.
  for (size_t i = 0; i < delayed_messages_.size(); ++i)
    utility_host_->Send(delayed_messages_[i]);
  delayed_messages_.weak_clear();
}

#else  // OS_POSIX

void ServiceDiscoveryHostClient::StartOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!utility_host_);
  utility_host_ = UtilityProcessHost::Create(
      this, base::MessageLoopProxy::current().get())->AsWeakPtr();
  if (!utility_host_)
    return;
  utility_host_->EnableMDns();
  utility_host_->StartBatchMode();
  // Windows does not enumerate networks here.
  DCHECK(delayed_messages_.empty());
}

#endif  // OS_POSIX

void ServiceDiscoveryHostClient::ShutdownOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (utility_host_) {
    utility_host_->Send(new LocalDiscoveryMsg_ShutdownLocalDiscovery);
    utility_host_->EndBatchMode();
    utility_host_.reset();
  }
  error_callback_ = base::Closure();
}

void ServiceDiscoveryHostClient::Send(IPC::Message* msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ServiceDiscoveryHostClient::SendOnIOThread, this, msg));
}

void ServiceDiscoveryHostClient::SendOnIOThread(IPC::Message* msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (utility_host_) {
    utility_host_->Send(msg);
  } else {
    delayed_messages_.push_back(msg);
  }
}

void ServiceDiscoveryHostClient::OnProcessCrashed(int exit_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!utility_host_);
  OnError();
}

bool ServiceDiscoveryHostClient::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceDiscoveryHostClient, message)
    IPC_MESSAGE_HANDLER(LocalDiscoveryHostMsg_Error, OnError)
    IPC_MESSAGE_HANDLER(LocalDiscoveryHostMsg_WatcherCallback,
                        OnWatcherCallback)
    IPC_MESSAGE_HANDLER(LocalDiscoveryHostMsg_ResolverCallback,
                        OnResolverCallback)
    IPC_MESSAGE_HANDLER(LocalDiscoveryHostMsg_LocalDomainResolverCallback,
                        OnLocalDomainResolverCallback)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ServiceDiscoveryHostClient::InvalidateWatchers() {
  WatcherCallbacks service_watcher_callbacks;
  service_watcher_callbacks_.swap(service_watcher_callbacks);
  service_resolver_callbacks_.clear();
  domain_resolver_callbacks_.clear();

  for (WatcherCallbacks::iterator i = service_watcher_callbacks.begin();
       i != service_watcher_callbacks.end(); i++) {
    if (!i->second.is_null()) {
      i->second.Run(ServiceWatcher::UPDATE_INVALIDATED, "");
    }
  }
}

void ServiceDiscoveryHostClient::OnError() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!error_callback_.is_null())
    callback_runner_->PostTask(FROM_HERE, error_callback_);
}

void ServiceDiscoveryHostClient::OnWatcherCallback(
    uint64 id,
    ServiceWatcher::UpdateType update,
    const std::string& service_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ServiceDiscoveryHostClient::RunWatcherCallback, this, id,
                 update, service_name));
}

void ServiceDiscoveryHostClient::OnResolverCallback(
    uint64 id,
    ServiceResolver::RequestStatus status,
    const ServiceDescription& description) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ServiceDiscoveryHostClient::RunResolverCallback, this, id,
                 status, description));
}

void ServiceDiscoveryHostClient::OnLocalDomainResolverCallback(
    uint64 id,
    bool success,
    const net::IPAddressNumber& ip_address_ipv4,
    const net::IPAddressNumber& ip_address_ipv6) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ServiceDiscoveryHostClient::RunLocalDomainResolverCallback,
                 this, id, success, ip_address_ipv4, ip_address_ipv6));
}

void ServiceDiscoveryHostClient::RunWatcherCallback(
    uint64 id,
    ServiceWatcher::UpdateType update,
    const std::string& service_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WatcherCallbacks::iterator it = service_watcher_callbacks_.find(id);
  if (it != service_watcher_callbacks_.end() && !it->second.is_null())
    it->second.Run(update, service_name);
}

void ServiceDiscoveryHostClient::RunResolverCallback(
    uint64 id,
    ServiceResolver::RequestStatus status,
    const ServiceDescription& description) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ResolverCallbacks::iterator it = service_resolver_callbacks_.find(id);
  if (it != service_resolver_callbacks_.end() && !it->second.is_null())
    it->second.Run(status, description);
}

void ServiceDiscoveryHostClient::RunLocalDomainResolverCallback(
    uint64 id,
    bool success,
    const net::IPAddressNumber& ip_address_ipv4,
    const net::IPAddressNumber& ip_address_ipv6) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DomainResolverCallbacks::iterator it = domain_resolver_callbacks_.find(id);
  if (it != domain_resolver_callbacks_.end() && !it->second.is_null())
    it->second.Run(success, ip_address_ipv4, ip_address_ipv6);
}

}  // namespace local_discovery
