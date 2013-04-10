// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ramp up bitrate from a start point normally our "current_rate" as long as we
// have no packet loss or delay events.
// The first half of the ramp up curve follows a cubic function with its orgin
// at the estimated available bandwidth, onece the bitrate pass the halfway
// point between the estimated available bandwidth and the estimated max
// bandwidth it will follw a new cubic function with its orgin at the estimated
// max bandwidth.
#ifndef NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_BITRATE_RAMP_UP_H_
#define NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_BITRATE_RAMP_UP_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_time.h"

namespace net {

class NET_EXPORT_PRIVATE InterArrivalBitrateRampUp {
 public:
  explicit InterArrivalBitrateRampUp(const QuicClock* clock);

  // Call after a decision to lower the bitrate and after a probe.
  void Reset(QuicBandwidth current_rate,
             QuicBandwidth available_channel_estimate,
             QuicBandwidth channel_estimate);

  // Call everytime we get a new channel estimate.
  void UpdateChannelEstimate(QuicBandwidth channel_estimate);

  // Compute a new send pace to use.
  QuicBandwidth GetNewBitrate(QuicBandwidth sent_bitrate);

 private:
  uint32 CalcuateTimeToOriginPoint(QuicBandwidth rate_difference) const;

  static const QuicTime::Delta MaxCubicTimeInterval() {
    return QuicTime::Delta::FromMilliseconds(30);
  }

  const QuicClock* clock_;

  QuicBandwidth current_rate_;
  QuicBandwidth channel_estimate_;
  QuicBandwidth available_channel_estimate_;
  QuicBandwidth halfway_point_;

  // Time when this cycle started, after a Reset.
  QuicTime epoch_;

  // Time when we updated current_rate_.
  QuicTime last_update_time_;

  // Time to origin point of cubic function in 2^10 fractions of a second.
  uint32 time_to_origin_point_;

  DISALLOW_COPY_AND_ASSIGN(InterArrivalBitrateRampUp);
};

}  // namespace net
#endif  // NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_BITRATE_RAMP_UP_H_
