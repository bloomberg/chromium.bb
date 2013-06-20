// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "chrome/browser/local_discovery/service_discovery_client_impl.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/mdns_client_impl.h"
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
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x01,               // 1 RR (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x0c,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 1 second.
  0x00, 0x01,
  0x00, 0x08,        // RDLENGTH is 8 bytes.
  0x05, 'h', 'e', 'l', 'l', 'o',
  0xc0, 0x0c
};

const char kSamplePacketSRV[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x01,               // 1 RR (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  0x05, 'h', 'e', 'l', 'l', 'o',
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x21,        // TYPE is SRV.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 1 second.
  0x00, 0x01,
  0x00, 0x15,        // RDLENGTH is 21 bytes.
  0x00, 0x00,
  0x00, 0x00,
  0x22, 0xb8,  // port 8888
  0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
};

const char kSamplePacketTXT[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x01,               // 1 RR (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  0x05, 'h', 'e', 'l', 'l', 'o',
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x10,        // TYPE is PTR.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
  0x00, 0x01,
  0x00, 0x06,        // RDLENGTH is 21 bytes.
  0x05, 'h', 'e', 'l', 'l', 'o'
};

const char kSamplePacketSRVA[] = {
  // Header
  0x00, 0x00,               // ID is zeroed out
  0x81, 0x80,               // Standard query response, RA, no error
  0x00, 0x00,               // No questions (for simplicity)
  0x00, 0x02,               // 2 RR (answers)
  0x00, 0x00,               // 0 authority RRs
  0x00, 0x00,               // 0 additional RRs

  0x05, 'h', 'e', 'l', 'l', 'o',
  0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',
  0x04, '_', 't', 'c', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x21,        // TYPE is SRV.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 16 seconds.
  0x00, 0x10,
  0x00, 0x15,        // RDLENGTH is 21 bytes.
  0x00, 0x00,
  0x00, 0x00,
  0x22, 0xb8,  // port 8888
  0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,

  0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o',
  0x05, 'l', 'o', 'c', 'a', 'l',
  0x00,
  0x00, 0x01,        // TYPE is A.
  0x00, 0x01,        // CLASS is IN.
  0x00, 0x00,        // TTL (4 bytes) is 16 seconds.
  0x00, 0x10,
  0x00, 0x04,        // RDLENGTH is 4 bytes.
  0x01, 0x02,
  0x03, 0x04,
};


class MockDatagramSocket : public net::DatagramServerSocket {
 public:
  int Listen(const net::IPEndPoint& address) {
    return ListenInternal(address.ToString());
  }

  MOCK_METHOD1(ListenInternal, int(const std::string& address));

  MOCK_METHOD4(RecvFrom, int(net::IOBuffer* buffer, int size,
                             net::IPEndPoint* address,
                             const net::CompletionCallback& callback));

  int SendTo(net::IOBuffer* buf, int buf_len, const net::IPEndPoint& address,
             const net::CompletionCallback& callback) {
    return SendToInternal(std::string(buf->data(), buf_len), address.ToString(),
                          callback);
  }

  MOCK_METHOD3(SendToInternal, int(const std::string& packet,
                                   const std::string address,
                                   const net::CompletionCallback& callback));

  MOCK_METHOD1(SetReceiveBufferSize, bool(int32 size));
  MOCK_METHOD1(SetSendBufferSize, bool(int32 size));

  MOCK_METHOD0(Close, void());

  MOCK_CONST_METHOD1(GetPeerAddress, int(net::IPEndPoint* address));
  MOCK_CONST_METHOD1(GetLocalAddress, int(net::IPEndPoint* address));
  MOCK_CONST_METHOD0(NetLog, const net::BoundNetLog&());

  MOCK_METHOD0(AllowAddressReuse, void());
  MOCK_METHOD0(AllowBroadcast, void());

  int JoinGroup(const net::IPAddressNumber& group_address) const {
    return JoinGroupInternal(net::IPAddressToString(group_address));
  }

  MOCK_CONST_METHOD1(JoinGroupInternal, int(const std::string& group));

  int LeaveGroup(const net::IPAddressNumber& group_address) const {
    return LeaveGroupInternal(net::IPAddressToString(group_address));
  }

  MOCK_CONST_METHOD1(LeaveGroupInternal, int(const std::string& group));

  MOCK_METHOD1(SetMulticastTimeToLive, int(int ttl));

  MOCK_METHOD1(SetMulticastLoopbackMode, int(bool loopback));
};

class MockDatagramSocketFactory
    : public net::MDnsConnection::SocketFactory {
 public:
  MockDatagramSocketFactory() {
  }

  virtual ~MockDatagramSocketFactory() {
  }

  virtual scoped_ptr<net::DatagramServerSocket> CreateSocket() OVERRIDE {
    scoped_ptr<MockDatagramSocket> new_socket(
        new NiceMock<MockDatagramSocket>);

    ON_CALL(*new_socket, SendToInternal(_, _, _))
        .WillByDefault(Invoke(
            this,
            &MockDatagramSocketFactory::SendToInternal));

    ON_CALL(*new_socket, RecvFrom(_, _, _, _))
        .WillByDefault(Invoke(
            this,
            &MockDatagramSocketFactory::RecvFromInternal));

    return new_socket.PassAs<net::DatagramServerSocket>();
  }

  int SendToInternal(const std::string& packet, const std::string& address,
                     const net::CompletionCallback& callback) {
    OnSendTo(packet);
    return packet.size();
  }

  // The latest recieve callback is always saved, since the net::MDnsConnection
  // does not care which socket a packet is received on.
  int RecvFromInternal(net::IOBuffer* buffer, int size,
                       net::IPEndPoint* address,
                       const net::CompletionCallback& callback) {
    recv_buffer_ = buffer;
    recv_buffer_size_ = size;
    recv_callback_ = callback;
    return net::ERR_IO_PENDING;
  }

  void SimulateReceive(const char* packet, int size) {
    DCHECK(recv_buffer_size_ >= size);
    DCHECK(recv_buffer_.get());
    DCHECK(!recv_callback_.is_null());

    memcpy(recv_buffer_->data(), packet, size);
    net::CompletionCallback recv_callback = recv_callback_;
    recv_callback_.Reset();
    recv_callback.Run(size);
  }

  MOCK_METHOD1(OnSendTo, void(const std::string&));

 private:
  scoped_refptr<net::IOBuffer> recv_buffer_;
  int recv_buffer_size_;
  net::CompletionCallback recv_callback_;
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
      : socket_factory_(new MockDatagramSocketFactory),
        mdns_client_(
            scoped_ptr<net::MDnsConnection::SocketFactory>(
                socket_factory_)) {
    net::MDnsClient::SetInstance(&mdns_client_);
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

  MockDatagramSocketFactory* socket_factory_;
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

  EXPECT_CALL(delegate, OnServiceUpdated(ServiceWatcher::UPDATE_CHANGED,
                                         "hello._privet._tcp.local"))
      .Times(Exactly(2));

  socket_factory_->SimulateReceive(
      kSamplePacketSRV, sizeof(kSamplePacketSRV));

  socket_factory_->SimulateReceive(
      kSamplePacketTXT, sizeof(kSamplePacketTXT));

  watcher->ReadCachedServices();
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
