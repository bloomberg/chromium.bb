// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CONGESTION_CONTROL_TIMESTAMP_RECEIVER_H_
#define NET_QUIC_CONGESTION_CONTROL_TIMESTAMP_RECEIVER_H_

#include "base/basictypes.h"
#include "net/quic/congestion_control/receive_algorithm_interface.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE TimestampReceiver : public ReceiveAlgorithmInterface {
 public:
  TimestampReceiver();
  virtual ~TimestampReceiver();

  virtual bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* feedback) OVERRIDE;

  virtual void RecordIncomingPacket(QuicByteCount bytes,
                                    QuicPacketSequenceNumber sequence_number,
                                    QuicTime timestamp) OVERRIDE;

 private:
  // The set of received packets since the last feedback was sent, along with
  // their arrival times.
  TimeMap received_packet_times_;

  DISALLOW_COPY_AND_ASSIGN(TimestampReceiver);
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_TIMESTAMP_RECEIVER_H_
