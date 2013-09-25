// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_client_mdns.h"

#include "chrome/browser/local_discovery/service_discovery_host_client.h"
#include "content/public/browser/browser_thread.h"

namespace local_discovery {

using content::BrowserThread;

namespace {
const int kMaxRestartAttempts = 10;
const int kRestartDelayOnNetworkChangeSeconds = 3;
}

scoped_ptr<ServiceWatcher> ServiceDiscoveryClientMdns::CreateServiceWatcher(
    const std::string& service_type,
    const ServiceWatcher::UpdatedCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return host_client_->CreateServiceWatcher(service_type, callback);
}

scoped_ptr<ServiceResolver> ServiceDiscoveryClientMdns::CreateServiceResolver(
    const std::string& service_name,
    const ServiceResolver::ResolveCompleteCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return host_client_->CreateServiceResolver(service_name, callback);
}

scoped_ptr<LocalDomainResolver>
ServiceDiscoveryClientMdns::CreateLocalDomainResolver(
    const std::string& domain,
    net::AddressFamily address_family,
    const LocalDomainResolver::IPAddressCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return host_client_->CreateLocalDomainResolver(domain, address_family,
                                                 callback);
}

ServiceDiscoveryClientMdns::ServiceDiscoveryClientMdns()
    : restart_attempts_(kMaxRestartAttempts),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  StartNewClient();
}

ServiceDiscoveryClientMdns::~ServiceDiscoveryClientMdns() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  host_client_->Shutdown();
}

void ServiceDiscoveryClientMdns::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Only network changes resets kMaxRestartAttempts.
  restart_attempts_ = kMaxRestartAttempts;
  ScheduleStartNewClient();
}

void ServiceDiscoveryClientMdns::ScheduleStartNewClient() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  host_client_->Shutdown();
  weak_ptr_factory_.InvalidateWeakPtrs();
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ServiceDiscoveryClientMdns::StartNewClient,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kRestartDelayOnNetworkChangeSeconds));
}

void ServiceDiscoveryClientMdns::StartNewClient() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_refptr<ServiceDiscoveryHostClient> old_client = host_client_;
  if ((restart_attempts_--) > 0) {
    host_client_ = new ServiceDiscoveryHostClient();
    host_client_->Start(
        base::Bind(&ServiceDiscoveryClientMdns::ScheduleStartNewClient,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    host_client_ = NULL;
  }
  // Run when host_client_ is created. Callbacks created by InvalidateWatchers
  // may create new watchers.
  if (old_client)
    old_client->InvalidateWatchers();
}

}  // namespace local_discovery
