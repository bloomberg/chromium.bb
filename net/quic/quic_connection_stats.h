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
// 1. (C/S)HLO retransmission count.
// 2. SHLO received to first stream packet processed time.
// 3. CHLO sent to SHLO received time.
// 4. Number of migrations.
// 5. Number of out of order packets.
// 6. Number of connections that require more that 1-RTT.
// 7. Avg number of streams / session.
// 8. Number of duplicates received.
// 9. Fraction of data transferred that was padding.

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

  uint32 packets_revived;
  uint32 packets_dropped;  // duplicate or less than least unacked.
  uint32 rto_count;

  uint32 rtt;  // In microseconds
  uint64 estimated_bandwidth;
  // TODO(satyamshekhar): Add window_size, mss and mtu.
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONNECTION_STATS_H_
