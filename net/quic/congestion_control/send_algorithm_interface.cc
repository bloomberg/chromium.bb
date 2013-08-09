// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/send_algorithm_interface.h"

#include "net/quic/congestion_control/cubic.h"
#include "net/quic/congestion_control/fix_rate_sender.h"
#include "net/quic/congestion_control/tcp_cubic_sender.h"

namespace net {

const bool kUseReno = false;
// TODO(ianswett): Increase the max congestion window once the RTO logic is
// improved, particularly in cases when RTT is larger than the RTO. b/10075719
// Maximum number of outstanding packets for tcp.
const QuicTcpCongestionWindow kMaxTcpCongestionWindow = 50;

// Factory for send side congestion control algorithm.
SendAlgorithmInterface* SendAlgorithmInterface::Create(
    const QuicClock* clock,
    CongestionFeedbackType type) {
  switch (type) {
    case kTCP:
      return new TcpCubicSender(clock, kUseReno, kMaxTcpCongestionWindow);
    case kInterArrival:
      break;  // TODO(pwestin) Implement.
    case kFixRate:
      return new FixRateSender(clock);
  }
  return NULL;
}

}  // namespace net
