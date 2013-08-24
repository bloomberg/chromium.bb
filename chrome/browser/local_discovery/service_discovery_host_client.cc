// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_host_client.h"

#include "chrome/common/local_discovery/local_discovery_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"

namespace local_discovery {

using content::BrowserThread;
using content::UtilityProcessHost;

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
    host_->UnregisterWatcherCallback(id_);
    if (started_)
      host_->Send(new LocalDiscoveryMsg_DestroyWatcher(id_));
  }

  virtual void Start() OVERRIDE {
    DCHECK(!started_);
    host_->Send(new LocalDiscoveryMsg_StartWatcher(id_, service_type_));
    started_ = true;
  }

  virtual void DiscoverNewServices(bool force_update) OVERRIDE {
    DCHECK(started_);
    host_->Send(new LocalDiscoveryMsg_DiscoverServices(id_, force_update));
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
    host_->UnregisterResolverCallback(id_);
    if (started_)
      host_->Send(new LocalDiscoveryMsg_DestroyResolver(id_));
  }

  virtual void StartResolving() OVERRIDE {
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
    host_->UnregisterLocalDomainResolverCallback(id_);
    if (started_)
      host_->Send(new LocalDiscoveryMsg_DestroyLocalDomainResolver(id_));
  }

  virtual void Start() OVERRIDE {
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
}

ServiceDiscoveryHostClient::~ServiceDiscoveryHostClient() {
  // The ServiceDiscoveryHostClient may be destroyed from the IO thread or the
  // owning thread.
  DetachFromThread();
  DCHECK(service_watcher_callbacks_.empty());
  DCHECK(service_resolver_callbacks_.empty());
  DCHECK(domain_resolver_callbacks_.empty());
}

scoped_ptr<ServiceWatcher> ServiceDiscoveryHostClient::CreateServiceWatcher(
    const std::string& service_type,
    const ServiceWatcher::UpdatedCallback& callback) {
  DCHECK(CalledOnValidThread());
  return scoped_ptr<ServiceWatcher>(
      new ServiceWatcherProxy(this, service_type, callback));
}

scoped_ptr<ServiceResolver> ServiceDiscoveryHostClient::CreateServiceResolver(
    const std::string& service_name,
    const ServiceResolver::ResolveCompleteCallback& callback) {
  DCHECK(CalledOnValidThread());
  return scoped_ptr<ServiceResolver>(
      new ServiceResolverProxy(this, service_name, callback));
}

scoped_ptr<LocalDomainResolver>
ServiceDiscoveryHostClient::CreateLocalDomainResolver(
    const std::string& domain,
    net::AddressFamily address_family,
    const LocalDomainResolver::IPAddressCallback& callback) {
  DCHECK(CalledOnValidThread());
  return scoped_ptr<LocalDomainResolver>(new LocalDomainResolverProxy(
      this, domain, address_family, callback));
}

uint64 ServiceDiscoveryHostClient::RegisterWatcherCallback(
    const ServiceWatcher::UpdatedCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!ContainsKey(service_watcher_callbacks_, current_id_ + 1));
  service_watcher_callbacks_[++current_id_] = callback;
  return current_id_;
}

uint64 ServiceDiscoveryHostClient::RegisterResolverCallback(
    const ServiceResolver::ResolveCompleteCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!ContainsKey(service_resolver_callbacks_, current_id_ + 1));
  service_resolver_callbacks_[++current_id_] = callback;
  return current_id_;
}

uint64 ServiceDiscoveryHostClient::RegisterLocalDomainResolverCallback(
    const LocalDomainResolver::IPAddressCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!ContainsKey(domain_resolver_callbacks_, current_id_ + 1));
  domain_resolver_callbacks_[++current_id_] = callback;
  return current_id_;
}

void ServiceDiscoveryHostClient::UnregisterWatcherCallback(uint64 id) {
  DCHECK(CalledOnValidThread());
  DCHECK(ContainsKey(service_watcher_callbacks_, id));
  service_watcher_callbacks_.erase(id);
}

void ServiceDiscoveryHostClient::UnregisterResolverCallback(uint64 id) {
  DCHECK(CalledOnValidThread());
  DCHECK(ContainsKey(service_resolver_callbacks_, id));
  service_resolver_callbacks_.erase(id);
}

void ServiceDiscoveryHostClient::UnregisterLocalDomainResolverCallback(
    uint64 id) {
  DCHECK(CalledOnValidThread());
  DCHECK(ContainsKey(domain_resolver_callbacks_, id));
  domain_resolver_callbacks_.erase(id);
}

void ServiceDiscoveryHostClient::Start() {
  DCHECK(CalledOnValidThread());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ServiceDiscoveryHostClient::StartOnIOThread, this));
}

void ServiceDiscoveryHostClient::Shutdown() {
  DCHECK(CalledOnValidThread());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ServiceDiscoveryHostClient::ShutdownOnIOThread, this));
}

void ServiceDiscoveryHostClient::StartOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  utility_host_ = UtilityProcessHost::Create(
      this, base::MessageLoopProxy::current().get())->AsWeakPtr();
  if (utility_host_) {
    utility_host_->EnableZygote();
    utility_host_->EnableMDns();
    utility_host_->StartBatchMode();
  }
}

void ServiceDiscoveryHostClient::ShutdownOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (utility_host_) {
    utility_host_->Send(new LocalDiscoveryMsg_ShutdownLocalDiscovery);
    utility_host_->EndBatchMode();
  }
}

void ServiceDiscoveryHostClient::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ServiceDiscoveryHostClient::SendOnIOThread, this, msg));
}

void ServiceDiscoveryHostClient::SendOnIOThread(IPC::Message* msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (utility_host_)
    utility_host_->Send(msg);
}

bool ServiceDiscoveryHostClient::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceDiscoveryHostClient, message)
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
  DCHECK(CalledOnValidThread());
  WatcherCallbacks::iterator it = service_watcher_callbacks_.find(id);
  if (it != service_watcher_callbacks_.end() && !it->second.is_null())
    it->second.Run(update, service_name);
}

void ServiceDiscoveryHostClient::RunResolverCallback(
    uint64 id,
    ServiceResolver::RequestStatus status,
    const ServiceDescription& description) {
  DCHECK(CalledOnValidThread());
  ResolverCallbacks::iterator it = service_resolver_callbacks_.find(id);
  if (it != service_resolver_callbacks_.end() && !it->second.is_null())
    it->second.Run(status, description);
}

void ServiceDiscoveryHostClient::RunLocalDomainResolverCallback(
    uint64 id,
    bool success,
    const net::IPAddressNumber& ip_address_ipv4,
    const net::IPAddressNumber& ip_address_ipv6) {
  DCHECK(CalledOnValidThread());
  DomainResolverCallbacks::iterator it = domain_resolver_callbacks_.find(id);
  if (it != domain_resolver_callbacks_.end() && !it->second.is_null())
    it->second.Run(success, ip_address_ipv4, ip_address_ipv6);
}

ServiceDiscoveryHostClientFactory::ServiceDiscoveryHostClientFactory()
    : instance_(NULL), references_(0) {
}

ServiceDiscoveryHostClientFactory::~ServiceDiscoveryHostClientFactory() {
}

// static
ServiceDiscoveryHostClient* ServiceDiscoveryHostClientFactory::GetClient() {
  return GetInstance()->GetClientInternal();
}

// static
void ServiceDiscoveryHostClientFactory::ReleaseClient() {
  GetInstance()->ReleaseClientInternal();
}

// static
ServiceDiscoveryHostClientFactory*
ServiceDiscoveryHostClientFactory::GetInstance() {
  return Singleton<ServiceDiscoveryHostClientFactory>::get();
}

ServiceDiscoveryHostClient*
ServiceDiscoveryHostClientFactory::GetClientInternal() {
  DCHECK(CalledOnValidThread());
  if (references_ == 0) {
    instance_ = new ServiceDiscoveryHostClient;
    instance_->Start();
  }

  references_++;
  return instance_.get();
}

void ServiceDiscoveryHostClientFactory::ReleaseClientInternal() {
  DCHECK(CalledOnValidThread());
  references_--;
  if (references_ == 0) {
    instance_->Shutdown();
    instance_ = NULL;
  }
}

}  // namespace local_discovery
