// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/cubic.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "net/quic/congestion_control/cube_root.h"
#include "net/quic/quic_protocol.h"

using std::max;

namespace net {

namespace {
// Constants based on TCP defaults.
// The following constants are in 2^10 fractions of a second instead of ms to
// allow a 10 shift right to divide.
const int kCubeScale = 40;  // 1024*1024^3 (first 1024 is from 0.100^3)
                            // where 0.100 is 100 ms which is the scaling
                            // round trip time.
const int kCubeCongestionWindowScale = 410;
const uint64 kCubeFactor = (GG_UINT64_C(1) << kCubeScale) /
    kCubeCongestionWindowScale;
const uint32 kBetaSPDY = 939;  // Back off factor after loss for SPDY, reduces
                               // the CWND by 1/12th.
const uint32 kBetaLastMax = 871;  // Additional back off factor after loss for
                                  // the stored max value.
}  // namespace

Cubic::Cubic(const QuicClock* clock)
    : clock_(clock),
      epoch_(QuicTime::Zero()),
      last_update_time_(QuicTime::Zero()) {
  Reset();
}

void Cubic::Reset() {
  epoch_ = QuicTime::Zero();  // Reset time.
  last_update_time_ = QuicTime::Zero();  // Reset time.
  last_congestion_window_ = 0;
  last_max_congestion_window_ = 0;
  acked_packets_count_ = 0;
  estimated_tcp_congestion_window_ = 0;
  origin_point_congestion_window_ = 0;
  time_to_origin_point_ = 0;
  last_target_congestion_window_ = 0;
}

QuicTcpCongestionWindow Cubic::CongestionWindowAfterPacketLoss(
    QuicTcpCongestionWindow current_congestion_window) {
  if (current_congestion_window < last_max_congestion_window_) {
    // We never reached the old max, so assume we are competing with another
    // flow. Use our extra back off factor to allow the other flow to go up.
    last_max_congestion_window_ =
        (kBetaLastMax * current_congestion_window) >> 10;
  } else {
    last_max_congestion_window_ = current_congestion_window;
  }
  epoch_ = QuicTime::Zero();  // Reset time.
  return (current_congestion_window * kBetaSPDY) >> 10;
}

QuicTcpCongestionWindow Cubic::CongestionWindowAfterAck(
    QuicTcpCongestionWindow current_congestion_window,
    QuicTime::Delta delay_min) {
  acked_packets_count_ += 1;  // Packets acked.
  QuicTime current_time = clock_->ApproximateNow();

  // Cubic is "independent" of RTT, the update is limited by the time elapsed.
  if (last_congestion_window_ == current_congestion_window &&
      (current_time.Subtract(last_update_time_) <= MaxCubicTimeInterval())) {
    return max(last_target_congestion_window_,
               estimated_tcp_congestion_window_);
  }
  last_congestion_window_ = current_congestion_window;
  last_update_time_ = current_time;

  if (!epoch_.IsInitialized()) {
    // First ACK after a loss event.
    DVLOG(1) << "Start of epoch";
    epoch_ = current_time;  // Start of epoch.
    acked_packets_count_ = 1;  // Reset count.
    // Reset estimated_tcp_congestion_window_ to be in sync with cubic.
    estimated_tcp_congestion_window_ = current_congestion_window;
    if (last_max_congestion_window_ <= current_congestion_window) {
      time_to_origin_point_ = 0;
      origin_point_congestion_window_ = current_congestion_window;
    } else {
      time_to_origin_point_ = CubeRoot::Root(kCubeFactor *
          (last_max_congestion_window_ - current_congestion_window));
      origin_point_congestion_window_ =
          last_max_congestion_window_;
    }
  }
  // Change the time unit from microseconds to 2^10 fractions per second. Take
  // the round trip time in account. This is done to allow us to use shift as a
  // divide operator.
  int64 elapsed_time =
      (current_time.Add(delay_min).Subtract(epoch_).ToMicroseconds() << 10) /
      base::Time::kMicrosecondsPerSecond;

  int64 offset = time_to_origin_point_ - elapsed_time;
  QuicTcpCongestionWindow delta_congestion_window = (kCubeCongestionWindowScale
      * offset * offset * offset) >> kCubeScale;

  QuicTcpCongestionWindow target_congestion_window =
      origin_point_congestion_window_ - delta_congestion_window;

  // We have a new cubic congestion window.
  last_target_congestion_window_ = target_congestion_window;

  // Update estimated TCP congestion_window.
  // Note: we do a normal Reno congestion avoidance calculation not the
  // calculation described in section 3.3 TCP-friendly region of the document.
  while (acked_packets_count_ >= estimated_tcp_congestion_window_) {
    acked_packets_count_ -= estimated_tcp_congestion_window_;
    estimated_tcp_congestion_window_++;
  }
  // Compute target congestion_window based on cubic target and estimated TCP
  // congestion_window, use highest (fastest).
  if (target_congestion_window < estimated_tcp_congestion_window_) {
    target_congestion_window = estimated_tcp_congestion_window_;
  }
  DVLOG(1) << "Target congestion_window:" << target_congestion_window;
  return target_congestion_window;
}

}  // namespace net
