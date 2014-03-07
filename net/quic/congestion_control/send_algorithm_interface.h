// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The pure virtual class for send side congestion control algorithm.

#ifndef NET_QUIC_CONGESTION_CONTROL_SEND_ALGORITHM_INTERFACE_H_
#define NET_QUIC_CONGESTION_CONTROL_SEND_ALGORITHM_INTERFACE_H_

#include <algorithm>
#include <map>

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_connection_stats.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

class RttStats;

class NET_EXPORT_PRIVATE SendAlgorithmInterface {
 public:
  static SendAlgorithmInterface* Create(const QuicClock* clock,
                                        const RttStats* rtt_stats,
                                        CongestionFeedbackType type,
                                        QuicConnectionStats* stats);

  virtual ~SendAlgorithmInterface() {}

  virtual void SetFromConfig(const QuicConfig& config, bool is_server) = 0;

  // Called when we receive congestion feedback from remote peer.
  virtual void OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& feedback,
      QuicTime feedback_receive_time) = 0;

  // Called for each received ACK, with sequence number from remote peer.
  virtual void OnPacketAcked(QuicPacketSequenceNumber acked_sequence_number,
                             QuicByteCount acked_bytes) = 0;

  // Indicates a loss event of one packet. |sequence_number| is the
  // sequence number of the lost packet.
  virtual void OnPacketLost(QuicPacketSequenceNumber sequence_number,
                            QuicTime ack_receive_time) = 0;

  // Inform that we sent x bytes to the wire, and if that was a retransmission.
  // Returns true if the packet should be tracked by the congestion manager,
  // false otherwise. This is used by implementations such as tcp_cubic_sender
  // that do not count outgoing ACK packets against the congestion window.
  // Note: this function must be called for every packet sent to the wire.
  virtual bool OnPacketSent(QuicTime sent_time,
                            QuicPacketSequenceNumber sequence_number,
                            QuicByteCount bytes,
                            TransmissionType transmission_type,
                            HasRetransmittableData is_retransmittable) = 0;

  // Called when the retransmission timeout fires.  Neither OnPacketAbandoned
  // nor OnPacketLost will be called for these packets.
  virtual void OnRetransmissionTimeout(bool packets_retransmitted) = 0;

  // Called when a packet is timed out.
  virtual void OnPacketAbandoned(QuicPacketSequenceNumber sequence_number,
                                QuicByteCount abandoned_bytes) = 0;

  // Calculate the time until we can send the next packet.
  virtual QuicTime::Delta TimeUntilSend(
      QuicTime now,
      TransmissionType transmission_type,
      HasRetransmittableData has_retransmittable_data,
      IsHandshake handshake) = 0;

  // What's the current estimated bandwidth in bytes per second.
  // Returns 0 when it does not have an estimate.
  virtual QuicBandwidth BandwidthEstimate() const = 0;

  // Updates the smoothed RTT based on a new sample.
  // TODO(ianswett): Now that the RTT is managed by RTTStats, it may be
  // possible to remove this method.
  virtual void UpdateRtt(QuicTime::Delta rtt_sample) = 0;

  // Get the send algorithm specific retransmission delay, called RTO in TCP,
  // Note 1: the caller is responsible for sanity checking this value.
  // Note 2: this will return zero if we don't have enough data for an estimate.
  virtual QuicTime::Delta RetransmissionDelay() const = 0;

  // Returns the size of the current congestion window in bytes.  Note, this is
  // not the *available* window.  Some send algorithms may not use a congestion
  // window and will return 0.
  virtual QuicByteCount GetCongestionWindow() const = 0;
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_SEND_ALGORITHM_INTERFACE_H_
