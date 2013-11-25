// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <cmath>

#include "base/logging.h"
#include "base/rand_util.h"
#include "net/quic/congestion_control/inter_arrival_overuse_detector.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

static const double kPi = 3.14159265;

namespace net {
namespace test {

class InterArrivalOveruseDetectorTest : public ::testing::Test {
 protected:
  InterArrivalOveruseDetectorTest();

  QuicTime::Delta GaussianRandom(QuicTime::Delta mean,
                                 QuicTime::Delta standard_deviation);

  int Run100000Samples(int packets_per_burst,
                       QuicTime::Delta mean,
                       QuicTime::Delta standard_deviation);

  int RunUntilOveruse(int packets_per_burst,
                      QuicTime::Delta mean,
                      QuicTime::Delta standard_deviation,
                      QuicTime::Delta drift_per_burst,
                      QuicTime::Delta *estimated_buffer_delay);

  int RunUntilSteady(int packets_per_burst,
                     QuicTime::Delta mean,
                     QuicTime::Delta standard_deviation,
                     QuicTime::Delta drift_per_burst);

  int RunUntilNotDraining(int packets_per_burst,
                          QuicTime::Delta mean,
                          QuicTime::Delta standard_deviation,
                          QuicTime::Delta drift_per_burst);

  int RunUntilUnderusing(int packets_per_burst,
                         QuicTime::Delta mean,
                         QuicTime::Delta standard_deviation,
                         QuicTime::Delta drift_per_burst);

  void RunXBursts(int bursts,
                  int packets_per_burst,
                  QuicTime::Delta mean,
                  QuicTime::Delta standard_deviation,
                  QuicTime::Delta drift_per_burst);

  QuicPacketSequenceNumber sequence_number_;
  MockClock send_clock_;
  MockClock receive_clock_;
  QuicTime::Delta drift_from_mean_;
  InterArrivalOveruseDetector overuse_detector_;
  unsigned int seed_;
};

InterArrivalOveruseDetectorTest::InterArrivalOveruseDetectorTest()
    : sequence_number_(1),
      drift_from_mean_(QuicTime::Delta::Zero()),
      seed_(1234) {
}

QuicTime::Delta InterArrivalOveruseDetectorTest::GaussianRandom(
    QuicTime::Delta mean,
    QuicTime::Delta standard_deviation) {
  // Creating a Normal distribution variable from two independent uniform
  // variables based on the Box-Muller transform.
  double uniform1 = base::RandDouble();
  double uniform2 = base::RandDouble();

  QuicTime::Delta random = QuicTime::Delta::FromMicroseconds(
      static_cast<int>(standard_deviation.ToMicroseconds() *
          sqrt(-2 * log(uniform1)) * cos(2 * kPi * uniform2))).
              Add(mean).Subtract(drift_from_mean_);
  if (random < QuicTime::Delta::Zero()) {
    // Don't do negative deltas.
    drift_from_mean_ = drift_from_mean_.Subtract(mean);
    return QuicTime::Delta::Zero();
  }
  drift_from_mean_ = drift_from_mean_.Add(random).Subtract(mean);
  return random;
}

int InterArrivalOveruseDetectorTest::Run100000Samples(
    int packets_per_burst,
    QuicTime::Delta mean,
    QuicTime::Delta standard_deviation) {
  int unique_overuse = 0;
  int last_overuse = -1;
  for (int i = 0; i < 100000; ++i) {
    // Assume that we send out the packets with perfect pacing.
    send_clock_.AdvanceTime(mean);
    QuicTime send_time = send_clock_.ApproximateNow();
    // Do only one random delta for all packets in a burst.
    receive_clock_.AdvanceTime(GaussianRandom(mean, standard_deviation));
    QuicTime receive_time = receive_clock_.ApproximateNow();
    for (int j = 0; j <= packets_per_burst; ++j) {
      overuse_detector_.OnAcknowledgedPacket(sequence_number_++,
                                             send_time,
                                             (j == packets_per_burst),
                                             receive_time);
    }
    // We expect to randomly hit a few false detects, count the unique
    // overuse events, hence not multiple signals in a row.
    QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
    if (kBandwidthOverUsing == overuse_detector_.GetState(
        &estimated_buffer_delay)) {
      if (last_overuse + 1 != i) {
        unique_overuse++;
      }
      last_overuse = i;
    }
  }
  return unique_overuse;
}

int InterArrivalOveruseDetectorTest::RunUntilOveruse(
    int packets_per_burst,
    QuicTime::Delta mean,
    QuicTime::Delta standard_deviation,
    QuicTime::Delta drift_per_burst,
    QuicTime::Delta *estimated_buffer_delay) {
  // Simulate a higher send pace, that is too high.
  for (int i = 0; i < 1000; ++i) {
    send_clock_.AdvanceTime(mean);
    QuicTime send_time = send_clock_.ApproximateNow();
    // Do only one random delta for all packets in a burst.
    receive_clock_.AdvanceTime(GaussianRandom(mean.Add(drift_per_burst),
                                              standard_deviation));
    QuicTime receive_time = receive_clock_.ApproximateNow();
    for (int j = 0; j <= packets_per_burst; ++j) {
      overuse_detector_.OnAcknowledgedPacket(sequence_number_++,
                                             send_time,
                                             (j == packets_per_burst),
                                             receive_time);
    }
    if (kBandwidthOverUsing == overuse_detector_.GetState(
        estimated_buffer_delay)) {
      return i + 1;
    }
  }
  return -1;
}

int InterArrivalOveruseDetectorTest::RunUntilSteady(
    int packets_per_burst,
    QuicTime::Delta mean,
    QuicTime::Delta standard_deviation,
    QuicTime::Delta drift_per_burst) {
  // Simulate a lower send pace, that is lower than the capacity.
  for (int i = 0; i < 1000; ++i) {
    send_clock_.AdvanceTime(mean);
    QuicTime send_time = send_clock_.ApproximateNow();
    // Do only one random delta for all packets in a burst.
    receive_clock_.AdvanceTime(GaussianRandom(mean.Subtract(drift_per_burst),
                                              standard_deviation));
    QuicTime receive_time = receive_clock_.ApproximateNow();
    for (int j = 0; j <= packets_per_burst; ++j) {
      overuse_detector_.OnAcknowledgedPacket(sequence_number_++,
                                             send_time,
                                             (j == packets_per_burst),
                                             receive_time);
    }
    QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
    if (kBandwidthSteady ==
        overuse_detector_.GetState(&estimated_buffer_delay)) {
      return i + 1;
    }
  }
  return -1;
}

int InterArrivalOveruseDetectorTest::RunUntilNotDraining(
    int packets_per_burst,
    QuicTime::Delta mean,
    QuicTime::Delta standard_deviation,
    QuicTime::Delta drift_per_burst) {
  // Simulate a lower send pace, that is lower than the capacity.
  for (int i = 0; i < 1000; ++i) {
    send_clock_.AdvanceTime(mean);
    QuicTime send_time = send_clock_.ApproximateNow();
    // Do only one random delta for all packets in a burst.
    receive_clock_.AdvanceTime(GaussianRandom(mean.Subtract(drift_per_burst),
                                              standard_deviation));
    QuicTime receive_time = receive_clock_.ApproximateNow();
    for (int j = 0; j <= packets_per_burst; ++j) {
      overuse_detector_.OnAcknowledgedPacket(sequence_number_++,
                                             send_time,
                                             (j == packets_per_burst),
                                             receive_time);
    }
    QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
    if (kBandwidthDraining >
        overuse_detector_.GetState(&estimated_buffer_delay)) {
      return i + 1;
    }
  }
  return -1;
}

int InterArrivalOveruseDetectorTest::RunUntilUnderusing(
    int packets_per_burst,
    QuicTime::Delta mean,
    QuicTime::Delta standard_deviation,
    QuicTime::Delta drift_per_burst) {
  // Simulate a lower send pace, that is lower than the capacity.
  for (int i = 0; i < 1000; ++i) {
    send_clock_.AdvanceTime(mean);
    QuicTime send_time = send_clock_.ApproximateNow();
    // Do only one random delta for all packets in a burst.
    receive_clock_.AdvanceTime(GaussianRandom(mean.Subtract(drift_per_burst),
                                              standard_deviation));
    QuicTime receive_time = receive_clock_.ApproximateNow();
    for (int j = 0; j <= packets_per_burst; ++j) {
      overuse_detector_.OnAcknowledgedPacket(sequence_number_++,
                                             send_time,
                                             (j == packets_per_burst),
                                             receive_time);
    }
    QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
    if (kBandwidthUnderUsing == overuse_detector_.GetState(
        &estimated_buffer_delay)) {
      return i + 1;
    }
  }
  return -1;
}

void InterArrivalOveruseDetectorTest::RunXBursts(
    int bursts,
    int packets_per_burst,
    QuicTime::Delta mean,
    QuicTime::Delta standard_deviation,
    QuicTime::Delta drift_per_burst) {
  for (int i = 0; i < bursts; ++i) {
    send_clock_.AdvanceTime(mean);
    QuicTime send_time = send_clock_.ApproximateNow();
    // Do only one random delta for all packets in a burst.
    receive_clock_.AdvanceTime(GaussianRandom(mean.Add(drift_per_burst),
                                              standard_deviation));
    QuicTime receive_time = receive_clock_.ApproximateNow();
    for (int j = 0; j <= packets_per_burst; ++j) {
      overuse_detector_.OnAcknowledgedPacket(sequence_number_++,
                                             send_time,
                                             (j == packets_per_burst),
                                             receive_time);
    }
  }
}

// TODO(pwestin): test packet loss impact on accuracy.
// TODO(pwestin): test colored noise by dropping late frames.

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_TestNoise) {
  int count[100];
  memset(count, 0, sizeof(count));
  for (int i = 0; i < 10000; ++i) {
    QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(30);
    QuicTime::Delta standard_deviation = QuicTime::Delta::FromMilliseconds(10);
    count[GaussianRandom(mean, standard_deviation).ToMilliseconds()]++;
  }
  for (int j = 0; j < 100; ++j) {
    DLOG(INFO) << j << ":" << count[j];
  }
}

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_SimpleNonOveruse) {
  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta delta = QuicTime::Delta::FromMilliseconds(10);

  for (int i = 0; i < 1000; ++i) {
    QuicTime send_time = send_clock_.ApproximateNow();
    QuicTime receive_time = receive_clock_.ApproximateNow();
    overuse_detector_.OnAcknowledgedPacket(sequence_number++,
                                           send_time,
                                           true,
                                           receive_time);
    send_clock_.AdvanceTime(delta);
    receive_clock_.AdvanceTime(delta);
    QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
    EXPECT_EQ(kBandwidthSteady,
              overuse_detector_.GetState(&estimated_buffer_delay));
  }
}

TEST_F(InterArrivalOveruseDetectorTest,
       DISABLED_SimpleNonOveruseSendClockAhead) {
  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta delta = QuicTime::Delta::FromMilliseconds(10);
  send_clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1234));

  for (int i = 0; i < 1000; ++i) {
    QuicTime send_time = send_clock_.ApproximateNow();
    QuicTime receive_time = receive_clock_.ApproximateNow();
    overuse_detector_.OnAcknowledgedPacket(sequence_number++,
                                           send_time,
                                           true,
                                           receive_time);
    send_clock_.AdvanceTime(delta);
    receive_clock_.AdvanceTime(delta);
    QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
    EXPECT_EQ(kBandwidthSteady,
              overuse_detector_.GetState(&estimated_buffer_delay));
  }
}

TEST_F(InterArrivalOveruseDetectorTest,
       DISABLED_SimpleNonOveruseSendClockBehind) {
  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta delta = QuicTime::Delta::FromMilliseconds(10);
  receive_clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1234));

  for (int i = 0; i < 1000; ++i) {
    QuicTime send_time = send_clock_.ApproximateNow();
    QuicTime receive_time = receive_clock_.ApproximateNow();
    overuse_detector_.OnAcknowledgedPacket(sequence_number++,
                                           send_time,
                                           true,
                                           receive_time);
    send_clock_.AdvanceTime(delta);
    receive_clock_.AdvanceTime(delta);
    QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
    EXPECT_EQ(kBandwidthSteady,
              overuse_detector_.GetState(&estimated_buffer_delay));
  }
}

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_SimpleNonOveruseWithVariance) {
  QuicPacketSequenceNumber sequence_number = 1;
  for (int i = 0; i < 1000; ++i) {
    if (i % 2) {
      receive_clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
    } else {
      receive_clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(15));
    }
    QuicTime send_time = send_clock_.ApproximateNow();
    QuicTime receive_time = receive_clock_.ApproximateNow();
    overuse_detector_.OnAcknowledgedPacket(sequence_number++,
                                           send_time,
                                           true,
                                           receive_time);
    send_clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(10));
    QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
    EXPECT_EQ(kBandwidthSteady,
              overuse_detector_.GetState(&estimated_buffer_delay));
  }
}

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_SimpleOveruse) {
  QuicPacketSequenceNumber sequence_number = 1;
  QuicTime::Delta send_delta = QuicTime::Delta::FromMilliseconds(10);
  QuicTime::Delta received_delta = QuicTime::Delta::FromMilliseconds(5);
  QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());

  for (int i = 0; i < 100000; ++i) {
    send_clock_.AdvanceTime(send_delta);
    receive_clock_.AdvanceTime(received_delta);
    QuicTime send_time = send_clock_.ApproximateNow();
    QuicTime receive_time = receive_clock_.ApproximateNow();
    // Sending 2 packets the same time as that is what we expect to do.
    overuse_detector_.OnAcknowledgedPacket(sequence_number++,
                                           send_time,
                                           false,
                                           receive_time);
    receive_clock_.AdvanceTime(received_delta);
    receive_time = receive_clock_.ApproximateNow();
    overuse_detector_.OnAcknowledgedPacket(sequence_number++,
                                           send_time,
                                           true,
                                           receive_time);

    EXPECT_EQ(kBandwidthSteady,
              overuse_detector_.GetState(&estimated_buffer_delay));
  }
  // Simulate a higher send pace, that is too high by receiving 1 millisecond
  // late per packet.
  received_delta = QuicTime::Delta::FromMilliseconds(6);
  send_clock_.AdvanceTime(send_delta);
  receive_clock_.AdvanceTime(received_delta);
  QuicTime send_time = send_clock_.ApproximateNow();
  QuicTime receive_time = receive_clock_.ApproximateNow();
  overuse_detector_.OnAcknowledgedPacket(sequence_number++,
                                         send_time,
                                         false,
                                         receive_time);
  receive_clock_.AdvanceTime(received_delta);
  receive_time = receive_clock_.ApproximateNow();
  overuse_detector_.OnAcknowledgedPacket(sequence_number++,
                                         send_time,
                                         true,
                                         receive_time);
  EXPECT_EQ(kBandwidthOverUsing,
            overuse_detector_.GetState(&estimated_buffer_delay));
}

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_LowVariance10Kbit) {
  int packets_per_burst = 1;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(1000);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMilliseconds(5);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMilliseconds(5);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a higher send pace, that is too high.
  // With current tuning we require 6 updates with 5 milliseconds before
  // detection.
  // Resulting in a minimal buffer build up of 30 milliseconds.
  QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
  int bursts_until_overuse = RunUntilOveruse(packets_per_burst,
                                             mean,
                                             standard_deviation,
                                             drift_per_burst,
                                             &estimated_buffer_delay);
  EXPECT_GE(6, bursts_until_overuse);
  EXPECT_NEAR(40, estimated_buffer_delay.ToMilliseconds(), 15);

  // After draining the buffers we are back in a normal state.
  int bursts_until_not_draining = RunUntilNotDraining(packets_per_burst,
                                                      mean,
                                                      standard_deviation,
                                                      drift_per_burst);
  EXPECT_GE(6, bursts_until_not_draining);

  // After draining the buffer additionally we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(7, bursts_until_underusing);
}

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_HighVariance10Kbit) {
  int packets_per_burst = 1;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(1000);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMilliseconds(50);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMilliseconds(50);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a higher send pace, that is too high.
  // With current tuning we require 6 updates with 50 milliseconds before
  // detection.
  // Resulting in a minimal buffer build up of 300 milliseconds.
  QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
  int bursts_until_overuse = RunUntilOveruse(packets_per_burst,
                                             mean,
                                             standard_deviation,
                                             drift_per_burst,
                                             &estimated_buffer_delay);
  EXPECT_GE(6, bursts_until_overuse);
  EXPECT_NEAR(400, estimated_buffer_delay.ToMilliseconds(), 150);

  // After draining the buffers we are back in a normal state.
  int bursts_until_not_draining = RunUntilNotDraining(packets_per_burst,
                                                      mean,
                                                      standard_deviation,
                                                      drift_per_burst);
  EXPECT_GE(6, bursts_until_not_draining);

  // After draining the buffer additionally we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(7, bursts_until_underusing);
}

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_LowVariance100Kbit) {
  int packets_per_burst = 1;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(100);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMilliseconds(5);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMilliseconds(5);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a higher send pace, that is too high.
  // With current tuning we require 6 updates with 5 milliseconds
  // before detection.
  // Resulting in a minimal buffer build up of 30 ms.
  QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
  int bursts_until_overuse = RunUntilOveruse(packets_per_burst,
                                             mean,
                                             standard_deviation,
                                             drift_per_burst,
                                             &estimated_buffer_delay);
  EXPECT_GE(6, bursts_until_overuse);
  EXPECT_NEAR(40, estimated_buffer_delay.ToMilliseconds(), 15);

  // Simulate an RTT of 100 ms. Hence overusing for additional 100 ms before
  // detection.
  RunXBursts(1,
             packets_per_burst,
             mean,
             standard_deviation,
             drift_per_burst);

  // After draining the buffers we are back in a normal state.
  int bursts_until_not_draining = RunUntilNotDraining(packets_per_burst,
                                                      mean,
                                                      standard_deviation,
                                                      drift_per_burst);
  EXPECT_GE(7, bursts_until_not_draining);

  // After draining the buffer additionally we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(5, bursts_until_underusing);
}

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_HighVariance100Kbit) {
  int packets_per_burst = 1;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(100);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMilliseconds(50);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMilliseconds(50);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a higher send pace, that is too high.
  // With current tuning we require 4 updates with 50 milliseconds
  // before detection.
  // Resulting in a minimal buffer build up of 200 ms.
  QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
  int bursts_until_overuse = RunUntilOveruse(packets_per_burst,
                                             mean,
                                             standard_deviation,
                                             drift_per_burst,
                                             &estimated_buffer_delay);
  EXPECT_GE(4, bursts_until_overuse);
  EXPECT_NEAR(300, estimated_buffer_delay.ToMilliseconds(), 150);

  // Simulate an RTT of 100 ms. Hence overusing for additional 100 ms before
  // detection.
  RunXBursts(1,
             packets_per_burst,
             mean,
             standard_deviation,
             drift_per_burst);

  // After draining the buffers we are back in a normal state.
  int bursts_until_not_draining = RunUntilNotDraining(packets_per_burst,
                                                      mean,
                                                      standard_deviation,
                                                      drift_per_burst);
  EXPECT_GE(5, bursts_until_not_draining);

  // After draining the buffer additionally we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(5, bursts_until_underusing);
}

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_HighVariance1Mbit) {
  int packets_per_burst = 1;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(10);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMilliseconds(5);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMilliseconds(5);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a higher send pace, that is too high.
  // With current tuning we require 4 updates with 5 millisecond
  // before detection.
  // Resulting in a minimal buffer build up of 20 ms.
  QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
  int bursts_until_overuse = RunUntilOveruse(packets_per_burst,
                                             mean,
                                             standard_deviation,
                                             drift_per_burst,
                                             &estimated_buffer_delay);
  EXPECT_GE(4, bursts_until_overuse);
  EXPECT_NEAR(30, estimated_buffer_delay.ToMilliseconds(), 15);

  // Simulate an RTT of 100 ms. Hence overusing for additional 100 ms before
  // detection.
  RunXBursts(10,
             packets_per_burst,
             mean,
             standard_deviation,
             drift_per_burst);

  // After draining the buffers we are back in a normal state.
  int bursts_until_not_draining = RunUntilNotDraining(packets_per_burst,
                                                      mean,
                                                      standard_deviation,
                                                      drift_per_burst);
  EXPECT_GE(14, bursts_until_not_draining);

  // After draining the buffer additionally we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(4, bursts_until_underusing);
}

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_LowVariance1Mbit) {
  int packets_per_burst = 1;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(10);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMilliseconds(1);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMilliseconds(1);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a higher send pace, that is too high.
  // With current tuning we require 6 updates with 1 millisecond
  // before detection.
  // Resulting in a minimal buffer build up of 6 ms.
  QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
  int bursts_until_overuse = RunUntilOveruse(packets_per_burst,
                                             mean,
                                             standard_deviation,
                                             drift_per_burst,
                                             &estimated_buffer_delay);
  EXPECT_GE(6, bursts_until_overuse);
  EXPECT_NEAR(8, estimated_buffer_delay.ToMilliseconds(), 3);

  // Simulate an RTT of 100 ms. Hence overusing for additional 100 ms before
  // detection.
  RunXBursts(10,
             packets_per_burst,
             mean,
             standard_deviation,
             drift_per_burst);

  // After draining the buffers we are back in a normal state.
  int bursts_until_not_draining = RunUntilNotDraining(packets_per_burst,
                                                      mean,
                                                      standard_deviation,
                                                      drift_per_burst);
  EXPECT_GE(16, bursts_until_not_draining);

  // After draining the buffer additionally we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(4, bursts_until_underusing);
}

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_HighVariance20Mbit) {
  int packets_per_burst = 2;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(1);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMicroseconds(500);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMicroseconds(500);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a higher send pace, that is too high.
  // With current tuning we require 4 updates with 500 microsecond
  // before detection.
  // Resulting in a minimal buffer build up of 2 ms.
  QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
  int bursts_until_overuse = RunUntilOveruse(packets_per_burst,
                                             mean,
                                             standard_deviation,
                                             drift_per_burst,
                                             &estimated_buffer_delay);
  EXPECT_GE(4, bursts_until_overuse);
  EXPECT_NEAR(3, estimated_buffer_delay.ToMilliseconds(), 2);

  // Simulate an RTT of 100 ms. Hence overusing for additional 100 ms before
  // detection.
  RunXBursts(100,
             packets_per_burst,
             mean,
             standard_deviation,
             drift_per_burst);

  // After draining the buffers we are back in a normal state.
  int bursts_until_not_draining = RunUntilNotDraining(packets_per_burst,
                                                      mean,
                                                      standard_deviation,
                                                      drift_per_burst);
  EXPECT_NEAR(100, bursts_until_not_draining, 10);

  // After draining the buffer additionally we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(1, bursts_until_underusing);
}

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_HighVariance100Mbit) {
  int packets_per_burst = 10;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(1);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMicroseconds(500);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMicroseconds(500);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a higher send pace, that is too high.
  // With current tuning we require 4 updates with 500 microsecond
  // before detection.
  // Resulting in a minimal buffer build up of 2 ms.
  QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
  int bursts_until_overuse = RunUntilOveruse(packets_per_burst,
                                             mean,
                                             standard_deviation,
                                             drift_per_burst,
                                             &estimated_buffer_delay);
  EXPECT_GE(4, bursts_until_overuse);
  EXPECT_NEAR(3, estimated_buffer_delay.ToMilliseconds(), 2);

  // Simulate an RTT of 100 ms. Hence overusing for additional 100 ms before
  // detection.
  RunXBursts(100,
             packets_per_burst,
             mean,
             standard_deviation,
             drift_per_burst);

  // After draining the buffers we are back in a normal state.
  int bursts_until_not_draining = RunUntilNotDraining(packets_per_burst,
                                                      mean,
                                                      standard_deviation,
                                                      drift_per_burst);
  EXPECT_NEAR(100, bursts_until_not_draining, 10);

  // After draining the buffer additionally we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(1, bursts_until_underusing);
}

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_VeryHighVariance100Mbit) {
  int packets_per_burst = 10;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(1);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMilliseconds(5);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMicroseconds(500);

  // We get false overuse in this scenario due to that the standard deviation is
  // higher than our mean and the fact that a delta time can't be negative. This
  // results in an under estimated standard deviation in the estimator causing
  // false detects.
  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(2000, overuse_signals);

  // Simulate a higher send pace, that is too high.
  // With current tuning we require 25 updates with 500 microsecond
  // before detection.
  // Resulting in a minimal buffer build up of 12.5 ms.
  QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
  int bursts_until_overuse = RunUntilOveruse(packets_per_burst,
                                             mean,
                                             standard_deviation,
                                             drift_per_burst,
                                             &estimated_buffer_delay);
  EXPECT_GE(17, bursts_until_overuse);
  EXPECT_NEAR(22, estimated_buffer_delay.ToMilliseconds(), 15);

  // Simulate an RTT of 100 ms. Hence overusing for additional 100 ms before
  // detection.
  RunXBursts(100,
             packets_per_burst,
             mean,
             standard_deviation,
             drift_per_burst);

  // After draining the buffers we are back in a normal state.
  int bursts_until_not_draining = RunUntilNotDraining(packets_per_burst,
                                                      mean,
                                                      standard_deviation,
                                                      drift_per_burst);
  EXPECT_GE(117, bursts_until_not_draining);

  // After draining the buffer additionally we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(1, bursts_until_underusing);
}

//
//  Tests simulating big drop in bitrate.
//

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_LowVariance1MbitTo100Kbit) {
  int packets_per_burst = 1;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(10);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMilliseconds(1);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMilliseconds(100);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a higher send pace, that is too high.
  // With current tuning we require 1 update with 100 millisecond
  // before detection.
  // Resulting in a minimal buffer build up of 100 ms.
  QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
  int bursts_until_overuse = RunUntilOveruse(packets_per_burst,
                                             mean,
                                             standard_deviation,
                                             drift_per_burst,
                                             &estimated_buffer_delay);
  EXPECT_GE(1, bursts_until_overuse);
  EXPECT_NEAR(104, estimated_buffer_delay.ToMilliseconds(), 3);

  // Back off 20% lower than estimate to drain.
  mean = QuicTime::Delta::FromMilliseconds(100);
  drift_per_burst = QuicTime::Delta::FromMilliseconds(20);

  // After draining the buffers we are back in a normal state.
  int bursts_until_not_draining = RunUntilNotDraining(packets_per_burst,
                                                      mean,
                                                      standard_deviation,
                                                      drift_per_burst);
  EXPECT_GE(5, bursts_until_not_draining);

  // After draining the buffer additionally we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(3, bursts_until_underusing);
}

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_HighVariance20MbitTo1Mbit) {
  int packets_per_burst = 2;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(1);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMicroseconds(500);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMilliseconds(10);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a higher send pace, that is too high.
  // With current tuning we require 1 update with 10 milliseconds
  // before detection.
  // Resulting in a minimal buffer build up of 10 ms.
  QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
  int bursts_until_overuse = RunUntilOveruse(packets_per_burst,
                                             mean,
                                             standard_deviation,
                                             drift_per_burst,
                                             &estimated_buffer_delay);
  EXPECT_GE(1, bursts_until_overuse);
  EXPECT_NEAR(12, estimated_buffer_delay.ToMilliseconds(), 2);

  // Back off 20% lower than estimate to drain.
  mean = QuicTime::Delta::FromMilliseconds(10);
  drift_per_burst = QuicTime::Delta::FromMilliseconds(2);

  // After draining the buffers we are back in a normal state.
  int bursts_until_not_draining = RunUntilNotDraining(packets_per_burst,
                                                      mean,
                                                      standard_deviation,
                                                      drift_per_burst);
  EXPECT_GE(5, bursts_until_not_draining);

  // After draining the buffer additionally we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(3, bursts_until_underusing);
}

//
//  Tests that we can detect slow drifts.
//

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_LowVariance1MbitSmallSteps) {
  int packets_per_burst = 1;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(10);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMilliseconds(1);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMicroseconds(100);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a higher send pace, that is too high.
  // With current tuning we require 41 updates with 100 microseconds before
  // detection.
  // Resulting in a minimal buffer build up of 4.1 ms.
  QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
  int bursts_until_overuse = RunUntilOveruse(packets_per_burst,
                                             mean,
                                             standard_deviation,
                                             drift_per_burst,
                                             &estimated_buffer_delay);
  EXPECT_GE(41, bursts_until_overuse);
  EXPECT_NEAR(7, estimated_buffer_delay.ToMilliseconds(), 3);

  // Simulate an RTT of 100 ms. Hence overusing for additional 100 ms before
  // detection.
  RunXBursts(10,
             packets_per_burst,
             mean,
             standard_deviation,
             drift_per_burst);

  // After draining the buffers we are back in a normal state.
  int bursts_until_not_draining = RunUntilNotDraining(packets_per_burst,
                                                      mean,
                                                      standard_deviation,
                                                      drift_per_burst);
  EXPECT_GE(29, bursts_until_not_draining);

  // After draining the buffer additionally we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(71, bursts_until_underusing);
}

TEST_F(InterArrivalOveruseDetectorTest, DISABLED_LowVariance1MbitTinySteps) {
  int packets_per_burst = 1;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(10);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMilliseconds(1);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMicroseconds(10);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a higher send pace, that is too high.
  // With current tuning we require 345 updates with 10 microseconds before
  // detection.
  // Resulting in a minimal buffer build up of 3.45 ms.
  QuicTime::Delta estimated_buffer_delay(QuicTime::Delta::Zero());
  int bursts_until_overuse = RunUntilOveruse(packets_per_burst,
                                             mean,
                                             standard_deviation,
                                             drift_per_burst,
                                             &estimated_buffer_delay);
  EXPECT_GE(345, bursts_until_overuse);
  EXPECT_NEAR(7, estimated_buffer_delay.ToMilliseconds(), 3);

  // Simulate an RTT of 100 ms. Hence overusing for additional 100 ms before
  // detection.
  RunXBursts(10,
             packets_per_burst,
             mean,
             standard_deviation,
             drift_per_burst);

  // After draining the buffers we are back in a normal state.
  int bursts_until_not_draining = RunUntilNotDraining(packets_per_burst,
                                                      mean,
                                                      standard_deviation,
                                                      drift_per_burst);
  EXPECT_GE(18, bursts_until_not_draining);

  // After draining the buffer additionally we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(683, bursts_until_underusing);
}

//
//  Tests simulating starting with full buffers.
//

TEST_F(InterArrivalOveruseDetectorTest,
       DISABLED_StartedWithFullBuffersHighVariance1Mbit) {
  int packets_per_burst = 1;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(10);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMilliseconds(5);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMilliseconds(5);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a lower send pace.
  // Draining the buffer until we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(6, bursts_until_underusing);

  // After draining the buffers we are back in a normal state.
  drift_per_burst = QuicTime::Delta::FromMilliseconds(0);
  int bursts_until_steady = RunUntilSteady(packets_per_burst,
                                           mean,
                                           standard_deviation,
                                           drift_per_burst);
  EXPECT_GE(6, bursts_until_steady);
}

TEST_F(InterArrivalOveruseDetectorTest,
       DISABLED_StartedWithFullBuffersHighVariance1MbitSlowDrift) {
  int packets_per_burst = 1;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(10);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMilliseconds(5);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMilliseconds(1);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a faster receive pace.
  // Draining the buffer until we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(21, bursts_until_underusing);

  // Simulate an RTT of 100 ms. Hence underusing for additional 100 ms before
  // detection.
  drift_per_burst = QuicTime::Delta::FromMilliseconds(-1);
  RunXBursts(10,
             packets_per_burst,
             mean,
             standard_deviation,
             drift_per_burst);

  // After draining the buffers we are back in a normal state.
  drift_per_burst = QuicTime::Delta::FromMilliseconds(0);
  int bursts_until_steady = RunUntilSteady(packets_per_burst,
                                           mean,
                                           standard_deviation,
                                           drift_per_burst);
  EXPECT_GE(4, bursts_until_steady);
}

TEST_F(InterArrivalOveruseDetectorTest,
       DISABLED_StartedWithFullBuffersLowVariance1Mbit) {
  int packets_per_burst = 1;
  QuicTime::Delta mean = QuicTime::Delta::FromMilliseconds(10);
  QuicTime::Delta standard_deviation = QuicTime::Delta::FromMilliseconds(1);
  QuicTime::Delta drift_per_burst = QuicTime::Delta::FromMilliseconds(1);

  int overuse_signals = Run100000Samples(packets_per_burst,
                                         mean,
                                         standard_deviation);
  EXPECT_GE(1, overuse_signals);

  // Simulate a lower send pace.
  // Draining the buffer until we detect an underuse.
  int bursts_until_underusing = RunUntilUnderusing(packets_per_burst,
                                                   mean,
                                                   standard_deviation,
                                                   drift_per_burst);
  EXPECT_GE(5, bursts_until_underusing);

  // Simulate an RTT of 100 ms. Hence underusing for additional 100 ms before
  // detection.
  drift_per_burst = QuicTime::Delta::FromMilliseconds(-1);
  RunXBursts(10,
             packets_per_burst,
             mean,
             standard_deviation,
             drift_per_burst);

  // After draining the buffers we are back in a normal state.
  drift_per_burst = QuicTime::Delta::FromMilliseconds(0);
  int bursts_until_steady = RunUntilSteady(packets_per_burst,
                                           mean,
                                           standard_deviation,
                                           drift_per_burst);
  EXPECT_GE(41, bursts_until_steady);
}

}  // namespace test
}  // namespace net
