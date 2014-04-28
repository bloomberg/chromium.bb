// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_client_utility.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/local_discovery/service_discovery_host_client.h"
#include "content/public/browser/browser_thread.h"

namespace local_discovery {

using content::BrowserThread;

namespace {
const int kMaxRestartAttempts = 10;
const int kRestartDelayOnNetworkChangeSeconds = 3;
const int kReportSuccessAfterSeconds = 10;
}

scoped_ptr<ServiceWatcher> ServiceDiscoveryClientUtility::CreateServiceWatcher(
    const std::string& service_type,
    const ServiceWatcher::UpdatedCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return host_client_->CreateServiceWatcher(service_type, callback);
}

scoped_ptr<ServiceResolver>
ServiceDiscoveryClientUtility::CreateServiceResolver(
    const std::string& service_name,
    const ServiceResolver::ResolveCompleteCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return host_client_->CreateServiceResolver(service_name, callback);
}

scoped_ptr<LocalDomainResolver>
ServiceDiscoveryClientUtility::CreateLocalDomainResolver(
    const std::string& domain,
    net::AddressFamily address_family,
    const LocalDomainResolver::IPAddressCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return host_client_->CreateLocalDomainResolver(domain, address_family,
                                                 callback);
}

ServiceDiscoveryClientUtility::ServiceDiscoveryClientUtility()
    : restart_attempts_(kMaxRestartAttempts),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  StartNewClient();
}

ServiceDiscoveryClientUtility::~ServiceDiscoveryClientUtility() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  host_client_->Shutdown();
}

void ServiceDiscoveryClientUtility::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Only network changes resets kMaxRestartAttempts.
  restart_attempts_ = kMaxRestartAttempts;
  ScheduleStartNewClient();
}

void ServiceDiscoveryClientUtility::ScheduleStartNewClient() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  host_client_->Shutdown();
  weak_ptr_factory_.InvalidateWeakPtrs();
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ServiceDiscoveryClientUtility::StartNewClient,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kRestartDelayOnNetworkChangeSeconds));
}

void ServiceDiscoveryClientUtility::StartNewClient() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_refptr<ServiceDiscoveryHostClient> old_client = host_client_;
  if ((restart_attempts_--) > 0) {
    host_client_ = new ServiceDiscoveryHostClient();
    host_client_->Start(
        base::Bind(&ServiceDiscoveryClientUtility::ScheduleStartNewClient,
                   weak_ptr_factory_.GetWeakPtr()));

    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ServiceDiscoveryClientUtility::ReportSuccess,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(kReportSuccessAfterSeconds));
  } else {
    restart_attempts_ = -1;
    ReportSuccess();
  }
  // Run when host_client_ is created. Callbacks created by InvalidateWatchers
  // may create new watchers.
  if (old_client)
    old_client->InvalidateWatchers();
}

void ServiceDiscoveryClientUtility::ReportSuccess() {
  UMA_HISTOGRAM_COUNTS_100("LocalDiscovery.ClientRestartAttempts",
                           kMaxRestartAttempts - restart_attempts_);
}

}  // namespace local_discovery
