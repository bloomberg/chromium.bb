// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/timestamp_receiver.h"

#include <utility>

#include "base/basictypes.h"

namespace net {

TimestampReceiver::TimestampReceiver() {
}

TimestampReceiver::~TimestampReceiver() {
}

bool TimestampReceiver::GenerateCongestionFeedback(
    QuicCongestionFeedbackFrame* feedback) {
  if (received_packet_times_.size() <= 1) {
    // Don't waste resources by sending a feedback frame for only one packet.
    return false;
  }
  feedback->type = kTimestamp;

  // Copy our current receive set to our feedback message, we will not resend
  // this data if it is lost.
  feedback->timestamp.received_packet_times = received_packet_times_;

  // Prepare for the next set of arriving packets by clearing our current set.
  received_packet_times_.clear();
  return true;
}

void TimestampReceiver::RecordIncomingPacket(
    QuicByteCount /*bytes*/,
    QuicPacketSequenceNumber sequence_number,
    QuicTime timestamp) {
  received_packet_times_.insert(std::make_pair(sequence_number, timestamp));
}

}  // namespace net
