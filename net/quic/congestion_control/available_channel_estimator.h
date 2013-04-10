// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on the inter arrival time of the received packets relative to the time
// those packets where sent we can estimate the available capacity of the
// channel.
// We can only use packet trains that are sent out faster than the acctual
// available channel capacity.
// Note 1: this is intended to be a temporary class created when you send out a
// channel probing burst. Once the last packet have arrived you ask it for an
// estimate.
// Note 2: During this phase we should not update the overuse detector.
#ifndef NET_QUIC_CONGESTION_CONTROL_AVAILABLE_CHANNEL_ESTIMATOR_H_
#define NET_QUIC_CONGESTION_CONTROL_AVAILABLE_CHANNEL_ESTIMATOR_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

enum NET_EXPORT_PRIVATE AvailableChannelEstimateState {
  kAvailableChannelEstimateUnknown = 0,
  kAvailableChannelEstimateUncertain = 1,
  kAvailableChannelEstimateGood = 2,
  kAvailableChannelEstimateSenderLimited = 3,
};

class NET_EXPORT_PRIVATE AvailableChannelEstimator {
 public:
  explicit AvailableChannelEstimator(
      QuicPacketSequenceNumber first_sequence_number,
      QuicTime first_send_time,
      QuicTime first_receive_time);

  // Update the statistics with each receive time, for every packet we get a
  // feedback message for.
  void OnIncomingFeedback(QuicPacketSequenceNumber sequence_number,
                          QuicByteCount packet_size,
                          QuicTime sent_time,
                          QuicTime receive_time);

  // Get the current estimated available channel capacity.
  // bandwidth_estimate is invalid if kAvailableChannelEstimateUnknown
  // is returned.
  AvailableChannelEstimateState GetAvailableChannelEstimate(
      QuicBandwidth* bandwidth_estimate) const;

 private:
  const QuicPacketSequenceNumber first_sequence_number_;
  const QuicTime first_send_time_;
  const QuicTime first_receive_time_;
  QuicPacketSequenceNumber last_incorporated_sequence_number_;
  QuicTime last_time_sent_;
  QuicTime last_receive_time_;
  int number_of_sequence_numbers_;
  QuicByteCount received_bytes_;
};

}  // namespace net
#endif  // NET_QUIC_CONGESTION_CONTROL_AVAILABLE_CHANNEL_ESTIMATOR_H_
