// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CORE_SESSION_NOTIFIER_INTERFACE_H_
#define NET_QUIC_CORE_SESSION_NOTIFIER_INTERFACE_H_

#include "net/quic/core/frames/quic_frame.h"
#include "net/quic/core/quic_time.h"

namespace net {

// Pure virtual class to be notified when a packet containing a frame is acked
// or lost.
class QUIC_EXPORT_PRIVATE SessionNotifierInterface {
 public:
  virtual ~SessionNotifierInterface() {}

  // Called when |frame| is acked. Returns true if any new data gets acked,
  // returns false otherwise.
  virtual bool OnFrameAcked(const QuicFrame& frame,
                            QuicTime::Delta ack_delay_time) = 0;

  // Called when |frame| is retransmitted.
  virtual void OnStreamFrameRetransmitted(const QuicStreamFrame& frame) = 0;

  // Called when |frame| is considered as lost.
  virtual void OnFrameLost(const QuicFrame& frame) = 0;
};

}  // namespace net

#endif  // NET_QUIC_CORE_SESSION_NOTIFIER_INTERFACE_H_
