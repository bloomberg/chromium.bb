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
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

ACTION_P2(VerifyMessageBytesWithoutId, expected_data, expected_size) {
  const uint8_t* actual_data = reinterpret_cast<const uint8_t*>(arg0);
  const size_t actual_size = arg1;
  EXPECT_EQ(actual_size, expected_size);
  // Start at bytes[2] to skip a generated message ID.
  for (size_t i = 2; i < actual_size; ++i) {
    EXPECT_EQ(actual_data[i], expected_data[i]);
  }
}

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
                    DnsType::kANY,
                    DnsClass::kIN,
                    ResponseType::kMulticast),
        a_record_(DomainName{"testing", "local"},
                  DnsType::kA,
                  DnsClass::kIN,
                  RecordType::kShared,
                  std::chrono::seconds(120),
                  ARecordRdata(IPAddress{172, 0, 0, 1})) {}

  template <class TrackerType, class TrackedType>
  void TrackerStartStop(TrackedType tracked_data) {
    TrackerType tracker(&sender_, &task_runner_, &FakeClock::now, &random_);
    EXPECT_EQ(tracker.IsStarted(), false);
    EXPECT_EQ(tracker.Stop(), Error(Error::Code::kOperationInvalid));
    EXPECT_EQ(tracker.IsStarted(), false);
    EXPECT_EQ(tracker.Start(tracked_data), Error(Error::Code::kNone));
    EXPECT_EQ(tracker.IsStarted(), true);
    EXPECT_EQ(tracker.Start(tracked_data),
              Error(Error::Code::kOperationInvalid));
    EXPECT_EQ(tracker.IsStarted(), true);
    EXPECT_EQ(tracker.Stop(), Error(Error::Code::kNone));
    EXPECT_EQ(tracker.IsStarted(), false);
  }

  template <class TrackerType, class TrackedType>
  void TrackerNoQueryAfterStop(TrackedType tracked_data) {
    TrackerType tracker(&sender_, &task_runner_, &FakeClock::now, &random_);
    EXPECT_EQ(tracker.Start(tracked_data), Error(Error::Code::kNone));
    EXPECT_EQ(tracker.Stop(), Error(Error::Code::kNone));
    EXPECT_CALL(socket_, SendMessage(_, _, _)).Times(0);
    // Advance fake clock by a long time interval to make sure if there's a
    // scheduled task, it will run.
    clock_.Advance(std::chrono::hours(1));
  }

  template <class TrackerType, class TrackedType>
  void TrackerNoQueryAfterDestruction(TrackedType tracked_data) {
    {
      TrackerType tracker(&sender_, &task_runner_, &FakeClock::now, &random_);
      tracker.Start(tracked_data);
    }
    EXPECT_CALL(socket_, SendMessage(_, _, _)).Times(0);
    // Advance fake clock by a long time interval to make sure if there's a
    // scheduled task, it will run.
    clock_.Advance(std::chrono::hours(1));
  }

 protected:
  // clang-format off
  const std::vector<uint8_t> kQuestionQueryBytes = {
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
      0x00, 0xFF,  // TYPE = ANY (255)
      0x00, 0x01,  // CLASS = IN (1)
  };

  const std::vector<uint8_t> kRecordQueryBytes = {
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
  MdnsRecord a_record_;
};

// Records are re-queried at 80%, 85%, 90% and 95% TTL as per RFC 6762
// Section 5.2 There are no subsequent queries to refresh the record after that,
// the record is expired after TTL has passed since the start of tracking.
// Random variance required is from 0% to 2%, making these times at most 82%,
// 87%, 92% and 97% TTL. Fake clock is advanced to 83%, 88%, 93% and 98% to make
// sure that task gets executed.
// https://tools.ietf.org/html/rfc6762#section-5.2

TEST_F(MdnsTrackerTest, RecordTrackerStartStop) {
  TrackerStartStop<MdnsRecordTracker>(a_record_);
}

TEST_F(MdnsTrackerTest, RecordTrackerQueryAfterDelay) {
  MdnsRecordTracker tracker(&sender_, &task_runner_, &FakeClock::now, &random_);
  tracker.Start(a_record_);
  // Only expect 4 queries being sent, when record reaches it's TTL it's
  // considered expired and another query is not sent
  constexpr double kTtlFractions[] = {0.83, 0.88, 0.93, 0.98, 1.00};
  EXPECT_CALL(socket_, SendMessage(_, _, _)).Times(4);

  Clock::duration time_passed{0};
  for (double fraction : kTtlFractions) {
    Clock::duration time_till_refresh =
        std::chrono::duration_cast<Clock::duration>(a_record_.ttl() * fraction);
    Clock::duration delta = time_till_refresh - time_passed;
    time_passed = time_till_refresh;
    clock_.Advance(delta);
  }
}

TEST_F(MdnsTrackerTest, RecordTrackerSendsMessage) {
  MdnsRecordTracker tracker(&sender_, &task_runner_, &FakeClock::now, &random_);
  tracker.Start(a_record_);

  EXPECT_CALL(socket_, SendMessage(_, _, _))
      .WillOnce(WithArgs<0, 1>(VerifyMessageBytesWithoutId(
          kRecordQueryBytes.data(), kRecordQueryBytes.size())));

  clock_.Advance(
      std::chrono::duration_cast<Clock::duration>(a_record_.ttl() * 0.83));
}

TEST_F(MdnsTrackerTest, RecordTrackerNoQueryAfterStop) {
  TrackerNoQueryAfterStop<MdnsRecordTracker>(a_record_);
}

TEST_F(MdnsTrackerTest, RecordTrackerNoQueryAfterDestruction) {
  TrackerNoQueryAfterDestruction<MdnsRecordTracker>(a_record_);
}

TEST_F(MdnsTrackerTest, RecordTrackerNoQueryAfterLateTask) {
  MdnsRecordTracker tracker(&sender_, &task_runner_, &FakeClock::now, &random_);
  tracker.Start(a_record_);
  // If task runner was too busy and callback happened too late, there should be
  // no query and instead the record will expire.
  // Check lower bound for task being late (TTL) and an arbitrarily long time
  // interval to ensure the query is not sent a later time.
  EXPECT_CALL(socket_, SendMessage(_, _, _)).Times(0);
  clock_.Advance(a_record_.ttl());
  clock_.Advance(std::chrono::hours(1));
}

// Initial query is delayed for up to 120 ms as per RFC 6762 Section 5.2
// Subsequent queries happen no sooner than a second after the initial query and
// the interval between the queries increases at least by a factor of 2 for each
// next query up until it's capped at 1 hour.
// https://tools.ietf.org/html/rfc6762#section-5.2

TEST_F(MdnsTrackerTest, QuestionTrackerStartStop) {
  TrackerStartStop<MdnsQuestionTracker>(a_question_);
}

TEST_F(MdnsTrackerTest, QuestionTrackerQueryAfterDelay) {
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

TEST_F(MdnsTrackerTest, QuestionTrackerSendsMessage) {
  MdnsQuestionTracker tracker(&sender_, &task_runner_, &FakeClock::now,
                              &random_);
  tracker.Start(a_question_);

  EXPECT_CALL(socket_, SendMessage(_, _, _))
      .WillOnce(WithArgs<0, 1>(VerifyMessageBytesWithoutId(
          kQuestionQueryBytes.data(), kQuestionQueryBytes.size())));

  clock_.Advance(std::chrono::milliseconds(120));
}

TEST_F(MdnsTrackerTest, QuestionTrackerNoQueryAfterStop) {
  TrackerNoQueryAfterStop<MdnsQuestionTracker>(a_question_);
}

TEST_F(MdnsTrackerTest, QuestionTrackerNoQueryAfterDestruction) {
  TrackerNoQueryAfterDestruction<MdnsQuestionTracker>(a_question_);
}

}  // namespace mdns
}  // namespace cast
