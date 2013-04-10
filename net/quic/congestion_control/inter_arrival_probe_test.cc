// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/congestion_control/inter_arrival_probe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class InterArrivalProbeTest : public ::testing::Test {
 protected:
  InterArrivalProbeTest() : start_(QuicTime::Zero()) {
  }

  InterArrivalProbe probe_;
  QuicTime start_;
};

TEST_F(InterArrivalProbeTest, CongestionWindow) {
  for (size_t i = 0; i < 10; i++) {
    probe_.OnSentPacket(kMaxPacketSize);
    EXPECT_EQ((9 - i) * kMaxPacketSize, probe_.GetAvailableCongestionWindow());
  }
  probe_.OnAcknowledgedPacket(kMaxPacketSize);
  EXPECT_EQ(kMaxPacketSize, probe_.GetAvailableCongestionWindow());

  probe_.OnSentPacket(kMaxPacketSize);
  EXPECT_EQ(0u, probe_.GetAvailableCongestionWindow());
}

TEST_F(InterArrivalProbeTest, Estimate) {
  QuicPacketSequenceNumber sequence_number = 1;
  QuicByteCount bytes_sent = kMaxPacketSize;
  QuicTime time_received = start_.Add(QuicTime::Delta::FromMilliseconds(10));
  QuicTime time_sent = start_.Add(QuicTime::Delta::FromMilliseconds(1));
  QuicBandwidth available_channel_estimate = QuicBandwidth::Zero();

  for (size_t i = 0; i < 10; ++i) {
    EXPECT_FALSE(probe_.GetEstimate(&available_channel_estimate));

    probe_.OnIncomingFeedback(sequence_number++,
                              bytes_sent,
                              time_sent,
                              time_received);
    time_sent = time_sent.Add(QuicTime::Delta::FromMilliseconds(1));
    time_received = time_received.Add(QuicTime::Delta::FromMilliseconds(10));
  }
  EXPECT_TRUE(probe_.GetEstimate(&available_channel_estimate));
  EXPECT_EQ(kMaxPacketSize * 100,
            static_cast<uint64>(available_channel_estimate.ToBytesPerSecond()));
}

TEST_F(InterArrivalProbeTest, EstimateWithLoss) {
  QuicPacketSequenceNumber sequence_number = 1;
  QuicByteCount bytes_sent = kMaxPacketSize;
  QuicTime time_received = start_.Add(QuicTime::Delta::FromMilliseconds(10));
  QuicTime time_sent = start_.Add(QuicTime::Delta::FromMilliseconds(1));
  QuicBandwidth available_channel_estimate = QuicBandwidth::Zero();

  for (size_t i = 0; i < 6; ++i) {
    EXPECT_FALSE(probe_.GetEstimate(&available_channel_estimate));

    probe_.OnIncomingFeedback(sequence_number,
                              bytes_sent,
                              time_sent,
                              time_received);
    sequence_number += 2;
    time_sent = time_sent.Add(QuicTime::Delta::FromMilliseconds(1));
    time_received = time_received.Add(QuicTime::Delta::FromMilliseconds(10));
  }
  EXPECT_TRUE(probe_.GetEstimate(&available_channel_estimate));
  EXPECT_EQ(kMaxPacketSize * 50,
            static_cast<uint64>(available_channel_estimate.ToBytesPerSecond()));
}

}  // namespace test
}  // namespace net
