// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_unacked_packet_map.h"

#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::min;

namespace net {
namespace test {
namespace {

// Default packet length.
const uint32 kDefaultAckLength = 50;
const uint32 kDefaultLength = 1000;

class QuicUnackedPacketMapTest : public ::testing::Test {
 protected:
  QuicUnackedPacketMapTest()
      : now_(QuicTime::Zero().Add(QuicTime::Delta::FromMilliseconds(1000))) {
  }

  SerializedPacket CreateRetransmittablePacket(
      QuicPacketSequenceNumber sequence_number) {
    return SerializedPacket(sequence_number, PACKET_1BYTE_SEQUENCE_NUMBER, NULL,
                            0, new RetransmittableFrames());
  }

  SerializedPacket CreateNonRetransmittablePacket(
      QuicPacketSequenceNumber sequence_number) {
    return SerializedPacket(
        sequence_number, PACKET_1BYTE_SEQUENCE_NUMBER, NULL, 0, NULL);
  }

  void VerifyInFlightPackets(QuicPacketSequenceNumber* packets,
                             size_t num_packets) {
    unacked_packets_.RemoveObsoletePackets();
    if (num_packets == 0) {
      EXPECT_FALSE(unacked_packets_.HasInFlightPackets());
      EXPECT_FALSE(unacked_packets_.HasMultipleInFlightPackets());
      return;
    }
    if (num_packets == 1) {
      EXPECT_TRUE(unacked_packets_.HasInFlightPackets());
      EXPECT_FALSE(unacked_packets_.HasMultipleInFlightPackets());
      ASSERT_TRUE(unacked_packets_.IsUnacked(packets[0]));
      EXPECT_TRUE(unacked_packets_.GetTransmissionInfo(packets[0]).in_flight);
    }
    for (size_t i = 0; i < num_packets; ++i) {
      ASSERT_TRUE(unacked_packets_.IsUnacked(packets[i]));
      EXPECT_TRUE(unacked_packets_.GetTransmissionInfo(packets[i]).in_flight);
    }
    size_t in_flight_count = 0;
    for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
         it != unacked_packets_.end(); ++it) {
      if (it->in_flight) {
        ++in_flight_count;
      }
    }
    EXPECT_EQ(num_packets, in_flight_count);
  }

  void VerifyUnackedPackets(QuicPacketSequenceNumber* packets,
                            size_t num_packets) {
    unacked_packets_.RemoveObsoletePackets();
    if (num_packets == 0) {
      EXPECT_FALSE(unacked_packets_.HasUnackedPackets());
      EXPECT_FALSE(unacked_packets_.HasUnackedRetransmittableFrames());
      return;
    }
    EXPECT_TRUE(unacked_packets_.HasUnackedPackets());
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_TRUE(unacked_packets_.IsUnacked(packets[i])) << packets[i];
    }
    EXPECT_EQ(num_packets, unacked_packets_.GetNumUnackedPacketsDebugOnly());
  }

  void VerifyRetransmittablePackets(QuicPacketSequenceNumber* packets,
                                    size_t num_packets) {
    unacked_packets_.RemoveObsoletePackets();
    size_t num_retransmittable_packets = 0;
    for (QuicUnackedPacketMap::const_iterator it = unacked_packets_.begin();
         it != unacked_packets_.end(); ++it) {
      if (it->retransmittable_frames != NULL) {
        ++num_retransmittable_packets;
      }
    }
    EXPECT_EQ(num_packets, num_retransmittable_packets);
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_TRUE(unacked_packets_.HasRetransmittableFrames(packets[i]))
          << " packets[" << i << "]:" << packets[i];
    }
  }

  QuicUnackedPacketMap unacked_packets_;
  QuicTime now_;
};

TEST_F(QuicUnackedPacketMapTest, RttOnly) {
  // Acks are only tracked for RTT measurement purposes.
  unacked_packets_.AddPacket(CreateNonRetransmittablePacket(1));
  unacked_packets_.SetSent(1, now_, kDefaultAckLength, false);

  QuicPacketSequenceNumber unacked[] = { 1 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyInFlightPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);

  unacked_packets_.IncreaseLargestObserved(1);
  VerifyUnackedPackets(NULL, 0);
  VerifyInFlightPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);
}

TEST_F(QuicUnackedPacketMapTest, DiscardOldRttOnly) {
  // Acks are only tracked for RTT measurement purposes, and are discarded
  // when more than 200 accumulate.
  const size_t kNumUnackedPackets = 200;
  for (size_t i = 1; i < 400; ++i) {
    unacked_packets_.AddPacket(CreateNonRetransmittablePacket(i));
    unacked_packets_.SetSent(i, now_, kDefaultAckLength, false);
    unacked_packets_.RemoveObsoletePackets();
    EXPECT_EQ(min(i, kNumUnackedPackets),
              unacked_packets_.GetNumUnackedPacketsDebugOnly());
  }
}

TEST_F(QuicUnackedPacketMapTest, RetransmittableInflightAndRtt) {
  // Simulate a retransmittable packet being sent and acked.
  unacked_packets_.AddPacket(CreateRetransmittablePacket(1));
  unacked_packets_.SetSent(1, now_, kDefaultLength, true);

  QuicPacketSequenceNumber unacked[] = { 1 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyInFlightPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(unacked, arraysize(unacked));

  unacked_packets_.RemoveRetransmittability(1);
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyInFlightPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);

  unacked_packets_.IncreaseLargestObserved(1);
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyInFlightPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);

  unacked_packets_.RemoveFromInFlight(1);
  VerifyUnackedPackets(NULL, 0);
  VerifyInFlightPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);
}

TEST_F(QuicUnackedPacketMapTest, RetransmittedPacket) {
  // Simulate a retransmittable packet being sent, retransmitted, and the first
  // transmission being acked.
  unacked_packets_.AddPacket(CreateRetransmittablePacket(1));
  unacked_packets_.SetSent(1, now_, kDefaultLength, true);
  unacked_packets_.OnRetransmittedPacket(1, 2, LOSS_RETRANSMISSION);
  unacked_packets_.SetSent(2, now_, kDefaultLength, true);

  QuicPacketSequenceNumber unacked[] = { 1, 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyInFlightPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 2 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));

  unacked_packets_.RemoveRetransmittability(1);
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyInFlightPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);

  unacked_packets_.IncreaseLargestObserved(2);
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyInFlightPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(NULL, 0);

  unacked_packets_.RemoveFromInFlight(2);
  QuicPacketSequenceNumber unacked2[] = { 1 };
  VerifyUnackedPackets(unacked, arraysize(unacked2));
  VerifyInFlightPackets(unacked, arraysize(unacked2));
  VerifyRetransmittablePackets(NULL, 0);

  unacked_packets_.RemoveFromInFlight(1);
  VerifyUnackedPackets(NULL, 0);
  VerifyInFlightPackets(NULL, 0);
  VerifyRetransmittablePackets(NULL, 0);
}

TEST_F(QuicUnackedPacketMapTest, RetransmitThreeTimes) {
  // Simulate a retransmittable packet being sent and retransmitted twice.
  unacked_packets_.AddPacket(CreateRetransmittablePacket(1));
  unacked_packets_.SetSent(1, now_, kDefaultLength, true);
  unacked_packets_.AddPacket(CreateRetransmittablePacket(2));
  unacked_packets_.SetSent(2, now_, kDefaultLength, true);

  QuicPacketSequenceNumber unacked[] = { 1, 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyInFlightPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 1, 2 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));

  // Early retransmit 1 as 3 and send new data as 4.
  unacked_packets_.IncreaseLargestObserved(2);
  unacked_packets_.RemoveFromInFlight(2);
  unacked_packets_.RemoveRetransmittability(2);
  unacked_packets_.RemoveFromInFlight(1);
  unacked_packets_.OnRetransmittedPacket(1, 3, LOSS_RETRANSMISSION);
  unacked_packets_.SetSent(3, now_, kDefaultLength, true);
  unacked_packets_.AddPacket(CreateRetransmittablePacket(4));
  unacked_packets_.SetSent(4, now_, kDefaultLength, true);

  QuicPacketSequenceNumber unacked2[] = { 1, 3, 4 };
  VerifyUnackedPackets(unacked2, arraysize(unacked2));
  QuicPacketSequenceNumber pending2[] = { 3, 4, };
  VerifyInFlightPackets(pending2, arraysize(pending2));
  QuicPacketSequenceNumber retransmittable2[] = { 3, 4 };
  VerifyRetransmittablePackets(retransmittable2, arraysize(retransmittable2));

  // Early retransmit 3 (formerly 1) as 5, and remove 1 from unacked.
  unacked_packets_.IncreaseLargestObserved(4);
  unacked_packets_.RemoveFromInFlight(4);
  unacked_packets_.RemoveRetransmittability(4);
  unacked_packets_.OnRetransmittedPacket(3, 5, LOSS_RETRANSMISSION);
  unacked_packets_.SetSent(5, now_, kDefaultLength, true);
  unacked_packets_.AddPacket(CreateRetransmittablePacket(6));
  unacked_packets_.SetSent(6, now_, kDefaultLength, true);

  QuicPacketSequenceNumber unacked3[] = { 3, 5, 6 };
  VerifyUnackedPackets(unacked3, arraysize(unacked3));
  QuicPacketSequenceNumber pending3[] = { 3, 5, 6 };
  VerifyInFlightPackets(pending3, arraysize(pending3));
  QuicPacketSequenceNumber retransmittable3[] = { 5, 6 };
  VerifyRetransmittablePackets(retransmittable3, arraysize(retransmittable3));

  // Early retransmit 5 as 7 and ensure in flight packet 3 is not removed.
  unacked_packets_.IncreaseLargestObserved(6);
  unacked_packets_.RemoveFromInFlight(6);
  unacked_packets_.RemoveRetransmittability(6);
  unacked_packets_.OnRetransmittedPacket(5, 7, LOSS_RETRANSMISSION);
  unacked_packets_.SetSent(7, now_, kDefaultLength, true);

  QuicPacketSequenceNumber unacked4[] = { 3, 5, 7 };
  VerifyUnackedPackets(unacked4, arraysize(unacked4));
  QuicPacketSequenceNumber pending4[] = { 3, 5, 7 };
  VerifyInFlightPackets(pending4, arraysize(pending4));
  QuicPacketSequenceNumber retransmittable4[] = { 7 };
  VerifyRetransmittablePackets(retransmittable4, arraysize(retransmittable4));

  // Remove the older two transmissions from in flight.
  unacked_packets_.RemoveFromInFlight(3);
  unacked_packets_.RemoveFromInFlight(5);
  QuicPacketSequenceNumber pending5[] = { 7 };
  VerifyInFlightPackets(pending5, arraysize(pending5));

  // Now test ClearAllPreviousTransmissions, leaving one packet.
  unacked_packets_.ClearAllPreviousRetransmissions();
  QuicPacketSequenceNumber unacked5[] = { 7 };
  VerifyUnackedPackets(unacked5, arraysize(unacked5));
  QuicPacketSequenceNumber retransmittable5[] = { 7 };
  VerifyRetransmittablePackets(retransmittable5, arraysize(retransmittable5));
}

TEST_F(QuicUnackedPacketMapTest, RetransmitFourTimes) {
  // Simulate a retransmittable packet being sent and retransmitted twice.
  unacked_packets_.AddPacket(CreateRetransmittablePacket(1));
  unacked_packets_.SetSent(1, now_, kDefaultLength, true);
  unacked_packets_.AddPacket(CreateRetransmittablePacket(2));
  unacked_packets_.SetSent(2, now_, kDefaultLength, true);

  QuicPacketSequenceNumber unacked[] = { 1, 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyInFlightPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 1, 2 };
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));

  // Early retransmit 1 as 3.
  unacked_packets_.IncreaseLargestObserved(2);
  unacked_packets_.RemoveFromInFlight(2);
  unacked_packets_.RemoveRetransmittability(2);
  unacked_packets_.RemoveFromInFlight(1);
  unacked_packets_.OnRetransmittedPacket(1, 3, LOSS_RETRANSMISSION);
  unacked_packets_.SetSent(3, now_, kDefaultLength, true);

  QuicPacketSequenceNumber unacked2[] = { 1, 3 };
  VerifyUnackedPackets(unacked2, arraysize(unacked2));
  QuicPacketSequenceNumber pending2[] = { 3 };
  VerifyInFlightPackets(pending2, arraysize(pending2));
  QuicPacketSequenceNumber retransmittable2[] = { 3 };
  VerifyRetransmittablePackets(retransmittable2, arraysize(retransmittable2));

  // TLP 3 (formerly 1) as 4, and don't remove 1 from unacked.
  unacked_packets_.OnRetransmittedPacket(3, 4, TLP_RETRANSMISSION);
  unacked_packets_.SetSent(4, now_, kDefaultLength, true);
  unacked_packets_.AddPacket(CreateRetransmittablePacket(5));
  unacked_packets_.SetSent(5, now_, kDefaultLength, true);

  QuicPacketSequenceNumber unacked3[] = { 1, 3, 4, 5 };
  VerifyUnackedPackets(unacked3, arraysize(unacked3));
  QuicPacketSequenceNumber pending3[] = { 3, 4, 5 };
  VerifyInFlightPackets(pending3, arraysize(pending3));
  QuicPacketSequenceNumber retransmittable3[] = { 4, 5 };
  VerifyRetransmittablePackets(retransmittable3, arraysize(retransmittable3));

  // Early retransmit 4 as 6 and ensure in flight packet 3 is removed.
  unacked_packets_.IncreaseLargestObserved(5);
  unacked_packets_.RemoveFromInFlight(5);
  unacked_packets_.RemoveRetransmittability(5);
  unacked_packets_.RemoveFromInFlight(3);
  unacked_packets_.RemoveFromInFlight(4);
  unacked_packets_.OnRetransmittedPacket(4, 6, LOSS_RETRANSMISSION);
  unacked_packets_.SetSent(6, now_, kDefaultLength, true);

  QuicPacketSequenceNumber unacked4[] = { 4, 6 };
  VerifyUnackedPackets(unacked4, arraysize(unacked4));
  QuicPacketSequenceNumber pending4[] = { 6 };
  VerifyInFlightPackets(pending4, arraysize(pending4));
  QuicPacketSequenceNumber retransmittable4[] = { 6 };
  VerifyRetransmittablePackets(retransmittable4, arraysize(retransmittable4));
}

TEST_F(QuicUnackedPacketMapTest, RestoreInflight) {
  // Simulate a retransmittable packet being sent, retransmitted, and the first
  // transmission being acked.
  unacked_packets_.AddPacket(CreateRetransmittablePacket(1));
  unacked_packets_.SetSent(1, now_, kDefaultLength, true);
  unacked_packets_.OnRetransmittedPacket(1, 2, RTO_RETRANSMISSION);
  unacked_packets_.RemoveFromInFlight(1);
  unacked_packets_.SetSent(2, now_, kDefaultLength, true);

  QuicPacketSequenceNumber unacked[] = { 1, 2 };
  VerifyUnackedPackets(unacked, arraysize(unacked));
  QuicPacketSequenceNumber retransmittable[] = { 2 };
  VerifyInFlightPackets(retransmittable, arraysize(retransmittable));
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
  EXPECT_EQ(kDefaultLength, unacked_packets_.bytes_in_flight());

  // Simulate an F-RTO, and restore 1 to flight.
  unacked_packets_.RestoreInFlight(1);
  VerifyUnackedPackets(unacked, arraysize(unacked));
  VerifyInFlightPackets(unacked, arraysize(unacked));
  VerifyRetransmittablePackets(retransmittable, arraysize(retransmittable));
  EXPECT_EQ(2 * kDefaultLength, unacked_packets_.bytes_in_flight());
}

TEST_F(QuicUnackedPacketMapTest, SendWithGap) {
  // Simulate a retransmittable packet being sent, retransmitted, and the first
  // transmission being acked.
  unacked_packets_.AddPacket(CreateRetransmittablePacket(1));
  unacked_packets_.SetSent(1, now_, kDefaultLength, true);
  unacked_packets_.AddPacket(CreateRetransmittablePacket(3));
  unacked_packets_.SetSent(3, now_, kDefaultLength, true);
  unacked_packets_.OnRetransmittedPacket(1, 5, LOSS_RETRANSMISSION);
  unacked_packets_.SetSent(5, now_, kDefaultLength, true);

  EXPECT_EQ(1u, unacked_packets_.GetLeastUnacked());
  EXPECT_TRUE(unacked_packets_.IsUnacked(1));
  EXPECT_FALSE(unacked_packets_.IsUnacked(2));
  EXPECT_TRUE(unacked_packets_.IsUnacked(3));
  EXPECT_FALSE(unacked_packets_.IsUnacked(4));
  EXPECT_TRUE(unacked_packets_.IsUnacked(5));
  EXPECT_EQ(5u, unacked_packets_.largest_sent_packet());
}


}  // namespace
}  // namespace test
}  // namespace net
