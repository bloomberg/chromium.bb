// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_receiver.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/time.h"
#include "platform/test/mock_udp_socket.h"

namespace cast {
namespace mdns {

using ::testing::_;
using ::testing::Return;
using MockUdpSocket = openscreen::platform::MockUdpSocket;

// TODO(yakimakha): Update tests to use a fake NetworkRunner when implemented
class MockNetworkRunner : public NetworkRunner {
 public:
  MOCK_METHOD2(ReadRepeatedly, Error(UdpSocket*, UdpReadCallback*));
  MOCK_METHOD1(CancelRead, Error(UdpSocket*));

  void PostPackagedTask(Task task) override {}
  void PostPackagedTaskWithDelay(
      Task task,
      openscreen::platform::Clock::duration delay) override {}
};

class MockMdnsReceiverDelegate : public MdnsReceiver::Delegate {
 public:
  MOCK_METHOD2(OnQueryReceived, void(const MdnsMessage&, const IPEndpoint&));
  MOCK_METHOD2(OnResponseReceived, void(const MdnsMessage&, const IPEndpoint&));
};

TEST(MdnsReceiverTest, ReceiveQuery) {
  // clang-format off
  const std::vector<uint8_t> kQueryBytes = {
      0x00, 0x01,  // ID = 1
      0x00, 0x00,  // FLAGS = None
      0x00, 0x01,  // Question count
      0x00, 0x00,  // Answer count
      0x00, 0x00,  // Authority count
      0x00, 0x00,  // Additional count
      // Question
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x01,  // TYPE = A (1)
      0x00, 0x01,  // CLASS = IN (1)
  };
  // clang-format on

  std::unique_ptr<openscreen::platform::MockUdpSocket> socket_info =
      MockUdpSocket::CreateDefault(openscreen::IPAddress::Version::kV4);
  MockNetworkRunner runner;
  MockMdnsReceiverDelegate delegate;
  MdnsReceiver receiver(socket_info.get(), &runner, &delegate);

  EXPECT_CALL(runner, ReadRepeatedly(socket_info.get(), _))
      .WillOnce(Return(Error::Code::kNone));
  receiver.Start();

  MdnsQuestion question(DomainName{"testing", "local"}, DnsType::kA,
                        DnsClass::kIN, false);
  MdnsMessage message(1, MessageType::Query);
  message.AddQuestion(question);

  UdpPacket packet(kQueryBytes.size());
  packet.assign(kQueryBytes.data(), kQueryBytes.data() + kQueryBytes.size());
  packet.set_source(
      IPEndpoint{.address = IPAddress(192, 168, 1, 1), .port = 31337});
  packet.set_destination(
      IPEndpoint{.address = IPAddress(kDefaultMulticastGroupIPv4),
                 .port = kDefaultMulticastPort});

  // Imitate a call to OnRead from NetworkRunner by calling it manually here
  EXPECT_CALL(delegate, OnQueryReceived(message, packet.source())).Times(1);
  receiver.OnRead(std::move(packet), &runner);

  EXPECT_CALL(runner, CancelRead(socket_info.get()))
      .WillOnce(Return(Error::Code::kNone));
  receiver.Stop();
}

TEST(MdnsReceiverTest, ReceiveResponse) {
  // clang-format off
  const std::vector<uint8_t> kResponseBytes = {
      0x00, 0x01,  // ID = 1
      0x84, 0x00,  // FLAGS = AA | RESPONSE
      0x00, 0x00,  // Question count
      0x00, 0x01,  // Answer count
      0x00, 0x00,  // Authority count
      0x00, 0x00,  // Additional count
      // Answer
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x01,              // TYPE = A (1)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x04,              // RDLENGTH = 4 bytes
      0xac, 0x00, 0x00, 0x01,  // 172.0.0.1
  };

  constexpr uint8_t kIPv6AddressBytes[] = {
      0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x02, 0xb3, 0xff, 0xfe, 0x1e, 0x83, 0x29,
  };
  // clang-format on

  std::unique_ptr<openscreen::platform::MockUdpSocket> socket_info =
      MockUdpSocket::CreateDefault(openscreen::IPAddress::Version::kV6);
  MockNetworkRunner runner;
  MockMdnsReceiverDelegate delegate;
  MdnsReceiver receiver(socket_info.get(), &runner, &delegate);

  EXPECT_CALL(runner, ReadRepeatedly(socket_info.get(), _))
      .WillOnce(Return(Error::Code::kNone));
  receiver.Start();

  MdnsRecord record(DomainName{"testing", "local"}, DnsType::kA, DnsClass::kIN,
                    false, 120, ARecordRdata(IPAddress{172, 0, 0, 1}));
  MdnsMessage message(1, MessageType::Response);
  message.AddAnswer(record);

  UdpPacket packet(kResponseBytes.size());
  packet.assign(kResponseBytes.data(),
                kResponseBytes.data() + kResponseBytes.size());
  packet.set_source(
      IPEndpoint{.address = IPAddress(kIPv6AddressBytes), .port = 31337});
  packet.set_destination(
      IPEndpoint{.address = IPAddress(kDefaultMulticastGroupIPv6),
                 .port = kDefaultMulticastPort});

  // Imitate a call to OnRead from NetworkRunner by calling it manually here
  EXPECT_CALL(delegate, OnResponseReceived(message, packet.source())).Times(1);
  receiver.OnRead(std::move(packet), &runner);

  EXPECT_CALL(runner, CancelRead(socket_info.get()))
      .WillOnce(Return(Error::Code::kNone));
  receiver.Stop();
}

}  // namespace mdns
}  // namespace cast
