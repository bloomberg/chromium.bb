// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_trackers.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace cast {
namespace mdns {

using openscreen::platform::FakeClock;
using openscreen::platform::FakeTaskRunner;
using openscreen::platform::NetworkInterfaceIndex;
using ::testing::_;
using ::testing::Args;
using ::testing::Return;

class MockUdpSocket : public UdpSocket {
 public:
  MockUdpSocket(TaskRunner* task_runner) : UdpSocket(task_runner, nullptr) {}
  ~MockUdpSocket() { CloseIfOpen(); }
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

class MdnsTrackerTest : public ::testing::Test {
 public:
  MdnsTrackerTest()
      : clock_(Clock::now()),
        task_runner_(&clock_),
        socket_(&task_runner_),
        sender_(&socket_),
        a_question_(DomainName{"testing", "local"},
                    DnsType::kA,
                    DnsClass::kIN,
                    ResponseType::kMulticast) {}

 protected:
  // clang-format off
  const std::vector<uint8_t> kQueryBytes = {
      0x00, 0x00,  // ID = 0
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
  FakeClock clock_;
  FakeTaskRunner task_runner_;
  MockUdpSocket socket_;
  MdnsSender sender_;
  MdnsRandom random_;

  MdnsQuestion a_question_;
};

TEST_F(MdnsTrackerTest, QueryTrackerStartStop) {
  MdnsQuestionTracker tracker(&sender_, &task_runner_, &FakeClock::now,
                              &random_);
  EXPECT_EQ(tracker.IsStarted(), false);
  EXPECT_EQ(tracker.Stop(), Error(Error::Code::kOperationInvalid));
  EXPECT_EQ(tracker.IsStarted(), false);
  EXPECT_EQ(tracker.Start(a_question_), Error(Error::Code::kNone));
  EXPECT_EQ(tracker.IsStarted(), true);
  EXPECT_EQ(tracker.Start(a_question_), Error(Error::Code::kOperationInvalid));
  EXPECT_EQ(tracker.IsStarted(), true);
  EXPECT_EQ(tracker.Stop(), Error(Error::Code::kNone));
  EXPECT_EQ(tracker.IsStarted(), false);
}

// Initial query is delayed for up to 120 ms as per RFC 6762 Section 5.2
// Subsequent queries happen no sooner than a second after the initial query and
// the interval between the queries increases at least by a factor of 2 for each
// next query up until it's capped at 1 hour.
// https://tools.ietf.org/html/rfc6762#section-5.2

TEST_F(MdnsTrackerTest, QueryTrackerQueryAfterDelay) {
  MdnsQuestionTracker tracker(&sender_, &task_runner_, &FakeClock::now,
                              &random_);
  tracker.Start(a_question_);

  EXPECT_CALL(socket_, SendMessage(_, _, _)).Times(1);
  clock_.Advance(std::chrono::milliseconds(120));

  std::chrono::seconds interval{1};
  while (interval < std::chrono::hours(1)) {
    EXPECT_CALL(socket_, SendMessage(_, _, _)).Times(1);
    clock_.Advance(interval);
    interval *= 2;
  }
}

TEST_F(MdnsTrackerTest, QueryTrackerSendsMessage) {
  MdnsQuestionTracker tracker(&sender_, &task_runner_, &FakeClock::now,
                              &random_);
  tracker.Start(a_question_);

  EXPECT_CALL(socket_, SendMessage(_, kQueryBytes.size(), _))
      .WillOnce(testing::WithArgs<0, 1>(
          testing::Invoke([this](const void* data, size_t size) {
            EXPECT_EQ(size, kQueryBytes.size());
            const uint8_t* bytes = static_cast<const uint8_t*>(data);
            // Start at bytes[2] to skip a generated message ID.
            for (size_t i = 2; i < size; ++i) {
              EXPECT_EQ(bytes[i], kQueryBytes[i]);
            }
          })));
  clock_.Advance(std::chrono::milliseconds(120));
}

TEST_F(MdnsTrackerTest, QueryTrackerNoQueryAfterStop) {
  MdnsQuestionTracker tracker(&sender_, &task_runner_, &FakeClock::now,
                              &random_);

  EXPECT_EQ(tracker.Start(a_question_), Error(Error::Code::kNone));
  EXPECT_EQ(tracker.Stop(), Error(Error::Code::kNone));
  EXPECT_CALL(socket_, SendMessage(_, _, _)).Times(0);
  // Advance fake clock by a long time interval to make sure if there's a
  // scheduled task, it will run.
  clock_.Advance(std::chrono::hours(1));
}

TEST_F(MdnsTrackerTest, QueryTrackerNoQueryAfterDestruction) {
  {
    MdnsQuestionTracker tracker(&sender_, &task_runner_, &FakeClock::now,
                                &random_);
    tracker.Start(a_question_);
  }
  EXPECT_CALL(socket_, SendMessage(_, _, _)).Times(0);
  // Advance fake clock by a long time interval to make sure if there's a
  // scheduled task, it will run.
  clock_.Advance(std::chrono::hours(1));
}

}  // namespace mdns
}  // namespace cast
