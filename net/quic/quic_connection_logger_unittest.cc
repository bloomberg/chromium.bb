// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_logger.h"

#include "net/quic/quic_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class QuicConnectionLoggerPeer {
 public:
  static size_t num_truncated_acks_sent(const QuicConnectionLogger& logger) {
    return logger.num_truncated_acks_sent_;
  }
};

class QuicConnectionLoggerTest : public ::testing::Test {
 protected:
  QuicConnectionLoggerTest() : logger_(net_log_) {}

  BoundNetLog net_log_;
  QuicConnectionLogger logger_;
};

TEST_F(QuicConnectionLoggerTest, TruncatedAcksSentNotChanged) {
  QuicAckFrame frame;
  logger_.OnFrameAddedToPacket(QuicFrame(&frame));
  EXPECT_EQ(0u, QuicConnectionLoggerPeer::num_truncated_acks_sent(logger_));

  for (QuicPacketSequenceNumber i = 0; i < 256; ++i) {
    frame.missing_packets.insert(i);
  }
  logger_.OnFrameAddedToPacket(QuicFrame(&frame));
  EXPECT_EQ(0u, QuicConnectionLoggerPeer::num_truncated_acks_sent(logger_));
}

TEST_F(QuicConnectionLoggerTest, TruncatedAcksSent) {
  QuicAckFrame frame;
  for (QuicPacketSequenceNumber i = 0; i < 512; i += 2) {
    frame.missing_packets.insert(i);
  }
  logger_.OnFrameAddedToPacket(QuicFrame(&frame));
  EXPECT_EQ(1u, QuicConnectionLoggerPeer::num_truncated_acks_sent(logger_));
}

}  // namespace test
}  // namespace net
