// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "chrome/browser/local_discovery/service_discovery_client_impl.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/mdns_client_impl.h"
#include "net/dns/mock_mdns_socket_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::Return;
using ::testing::Exactly;

namespace local_discovery {

namespace {

const char kSamplePacketPTR[] = {
  // Header
  '\x00', '\x00',               // ID is zeroed out
  '\x81', '\x80',               // Standard query response, RA, no error
  '\x00', '\x00',               // No questions (for simplicity)
  '\x00', '\x01',               // 1 RR (answers)
  '\x00', '\x00',               // 0 authority RRs
  '\x00', '\x00',               // 0 additional RRs

  '\x07', '_', 'p', 'r', 'i', 'v', 'e', 't',
  '\x04', '_', 't', 'c', 'p',
  '\x05', 'l', 'o', 'c', 'a', 'l',
  '\x00',
  '\x00', '\x0c',        // TYPE is PTR.
  '\x00', '\x01',        // CLASS is IN.
  '\x00', '\x00',        // TTL (4 bytes) is 1 second.
  '\x00', '\x01',
  '\x00', '\x08',        // RDLENGTH is 8 bytes.
  '\x05', 'h', 'e', 'l', 'l', 'o',
  '\xc0', '\x0c'
};

const char kSamplePacketSRV[] = {
  // Header
  '\x00', '\x00',               // ID is zeroed out
  '\x81', '\x80',               // Standard query response, RA, no error
  '\x00', '\x00',               // No questions (for simplicity)
  '\x00', '\x01',               // 1 RR (answers)
  '\x00', '\x00',               // 0 authority RRs
  '\x00', '\x00',               // 0 additional RRs

  '\x05', 'h', 'e', 'l', 'l', 'o',
  '\x07', '_', 'p', 'r', 'i', 'v', 'e', 't',
  '\x04', '_', 't', 'c', 'p',
  '\x05', 'l', 'o', 'c', 'a', 'l',
  '\x00',
  '\x00', '\x21',        // TYPE is SRV.
  '\x00', '\x01',        // CLASS is IN.
  '\x00', '\x00',        // TTL (4 bytes) is 1 second.
  '\x00', '\x01',
  '\x00', '\x15',        // RDLENGTH is 21 bytes.
  '\x00', '\x00',
  '\x00', '\x00',
  '\x22', '\xb8',  // port 8888
  '\x07', 'm', 'y', 'h', 'e', 'l', 'l', 'o',
  '\x05', 'l', 'o', 'c', 'a', 'l',
  '\x00',
};

const char kSamplePacketTXT[] = {
  // Header
  '\x00', '\x00',               // ID is zeroed out
  '\x81', '\x80',               // Standard query response, RA, no error
  '\x00', '\x00',               // No questions (for simplicity)
  '\x00', '\x01',               // 1 RR (answers)
  '\x00', '\x00',               // 0 authority RRs
  '\x00', '\x00',               // 0 additional RRs

  '\x05', 'h', 'e', 'l', 'l', 'o',
  '\x07', '_', 'p', 'r', 'i', 'v', 'e', 't',
  '\x04', '_', 't', 'c', 'p',
  '\x05', 'l', 'o', 'c', 'a', 'l',
  '\x00',
  '\x00', '\x10',        // TYPE is PTR.
  '\x00', '\x01',        // CLASS is IN.
  '\x00', '\x00',        // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
  '\x00', '\x01',
  '\x00', '\x06',        // RDLENGTH is 21 bytes.
  '\x05', 'h', 'e', 'l', 'l', 'o'
};

const char kSamplePacketSRVA[] = {
  // Header
  '\x00', '\x00',               // ID is zeroed out
  '\x81', '\x80',               // Standard query response, RA, no error
  '\x00', '\x00',               // No questions (for simplicity)
  '\x00', '\x02',               // 2 RR (answers)
  '\x00', '\x00',               // 0 authority RRs
  '\x00', '\x00',               // 0 additional RRs

  '\x05', 'h', 'e', 'l', 'l', 'o',
  '\x07', '_', 'p', 'r', 'i', 'v', 'e', 't',
  '\x04', '_', 't', 'c', 'p',
  '\x05', 'l', 'o', 'c', 'a', 'l',
  '\x00',
  '\x00', '\x21',        // TYPE is SRV.
  '\x00', '\x01',        // CLASS is IN.
  '\x00', '\x00',        // TTL (4 bytes) is 16 seconds.
  '\x00', '\x10',
  '\x00', '\x15',        // RDLENGTH is 21 bytes.
  '\x00', '\x00',
  '\x00', '\x00',
  '\x22', '\xb8',  // port 8888
  '\x07', 'm', 'y', 'h', 'e', 'l', 'l', 'o',
  '\x05', 'l', 'o', 'c', 'a', 'l',
  '\x00',

  '\x07', 'm', 'y', 'h', 'e', 'l', 'l', 'o',
  '\x05', 'l', 'o', 'c', 'a', 'l',
  '\x00',
  '\x00', '\x01',        // TYPE is A.
  '\x00', '\x01',        // CLASS is IN.
  '\x00', '\x00',        // TTL (4 bytes) is 16 seconds.
  '\x00', '\x10',
  '\x00', '\x04',        // RDLENGTH is 4 bytes.
  '\x01', '\x02',
  '\x03', '\x04',
};

class MockServiceWatcherDelegate : public ServiceWatcher::Delegate {
 public:
  MockServiceWatcherDelegate() {}
  virtual ~MockServiceWatcherDelegate() {}

  MOCK_METHOD2(OnServiceUpdated, void(ServiceWatcher::UpdateType,
                                      const std::string&));
};

class ServiceDiscoveryTest : public ::testing::Test {
 public:
  ServiceDiscoveryTest()
      : socket_factory_(new net::MockMDnsSocketFactory),
        mdns_client_(
            scoped_ptr<net::MDnsConnection::SocketFactory>(
                socket_factory_)) {
    net::MDnsClient::SetInstance(&mdns_client_);
    mdns_client_.StartListening();
  }

  virtual ~ServiceDiscoveryTest() {
    net::MDnsClient::SetInstance(NULL);
  }

 protected:
  void RunFor(base::TimeDelta time_period) {
    base::CancelableCallback<void()> callback(base::Bind(
        &ServiceDiscoveryTest::Stop, base::Unretained(this)));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, callback.callback(), time_period);

    base::MessageLoop::current()->Run();
    callback.Cancel();
  }

  void Stop() {
    base::MessageLoop::current()->Quit();
  }

  net::MockMDnsSocketFactory* socket_factory_;
  net::MDnsClientImpl mdns_client_;
  ServiceDiscoveryClientImpl service_discovery_client_;
  base::MessageLoop loop_;
};

TEST_F(ServiceDiscoveryTest, AddRemoveService) {
  scoped_ptr<ServiceWatcher> watcher;
  StrictMock<MockServiceWatcherDelegate> delegate;

  watcher = service_discovery_client_.CreateServiceWatcher(
      "_privet._tcp.local", &delegate);

  watcher->Start();

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_ADDED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  socket_factory_->SimulateReceive(
      kSamplePacketPTR, sizeof(kSamplePacketPTR));

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_REMOVED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  RunFor(base::TimeDelta::FromSeconds(2));
};

TEST_F(ServiceDiscoveryTest, DiscoverNewServices) {
  scoped_ptr<ServiceWatcher> watcher;
  StrictMock<MockServiceWatcherDelegate> delegate;

  watcher = service_discovery_client_.CreateServiceWatcher(
      "_privet._tcp.local", &delegate);

  watcher->Start();

  EXPECT_CALL(*socket_factory_, OnSendTo(_))
      .Times(2);

  watcher->DiscoverNewServices(false);
};

TEST_F(ServiceDiscoveryTest, GetAvailableServices) {
  NiceMock<MockServiceWatcherDelegate> delegate;

  std::vector<std::string> data_expected;
  std::vector<std::string> data;

  data_expected.push_back("hello._privet._tcp.local");

  scoped_ptr<ServiceWatcher> watcher =
      service_discovery_client_.CreateServiceWatcher(
          "_privet._tcp.local", &delegate);

  watcher->Start();

  socket_factory_->SimulateReceive(
      kSamplePacketPTR, sizeof(kSamplePacketPTR));

  watcher->GetAvailableServices(&data);

  EXPECT_EQ(data, data_expected);
};

TEST_F(ServiceDiscoveryTest, ReadCachedServices) {
  NiceMock<MockServiceWatcherDelegate> delegate_irrelevant;
  scoped_ptr<ServiceWatcher> watcher_irrelevant =
      service_discovery_client_.CreateServiceWatcher(
          "_privet._tcp.local", &delegate_irrelevant);

  watcher_irrelevant->Start();

  socket_factory_->SimulateReceive(
      kSamplePacketPTR, sizeof(kSamplePacketPTR));

  StrictMock<MockServiceWatcherDelegate> delegate;
  scoped_ptr<ServiceWatcher> watcher =
      service_discovery_client_.CreateServiceWatcher(
          "_privet._tcp.local", &delegate);

  watcher->Start();

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_ADDED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  watcher->ReadCachedServices();

  base::MessageLoop::current()->RunUntilIdle();
};

TEST_F(ServiceDiscoveryTest, OnServiceChanged) {
  StrictMock<MockServiceWatcherDelegate> delegate;
  scoped_ptr<ServiceWatcher> watcher =
      service_discovery_client_.CreateServiceWatcher(
          "_privet._tcp.local", &delegate);

  watcher->Start();

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_ADDED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  socket_factory_->SimulateReceive(
      kSamplePacketPTR, sizeof(kSamplePacketPTR));

  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_CHANGED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  socket_factory_->SimulateReceive(
      kSamplePacketSRV, sizeof(kSamplePacketSRV));

  socket_factory_->SimulateReceive(
      kSamplePacketTXT, sizeof(kSamplePacketTXT));

  base::MessageLoop::current()->RunUntilIdle();
};

TEST_F(ServiceDiscoveryTest, SinglePacket) {
  StrictMock<MockServiceWatcherDelegate> delegate;
  scoped_ptr<ServiceWatcher> watcher =
      service_discovery_client_.CreateServiceWatcher(
          "_privet._tcp.local", &delegate);

  watcher->Start();

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_ADDED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  socket_factory_->SimulateReceive(
      kSamplePacketPTR, sizeof(kSamplePacketPTR));

  // Reset the "already updated" flag.
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_CHANGED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(1));

  socket_factory_->SimulateReceive(
      kSamplePacketSRV, sizeof(kSamplePacketSRV));

  socket_factory_->SimulateReceive(
      kSamplePacketTXT, sizeof(kSamplePacketTXT));

  base::MessageLoop::current()->RunUntilIdle();
};

class ServiceResolverTest : public ServiceDiscoveryTest {
 public:
  ServiceResolverTest() {
    metadata_expected_.push_back("hello");
    address_expected_ = net::HostPortPair("myhello.local", 8888);
    ip_address_expected_.push_back(1);
    ip_address_expected_.push_back(2);
    ip_address_expected_.push_back(3);
    ip_address_expected_.push_back(4);
  }

  ~ServiceResolverTest() {
  }

  void SetUp()  {
    resolver_ = service_discovery_client_.CreateServiceResolver(
        "hello._privet._tcp.local",
        base::Bind(&ServiceResolverTest::OnFinishedResolving,
                   base::Unretained(this)));
  }

  void OnFinishedResolving(ServiceResolver::RequestStatus request_status,
                           const ServiceDescription& service_description) {
    OnFinishedResolvingInternal(request_status);
  }

  MOCK_METHOD1(OnFinishedResolvingInternal, void(
      ServiceResolver::RequestStatus));

 protected:
  scoped_ptr<ServiceResolver> resolver_;
  net::IPAddressNumber ip_address_;
  net::HostPortPair address_expected_;
  std::vector<std::string> metadata_expected_;
  net::IPAddressNumber ip_address_expected_;
};

TEST_F(ServiceResolverTest, TxtAndSrvButNoA) {
  EXPECT_CALL(*socket_factory_, OnSendTo(_))
      .Times(4);

  EXPECT_FALSE(resolver_->IsResolving());
  EXPECT_FALSE(resolver_->HasResolved());
  EXPECT_TRUE(resolver_->StartResolving());
  EXPECT_TRUE(resolver_->IsResolving());
  EXPECT_FALSE(resolver_->HasResolved());

  socket_factory_->SimulateReceive(
      kSamplePacketSRV, sizeof(kSamplePacketSRV));

  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_CALL(*this, OnFinishedResolvingInternal(
      ServiceResolver::STATUS_SUCCESS));

  socket_factory_->SimulateReceive(
      kSamplePacketTXT, sizeof(kSamplePacketTXT));

  EXPECT_EQ(address_expected_.ToString(),
            resolver_->GetServiceDescription().address.ToString());
  EXPECT_EQ(metadata_expected_, resolver_->GetServiceDescription().metadata);
  EXPECT_EQ(net::IPAddressNumber(),
            resolver_->GetServiceDescription().ip_address);
};

TEST_F(ServiceResolverTest, TxtSrvAndA) {
  EXPECT_CALL(*socket_factory_, OnSendTo(_))
      .Times(4);

  EXPECT_FALSE(resolver_->IsResolving());
  EXPECT_FALSE(resolver_->HasResolved());
  EXPECT_TRUE(resolver_->StartResolving());
  EXPECT_TRUE(resolver_->IsResolving());
  EXPECT_FALSE(resolver_->HasResolved());

  EXPECT_CALL(*this, OnFinishedResolvingInternal(
      ServiceResolver::STATUS_SUCCESS));

  socket_factory_->SimulateReceive(
      kSamplePacketTXT, sizeof(kSamplePacketTXT));

  socket_factory_->SimulateReceive(
      kSamplePacketSRVA, sizeof(kSamplePacketSRVA));

  EXPECT_EQ(address_expected_.ToString(),
            resolver_->GetServiceDescription().address.ToString());
  EXPECT_EQ(metadata_expected_, resolver_->GetServiceDescription().metadata);
  EXPECT_EQ(ip_address_expected_,
            resolver_->GetServiceDescription().ip_address);
};

TEST_F(ServiceResolverTest, JustSrv) {
  EXPECT_CALL(*socket_factory_, OnSendTo(_))
      .Times(4);

  EXPECT_FALSE(resolver_->IsResolving());
  EXPECT_FALSE(resolver_->HasResolved());
  EXPECT_TRUE(resolver_->StartResolving());
  EXPECT_TRUE(resolver_->IsResolving());
  EXPECT_FALSE(resolver_->HasResolved());

  EXPECT_CALL(*this, OnFinishedResolvingInternal(
      ServiceResolver::STATUS_SUCCESS));

  socket_factory_->SimulateReceive(
      kSamplePacketSRVA, sizeof(kSamplePacketSRVA));

  // TODO(noamsml): When NSEC record support is added, change this to use an
  // NSEC record.
  RunFor(base::TimeDelta::FromSeconds(4));

  EXPECT_EQ(address_expected_.ToString(),
            resolver_->GetServiceDescription().address.ToString());
  EXPECT_EQ(std::vector<std::string>() ,
            resolver_->GetServiceDescription().metadata);
  EXPECT_EQ(ip_address_expected_,
            resolver_->GetServiceDescription().ip_address);
};

TEST_F(ServiceResolverTest, WithNothing) {
  EXPECT_CALL(*socket_factory_, OnSendTo(_))
      .Times(4);

  EXPECT_FALSE(resolver_->IsResolving());
  EXPECT_FALSE(resolver_->HasResolved());
  EXPECT_TRUE(resolver_->StartResolving());
  EXPECT_TRUE(resolver_->IsResolving());
  EXPECT_FALSE(resolver_->HasResolved());

  EXPECT_CALL(*this, OnFinishedResolvingInternal(
      ServiceResolver::STATUS_REQUEST_TIMEOUT));

  // TODO(noamsml): When NSEC record support is added, change this to use an
  // NSEC record.
  RunFor(base::TimeDelta::FromSeconds(4));
};

}  // namespace

}  // namespace local_discovery
