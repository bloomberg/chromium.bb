// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/send_algorithm_interface.h"

#include "net/quic/congestion_control/fix_rate_sender.h"
#include "net/quic/congestion_control/inter_arrival_sender.h"
#include "net/quic/congestion_control/tcp_cubic_sender.h"
#include "net/quic/quic_protocol.h"

namespace net {

const bool kUseReno = false;

// Factory for send side congestion control algorithm.
SendAlgorithmInterface* SendAlgorithmInterface::Create(
    const QuicClock* clock,
    CongestionFeedbackType type,
    QuicConnectionStats* stats) {
  switch (type) {
    case kTCP:
      return new TcpCubicSender(clock, kUseReno, kMaxTcpCongestionWindow,
                                stats);
    case kInterArrival:
      return new InterArrivalSender(clock);
    case kFixRate:
      return new FixRateSender(clock);
  }
  return NULL;
}

}  // namespace net
