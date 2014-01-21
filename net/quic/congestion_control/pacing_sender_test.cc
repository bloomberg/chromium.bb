// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/pacing_sender.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::StrictMock;

namespace net {
namespace test {

class PacingSenderTest : public ::testing::Test {
 protected:
  PacingSenderTest()
      : zero_time_(QuicTime::Delta::Zero()),
        infinite_time_(QuicTime::Delta::Infinite()),
        sequence_number_(1),
        mock_sender_(new StrictMock<MockSendAlgorithm>()),
        pacing_sender_(new PacingSender(mock_sender_,
                                        QuicTime::Delta::FromMilliseconds(1))) {
    // Pick arbitrary time.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(9));
  }

  virtual ~PacingSenderTest() {}

  void CheckPacketIsSentImmediately() {
    // In order for the packet to be sendable, the underlying sender must
    // permit it to be sent immediately.
    EXPECT_CALL(*mock_sender_, TimeUntilSend(clock_.Now(),
                                             NOT_RETRANSMISSION,
                                             HAS_RETRANSMITTABLE_DATA,
                                             NOT_HANDSHAKE))
        .WillOnce(Return(zero_time_));
    // Verify that the packet can be sent immediately.
    EXPECT_EQ(zero_time_,
              pacing_sender_->TimeUntilSend(clock_.Now(), NOT_RETRANSMISSION,
                                            HAS_RETRANSMITTABLE_DATA,
                                            NOT_HANDSHAKE));

    // Actually send the packet.
    EXPECT_CALL(*mock_sender_,
                OnPacketSent(clock_.Now(), sequence_number_, kMaxPacketSize,
                             NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA));
    pacing_sender_->OnPacketSent(clock_.Now(), sequence_number_++,
                                 kMaxPacketSize, NOT_RETRANSMISSION,
                                 HAS_RETRANSMITTABLE_DATA);
  }

  void CheckAckIsSentImmediately() {
    // In order for the ack to be sendable, the underlying sender must
    // permit it to be sent immediately.
    EXPECT_CALL(*mock_sender_, TimeUntilSend(clock_.Now(),
                                             NOT_RETRANSMISSION,
                                             NO_RETRANSMITTABLE_DATA,
                                             NOT_HANDSHAKE))
        .WillOnce(Return(zero_time_));
    // Verify that the ACK can be sent immediately.
    EXPECT_EQ(zero_time_,
              pacing_sender_->TimeUntilSend(clock_.Now(), NOT_RETRANSMISSION,
                                            NO_RETRANSMITTABLE_DATA,
                                            NOT_HANDSHAKE));

    // Actually send the packet.
    EXPECT_CALL(*mock_sender_,
                OnPacketSent(clock_.Now(), sequence_number_, kMaxPacketSize,
                             NOT_RETRANSMISSION, NO_RETRANSMITTABLE_DATA));
    pacing_sender_->OnPacketSent(clock_.Now(), sequence_number_++,
                                 kMaxPacketSize, NOT_RETRANSMISSION,
                                 NO_RETRANSMITTABLE_DATA);
  }

  void CheckPacketIsDelayed(QuicTime::Delta delay) {
    // In order for the packet to be sendable, the underlying sender must
    // permit it to be sent immediately.
    EXPECT_CALL(*mock_sender_, TimeUntilSend(clock_.Now(),
                                             NOT_RETRANSMISSION,
                                             HAS_RETRANSMITTABLE_DATA,
                                             NOT_HANDSHAKE))
        .WillOnce(Return(zero_time_));
    // Verify that the packet is delayed.
    EXPECT_EQ(delay.ToMicroseconds(),
              pacing_sender_->TimeUntilSend(clock_.Now(), NOT_RETRANSMISSION,
                                            HAS_RETRANSMITTABLE_DATA,
                                            NOT_HANDSHAKE).ToMicroseconds());
  }

  const QuicTime::Delta zero_time_;
  const QuicTime::Delta infinite_time_;
  MockClock clock_;
  QuicPacketSequenceNumber sequence_number_;
  StrictMock<MockSendAlgorithm>* mock_sender_;
  scoped_ptr<PacingSender> pacing_sender_;
};

TEST_F(PacingSenderTest, NoSend) {
  EXPECT_CALL(*mock_sender_, TimeUntilSend(clock_.Now(),
                                           NOT_RETRANSMISSION,
                                           HAS_RETRANSMITTABLE_DATA,
                                           NOT_HANDSHAKE))
      .WillOnce(Return(infinite_time_));
  EXPECT_EQ(infinite_time_,
            pacing_sender_->TimeUntilSend(clock_.Now(), NOT_RETRANSMISSION,
                                          HAS_RETRANSMITTABLE_DATA,
                                          NOT_HANDSHAKE));
}

TEST_F(PacingSenderTest, SendNow) {
  EXPECT_CALL(*mock_sender_, TimeUntilSend(clock_.Now(),
                                           NOT_RETRANSMISSION,
                                           HAS_RETRANSMITTABLE_DATA,
                                           NOT_HANDSHAKE))
      .WillOnce(Return(zero_time_));
  EXPECT_EQ(zero_time_,
            pacing_sender_->TimeUntilSend(clock_.Now(), NOT_RETRANSMISSION,
                                          HAS_RETRANSMITTABLE_DATA,
                                          NOT_HANDSHAKE));
}

TEST_F(PacingSenderTest, VariousSending) {
  // Configure bandwith of 1 packet per 2 ms, for which the pacing rate
  // will be 1 packet per 1 ms.
  EXPECT_CALL(*mock_sender_, BandwidthEstimate())
      .WillRepeatedly(Return(QuicBandwidth::FromBytesAndTimeDelta(
          kMaxPacketSize, QuicTime::Delta::FromMilliseconds(2))));

  // Send a whole pile of packets, and verify that they are not paced.
  for (int i = 0 ; i < 1000; ++i) {
    CheckPacketIsSentImmediately();
  }

  // Now update the RTT and verify that packets are actually paced.
  QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(1);
  EXPECT_CALL(*mock_sender_, UpdateRtt(rtt));
  pacing_sender_->UpdateRtt(rtt);

  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();

  // The first packet was a "make up", then we sent two packets "into the
  // future", so the delay should be 2.
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));

  // Wake up on time.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(2));
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));
  CheckAckIsSentImmediately();

  // Wake up late.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(4));
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsSentImmediately();
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));

  // Wake up too early.
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));

  // Wake up early, but after enough time has passed to permit a send.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
  CheckPacketIsSentImmediately();
  CheckPacketIsDelayed(QuicTime::Delta::FromMilliseconds(2));
}

}  // namespace test
}  // namespace net
