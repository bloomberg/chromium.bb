// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/tcp_loss_algorithm.h"

#include "net/quic/quic_protocol.h"

namespace net {

namespace {
// TCP retransmits after 3 nacks.
static const size_t kNumberOfNacksBeforeRetransmission = 3;
}

TCPLossAlgorithm::TCPLossAlgorithm() { }

// Uses nack counts to decide when packets are lost.
SequenceNumberSet TCPLossAlgorithm::DetectLostPackets(
    const QuicUnackedPacketMap& unacked_packets,
    const QuicTime& time,
    QuicPacketSequenceNumber largest_observed,
    QuicTime::Delta srtt) {
  SequenceNumberSet lost_packets;

  for (QuicUnackedPacketMap::const_iterator it = unacked_packets.begin();
       it != unacked_packets.end() && it->first <= largest_observed; ++it) {
    if (!it->second.pending) {
      continue;
    }
    size_t num_nacks_needed = kNumberOfNacksBeforeRetransmission;
    // Check for early retransmit(RFC5827) when the last packet gets acked and
    // the there are fewer than 4 pending packets.
    // TODO(ianswett): Set a retransmission timer instead of losing the packet
    // and retransmitting immediately.
    if (it->second.retransmittable_frames &&
        unacked_packets.largest_sent_packet() == largest_observed) {
      num_nacks_needed = largest_observed - it->first;
    }

    if (it->second.nack_count < num_nacks_needed) {
      continue;
    }

    lost_packets.insert(it->first);
  }

  return lost_packets;
}

}  // namespace net
