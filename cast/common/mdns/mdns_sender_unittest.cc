// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_sender.h"

#include "cast/common/mdns/mdns_records.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_udp_socket.h"

namespace cast {
namespace mdns {

using openscreen::Error;
using openscreen::IPAddress;
using openscreen::IPEndpoint;
using openscreen::platform::FakeUdpSocket;
using testing::_;
using testing::Args;
using testing::Return;

namespace {

MATCHER_P(
    VoidPointerMatchesBytes,
    expected_data,
    "Matches data at the pointer against the provided C-style byte array.") {
  const uint8_t* actual_data = static_cast<const uint8_t*>(arg);
  for (size_t i = 0; i < expected_data.size(); ++i) {
    if (actual_data[i] != expected_data[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace

class MdnsSenderTest : public testing::Test {
 public:
  MdnsSenderTest()
      : a_question_(DomainName{"testing", "local"},
                    DnsType::kA,
                    DnsClass::kIN,
                    ResponseType::kMulticast),
        a_record_(DomainName{"testing", "local"},
                  DnsType::kA,
                  DnsClass::kIN,
                  RecordType::kShared,
                  std::chrono::seconds(120),
                  ARecordRdata(IPAddress{172, 0, 0, 1})),
        query_message_(1, MessageType::Query),
        response_message_(1, MessageType::Response),
        ipv4_multicast_endpoint_{
            .address = IPAddress(kDefaultMulticastGroupIPv4),
            .port = kDefaultMulticastPort},
        ipv6_multicast_endpoint_{
            .address = IPAddress(kDefaultMulticastGroupIPv6),
            .port = kDefaultMulticastPort} {
    query_message_.AddQuestion(a_question_);
    response_message_.AddAnswer(a_record_);
  }

 protected:
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
  // clang-format on

  MdnsQuestion a_question_;
  MdnsRecord a_record_;
  MdnsMessage query_message_;
  MdnsMessage response_message_;
  IPEndpoint ipv4_multicast_endpoint_;
  IPEndpoint ipv6_multicast_endpoint_;
};

TEST_F(MdnsSenderTest, SendMulticastIPv4) {
  std::unique_ptr<FakeUdpSocket> socket_info =
      FakeUdpSocket::CreateDefault(IPAddress::Version::kV4);
  MdnsSender sender(socket_info.get());
  socket_info->EnqueueSendResult(Error::Code::kNone);
  EXPECT_CALL(*socket_info->client_mock(), OnSendError(_, _)).Times(0);
  EXPECT_EQ(sender.SendMulticast(query_message_), Error::Code::kNone);
  EXPECT_EQ(socket_info->send_queue_size(), size_t{0});
}

TEST_F(MdnsSenderTest, SendMulticastIPv6) {
  std::unique_ptr<FakeUdpSocket> socket_info =
      FakeUdpSocket::CreateDefault(IPAddress::Version::kV6);
  MdnsSender sender(socket_info.get());
  socket_info->EnqueueSendResult(Error::Code::kNone);
  EXPECT_CALL(*socket_info->client_mock(), OnSendError(_, _)).Times(0);
  EXPECT_EQ(sender.SendMulticast(query_message_), Error::Code::kNone);
  EXPECT_EQ(socket_info->send_queue_size(), size_t{0});
}

TEST_F(MdnsSenderTest, SendUnicastIPv4) {
  IPEndpoint endpoint{.address = IPAddress{192, 168, 1, 1}, .port = 31337};

  std::unique_ptr<FakeUdpSocket> socket_info =
      FakeUdpSocket::CreateDefault(IPAddress::Version::kV4);
  MdnsSender sender(socket_info.get());
  socket_info->EnqueueSendResult(Error::Code::kNone);
  EXPECT_CALL(*socket_info->client_mock(), OnSendError(_, _)).Times(0);
  EXPECT_EQ(sender.SendUnicast(response_message_, endpoint),
            Error::Code::kNone);
  EXPECT_EQ(socket_info->send_queue_size(), size_t{0});
}

TEST_F(MdnsSenderTest, SendUnicastIPv6) {
  constexpr uint8_t kIPv6AddressBytes[] = {
      0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x02, 0xb3, 0xff, 0xfe, 0x1e, 0x83, 0x29,
  };
  IPEndpoint endpoint{.address = IPAddress(kIPv6AddressBytes), .port = 31337};

  std::unique_ptr<FakeUdpSocket> socket_info =
      FakeUdpSocket::CreateDefault(IPAddress::Version::kV6);
  MdnsSender sender(socket_info.get());
  socket_info->EnqueueSendResult(Error::Code::kNone);
  EXPECT_CALL(*socket_info->client_mock(), OnSendError(_, _)).Times(0);
  EXPECT_EQ(sender.SendUnicast(response_message_, endpoint),
            Error::Code::kNone);
  EXPECT_EQ(socket_info->send_queue_size(), size_t{0});
}

TEST_F(MdnsSenderTest, MessageTooBig) {
  MdnsMessage big_message_(1, MessageType::Query);
  for (size_t i = 0; i < 100; ++i) {
    big_message_.AddQuestion(a_question_);
    big_message_.AddAnswer(a_record_);
  }
  std::unique_ptr<FakeUdpSocket> socket_info =
      FakeUdpSocket::CreateDefault(IPAddress::Version::kV4);
  MdnsSender sender(socket_info.get());
  socket_info->EnqueueSendResult(Error::Code::kNone);
  EXPECT_CALL(*socket_info->client_mock(), OnSendError(_, _)).Times(0);
  EXPECT_EQ(sender.SendMulticast(big_message_),
            Error::Code::kInsufficientBuffer);
  EXPECT_EQ(socket_info->send_queue_size(), size_t{1});
}

TEST_F(MdnsSenderTest, ReturnsErrorOnSocketFailure) {
  std::unique_ptr<FakeUdpSocket> socket_info =
      FakeUdpSocket::CreateDefault(IPAddress::Version::kV4);
  MdnsSender sender(socket_info.get());
  Error error = Error(Error::Code::kConnectionFailed, "error message");
  socket_info->EnqueueSendResult(error);
  EXPECT_CALL(*socket_info->client_mock(), OnSendError(_, error)).Times(1);
  EXPECT_EQ(sender.SendMulticast(query_message_), Error::Code::kNone);
  EXPECT_EQ(socket_info->send_queue_size(), size_t{0});
}

}  // namespace mdns
}  // namespace cast
