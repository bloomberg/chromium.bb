// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The pure virtual class for send side loss detection algorithm.

#ifndef NET_QUIC_CONGESTION_CONTROL_LOSS_DETECTION_INTERFACE_H_
#define NET_QUIC_CONGESTION_CONTROL_LOSS_DETECTION_INTERFACE_H_

#include "base/basictypes.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

class QuicUnackedPacketMap;

class NET_EXPORT_PRIVATE LossDetectionInterface {
 public:
  // Creates a TCP loss detector.
  static LossDetectionInterface* Create();

  virtual ~LossDetectionInterface() {}

  // Called when a new ack arrives or the loss alarm fires.
  virtual SequenceNumberSet DetectLostPackets(
      const QuicUnackedPacketMap& unacked_packets,
      const QuicTime& time,
      QuicPacketSequenceNumber largest_observed,
      QuicTime::Delta srtt,
      QuicTime::Delta latest_rtt) = 0;

  // Get the time the LossDetectionAlgorithm wants to re-evaluate losses.
  // Returns QuicTime::Zero if no alarm needs to be set.
  virtual QuicTime GetLossTimeout() const = 0;
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_LOSS_DETECTION_INTERFACE_H_
