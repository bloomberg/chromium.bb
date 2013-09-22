// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_client_mac.h"

#import <Foundation/Foundation.h>
#import <arpa/inet.h>
#import <net/if_dl.h>

#include "base/memory/singleton.h"

using local_discovery::ServiceWatcherImplMac;
using local_discovery::ServiceResolverImplMac;

@interface NetServiceBrowserDelegate
    : NSObject<NSNetServiceBrowserDelegate, NSNetServiceDelegate> {
 @private
  ServiceWatcherImplMac* serviceWatcherImpl_;  // weak.
  base::scoped_nsobject<NSMutableArray> services_;
}

- (id)initWithServiceWatcher:(ServiceWatcherImplMac*)serviceWatcherImpl;

@end

@interface NetServiceDelegate : NSObject <NSNetServiceDelegate> {
  @private
   ServiceResolverImplMac* serviceResolverImpl_;
}

-(id) initWithServiceResolver:(ServiceResolverImplMac*)serviceResolverImpl;

@end

namespace local_discovery {

namespace {

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

  const uint8* record_bytes = reinterpret_cast<const uint8*>([record bytes]);
  const uint8* const record_end = record_bytes + [record length];
  // TODO(justinlin): More strict bounds checking.
  while (record_bytes < record_end) {
    uint8 size = *record_bytes++;
    if (size <= 0)
      continue;

    if (record_bytes + size <= record_end) {
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
  return scoped_ptr<ServiceWatcher>(new ServiceWatcherImplMac(service_type,
                                                              callback));
}

scoped_ptr<ServiceResolver> ServiceDiscoveryClientMac::CreateServiceResolver(
    const std::string& service_name,
    const ServiceResolver::ResolveCompleteCallback& callback) {
  return scoped_ptr<ServiceResolver>(new ServiceResolverImplMac(service_name,
                                                                callback));
}

scoped_ptr<LocalDomainResolver>
ServiceDiscoveryClientMac::CreateLocalDomainResolver(
    const std::string& domain,
    net::AddressFamily address_family,
    const LocalDomainResolver::IPAddressCallback& callback) {
  NOTIMPLEMENTED();  // TODO(justinlin): Implement.
  return scoped_ptr<LocalDomainResolver>();
}

ServiceWatcherImplMac::ServiceWatcherImplMac(
    const std::string& service_type,
    const ServiceWatcher::UpdatedCallback& callback)
    : service_type_(service_type), callback_(callback), started_(false) {}

ServiceWatcherImplMac::~ServiceWatcherImplMac() {}

void ServiceWatcherImplMac::Start() {
  DCHECK(!started_);
  delegate_.reset([[NetServiceBrowserDelegate alloc]
                        initWithServiceWatcher:this]);
  browser_.reset([[NSNetServiceBrowser alloc] init]);
  [browser_ setDelegate:delegate_];
  started_ = true;
}

// TODO(justinlin): Implement flushing DNS cache to respect parameter.
void ServiceWatcherImplMac::DiscoverNewServices(bool force_update) {
  DCHECK(started_);

  std::string instance;
  std::string type;
  std::string domain;

  if (!ExtractServiceInfo(service_type_, false, &instance, &type, &domain))
    return;

  DCHECK(instance.empty());
  DVLOG(1) << "Listening for service type '" << type
           << "' on domain '" << domain << "'";

  [browser_ searchForServicesOfType:[NSString stringWithUTF8String:type.c_str()]
            inDomain:[NSString stringWithUTF8String:domain.c_str()]];
}

std::string ServiceWatcherImplMac::GetServiceType() const {
  return service_type_;
}

void ServiceWatcherImplMac::OnServicesUpdate(ServiceWatcher::UpdateType update,
                                             const std::string& service) {
  callback_.Run(update, service + "." + service_type_);
}

ServiceResolverImplMac::ServiceResolverImplMac(
    const std::string& service_name,
    const ServiceResolver::ResolveCompleteCallback& callback)
    : service_name_(service_name), callback_(callback), has_resolved_(false) {
  std::string instance;
  std::string type;
  std::string domain;

  if (ExtractServiceInfo(service_name, true, &instance, &type, &domain)) {
    delegate_.reset([[NetServiceDelegate alloc] initWithServiceResolver:this]);
    service_.reset(
        [[NSNetService alloc]
            initWithDomain:[[NSString alloc] initWithUTF8String:domain.c_str()]
            type:[[NSString alloc] initWithUTF8String:type.c_str()]
            name:[[NSString alloc] initWithUTF8String:instance.c_str()]]);
    [service_ setDelegate:delegate_];
  }
}

ServiceResolverImplMac::~ServiceResolverImplMac() {}

void ServiceResolverImplMac::StartResolving() {
  if (!service_.get())
    return;

  DVLOG(1) << "Resolving service " << service_name_;
  [service_ resolveWithTimeout:kResolveTimeout];
}

std::string ServiceResolverImplMac::GetName() const {
  return service_name_;
}

void ServiceResolverImplMac::OnResolveUpdate(RequestStatus status) {
  if (status == STATUS_SUCCESS) {
    service_description_.service_name = service_name_;

    for (NSData* address in [service_ addresses]) {
      const void* bytes = [address bytes];
      // TODO(justinlin): Handle IPv6 addresses?
      if (static_cast<const sockaddr*>(bytes)->sa_family == AF_INET) {
        const sockaddr_in* sock = static_cast<const sockaddr_in*>(bytes);
        char addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sock->sin_addr, addr, INET_ADDRSTRLEN);
        service_description_.address =
            net::HostPortPair(addr, ntohs(sock->sin_port));
        net::ParseIPLiteralToNumber(addr, &service_description_.ip_address);
        break;
      }
    }

    ParseTxtRecord([service_ TXTRecordData], &service_description_.metadata);

    // TODO(justinlin): Implement last_seen.
    service_description_.last_seen = base::Time::Now();
    callback_.Run(status, service_description_);
  } else {
    callback_.Run(status, ServiceDescription());
  }
  has_resolved_ = true;
}

void ServiceResolverImplMac::SetServiceForTesting(
    base::scoped_nsobject<NSNetService> service) {
  service_ = service;
}

}  // namespace local_discovery

@implementation NetServiceBrowserDelegate

- (id)initWithServiceWatcher:(ServiceWatcherImplMac*)serviceWatcherImpl {
  if ((self = [super init])) {
    serviceWatcherImpl_ = serviceWatcherImpl;
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

  serviceWatcherImpl_->OnServicesUpdate(
      local_discovery::ServiceWatcher::UPDATE_ADDED,
      [[netService name] UTF8String]);
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)netServiceBrowser
        didRemoveService:(NSNetService *)netService
        moreComing:(BOOL)moreServicesComing {
  serviceWatcherImpl_->OnServicesUpdate(
      local_discovery::ServiceWatcher::UPDATE_REMOVED,
      [[netService name] UTF8String]);

  NSUInteger index = [services_ indexOfObject:netService];
  if (index != NSNotFound) {
    // Stop monitoring this service for updates.
    [[services_ objectAtIndex:index] stopMonitoring];
    [services_ removeObjectAtIndex:index];
  }
}

- (void)netService:(NSNetService *)sender
        didUpdateTXTRecordData:(NSData *)data {
  serviceWatcherImpl_->OnServicesUpdate(
      local_discovery::ServiceWatcher::UPDATE_CHANGED,
      [[sender name] UTF8String]);
}

@end

@implementation NetServiceDelegate

-(id) initWithServiceResolver:(ServiceResolverImplMac*)serviceResolverImpl {
  if ((self = [super init])) {
    serviceResolverImpl_ = serviceResolverImpl;
  }
  return self;
}

- (void)netServiceDidResolveAddress:(NSNetService *)sender {
  serviceResolverImpl_->OnResolveUpdate(
      local_discovery::ServiceResolver::STATUS_SUCCESS);
}

- (void)netService:(NSNetService *)sender
        didNotResolve:(NSDictionary *)errorDict {
  serviceResolverImpl_->OnResolveUpdate(
      local_discovery::ServiceResolver::STATUS_REQUEST_TIMEOUT);
}

@end
