// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_RTT_AND_SEND_RATE_CALCULATOR_H_
#define NET_CURVECP_RTT_AND_SEND_RATE_CALCULATOR_H_
#pragma once

#include "base/basictypes.h"
#include "base/time.h"

namespace net {

// Calculator for tracking the current RTT value.
// Measurements are in microseconds.
class RttAndSendRateCalculator {
 public:
  RttAndSendRateCalculator();

  // Returns the current, average RTT, in microseconds.
  int32 rtt_average() const { return rtt_average_; }

  // Returns the current, RTT timeout, in microseconds.
  int32 rtt_timeout() const { return rtt_timeout_; }

  // The current send rate.
  // Measurement is in microseconds between sends.
  int32 send_rate() const { return send_rate_; }

  int32 rtt_hi() const { return rtt_highwater_; }
  int32 rtt_lo() const { return rtt_lowwater_; }

  // Called when a new RTT sample is measured.
  void OnMessage(int32 rtt_us);

  // Adjusts the sendrate when a timeout condition has occurred.
  void OnTimeout();

 private:
  // Updates RTT
  void UpdateRTT(int32 rtt_us);

  // Adjusts the send rate when a message is received.
  void AdjustSendRate();

  void AttemptToDoubleSendRate();

  // returns a random number from 0 to val-1.
  int32 randommod(int32 val);

  int32 rtt_latest_;            // The most recent RTT
  int32 rtt_average_;
  int32 rtt_deviation_;
  int32 rtt_lowwater_;
  int32 rtt_highwater_;
  int32 rtt_timeout_;
  int32 send_rate_;
  base::TimeTicks last_sample_time_;
  base::TimeTicks last_send_adjust_time_;
  base::TimeTicks last_edge_time_;
  base::TimeTicks last_doubling_time_;
  base::TimeTicks last_loss_time_;
  bool rtt_seenrecenthigh_;
  bool rtt_seenrecentlow_;
  bool rtt_seenolderhigh_;
  bool rtt_seenolderlow_;
  bool rtt_phase_;
};

}  // namespace net

#endif  //  NET_CURVECP_RTT_AND_SEND_RATE_CALCULATOR_H_
