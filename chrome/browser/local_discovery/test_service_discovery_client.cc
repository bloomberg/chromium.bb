// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/test_service_discovery_client.h"

#include "chrome/utility/local_discovery/service_discovery_client_impl.h"
#include "net/dns/mdns_client_impl.h"

namespace local_discovery {

TestServiceDiscoveryClient::TestServiceDiscoveryClient() {
}

TestServiceDiscoveryClient::~TestServiceDiscoveryClient() {
}

void TestServiceDiscoveryClient::Start() {
  mock_socket_factory_ = new net::MockMDnsSocketFactory();
  mdns_client_.reset(new net::MDnsClientImpl(
      scoped_ptr<net::MDnsConnection::SocketFactory>(mock_socket_factory_)));
  service_discovery_client_impl_.reset(new ServiceDiscoveryClientImpl(
      mdns_client_.get()));
  mdns_client_->StartListening();

  EXPECT_CALL(*mock_socket_factory_, OnSendTo(testing::_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Invoke(this,
                                      &TestServiceDiscoveryClient::OnSendTo));
}

scoped_ptr<ServiceWatcher> TestServiceDiscoveryClient::CreateServiceWatcher(
      const std::string& service_type,
      const ServiceWatcher::UpdatedCallback& callback) {
  return service_discovery_client_impl_->CreateServiceWatcher(service_type,
                                                              callback);
}

scoped_ptr<ServiceResolver> TestServiceDiscoveryClient::CreateServiceResolver(
      const std::string& service_name,
      const ServiceResolver::ResolveCompleteCallback& callback) {
  return service_discovery_client_impl_->CreateServiceResolver(service_name,
                                                              callback);
}

scoped_ptr<LocalDomainResolver>
TestServiceDiscoveryClient::CreateLocalDomainResolver(
      const std::string& domain,
      net::AddressFamily address_family,
      const LocalDomainResolver::IPAddressCallback& callback) {
  return service_discovery_client_impl_->CreateLocalDomainResolver(
      domain, address_family, callback);
}

void TestServiceDiscoveryClient::SimulateReceive(const uint8* packet,
                                                 int size) {
  mock_socket_factory_->SimulateReceive(packet, size);
}

}  // namespace local_discovery
