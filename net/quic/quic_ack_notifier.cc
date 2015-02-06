// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_ack_notifier.h"

#include <set>

#include "base/logging.h"
#include "base/stl_util.h"

using base::hash_map;
using std::make_pair;

namespace net {

QuicAckNotifier::DelegateInterface::DelegateInterface() {}

QuicAckNotifier::DelegateInterface::~DelegateInterface() {}

QuicAckNotifier::QuicAckNotifier(DelegateInterface* delegate)
    : delegate_(delegate),
      unacked_packets_(0),
      retransmitted_packet_count_(0),
      retransmitted_byte_count_(0) {
  DCHECK(delegate);
}

QuicAckNotifier::~QuicAckNotifier() {
}

void QuicAckNotifier::AddSequenceNumber(
    const QuicPacketSequenceNumber& sequence_number,
    int packet_payload_size) {
  ++unacked_packets_;
  DVLOG(1) << "AckNotifier waiting for packet: " << sequence_number;
}

bool QuicAckNotifier::OnAck(QuicPacketSequenceNumber sequence_number,
                            QuicTime::Delta delta_largest_observed) {
  if (unacked_packets_ <= 0) {
    LOG(DFATAL) << "Acked more packets than were tracked.";
    return true;
  }
  --unacked_packets_;
  if (IsEmpty()) {
    // We have seen all the sequence numbers we were waiting for, trigger
    // callback notification.
    delegate_->OnAckNotification(retransmitted_packet_count_,
                                 retransmitted_byte_count_,
                                 delta_largest_observed);
    return true;
  }
  return false;
}

void QuicAckNotifier::OnPacketRetransmitted(int packet_payload_size) {
  ++retransmitted_packet_count_;
  retransmitted_byte_count_ += packet_payload_size;
}

};  // namespace net
