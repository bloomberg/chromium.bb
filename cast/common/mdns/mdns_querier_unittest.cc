// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_querier.h"

#include "cast/common/mdns/mdns_random.h"
#include "cast/common/mdns/mdns_receiver.h"
#include "cast/common/mdns/mdns_record_changed_callback.h"
#include "cast/common/mdns/mdns_sender.h"
#include "cast/common/mdns/mdns_writer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/udp_packet.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace cast {
namespace mdns {

using openscreen::IPAddress;
using openscreen::IPEndpoint;
using openscreen::platform::Clock;
using openscreen::platform::FakeClock;
using openscreen::platform::FakeTaskRunner;
using openscreen::platform::NetworkInterfaceIndex;
using openscreen::platform::TaskRunner;
using openscreen::platform::UdpPacket;
using openscreen::platform::UdpSocket;
using testing::_;
using testing::Args;
using testing::Invoke;
using testing::Return;
using testing::WithArgs;

// Only compare NAME, CLASS, TYPE and RDATA
ACTION_P(PartialCompareRecords, expected) {
  const MdnsRecord& actual = arg0;
  EXPECT_TRUE(actual.name() == expected.name());
  EXPECT_TRUE(actual.dns_class() == expected.dns_class());
  EXPECT_TRUE(actual.dns_type() == expected.dns_type());
  EXPECT_TRUE(actual.rdata() == expected.rdata());
}

class MockUdpSocket : public UdpSocket {
 public:
  MOCK_METHOD(bool, IsIPv4, (), (const, override));
  MOCK_METHOD(bool, IsIPv6, (), (const, override));
  MOCK_METHOD(IPEndpoint, GetLocalEndpoint, (), (const, override));
  MOCK_METHOD(void, Bind, (), (override));
  MOCK_METHOD(void,
              SetMulticastOutboundInterface,
              (NetworkInterfaceIndex),
              (override));
  MOCK_METHOD(void,
              JoinMulticastGroup,
              (const IPAddress&, NetworkInterfaceIndex),
              (override));
  MOCK_METHOD(void,
              SendMessage,
              (const void*, size_t, const IPEndpoint&),
              (override));
  MOCK_METHOD(void, SetDscp, (DscpMode), (override));
};

class MockRecordChangedCallback : public MdnsRecordChangedCallback {
 public:
  MOCK_METHOD(void,
              OnRecordChanged,
              (const MdnsRecord&, RecordChangedEvent event),
              (override));
};

class MdnsQuerierTest : public testing::Test {
 public:
  MdnsQuerierTest()
      : clock_(Clock::now()),
        task_runner_(&clock_),
        sender_(&socket_),
        receiver_(&socket_),
        record0_created_(DomainName{"testing", "local"},
                         DnsType::kA,
                         DnsClass::kIN,
                         RecordType::kShared,
                         std::chrono::seconds(120),
                         ARecordRdata(IPAddress{172, 0, 0, 1})),
        record0_updated_(DomainName{"testing", "local"},
                         DnsType::kA,
                         DnsClass::kIN,
                         RecordType::kShared,
                         std::chrono::seconds(120),
                         ARecordRdata(IPAddress{172, 0, 0, 2})),
        record0_deleted_(DomainName{"testing", "local"},
                         DnsType::kA,
                         DnsClass::kIN,
                         RecordType::kShared,
                         std::chrono::seconds(0),  // a good bye record
                         ARecordRdata(IPAddress{172, 0, 0, 2})),
        record1_created_(DomainName{"poking", "local"},
                         DnsType::kA,
                         DnsClass::kIN,
                         RecordType::kShared,
                         std::chrono::seconds(120),
                         ARecordRdata(IPAddress{192, 168, 0, 1})) {
    receiver_.Start();
  }

  std::unique_ptr<MdnsQuerier> CreateQuerier() {
    return std::make_unique<MdnsQuerier>(&sender_, &receiver_, &task_runner_,
                                         &FakeClock::now, &random_);
  }

 protected:
  UdpPacket CreatePacketWithRecord(const MdnsRecord& record) {
    MdnsMessage message(CreateMessageId(), MessageType::Response);
    message.AddAnswer(record);
    std::vector<uint8_t> buffer(message.MaxWireSize());
    MdnsWriter writer(buffer.data(), buffer.size());
    writer.Write(message);
    UdpPacket packet(writer.offset());
    packet.assign(buffer.data(), buffer.data() + writer.offset());
    return packet;
  }

  FakeClock clock_;
  FakeTaskRunner task_runner_;
  MockUdpSocket socket_;
  MdnsSender sender_;
  MdnsReceiver receiver_;
  MdnsRandom random_;

  MdnsRecord record0_created_;
  MdnsRecord record0_updated_;
  MdnsRecord record0_deleted_;
  MdnsRecord record1_created_;
  MdnsRecord record1_updated_;
  MdnsRecord record1_deleted_;
};

TEST_F(MdnsQuerierTest, RecordCreatedUpdatedDeleted) {
  std::unique_ptr<MdnsQuerier> querier = CreateQuerier();
  MockRecordChangedCallback callback;

  EXPECT_CALL(socket_, SendMessage(_, _, _));
  querier->StartQuery(DomainName{"testing", "local"}, DnsType::kA,
                      DnsClass::kIN, &callback);

  EXPECT_CALL(callback, OnRecordChanged(_, RecordChangedEvent::kCreated))
      .WillOnce(WithArgs<0>(PartialCompareRecords(record0_created_)));
  EXPECT_CALL(callback, OnRecordChanged(_, RecordChangedEvent::kUpdated))
      .WillOnce(WithArgs<0>(PartialCompareRecords(record0_updated_)));
  EXPECT_CALL(callback, OnRecordChanged(_, RecordChangedEvent::kDeleted))
      .WillOnce(WithArgs<0>(PartialCompareRecords(record0_deleted_)));

  receiver_.OnRead(&socket_, CreatePacketWithRecord(record0_created_));
  receiver_.OnRead(&socket_, CreatePacketWithRecord(record0_updated_));
  receiver_.OnRead(&socket_, CreatePacketWithRecord(record0_deleted_));

  // Advance clock for expiration to happen, since it's delayed by 1 second as
  // per RFC 6762
  clock_.Advance(std::chrono::seconds(1));
}

TEST_F(MdnsQuerierTest, StartQueryTwice) {
  std::unique_ptr<MdnsQuerier> querier = CreateQuerier();
  MockRecordChangedCallback callback;

  querier->StartQuery(DomainName{"testing", "local"}, DnsType::kA,
                      DnsClass::kIN, &callback);
  querier->StartQuery(DomainName{"testing", "local"}, DnsType::kA,
                      DnsClass::kIN, &callback);

  EXPECT_CALL(callback, OnRecordChanged(_, _)).Times(1);

  receiver_.OnRead(&socket_, CreatePacketWithRecord(record0_created_));
}

TEST_F(MdnsQuerierTest, NoRecordChangesAfterStop) {
  std::unique_ptr<MdnsQuerier> querier = CreateQuerier();
  MockRecordChangedCallback callback;
  querier->StartQuery(DomainName{"testing", "local"}, DnsType::kA,
                      DnsClass::kIN, &callback);
  EXPECT_CALL(callback, OnRecordChanged(_, _)).Times(1);
  receiver_.OnRead(&socket_, CreatePacketWithRecord(record0_created_));
  querier->StopQuery(DomainName{"testing", "local"}, DnsType::kA, DnsClass::kIN,
                     &callback);
  receiver_.OnRead(&socket_, CreatePacketWithRecord(record0_updated_));
}

TEST_F(MdnsQuerierTest, StopQueryTwice) {
  std::unique_ptr<MdnsQuerier> querier = CreateQuerier();
  MockRecordChangedCallback callback;
  querier->StartQuery(DomainName{"testing", "local"}, DnsType::kA,
                      DnsClass::kIN, &callback);
  EXPECT_CALL(callback, OnRecordChanged(_, _)).Times(0);
  querier->StopQuery(DomainName{"testing", "local"}, DnsType::kA, DnsClass::kIN,
                     &callback);
  querier->StopQuery(DomainName{"testing", "local"}, DnsType::kA, DnsClass::kIN,
                     &callback);
  receiver_.OnRead(&socket_, CreatePacketWithRecord(record0_created_));
}

TEST_F(MdnsQuerierTest, StopNonExistingQuery) {
  // Just making sure nothing crashes.
  std::unique_ptr<MdnsQuerier> querier = CreateQuerier();
  MockRecordChangedCallback callback;
  querier->StopQuery(DomainName{"testing", "local"}, DnsType::kA, DnsClass::kIN,
                     &callback);
}

TEST_F(MdnsQuerierTest, IrrelevantRecordReceived) {
  std::unique_ptr<MdnsQuerier> querier = CreateQuerier();
  MockRecordChangedCallback callback;
  querier->StartQuery(DomainName{"testing", "local"}, DnsType::kA,
                      DnsClass::kIN, &callback);
  EXPECT_CALL(callback, OnRecordChanged(_, _)).Times(1);
  receiver_.OnRead(&socket_, CreatePacketWithRecord(record0_created_));
  receiver_.OnRead(&socket_, CreatePacketWithRecord(record1_created_));
}

TEST_F(MdnsQuerierTest, DifferentCallersSameQuestion) {
  std::unique_ptr<MdnsQuerier> querier = CreateQuerier();
  MockRecordChangedCallback callback1;
  MockRecordChangedCallback callback2;
  querier->StartQuery(DomainName{"testing", "local"}, DnsType::kA,
                      DnsClass::kIN, &callback1);
  querier->StartQuery(DomainName{"testing", "local"}, DnsType::kA,
                      DnsClass::kIN, &callback2);
  EXPECT_CALL(callback1, OnRecordChanged(_, _)).Times(1);
  EXPECT_CALL(callback2, OnRecordChanged(_, _)).Times(1);
  receiver_.OnRead(&socket_, CreatePacketWithRecord(record0_created_));
}

TEST_F(MdnsQuerierTest, DifferentCallersDifferentQuestions) {
  std::unique_ptr<MdnsQuerier> querier = CreateQuerier();
  MockRecordChangedCallback callback1;
  MockRecordChangedCallback callback2;
  querier->StartQuery(DomainName{"testing", "local"}, DnsType::kA,
                      DnsClass::kIN, &callback1);
  querier->StartQuery(DomainName{"poking", "local"}, DnsType::kA, DnsClass::kIN,
                      &callback2);
  EXPECT_CALL(callback1, OnRecordChanged(_, _)).Times(1);
  EXPECT_CALL(callback2, OnRecordChanged(_, _)).Times(1);
  receiver_.OnRead(&socket_, CreatePacketWithRecord(record0_created_));
  receiver_.OnRead(&socket_, CreatePacketWithRecord(record1_created_));
}

TEST_F(MdnsQuerierTest, SameCallerDifferentQuestions) {
  std::unique_ptr<MdnsQuerier> querier = CreateQuerier();
  MockRecordChangedCallback callback;
  querier->StartQuery(DomainName{"testing", "local"}, DnsType::kA,
                      DnsClass::kIN, &callback);
  querier->StartQuery(DomainName{"poking", "local"}, DnsType::kA, DnsClass::kIN,
                      &callback);
  EXPECT_CALL(callback, OnRecordChanged(_, _)).Times(2);
  receiver_.OnRead(&socket_, CreatePacketWithRecord(record0_created_));
  receiver_.OnRead(&socket_, CreatePacketWithRecord(record1_created_));
}

}  // namespace mdns
}  // namespace cast
