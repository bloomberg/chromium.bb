// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/local_discovery/service_discovery_client_impl.h"
#include "net/dns/mdns_client_impl.h"
#include "net/dns/mock_mdns_socket_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace local_discovery {

namespace {

const char kSamplePacketA[] = {
  // Header
  '\x00', '\x00',               // ID is zeroed out
  '\x81', '\x80',               // Standard query response, RA, no error
  '\x00', '\x00',               // No questions (for simplicity)
  '\x00', '\x01',               // 1 RR (answers)
  '\x00', '\x00',               // 0 authority RRs
  '\x00', '\x00',               // 0 additional RRs

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

const char kSamplePacketAAAA[] = {
  // Header
  '\x00', '\x00',               // ID is zeroed out
  '\x81', '\x80',               // Standard query response, RA, no error
  '\x00', '\x00',               // No questions (for simplicity)
  '\x00', '\x01',               // 1 RR (answers)
  '\x00', '\x00',               // 0 authority RRs
  '\x00', '\x00',               // 0 additional RRs

  '\x07', 'm', 'y', 'h', 'e', 'l', 'l', 'o',
  '\x05', 'l', 'o', 'c', 'a', 'l',
  '\x00',
  '\x00', '\x1C',        // TYPE is AAAA.
  '\x00', '\x01',        // CLASS is IN.
  '\x00', '\x00',        // TTL (4 bytes) is 16 seconds.
  '\x00', '\x10',
  '\x00', '\x10',        // RDLENGTH is 4 bytes.
  '\x00', '\x0A', '\x00', '\x00',
  '\x00', '\x00', '\x00', '\x00',
  '\x00', '\x01', '\x00', '\x02',
  '\x00', '\x03', '\x00', '\x04',
};

class LocalDomainResolverTest : public testing::Test {
 public:
  LocalDomainResolverTest() : socket_factory_(new net::MockMDnsSocketFactory),
    mdns_client_(
        scoped_ptr<net::MDnsConnection::SocketFactory>(
            socket_factory_)) {
  }

  ~LocalDomainResolverTest() {
  }

  virtual void SetUp() OVERRIDE {
    mdns_client_.StartListening();
  }

  void AddressCallback(bool resolved, const net::IPAddressNumber& address) {
    if (address == net::IPAddressNumber()) {
      AddressCallbackInternal(resolved, "");
    } else {
      AddressCallbackInternal(resolved, net::IPAddressToString(address));
    }
  }

  void RunFor(base::TimeDelta time_period) {
    base::CancelableCallback<void()> callback(base::Bind(
        &base::MessageLoop::Quit,
        base::Unretained(base::MessageLoop::current())));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, callback.callback(), time_period);

    base::MessageLoop::current()->Run();
    callback.Cancel();
  }

  MOCK_METHOD2(AddressCallbackInternal,
               void(bool resolved, std::string address));

  net::MockMDnsSocketFactory* socket_factory_;
  net::MDnsClientImpl mdns_client_;
  base::MessageLoop message_loop_;
};

TEST_F(LocalDomainResolverTest, ResolveDomainA) {
  LocalDomainResolverImpl resolver(
      "myhello.local", net::ADDRESS_FAMILY_IPV4,
      base::Bind(&LocalDomainResolverTest::AddressCallback,
                 base::Unretained(this)), &mdns_client_);

  EXPECT_CALL(*socket_factory_, OnSendTo(_))
      .Times(2);  // Twice per query

  resolver.Start();

  EXPECT_CALL(*this, AddressCallbackInternal(true, "1.2.3.4"));

  socket_factory_->SimulateReceive(
      kSamplePacketA, sizeof(kSamplePacketA));
}

TEST_F(LocalDomainResolverTest, ResolveDomainAAAA) {
  LocalDomainResolverImpl resolver(
      "myhello.local", net::ADDRESS_FAMILY_IPV6,
      base::Bind(&LocalDomainResolverTest::AddressCallback,
                 base::Unretained(this)), &mdns_client_);

  EXPECT_CALL(*socket_factory_, OnSendTo(_))
      .Times(2);  // Twice per query

  resolver.Start();

  EXPECT_CALL(*this, AddressCallbackInternal(true, "a::1:2:3:4"));

  socket_factory_->SimulateReceive(
      kSamplePacketAAAA, sizeof(kSamplePacketAAAA));
}

TEST_F(LocalDomainResolverTest, ResolveDomainAny) {
  LocalDomainResolverImpl resolver(
      "myhello.local", net::ADDRESS_FAMILY_UNSPECIFIED,
      base::Bind(&LocalDomainResolverTest::AddressCallback,
                 base::Unretained(this)), &mdns_client_);

  EXPECT_CALL(*socket_factory_, OnSendTo(_))
      .Times(4);  // Twice per query

  resolver.Start();

  EXPECT_CALL(*this, AddressCallbackInternal(true, "a::1:2:3:4"));

  socket_factory_->SimulateReceive(
      kSamplePacketAAAA, sizeof(kSamplePacketAAAA));
}

TEST_F(LocalDomainResolverTest, ResolveDomainNone) {
  LocalDomainResolverImpl resolver(
      "myhello.local", net::ADDRESS_FAMILY_UNSPECIFIED,
      base::Bind(&LocalDomainResolverTest::AddressCallback,
                 base::Unretained(this)), &mdns_client_);

  EXPECT_CALL(*socket_factory_, OnSendTo(_))
      .Times(4);  // Twice per query

  resolver.Start();

  EXPECT_CALL(*this, AddressCallbackInternal(false, ""));

  RunFor(base::TimeDelta::FromSeconds(4));
}

}  // namespace

}  // namespace local_discovery
