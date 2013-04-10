// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_RECEIVER_H_
#define NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_RECEIVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/receive_algorithm_interface.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE InterArrivalReceiver
    : public ReceiveAlgorithmInterface {
 public:
  InterArrivalReceiver();
  virtual ~InterArrivalReceiver();

  // Start implementation of ReceiveAlgorithmInterface.
  virtual bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* feedback) OVERRIDE;

  virtual void RecordIncomingPacket(QuicByteCount bytes,
                                    QuicPacketSequenceNumber sequence_number,
                                    QuicTime timestamp,
                                    bool revived) OVERRIDE;
  // End implementation of ReceiveAlgorithmInterface.

 private:
  // We need to keep track of FEC recovered packets.
  int accumulated_number_of_recoverd_lost_packets_;

  // The set of received packets since the last feedback was sent, along with
  // their arrival times.
  TimeMap received_packet_times_;

  DISALLOW_COPY_AND_ASSIGN(InterArrivalReceiver);
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_INTER_ARRIVAL_RECEIVER_H_
