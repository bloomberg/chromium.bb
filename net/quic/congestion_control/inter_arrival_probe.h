// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Class that handle the initial probing phase of inter arrival congestion
// control.
#ifndef NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_PROBE_H_
#define NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_PROBE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/available_channel_estimator.h"
#include "net/quic/quic_bandwidth.h"

namespace net {

class NET_EXPORT_PRIVATE InterArrivalProbe {
 public:
  InterArrivalProbe();
  ~InterArrivalProbe();

  // Call every time a packet is sent to the network.
  void OnSentPacket(QuicByteCount bytes);

  // Call once for each sent packet that we receive an acknowledgement from
  // the peer for.
  void OnAcknowledgedPacket(QuicByteCount bytes);

  // Call to get the number of bytes that can be sent as part of this probe.
  QuicByteCount GetAvailableCongestionWindow();

  // Call once for each sent packet we receive a congestion feedback from the
  // peer for.
  // If a peer sends both and ack and feedback for a sent packet, both
  // OnAcknowledgedPacket and OnIncomingFeedback should be called.
  void OnIncomingFeedback(QuicPacketSequenceNumber sequence_number,
                          QuicByteCount bytes_sent,
                          QuicTime time_sent,
                          QuicTime time_received);

  // Returns false as long as we are probing, available_channel_estimate is
  // invalid during that time. When the probe is completed this function return
  // true and available_channel_estimate contains the estimate.
  bool GetEstimate(QuicBandwidth* available_channel_estimate);

 private:
  scoped_ptr<AvailableChannelEstimator> available_channel_estimator_;
  QuicPacketSequenceNumber first_sequence_number_;
  bool estimate_available_;
  QuicBandwidth available_channel_estimate_;
  QuicByteCount unacked_data_;

  DISALLOW_COPY_AND_ASSIGN(InterArrivalProbe);
};

}  // namespace net
#endif  // NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_PROBE_H_
