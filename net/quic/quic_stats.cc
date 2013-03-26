// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stats.h"

namespace net {

QuicConnectionStats::QuicConnectionStats()
    : bytes_sent(0),
      packets_sent(0),
      bytes_received(0),
      packets_received(0),
      bytes_retransmitted(0),
      packets_retransmitted(0),
      packets_revived(0),
      packets_dropped(0),
      rto_count(0),
      rtt(0),
      estimated_bandwidth(0) {
}

QuicConnectionStats::~QuicConnectionStats() {}

}  // namespace net
