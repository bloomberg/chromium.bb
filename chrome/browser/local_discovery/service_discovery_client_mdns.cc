// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_client_mdns.h"

#include "chrome/browser/local_discovery/service_discovery_host_client.h"
#include "content/public/browser/browser_thread.h"

namespace local_discovery {

using content::BrowserThread;

namespace {
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

ServiceDiscoveryClientMdns::ServiceDiscoveryClientMdns() {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  host_client_ = new ServiceDiscoveryHostClient();
  host_client_->Start();
}

ServiceDiscoveryClientMdns::~ServiceDiscoveryClientMdns() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  host_client_->Shutdown();
}

void ServiceDiscoveryClientMdns::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  host_client_->Shutdown();
  network_change_callback_.Reset(
      base::Bind(&ServiceDiscoveryClientMdns::StartNewClient,
                 base::Unretained(this)));  // Unretained to avoid ref cycle.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      network_change_callback_.callback(),
      base::TimeDelta::FromSeconds(kRestartDelayOnNetworkChangeSeconds));
}

void ServiceDiscoveryClientMdns::StartNewClient() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_refptr<ServiceDiscoveryHostClient> old_client = host_client_;
  host_client_ = new ServiceDiscoveryHostClient();
  host_client_->Start();
  // Run when host_client_ is created. Callbacks created by InvalidateWatchers
  // may create new watchers.
  old_client->InvalidateWatchers();
}

}  // namespace local_discovery
