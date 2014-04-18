// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class is a helper class to TcpCubicSender.
// Slow start is the initial startup phase of TCP, it lasts until first packet
// loss. This class implements hybrid slow start of the TCP cubic send side
// congestion algorithm. The key feaure of hybrid slow start is that it tries to
// avoid running into the wall too hard during the slow start phase, which
// the traditional TCP implementation does.
// http://netsrv.csc.ncsu.edu/export/hybridstart_pfldnet08.pdf
// http://research.csc.ncsu.edu/netsrv/sites/default/files/hystart_techreport_2008.pdf

#ifndef NET_QUIC_CONGESTION_CONTROL_HYBRID_SLOW_START_H_
#define NET_QUIC_CONGESTION_CONTROL_HYBRID_SLOW_START_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

class RttStats;

class NET_EXPORT_PRIVATE HybridSlowStart {
 public:
  explicit HybridSlowStart(const QuicClock* clock);

  void OnPacketAcked(QuicPacketSequenceNumber acked_sequence_number,
                     bool in_slow_start);

  void OnPacketSent(QuicPacketSequenceNumber sequence_number,
                    QuicByteCount available_send_window);

  bool ShouldExitSlowStart(const RttStats* rtt_stats,
                           int64 congestion_window);

  // Start a new slow start phase.
  void Restart();

  // TODO(ianswett): The following methods should be private, but that requires
  // a follow up CL to update the unit test.
  // Returns true if this ack the last sequence number of our current slow start
  // round.
  // Call Reset if this returns true.
  bool IsEndOfRound(QuicPacketSequenceNumber ack) const;

  // Call for each round (burst) in the slow start phase.
  void Reset(QuicPacketSequenceNumber end_sequence_number);

  // rtt: it the RTT for this ack packet.
  // min_rtt: is the lowest delay (RTT) we have seen during the session.
  // Returns true if slow start should be exited early, false otherwise.
  bool UpdateAndMaybeExit(QuicTime::Delta rtt, QuicTime::Delta min_rtt);

  // Whether slow start has started.
  bool started() const {
    return started_;
  }

 private:
  const QuicClock* clock_;
  // Whether the hybrid slow start has been started.
  bool started_;
  bool found_ack_train_;
  bool found_delay_;
  QuicTime round_start_;  // Beginning of each slow start round.
  // We need to keep track of the end sequence number of each RTT "burst".
  bool update_end_sequence_number_;
  // TODO(ianswett): This should be redundant to the above, but was moved
  // from TcpCubicSender to ensure the unit tests continued to pass.
  QuicPacketSequenceNumber sender_end_sequence_number_;
  QuicPacketSequenceNumber end_sequence_number_;  // End of slow start round.
  // Last time when the spacing between ack arrivals was less than 2 ms.
  // Defaults to the beginning of the round.
  QuicTime last_close_ack_pair_time_;
  uint32 rtt_sample_count_;  // Number of rtt samples in the current round.
  QuicTime::Delta current_min_rtt_;  // The minimum rtt of current round.

  DISALLOW_COPY_AND_ASSIGN(HybridSlowStart);
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_HYBRID_SLOW_START_H_
