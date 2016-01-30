// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_client_mac.h"

#import <arpa/inet.h>
#import <Foundation/Foundation.h>
#import <net/if_dl.h>
#include <stddef.h>
#include <stdint.h>

#include "base/debug/dump_without_crashing.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "net/base/ip_address_number.h"
#include "net/base/ip_endpoint.h"

using local_discovery::ServiceWatcherImplMac;
using local_discovery::ServiceResolverImplMac;

@interface NetServiceBrowserDelegate
    : NSObject<NSNetServiceBrowserDelegate, NSNetServiceDelegate> {
 @private
  ServiceWatcherImplMac::NetServiceBrowserContainer* container_;  // weak.
  base::scoped_nsobject<NSMutableArray> services_;
}

- (id)initWithContainer:
        (ServiceWatcherImplMac::NetServiceBrowserContainer*)serviceWatcherImpl;

@end

@interface NetServiceDelegate : NSObject <NSNetServiceDelegate> {
  @private
   ServiceResolverImplMac::NetServiceContainer* container_;
}

- (id)initWithContainer:
        (ServiceResolverImplMac::NetServiceContainer*)serviceResolverImpl;

@end

namespace local_discovery {

namespace {

const char kServiceDiscoveryThreadName[] = "Service Discovery Thread";

const NSTimeInterval kResolveTimeout = 10.0;

// Extracts the instance name, name type and domain from a full service name or
// the service type and domain from a service type. Returns true if successful.
// TODO(justinlin): This current only handles service names with format
// <name>._<protocol2>._<protocol1>.<domain>. Service names with
// subtypes will not parse correctly:
// <name>._<type>._<sub>._<protocol2>._<protocol1>.<domain>.
bool ExtractServiceInfo(const std::string& service,
                        bool is_service_name,
                        std::string* instance,
                        std::string* type,
                        std::string* domain) {
  if (service.empty())
    return false;

  const size_t last_period = service.find_last_of('.');
  if (last_period == std::string::npos || service.length() <= last_period)
    return false;

  if (!is_service_name) {
    *instance = std::string();
    *type = service.substr(0, last_period) + ".";
  } else {
    // Find third last period that delimits type and instance name.
    size_t type_period = last_period;
    for (int i = 0; i < 2; ++i) {
      type_period = service.find_last_of('.', type_period - 1);
      if (type_period == std::string::npos)
        return false;
    }

    *instance = service.substr(0, type_period);
    *type = service.substr(type_period + 1, last_period - type_period);
  }
  *domain = service.substr(last_period + 1) + ".";

  return !domain->empty() &&
         !type->empty() &&
         (!is_service_name || !instance->empty());
}

void ParseTxtRecord(NSData* record, std::vector<std::string>* output) {
  if (record == nil || [record length] <= 1)
    return;

  VLOG(1) << "ParseTxtRecord: " << [record length];

  const char* record_bytes = reinterpret_cast<const char*>([record bytes]);
  const char* const record_end = record_bytes + [record length];
  // TODO(justinlin): More strict bounds checking.
  while (record_bytes < record_end) {
    uint8_t size = *record_bytes++;
    if (size <= 0)
      continue;

    if (record_bytes + size <= record_end) {
      VLOG(1) << "TxtRecord: "
              << std::string(record_bytes, static_cast<size_t>(size));
      output->push_back(
          [[[NSString alloc] initWithBytes:record_bytes
                             length:size
                             encoding:NSUTF8StringEncoding] UTF8String]);
    }
    record_bytes += size;
  }
}

}  // namespace

ServiceDiscoveryClientMac::ServiceDiscoveryClientMac() {}
ServiceDiscoveryClientMac::~ServiceDiscoveryClientMac() {}

scoped_ptr<ServiceWatcher> ServiceDiscoveryClientMac::CreateServiceWatcher(
    const std::string& service_type,
    const ServiceWatcher::UpdatedCallback& callback) {
  StartThreadIfNotStarted();
  VLOG(1) << "CreateServiceWatcher: " << service_type;
  return scoped_ptr<ServiceWatcher>(new ServiceWatcherImplMac(
      service_type, callback, service_discovery_thread_->task_runner()));
}

scoped_ptr<ServiceResolver> ServiceDiscoveryClientMac::CreateServiceResolver(
    const std::string& service_name,
    const ServiceResolver::ResolveCompleteCallback& callback) {
  StartThreadIfNotStarted();
  VLOG(1) << "CreateServiceResolver: " << service_name;
  return scoped_ptr<ServiceResolver>(new ServiceResolverImplMac(
      service_name, callback, service_discovery_thread_->task_runner()));
}

scoped_ptr<LocalDomainResolver>
ServiceDiscoveryClientMac::CreateLocalDomainResolver(
    const std::string& domain,
    net::AddressFamily address_family,
    const LocalDomainResolver::IPAddressCallback& callback) {
  NOTIMPLEMENTED();  // TODO(noamsml): Implement.
  VLOG(1) << "CreateLocalDomainResolver: " << domain;
  return scoped_ptr<LocalDomainResolver>();
}

void ServiceDiscoveryClientMac::StartThreadIfNotStarted() {
  if (!service_discovery_thread_) {
    service_discovery_thread_.reset(
        new base::Thread(kServiceDiscoveryThreadName));
    // Only TYPE_UI uses an NSRunLoop.
    base::Thread::Options options(base::MessageLoop::TYPE_UI, 0);
    service_discovery_thread_->StartWithOptions(options);
  }
}

ServiceWatcherImplMac::NetServiceBrowserContainer::NetServiceBrowserContainer(
    const std::string& service_type,
    const ServiceWatcher::UpdatedCallback& callback,
    scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner)
    : service_type_(service_type),
      callback_(callback),
      callback_runner_(base::ThreadTaskRunnerHandle::Get()),
      service_discovery_runner_(service_discovery_runner),
      weak_factory_(this) {
}

ServiceWatcherImplMac::NetServiceBrowserContainer::
    ~NetServiceBrowserContainer() {
  DCHECK(IsOnServiceDiscoveryThread());
}

void ServiceWatcherImplMac::NetServiceBrowserContainer::Start() {
  service_discovery_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NetServiceBrowserContainer::StartOnDiscoveryThread,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWatcherImplMac::NetServiceBrowserContainer::DiscoverNewServices() {
  service_discovery_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NetServiceBrowserContainer::DiscoverOnDiscoveryThread,
                 weak_factory_.GetWeakPtr()));
}

void
ServiceWatcherImplMac::NetServiceBrowserContainer::StartOnDiscoveryThread() {
  DCHECK(IsOnServiceDiscoveryThread());

  delegate_.reset([[NetServiceBrowserDelegate alloc] initWithContainer:this]);
  browser_.reset([[NSNetServiceBrowser alloc] init]);
  [browser_ setDelegate:delegate_];
}

void
ServiceWatcherImplMac::NetServiceBrowserContainer::DiscoverOnDiscoveryThread() {
  DCHECK(IsOnServiceDiscoveryThread());
  std::string instance;
  std::string type;
  std::string domain;

  if (!ExtractServiceInfo(service_type_, false, &instance, &type, &domain))
    return;

  DCHECK(instance.empty());
  DVLOG(1) << "Listening for service type '" << type
           << "' on domain '" << domain << "'";

  base::Time start_time = base::Time::Now();
  [browser_ searchForServicesOfType:[NSString stringWithUTF8String:type.c_str()]
            inDomain:[NSString stringWithUTF8String:domain.c_str()]];
  UMA_HISTOGRAM_TIMES("LocalDiscovery.MacBrowseCallTimes",
                      base::Time::Now() - start_time);
}

void ServiceWatcherImplMac::NetServiceBrowserContainer::OnServicesUpdate(
    ServiceWatcher::UpdateType update,
    const std::string& service) {
  callback_runner_->PostTask(FROM_HERE, base::Bind(callback_, update, service));
}

void ServiceWatcherImplMac::NetServiceBrowserContainer::DeleteSoon() {
  service_discovery_runner_->DeleteSoon(FROM_HERE, this);
}

ServiceWatcherImplMac::ServiceWatcherImplMac(
    const std::string& service_type,
    const ServiceWatcher::UpdatedCallback& callback,
    scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner)
    : service_type_(service_type),
      callback_(callback),
      started_(false),
      weak_factory_(this) {
  container_.reset(new NetServiceBrowserContainer(
      service_type,
      base::Bind(&ServiceWatcherImplMac::OnServicesUpdate,
                 weak_factory_.GetWeakPtr()),
      service_discovery_runner));
}

ServiceWatcherImplMac::~ServiceWatcherImplMac() {}

void ServiceWatcherImplMac::Start() {
  DCHECK(!started_);
  VLOG(1) << "ServiceWatcherImplMac::Start";
  container_->Start();
  started_ = true;
}

void ServiceWatcherImplMac::DiscoverNewServices(bool force_update) {
  DCHECK(started_);
  VLOG(1) << "ServiceWatcherImplMac::DiscoverNewServices";
  container_->DiscoverNewServices();
}

void ServiceWatcherImplMac::SetActivelyRefreshServices(
    bool actively_refresh_services) {
  DCHECK(started_);
  VLOG(1) << "ServiceWatcherImplMac::SetActivelyRefreshServices";
}

std::string ServiceWatcherImplMac::GetServiceType() const {
  return service_type_;
}

void ServiceWatcherImplMac::OnServicesUpdate(ServiceWatcher::UpdateType update,
                                             const std::string& service) {
  VLOG(1) << "ServiceWatcherImplMac::OnServicesUpdate: "
          << service + "." + service_type_;
  callback_.Run(update, service + "." + service_type_);
}

ServiceResolverImplMac::NetServiceContainer::NetServiceContainer(
    const std::string& service_name,
    const ServiceResolver::ResolveCompleteCallback& callback,
    scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner)
    : service_name_(service_name),
      callback_(callback),
      callback_runner_(base::ThreadTaskRunnerHandle::Get()),
      service_discovery_runner_(service_discovery_runner),
      weak_factory_(this) {
}

ServiceResolverImplMac::NetServiceContainer::~NetServiceContainer() {
  DCHECK(IsOnServiceDiscoveryThread());
}

void ServiceResolverImplMac::NetServiceContainer::StartResolving() {
  service_discovery_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NetServiceContainer::StartResolvingOnDiscoveryThread,
                 weak_factory_.GetWeakPtr()));
}

void ServiceResolverImplMac::NetServiceContainer::DeleteSoon() {
  service_discovery_runner_->DeleteSoon(FROM_HERE, this);
}

void
ServiceResolverImplMac::NetServiceContainer::StartResolvingOnDiscoveryThread() {
  DCHECK(IsOnServiceDiscoveryThread());
  std::string instance;
  std::string type;
  std::string domain;

  // The service object is set ahead of time by tests.
  if (service_)
    return;

  if (!ExtractServiceInfo(service_name_, true, &instance, &type, &domain))
    return OnResolveUpdate(ServiceResolver::STATUS_KNOWN_NONEXISTENT);

  VLOG(1) << "ServiceResolverImplMac::ServiceResolverImplMac::"
          << "StartResolvingOnDiscoveryThread: Success";
  delegate_.reset([[NetServiceDelegate alloc] initWithContainer:this]);
  service_.reset(
      [[NSNetService alloc]
          initWithDomain:[[NSString alloc] initWithUTF8String:domain.c_str()]
          type:[[NSString alloc] initWithUTF8String:type.c_str()]
          name:[[NSString alloc] initWithUTF8String:instance.c_str()]]);
  [service_ setDelegate:delegate_];

  [service_ resolveWithTimeout:kResolveTimeout];

  VLOG(1) << "ServiceResolverImplMac::NetServiceContainer::"
          << "StartResolvingOnDiscoveryThread: " << service_name_
          << ", instance: " << instance << ", type: " << type
          << ", domain: " << domain;
}

void ServiceResolverImplMac::NetServiceContainer::OnResolveUpdate(
    RequestStatus status) {
  if (status != STATUS_SUCCESS) {
    callback_runner_->PostTask(
        FROM_HERE, base::Bind(callback_, status, ServiceDescription()));
    return;
  }

  service_description_.service_name = service_name_;

  for (NSData* address in [service_ addresses]) {
    const void* bytes = [address bytes];
    int length = [address length];
    const sockaddr* socket = static_cast<const sockaddr*>(bytes);
    net::IPEndPoint end_point;
    if (end_point.FromSockAddr(socket, length)) {
      service_description_.address =
          net::HostPortPair::FromIPEndPoint(end_point);
      service_description_.ip_address = end_point.address().bytes();
      break;
    }
  }

  if (service_description_.address.host().empty()) {
    VLOG(1) << "Service IP is not resolved: " << service_name_;
    callback_runner_->PostTask(
        FROM_HERE,
        base::Bind(callback_, STATUS_KNOWN_NONEXISTENT, ServiceDescription()));
    return;
  }

  ParseTxtRecord([service_ TXTRecordData], &service_description_.metadata);

  // TODO(justinlin): Implement last_seen.
  service_description_.last_seen = base::Time::Now();
  callback_runner_->PostTask(
      FROM_HERE, base::Bind(callback_, status, service_description_));
}

void ServiceResolverImplMac::NetServiceContainer::SetServiceForTesting(
    base::scoped_nsobject<NSNetService> service) {
  service_ = service;
}

ServiceResolverImplMac::ServiceResolverImplMac(
    const std::string& service_name,
    const ServiceResolver::ResolveCompleteCallback& callback,
    scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner)
    : service_name_(service_name),
      callback_(callback),
      has_resolved_(false),
      weak_factory_(this) {
  container_.reset(new NetServiceContainer(
      service_name,
      base::Bind(&ServiceResolverImplMac::OnResolveComplete,
                 weak_factory_.GetWeakPtr()),
      service_discovery_runner));
}

ServiceResolverImplMac::~ServiceResolverImplMac() {}

void ServiceResolverImplMac::StartResolving() {
  container_->StartResolving();

  VLOG(1) << "Resolving service " << service_name_;
}

std::string ServiceResolverImplMac::GetName() const { return service_name_; }

void ServiceResolverImplMac::OnResolveComplete(
    RequestStatus status,
    const ServiceDescription& description) {
  VLOG(1) << "ServiceResolverImplMac::OnResolveComplete: " << service_name_
          << ", " << status;

  has_resolved_ = true;

  callback_.Run(status, description);
}

ServiceResolverImplMac::NetServiceContainer*
ServiceResolverImplMac::GetContainerForTesting() {
  return container_.get();
}

}  // namespace local_discovery

@implementation NetServiceBrowserDelegate

- (id)initWithContainer:
          (ServiceWatcherImplMac::NetServiceBrowserContainer*)container {
  if ((self = [super init])) {
    container_ = container;
    services_.reset([[NSMutableArray alloc] initWithCapacity:1]);
  }
  return self;
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)netServiceBrowser
        didFindService:(NSNetService *)netService
        moreComing:(BOOL)moreServicesComing {
  // Start monitoring this service for updates.
  [netService setDelegate:self];
  [netService startMonitoring];
  [services_ addObject:netService];

  container_->OnServicesUpdate(local_discovery::ServiceWatcher::UPDATE_ADDED,
                               [[netService name] UTF8String]);
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)netServiceBrowser
        didRemoveService:(NSNetService *)netService
        moreComing:(BOOL)moreServicesComing {
  NSUInteger index = [services_ indexOfObject:netService];
  if (index != NSNotFound) {
    container_->OnServicesUpdate(
        local_discovery::ServiceWatcher::UPDATE_REMOVED,
        [[netService name] UTF8String]);

    // Stop monitoring this service for updates.
    [[services_ objectAtIndex:index] stopMonitoring];
    [services_ removeObjectAtIndex:index];
  }
}

- (void)netService:(NSNetService *)sender
        didUpdateTXTRecordData:(NSData *)data {
  container_->OnServicesUpdate(local_discovery::ServiceWatcher::UPDATE_CHANGED,
                               [[sender name] UTF8String]);
}

@end

@implementation NetServiceDelegate

- (id)initWithContainer:
          (ServiceResolverImplMac::NetServiceContainer*)container {
  if ((self = [super init])) {
    container_ = container;
  }
  return self;
}

- (void)netServiceDidResolveAddress:(NSNetService *)sender {
  container_->OnResolveUpdate(local_discovery::ServiceResolver::STATUS_SUCCESS);
}

- (void)netService:(NSNetService *)sender
        didNotResolve:(NSDictionary *)errorDict {
  container_->OnResolveUpdate(
      local_discovery::ServiceResolver::STATUS_REQUEST_TIMEOUT);
}

@end
