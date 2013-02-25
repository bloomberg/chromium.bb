// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class that limits the congestion window to pace the packets.

#ifndef NET_QUIC_CONGESTION_CONTROL_PACED_SENDER_H_
#define NET_QUIC_CONGESTION_CONTROL_PACED_SENDER_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/leaky_bucket.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_time.h"

namespace net {

class NET_EXPORT_PRIVATE PacedSender {
 public:
  explicit PacedSender(QuicBandwidth bandwidth_estimate);

  // The estimated bandidth from the congestion algorithm changed.
  void UpdateBandwidthEstimate(QuicTime now, QuicBandwidth bandwidth_estimate);

  // A packet of size bytes was sent.
  void SentPacket(QuicTime now, QuicByteCount bytes);

  // Return time until we can send based on the pacing.
  QuicTime::Delta TimeUntilSend(QuicTime now, QuicTime::Delta time_until_send);

 private:
  // Helper object to track the rate data can leave the buffer for pacing.
  LeakyBucket leaky_bucket_;
  QuicBandwidth pace_;

  DISALLOW_COPY_AND_ASSIGN(PacedSender);
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_PACED_SENDER_H_
