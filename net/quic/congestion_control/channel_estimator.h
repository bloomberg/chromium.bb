// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on the inter arrival time of the received packets relative to the time
// those packets where sent we can estimate the max capacity of the channel.
// We can only use packet pair that are sent out faster than the acctual
// channel capacity.
#ifndef NET_QUIC_CONGESTION_CONTROL_CHANNEL_ESTIMATOR_H_
#define NET_QUIC_CONGESTION_CONTROL_CHANNEL_ESTIMATOR_H_

#include <list>
#include <map>

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/quic_max_sized_map.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

enum ChannelEstimateState {
  kChannelEstimateUnknown = 0,
  kChannelEstimateUncertain = 1,
  kChannelEstimateGood = 2
};

class NET_EXPORT_PRIVATE ChannelEstimator {
 public:
  ChannelEstimator();
  ~ChannelEstimator();

  // This method should be called each time we acquire a receive time for a
  // packet we previously sent.  It calculates deltas between consecutive
  // receive times, and may use that to update the channel bandwidth estimate.
  void OnAcknowledgedPacket(QuicPacketSequenceNumber sequence_number,
                            QuicByteCount packet_size,
                            QuicTime send_time,
                            QuicTime receive_time);

  // Get the current estimated state and channel capacity.
  // Note: estimate will not be valid when kChannelEstimateUnknown is returned.
  ChannelEstimateState GetChannelEstimate(QuicBandwidth* estimate) const;

 private:
  void UpdateFilter(QuicTime::Delta received_delta, QuicByteCount size_delta,
                    QuicPacketSequenceNumber sequence_number);

  QuicPacketSequenceNumber last_sequence_number_;
  QuicTime last_send_time_;
  QuicTime last_receive_time_;
  QuicMaxSizedMap<QuicBandwidth, QuicPacketSequenceNumber>
      sorted_bitrate_estimates_;

  DISALLOW_COPY_AND_ASSIGN(ChannelEstimator);
};

}  // namespace net
#endif  // NET_QUIC_CONGESTION_CONTROL_CHANNEL_ESTIMATOR_H_
