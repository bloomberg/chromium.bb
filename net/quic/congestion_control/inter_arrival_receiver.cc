// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/inter_arrival_receiver.h"

#include "base/basictypes.h"

namespace net {

InterArrivalReceiver::InterArrivalReceiver()
    : accumulated_number_of_recoverd_lost_packets_(0) {
}

InterArrivalReceiver::~InterArrivalReceiver() {
}

bool InterArrivalReceiver::GenerateCongestionFeedback(
    QuicCongestionFeedbackFrame* feedback) {
  if (received_packet_times_.size() <= 1) {
    // Don't waste resources by sending a feedback frame for only one packet.
    return false;
  }
  feedback->type = kInterArrival;
  feedback->inter_arrival.accumulated_number_of_lost_packets =
      accumulated_number_of_recoverd_lost_packets_;

  // Copy our current receive set to our feedback message, we will not resend
  // this data if it is lost.
  feedback->inter_arrival.received_packet_times = received_packet_times_;

  // Prepare for the next set of arriving packets by clearing our current set.
  received_packet_times_.clear();
  return true;
}

void InterArrivalReceiver::RecordIncomingPacket(
    QuicByteCount /*bytes*/,
    QuicPacketSequenceNumber sequence_number,
    QuicTime timestamp,
    bool revived) {
  if (revived) {
    ++accumulated_number_of_recoverd_lost_packets_;
  }
  received_packet_times_.insert(std::make_pair(sequence_number, timestamp));
}

}  // namespace net
