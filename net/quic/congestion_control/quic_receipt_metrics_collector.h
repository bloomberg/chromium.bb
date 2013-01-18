// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is the base class for QUIC receive side congestion control.
// This class will provide the QuicCongestionFeedbackFrame objects for outgoing
// packets if needed.
// The acctual receive side algorithm is implemented via the
// ReceiveAlgorithmInterface.

#ifndef NET_QUIC_CONGESTION_CONTROL_QUIC_RECEIPT_METRICS_COLLECTOR_H_
#define NET_QUIC_CONGESTION_CONTROL_QUIC_RECEIPT_METRICS_COLLECTOR_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/receive_algorithm_interface.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE QuicReceiptMetricsCollector {
 public:
  QuicReceiptMetricsCollector(const QuicClock* clock,
                              CongestionFeedbackType congestion_type);

  virtual ~QuicReceiptMetricsCollector();

  // Should be called before sending an ACK packet, to decide if we need
  // to attach a QuicCongestionFeedbackFrame block.
  // Returns false if no QuicCongestionFeedbackFrame block is needed.
  // Otherwise fills in feedback and returns true.
  virtual bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* feedback);

  // Should be called for each incoming packet.
  // bytes: the packet size in bytes including IP headers.
  // sequence_number: the unique sequence number from the QUIC packet header.
  // timestamp: the arrival time of the packet.
  // revived: true if the packet was lost and then recovered with help of a
  // FEC packet.
  virtual void RecordIncomingPacket(size_t bytes,
                                    QuicPacketSequenceNumber sequence_number,
                                    QuicTime timestamp,
                                    bool revived);

  // TODO(pwestin) Keep track of the number of FEC recovered packets.
  // Needed by all congestion control algorithms.

 private:
  scoped_ptr<ReceiveAlgorithmInterface> receive_algorithm_;
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_QUIC_RECEIPT_METRICS_COLLECTOR_H_
