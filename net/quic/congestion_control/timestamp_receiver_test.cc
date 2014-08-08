// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "net/quic/congestion_control/timestamp_receiver.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class TimestampReceiverTest : public ::testing::Test {
 protected:
  TimestampReceiver receiver_;
  MockClock clock_;
};

TEST_F(TimestampReceiverTest, SimpleReceiver) {
  QuicTime start = clock_.ApproximateNow();
  QuicTime::Delta received_delta = QuicTime::Delta::FromMilliseconds(10);
  clock_.AdvanceTime(received_delta);
  QuicTime receive_timestamp = clock_.ApproximateNow();
  receiver_.RecordIncomingPacket(1, 1, receive_timestamp);

  QuicCongestionFeedbackFrame feedback;
  // Do not generate a congestion feedback frame because it has only one packet.
  ASSERT_FALSE(receiver_.GenerateCongestionFeedback(&feedback));

  clock_.AdvanceTime(received_delta);
  receive_timestamp = clock_.ApproximateNow();
  // Packet not received; but rather revived by FEC.
  receiver_.RecordIncomingPacket(1, 2, receive_timestamp);
  clock_.AdvanceTime(received_delta);
  receive_timestamp = clock_.ApproximateNow();
  receiver_.RecordIncomingPacket(1, 3, receive_timestamp);

  ASSERT_TRUE(receiver_.GenerateCongestionFeedback(&feedback));

  EXPECT_EQ(kTimestamp, feedback.type);
  EXPECT_EQ(3u, feedback.timestamp.received_packet_times.size());
  TimeMap::iterator it = feedback.timestamp.received_packet_times.begin();
  EXPECT_EQ(1u, it->first);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), it->second.Subtract(start));
  it = feedback.timestamp.received_packet_times.begin();
  ++it;
  EXPECT_EQ(2u, it->first);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(20), it->second.Subtract(start));
  ++it;
  EXPECT_EQ(3u, it->first);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(30), it->second.Subtract(start));
}

}  // namespace test
}  // namespace net
