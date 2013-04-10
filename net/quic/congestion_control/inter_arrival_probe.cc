// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/inter_arrival_probe.h"

#include "base/basictypes.h"
#include "base/logging.h"

namespace {
const int kProbeSizePackets = 10;
const net::QuicByteCount kMinPacketSize = 500;
const int64 kDefaultBytesPerSecond = 40000;
const float kUncertainScaleFactor = 0.5;  // TODO(pwestin): revisit this factor.
}

namespace net {

InterArrivalProbe::InterArrivalProbe()
    : estimate_available_(false),
      available_channel_estimate_(QuicBandwidth::Zero()),
      unacked_data_(0) {
}

InterArrivalProbe::~InterArrivalProbe() {
}

bool InterArrivalProbe::GetEstimate(QuicBandwidth* available_channel_estimate) {
  if (!estimate_available_) {
    return false;
  }
  *available_channel_estimate = available_channel_estimate_;
  return true;
}

void InterArrivalProbe::OnSentPacket(QuicByteCount bytes) {
  if (!estimate_available_) {
    unacked_data_ += bytes;
  }
}

void InterArrivalProbe::OnAcknowledgedPacket(QuicByteCount bytes) {
  if (!estimate_available_) {
    DCHECK_LE(bytes, unacked_data_);
    unacked_data_ -= bytes;
  }
}

QuicByteCount InterArrivalProbe::GetAvailableCongestionWindow() {
  if (estimate_available_) {
    return 0;
  }
  return (kProbeSizePackets * kMaxPacketSize) - unacked_data_;
}

void InterArrivalProbe::OnIncomingFeedback(
    QuicPacketSequenceNumber sequence_number,
    QuicByteCount bytes_sent,
    QuicTime time_sent,
    QuicTime time_received) {
  if (estimate_available_) {
    return;
  }

  if (available_channel_estimator_.get() == NULL) {
    if (bytes_sent < kMinPacketSize) {
      // Packet too small to start the probe phase.
      return;
    }
    first_sequence_number_ = sequence_number;
    available_channel_estimator_.reset(new AvailableChannelEstimator(
        sequence_number, time_sent, time_received));
    return;
  }

  available_channel_estimator_->OnIncomingFeedback(sequence_number,
                                                   bytes_sent,
                                                   time_sent,
                                                   time_received);
  if (sequence_number < kProbeSizePackets - 1  + first_sequence_number_) {
    // We need more feedback before we have a probe estimate.
    return;
  }
  // Get the current estimated available channel capacity.
  // available_channel_estimate is invalid if kAvailableChannelEstimateUnknown
  // is returned.
  QuicBandwidth available_channel_estimate = QuicBandwidth::Zero();
  AvailableChannelEstimateState available_channel_estimate_state =
      available_channel_estimator_->GetAvailableChannelEstimate(
          &available_channel_estimate);
  switch (available_channel_estimate_state) {
    case kAvailableChannelEstimateUnknown:
      // Backup when we miss our probe.
      available_channel_estimate_ =
          QuicBandwidth::FromBytesPerSecond(kDefaultBytesPerSecond);
      break;
    case kAvailableChannelEstimateUncertain:
      available_channel_estimate_ =
          available_channel_estimate.Scale(kUncertainScaleFactor);
      break;
    case kAvailableChannelEstimateGood:
      available_channel_estimate_ = available_channel_estimate;
      break;
    case kAvailableChannelEstimateSenderLimited:
      available_channel_estimate_ =
          std::max(available_channel_estimate,
                   QuicBandwidth::FromBytesPerSecond(kDefaultBytesPerSecond));
      break;
  }
  estimate_available_ = true;
  available_channel_estimator_.reset(NULL);
  DLOG(INFO) << "Probe estimate:"
             << available_channel_estimate_.ToKBitsPerSecond()
             << " Kbits/s";
}

}  // namespace net
