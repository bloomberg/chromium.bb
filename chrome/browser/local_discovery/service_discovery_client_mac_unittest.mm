// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "chrome/common/local_discovery/service_discovery_client.h"
#include "chrome/browser/local_discovery/service_discovery_client_mac.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest_mac.h"

@interface TestNSNetService : NSNetService {
 @private
  NSData* data_;
}
- (id) initWithData:(NSData *)data;
@end

@implementation TestNSNetService

-(id) initWithData:(NSData *)data {
  if ((self = [super init])) {
    data_ = data;
  }
  return self;
}

- (NSArray *)addresses {
  return [NSMutableArray array];
}

- (NSData *)TXTRecordData {
  return data_;
}

@end

namespace local_discovery {

class ServiceDiscoveryClientMacTest : public CocoaTest {
 public:
  ServiceDiscoveryClientMacTest() : num_updates_(0), num_resolves_(0) {
    client_mac_ = new ServiceDiscoveryClientMac();
    client_ = client_mac_.get();
  }

  void OnServiceUpdated(
      ServiceWatcher::UpdateType update,
      const std::string& service_name) {
    last_update_ = update;
    last_service_name_ = service_name;
    num_updates_++;
  }


  void OnResolveComplete(
      ServiceResolver::RequestStatus status,
      const ServiceDescription& service_description) {
    last_status_ = status;
    last_service_description_ = service_description;
    num_resolves_++;
  }

 protected:
  scoped_refptr<ServiceDiscoveryClientMac> client_mac_;
  ServiceDiscoveryClient* client_;  // weak

  base::MessageLoop message_loop_;
  ServiceWatcher::UpdateType last_update_;
  std::string last_service_name_;
  int num_updates_;
  ServiceResolver::RequestStatus last_status_;
  ServiceDescription last_service_description_;
  int num_resolves_;
};

TEST_F(ServiceDiscoveryClientMacTest, ServiceWatcher) {
  const std::string test_service_type = "_testing._tcp.local";
  const std::string test_service_name = "Test.123";

  scoped_ptr<ServiceWatcher> watcher = client_->CreateServiceWatcher(
      test_service_type,
      base::Bind(&ServiceDiscoveryClientMacTest::OnServiceUpdated,
                 base::Unretained(this)));
  watcher->Start();

  // Weak pointer to implementation class.
  ServiceWatcherImplMac* watcher_impl =
      static_cast<ServiceWatcherImplMac*>(watcher.get());
  // Simulate service update events.
  watcher_impl->OnServicesUpdate(
      ServiceWatcher::UPDATE_ADDED, test_service_name);
  watcher_impl->OnServicesUpdate(
      ServiceWatcher::UPDATE_CHANGED, test_service_name);
  watcher_impl->OnServicesUpdate(
      ServiceWatcher::UPDATE_REMOVED, test_service_name);
  EXPECT_EQ(last_service_name_, test_service_name + "." + test_service_type);
  EXPECT_EQ(num_updates_, 3);
}

TEST_F(ServiceDiscoveryClientMacTest, ServiceResolver) {
  const std::string test_service_name = "Test.123._testing._tcp.local";
  scoped_ptr<ServiceResolver> resolver = client_->CreateServiceResolver(
      test_service_name,
      base::Bind(&ServiceDiscoveryClientMacTest::OnResolveComplete,
                 base::Unretained(this)));

  const uint8 record_bytes[] = { 2, 'a', 'b', 3, 'd', '=', 'e' };
  base::scoped_nsobject<NSNetService> test_service(
      [[TestNSNetService alloc] initWithData:
          [[NSData alloc] initWithBytes:record_bytes
                          length:arraysize(record_bytes)]]);

  ServiceResolverImplMac* resolver_impl =
      static_cast<ServiceResolverImplMac*>(resolver.get());
  resolver_impl->GetContainerForTesting()->SetServiceForTesting(test_service);
  resolver->StartResolving();

  resolver_impl->GetContainerForTesting()->OnResolveUpdate(
      ServiceResolver::STATUS_SUCCESS);

  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(1, num_resolves_);
  EXPECT_EQ(2u, last_service_description_.metadata.size());
}

}  // namespace local_discovery
