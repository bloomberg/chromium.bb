// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_stats.h"

using std::ostream;

namespace net {

QuicConnectionStats::QuicConnectionStats()
    : bytes_sent(0),
      packets_sent(0),
      stream_bytes_sent(0),
      bytes_received(0),
      packets_received(0),
      stream_bytes_received(0),
      bytes_retransmitted(0),
      packets_retransmitted(0),
      packets_spuriously_retransmitted(0),
      packets_lost(0),
      packets_revived(0),
      packets_dropped(0),
      crypto_retransmit_count(0),
      loss_timeout_count(0),
      tlp_count(0),
      rto_count(0),
      rtt(0),
      estimated_bandwidth(0),
      cwnd_increase_cubic_mode(0),
      cwnd_increase_reno_mode(0) {
}

QuicConnectionStats::~QuicConnectionStats() {}

ostream& operator<<(ostream& os, const QuicConnectionStats& s) {
  os << "{ bytes sent: " << s.bytes_sent
     << ", packets sent:" << s.packets_sent
     << ", stream bytes sent: " << s.stream_bytes_sent
     << ", bytes received: " << s.bytes_received
     << ", packets received: " << s.packets_received
     << ", stream bytes received: " << s.stream_bytes_received
     << ", bytes retransmitted: " << s.bytes_retransmitted
     << ", packets retransmitted: " << s.packets_retransmitted
     << ", packets_spuriously_retransmitted: "
     << s.packets_spuriously_retransmitted
     << ", packets lost: " << s.packets_lost
     << ", packets revived: " << s.packets_revived
     << ", packets dropped:" << s.packets_dropped
     << ", crypto retransmit count: " << s.crypto_retransmit_count
     << ", rto count: " << s.rto_count
     << ", tlp count: " << s.tlp_count
     << ", rtt(us): " << s.rtt
     << ", estimated_bandwidth: " << s.estimated_bandwidth
     << ", amount of cwnd increase in TCPCubic, in cubic mode: "
     << s.cwnd_increase_cubic_mode
     << ", amount of cwnd increase in TCPCubic, matched by or in reno mode: "
     << s.cwnd_increase_reno_mode
     << "}\n";
  return os;
}

}  // namespace net
