// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "net/curvecp/rtt_and_send_rate_calculator.h"

namespace net {

RttAndSendRateCalculator::RttAndSendRateCalculator()
    : rtt_latest_(0),
      rtt_average_(0),
      rtt_deviation_(0),
      rtt_lowwater_(0),
      rtt_highwater_(0),
      rtt_timeout_(base::Time::kMicrosecondsPerSecond),
      send_rate_(1000000),
      rtt_seenrecenthigh_(0),
      rtt_seenrecentlow_(0),
      rtt_seenolderhigh_(0),
      rtt_seenolderlow_(0),
      rtt_phase_(false) {
}

void RttAndSendRateCalculator::OnMessage(int32 rtt_us) {
  UpdateRTT(rtt_us);
  AdjustSendRate();
}

void RttAndSendRateCalculator::OnTimeout() {
  base::TimeDelta time_since_last_loss = last_sample_time_ - last_loss_time_;
  if (time_since_last_loss.InMilliseconds() < 4 * rtt_timeout_)
    return;
  send_rate_ *= 2;
  last_loss_time_ = last_sample_time_;
  last_edge_time_ = last_sample_time_;
}

// Updates RTT
void RttAndSendRateCalculator::UpdateRTT(int32 rtt_us) {
  rtt_latest_ = rtt_us;
  last_sample_time_ = base::TimeTicks::Now();

  // Initialize for the first sample.
  if (!rtt_average_) {
    rtt_average_ = rtt_latest_;
    rtt_deviation_ = rtt_latest_;
    rtt_highwater_ = rtt_latest_;
    rtt_lowwater_ = rtt_latest_;
  }

  // Jacobson's retransmission timeout calculation.
  int32 rtt_delta = rtt_latest_ - rtt_average_;
  rtt_average_ += rtt_delta / 8;
  if (rtt_delta < 0)
    rtt_delta = -rtt_delta;
  rtt_delta -= rtt_deviation_;
  rtt_deviation_ += rtt_delta / 4;
  rtt_timeout_ = rtt_average_ + (4 * rtt_deviation_);

  // Adjust for delayed acks with anti-spiking.
  // TODO(mbelshe): this seems high.
  rtt_timeout_ += 8 * send_rate_;

  // Recognize the top and bottom of the congestion cycle.
  rtt_delta = rtt_latest_ - rtt_highwater_;
  rtt_highwater_ += rtt_delta / 1024;

  rtt_delta = rtt_latest_ - rtt_lowwater_;
  if (rtt_delta > 0)
    rtt_lowwater_ += rtt_delta / 8192;
  else
    rtt_lowwater_ += rtt_delta / 256;
}

void RttAndSendRateCalculator::AdjustSendRate() {
  last_sample_time_ = base::TimeTicks::Now();

  base::TimeDelta time = last_sample_time_ - last_send_adjust_time_;

  // We only adjust the send rate approximately every 16 samples.
  if (time.InMicroseconds() < 16 * send_rate_)
    return;

  if (rtt_average_ >
      rtt_highwater_ + (5 * base::Time::kMicrosecondsPerMillisecond))
    rtt_seenrecenthigh_ = true;
  else if (rtt_average_ < rtt_lowwater_)
    rtt_seenrecentlow_ = true;

  last_send_adjust_time_ = last_sample_time_;

  // If too much time has elapsed, re-initialize the send_rate.
  if (time.InMicroseconds() >  10 * base::Time::kMicrosecondsPerSecond) {
    send_rate_ =  base::Time::kMicrosecondsPerSecond;  // restart.
    send_rate_ += randommod(send_rate_ / 8);
  }

  // TODO(mbelshe):  Why is 128us a lower bound?
  if (send_rate_ >= 128) {
    // Additive increase:  adjust 1/N by a constant c.
    // rtt-fair additive increase: adjust 1/N by a constant c every nanosec.
    // approximation: adjust 1/N by cN every N nanoseconds.

    // TODO(mbelshe): he used c == 2^-51. for nanosecs.
    //                I use c ==  2^31, for microsecs.

    // i.e. N <- 1/(1/N + cN) = N/(1 + cN^2) every N nanosec.
    if (false && send_rate_ < 16384) {
      // N/(1+cN^2) approx N - cN^3
      // TODO(mbelshe):  note that he is using (cN)^3 here, not what he wrote.
      int32 msec = send_rate_ / 128;
      send_rate_ -= msec * msec * msec;
    } else {
      double d = send_rate_;
      send_rate_ = d / (1 + d * d / 2147483648.0);  // 2^31
    }
  }

  if (rtt_phase_ == false) {
    if (rtt_seenolderhigh_) {
      rtt_phase_ = true;
      last_edge_time_ = last_sample_time_;
      send_rate_ += randommod(send_rate_ / 4);
    }
  } else {
    if (rtt_seenolderlow_) {
      rtt_phase_ = false;
    }
  }

  rtt_seenolderhigh_ = rtt_seenrecenthigh_;
  rtt_seenolderlow_ = rtt_seenrecentlow_;
  rtt_seenrecenthigh_ = false;
  rtt_seenrecentlow_ = false;

  AttemptToDoubleSendRate();
}

void RttAndSendRateCalculator::AttemptToDoubleSendRate() {
  base::TimeDelta time_since_edge = last_sample_time_ - last_edge_time_;
  base::TimeDelta time_since_doubling =
      last_sample_time_ - last_doubling_time_;


  int32 threshold = 0;
  if (time_since_edge.InMicroseconds() <
      base::Time::kMicrosecondsPerSecond * 60) {
    threshold = (4 * send_rate_) +
                (64 * rtt_timeout_) +
                (5 * base::Time::kMicrosecondsPerSecond);
  } else {
    threshold = (4 * send_rate_) +
                (2 * rtt_timeout_);
  }
  if (time_since_doubling.InMicroseconds() < threshold)
    return;

  if (send_rate_ < 64)
    return;

  send_rate_ /= 2;
  last_doubling_time_ = last_sample_time_;
}

// returns a random number from 0 to val-1.
int32 RttAndSendRateCalculator::randommod(int32 val) {
  return rand() % val;
}

}  // namespace net
