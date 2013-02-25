// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/paced_sender.h"

#include "net/quic/quic_protocol.h"

namespace net {

// To prevent too aggressive pacing we allow the following packet burst size.
const int64 kMinPacketBurstSize = 2;
// Max estimated time between calls to TimeUntilSend and
// AvailableCongestionWindow.
const int64 kMaxSchedulingDelayUs = 2000;

PacedSender::PacedSender(QuicBandwidth estimate)
    : leaky_bucket_(estimate),
      pace_(estimate) {
}

void PacedSender::UpdateBandwidthEstimate(QuicTime now,
                                          QuicBandwidth estimate) {
  leaky_bucket_.SetDrainingRate(now, estimate);
  pace_ = estimate;
}

void PacedSender::SentPacket(QuicTime now, QuicByteCount bytes) {
  leaky_bucket_.Add(now, bytes);
}

QuicTime::Delta PacedSender::TimeUntilSend(QuicTime now,
                                           QuicTime::Delta time_until_send) {
  if (time_until_send.ToMicroseconds() >= kMaxSchedulingDelayUs) {
    return time_until_send;
  }
  // Pace the data.
  QuicByteCount pacing_window = pace_.ToBytesPerPeriod(
      QuicTime::Delta::FromMicroseconds(kMaxSchedulingDelayUs));
  QuicByteCount min_window_size = kMinPacketBurstSize *  kMaxPacketSize;
  pacing_window = std::max(pacing_window, min_window_size);

  if (pacing_window > leaky_bucket_.BytesPending(now)) {
    // We have not filled our pacing window yet.
    return time_until_send;
  }
  return leaky_bucket_.TimeRemaining(now);
}

}  // namespace net
