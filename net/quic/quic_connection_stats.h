// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CONNECTION_STATS_H_
#define NET_QUIC_QUIC_CONNECTION_STATS_H_

#include <ostream>

#include "base/basictypes.h"
#include "net/base/net_export.h"

namespace net {
// TODO(satyamshekhar): Add more interesting stats:
// 1. SHLO received to first stream packet processed time.
// 2. CHLO sent to SHLO received time.
// 3. Number of migrations.
// 4. Number of out of order packets.
// 5. Number of connections that require more that 1-RTT.
// 6. Avg number of streams / session.
// 7. Number of duplicates received.

// Structure to hold stats for a QuicConnection.
struct NET_EXPORT_PRIVATE QuicConnectionStats {
  QuicConnectionStats();
  ~QuicConnectionStats();

  NET_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os, const QuicConnectionStats& s);

  uint64 bytes_sent;  // includes retransmissions, fec.
  uint32 packets_sent;
  uint64 stream_bytes_sent;  // non-retransmitted bytes sent in a stream frame.

  uint64 bytes_received;  // includes duplicate data for a stream, fec.
  uint32 packets_received;  // includes dropped packets
  uint64 stream_bytes_received;  // bytes received in a stream frame.

  uint64 bytes_retransmitted;
  uint32 packets_retransmitted;

  uint32 packets_spuriously_retransmitted;
  uint32 packets_lost;
  uint32 slowstart_packets_lost;  // Number of packets lost exiting slow start.

  uint32 packets_revived;
  uint32 packets_dropped;  // duplicate or less than least unacked.
  uint32 crypto_retransmit_count;
  // Count of times the loss detection alarm fired.  At least one packet should
  // be lost when the alarm fires.
  uint32 loss_timeout_count;
  uint32 tlp_count;
  uint32 rto_count;  // Count of times the rto timer fired.

  uint32 min_rtt_us;  // Minimum RTT in microseconds.
  uint32 srtt_us;  // Smoothed RTT in microseconds.
  uint64 estimated_bandwidth;  // In bytes per second.

  // Reordering stats for received packets.
  // Number of packets received out of sequence number order.
  uint32 packets_reordered;
  // Maximum reordering observed in sequence space.
  uint32 max_sequence_reordering;
  // Maximum reordering observed in microseconds
  uint32 max_time_reordering_us;

  // The following stats are used only in TcpCubicSender.
  // The number of loss events from TCP's perspective.  Each loss event includes
  // one or more lost packets.
  uint32 tcp_loss_events;
  // Total amount of cwnd increase by TCPCubic in congestion avoidance.
  uint32 cwnd_increase_congestion_avoidance;
  // Total amount of cwnd increase by TCPCubic in cubic mode.
  uint32 cwnd_increase_cubic_mode;
  // TODO(satyamshekhar): Add window_size, mss and mtu.
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONNECTION_STATS_H_
