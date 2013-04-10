// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is a helper class to the inter arrival congestion control. It
// provide a signal to the inter arrival congestion control of the estimated
// state of our transport channel. The estimate is based on the inter arrival
// time of the received packets relative to the time those packets were sent;
// we can estimate the build up of buffers on the network before packets are
// lost.
//
// Note: this class is not thread-safe.

#ifndef NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_OVERUSE_DETECTOR_H_
#define NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_OVERUSE_DETECTOR_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

enum NET_EXPORT_PRIVATE RateControlRegion {
  kRateControlRegionUnknown = 0,
  kRateControlRegionUnderMax = 1,
  kRateControlRegionNearMax = 2
};

// Note: Order is important.
enum NET_EXPORT_PRIVATE BandwidthUsage {
  kBandwidthSteady = 0,
  kBandwidthUnderUsing = 1,
  kBandwidthDraining = 2,
  kBandwidthOverUsing = 3,
};

//  Normal state transition diagram
//
//         kBandwidthUnderUsing
//                  |
//                  |
//           kBandwidthSteady
//             |          ^
//             |          |
//   kBandwidthOverUsing  |
//             |          |
//             |          |
//          kBandwidthDraining
//
//  The above transitions is in normal operation, with extreme values we don't
//  enforce the state transitions, hence you could in extreme scenarios go
//  between any states.
//
//  kBandwidthSteady       When the packets arrive in the same pace as we sent
//                         them. In this state we can increase our send pace.
//
//  kBandwidthOverUsing    When the packets arrive slower than the pace we sent
//                         them. In this state we should decrease our send pace.
//                         When we enter into this state we will also get an
//                         estimate on how much delay we have built up. The
//                         reduction in send pace should be chosen to drain the
//                         built up delay within reasonable time.
//
//  kBandwidthUnderUsing   When the packets arrive faster than the pace we sent
//                         them. In this state another stream disappeared from
//                         a shared link leaving us more available bandwidth.
//                         In this state we should hold our pace to make sure we
//                         fully drain the buffers before we start increasing
//                         our send rate. We do this to avoid operating with
//                         semi-full buffers.
//
//  kBandwidthDraining     We can only be in this state after we have been in a
//                         overuse state. In this state we should hold our pace
//                         to make sure we fully drain the buffers before we
//                         start increasing our send rate. We do this to avoid
//                         operating with semi-full buffers.

class NET_EXPORT_PRIVATE InterArrivalOveruseDetector {
 public:
  InterArrivalOveruseDetector();

  // Update the statistics with the received delta times, call for every
  // received delta time. This function assumes that there is no re-orderings.
  // If multiple packets are sent at the same time (identical send_time)
  // last_of_send_time should be set to false for all but the last calls to
  // this function. If there is only one packet sent at a given time
  // last_of_send_time must be true.
  // received_delta is the time difference between receiving this packet and the
  // previously received packet.
  void OnAcknowledgedPacket(QuicPacketSequenceNumber sequence_number,
                            QuicTime send_time,
                            bool last_of_send_time,
                            QuicTime receive_time);

  // Get the current estimated state and update the estimated congestion delay.
  // |estimated_congestion_delay| will be updated with the estimated built up
  // buffer delay; it must not be NULL as it will be updated with the estimate.
  // Note 1: estimated_buffer_delay will only be valid when kBandwidthOverUsing
  //         is returned.
  // Note 2: it's assumed that the pacer lower its send pace to drain the
  //         built up buffer within reasonable time. The pacer should use the
  //         estimated_buffer_delay as a guidance on how much to back off.
  // Note 3: The absolute value of estimated_congestion_delay is less reliable
  //         than the state itself. It is also biased to low since we can't know
  //         how full the buffers are when the flow starts.
  BandwidthUsage GetState(QuicTime::Delta* estimated_congestion_delay);

 private:
  struct PacketGroup {
    PacketGroup()
        : send_time(QuicTime::Zero()),
          last_receive_time(QuicTime::Zero()) {
    }
    QuicTime send_time;
    QuicTime last_receive_time;
  };

  // Update the statistics with the absolute receive time relative to the
  // absolute send time.
  void UpdateSendReceiveTimeOffset(QuicTime::Delta offset);

  // Update the filter with this new data point.
  void UpdateFilter(QuicTime::Delta received_delta,
                    QuicTime::Delta sent_delta);

  // Update the estimate with this residual.
  void UpdateDeltaEstimate(QuicTime::Delta residual);

  // Estimate the state based on the slope of the changes.
  void DetectSlope(int64 sigma_delta);

  // Estimate the state based on the accumulated drift of the changes.
  void DetectDrift(int64 sigma_delta);

  // Current grouping of packets that were sent at the same time.
  PacketGroup current_packet_group_;
  // Grouping of packets that were sent at the same time, just before the
  // current_packet_group_ above.
  PacketGroup previous_packet_group_;
  // Sequence number of the last acknowledged packet.
  QuicPacketSequenceNumber last_sequence_number_;
  // Number of received delta times with unique send time.
  int num_of_deltas_;
  // Estimated accumulation of received delta times.
  // Note: Can be negative and can drift over time which is why we bias it
  // towards 0 and reset it given some triggers.
  QuicTime::Delta accumulated_deltas_;
  // Current running mean of our received delta times.
  int delta_mean_;
  // Current running variance of our received delta times.
  int64 delta_variance_;
  // Number of overuse signals currently triggered in this state.
  // Note: negative represent underuse.
  int delta_overuse_counter_;
  // State estimated by the delta times.
  BandwidthUsage delta_estimate_;
  // Number of overuse signals currently triggered in this state.
  // Note: negative represent underuse.
  int slope_overuse_counter_;
  // State estimated by the slope of the delta times.
  BandwidthUsage slope_estimate_;
  // Lowest offset between send and receive time ever received in this session.
  QuicTime::Delta send_receive_offset_;
  // Last received time difference between our normalized send and receive time.
  QuicTime::Delta estimated_congestion_delay_;

  DISALLOW_COPY_AND_ASSIGN(InterArrivalOveruseDetector);
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_OVERUSE_DETECTOR_H_
