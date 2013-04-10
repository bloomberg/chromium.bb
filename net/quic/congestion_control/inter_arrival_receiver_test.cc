// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "net/quic/congestion_control/inter_arrival_receiver.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class InterArrivalReceiverTest : public ::testing::Test {
 protected:
  InterArrivalReceiver receiver_;
  MockClock clock_;
};

TEST_F(InterArrivalReceiverTest, SimpleReceiver) {
  QuicTime start = clock_.ApproximateNow();
  QuicTime::Delta received_delta = QuicTime::Delta::FromMilliseconds(10);
  clock_.AdvanceTime(received_delta);
  QuicTime receive_timestamp = clock_.ApproximateNow();
  receiver_.RecordIncomingPacket(1, 1, receive_timestamp, false);

  QuicCongestionFeedbackFrame feedback;
  ASSERT_FALSE(receiver_.GenerateCongestionFeedback(&feedback));

  clock_.AdvanceTime(received_delta);
  receive_timestamp = clock_.ApproximateNow();
  // Packet not received; but rather revived by FEC.
  receiver_.RecordIncomingPacket(1, 2, receive_timestamp, true);
  clock_.AdvanceTime(received_delta);
  receive_timestamp = clock_.ApproximateNow();
  receiver_.RecordIncomingPacket(1, 3, receive_timestamp, false);

  ASSERT_TRUE(receiver_.GenerateCongestionFeedback(&feedback));

  EXPECT_EQ(kInterArrival, feedback.type);
  EXPECT_EQ(1, feedback.inter_arrival.accumulated_number_of_lost_packets);
  EXPECT_EQ(3u, feedback.inter_arrival.received_packet_times.size());
  TimeMap::iterator it = feedback.inter_arrival.received_packet_times.begin();
  EXPECT_EQ(1u, it->first);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), it->second.Subtract(start));
  it = feedback.inter_arrival.received_packet_times.begin();
  it++;
  EXPECT_EQ(2u, it->first);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(20), it->second.Subtract(start));
  it++;
  EXPECT_EQ(3u, it->first);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(30), it->second.Subtract(start));
}

}  // namespace test
}  // namespace net
