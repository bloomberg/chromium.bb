// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The is the base class for QUIC send side congestion control.
// It decides when we can send a QUIC packet to the wire.
// This class handles the basic bookkeeping of sent bitrate and packet loss.
// The actual send side algorithm is implemented via the
// SendAlgorithmInterface.

#ifndef NET_QUIC_CONGESTION_CONTROL_QUIC_SEND_SCHEDULER_H_
#define NET_QUIC_CONGESTION_CONTROL_QUIC_SEND_SCHEDULER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/quic_receipt_metrics_collector.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_time.h"

namespace net {

class NET_EXPORT_PRIVATE QuicSendScheduler {
 public:
  // Enable pacing to prevent a large congestion window to be sent all at once,
  // when pacing is enabled a large congestion window will be sent in multiple
  // bursts of packet(s) instead of one big burst that might introduce packet
  // loss.
  QuicSendScheduler(const QuicClock* clock,
                    CongestionFeedbackType congestion_type);
  virtual ~QuicSendScheduler();

  // Called when we have received an ack frame from peer.
  virtual void OnIncomingAckFrame(const QuicAckFrame& frame);

  // Called when a congestion feedback frame is received from peer.
  virtual void OnIncomingQuicCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame);

  // Called when we have sent bytes to the peer.  This informs the manager both
  // the number of bytes sent and if they were retransmitted.
  virtual void SentPacket(QuicPacketSequenceNumber sequence_number,
                          size_t bytes,
                          bool is_retransmission);

  // Calculate the time until we can send the next packet to the wire.
  // Note 1: When kUnknownWaitTime is returned, there is no need to poll
  // TimeUntilSend again until we receive an OnIncomingAckFrame event.
  // Note 2: Send algorithms may or may not use |retransmit| in their
  // calculations.
  virtual QuicTime::Delta TimeUntilSend(bool is_retransmission);

  // Returns the current available congestion window in bytes, the number of
  // bytes that can be sent now.
  // Note: due to pacing this function might return a smaller value than the
  // real available congestion window. This way we hold off the sender to avoid
  // queuing in the lower layers in the stack.
  size_t AvailableCongestionWindow();

  int BandwidthEstimate();  // Current estimate, in bytes per second.

  int SentBandwidth();  // Current smooth acctually sent, in bytes per second.

  int PeakSustainedBandwidth();  // In bytes per second.

 private:
  typedef std::map<QuicPacketSequenceNumber, size_t> PendingPacketsMap;

  void CleanupPacketHistory();

  const QuicClock* clock_;
  int64 current_estimated_bandwidth_;
  int64 max_estimated_bandwidth_;
  QuicTime last_sent_packet_;
  scoped_ptr<SendAlgorithmInterface> send_algorithm_;
  SendAlgorithmInterface::SentPacketsMap packet_history_map_;
  PendingPacketsMap pending_packets_;
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_QUIC_SEND_SCHEDULER_H_
