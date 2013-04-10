// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/inter_arrival_state_machine.h"

#include "base/logging.h"

namespace  {
const int kIncreaseEventsBeforeDowngradingState = 5;
const int kDecreaseEventsBeforeUpgradingState = 2;
// Note: Can not be higher than kDecreaseEventsBeforeUpgradingState;
const int kLossEventsBeforeUpgradingState = 2;
// Timeout old loss and delay events after this time.
const int kEventTimeoutMs = 10000;
// A reasonable arbitrary chosen value for initial round trip time.
const int kInitialRttMs = 80;
}

namespace net {

InterArrivalStateMachine::InterArrivalStateMachine(const QuicClock* clock)
    : clock_(clock),
      current_state_(kInterArrivalStateStable),
      smoothed_rtt_(QuicTime::Delta::FromMilliseconds(kInitialRttMs)),
      decrease_event_count_(0),
      last_decrease_event_(QuicTime::Zero()),
      increase_event_count_(0),
      last_increase_event_(QuicTime::Zero()),
      loss_event_count_(0),
      last_loss_event_(QuicTime::Zero()),
      delay_event_count_(0),
      last_delay_event_(QuicTime::Zero()) {
}

InterArrivalState InterArrivalStateMachine::GetInterArrivalState() {
  return current_state_;
}

void InterArrivalStateMachine::IncreaseBitrateDecision() {
  // Multiple increase event without packet loss or delay events will drive
  // state back to stable.
  QuicTime current_time = clock_->ApproximateNow();
  if (current_time.Subtract(last_increase_event_) < smoothed_rtt_) {
    // Less than one RTT have passed; ignore this event.
    return;
  }
  last_increase_event_ = current_time;
  increase_event_count_++;
  decrease_event_count_ = 0;  // Reset previous decrease events.

  if (increase_event_count_ < kIncreaseEventsBeforeDowngradingState) {
    // Not enough increase events to change state.
    return;
  }
  increase_event_count_ = 0;  // Reset increase events.

  switch (current_state_) {
    case kInterArrivalStateStable:
      // Keep this state.
      break;
    case kInterArrivalStatePacketLoss:
      current_state_ = kInterArrivalStateStable;
      break;
    case kInterArrivalStateDelay:
      current_state_ = kInterArrivalStateStable;
      break;
    case kInterArrivalStateCompetingFlow:
      current_state_ = kInterArrivalStateDelay;
      break;
    case kInterArrivalStateCompetingTcpFLow:
      current_state_ = kInterArrivalStateDelay;
      break;
  }
}

void InterArrivalStateMachine::DecreaseBitrateDecision() {
  DCHECK(kDecreaseEventsBeforeUpgradingState >=
         kLossEventsBeforeUpgradingState);

  QuicTime current_time = clock_->ApproximateNow();
  if (current_time.Subtract(last_decrease_event_) < smoothed_rtt_) {
    // Less than one RTT have passed; ignore this event.
    return;
  }
  last_decrease_event_ = current_time;
  decrease_event_count_++;
  increase_event_count_ = 0;  // Reset previous increase events.
  if (decrease_event_count_ < kDecreaseEventsBeforeUpgradingState) {
    // Not enough decrease events to change state.
    return;
  }
  decrease_event_count_ = 0;  // Reset decrease events.

  switch (current_state_) {
    case kInterArrivalStateStable:
      if (delay_event_count_ == 0 && loss_event_count_ > 0) {
        // No recent delay events; only packet loss events.
        current_state_ = kInterArrivalStatePacketLoss;
      } else {
        current_state_ = kInterArrivalStateDelay;
      }
      break;
    case kInterArrivalStatePacketLoss:
      // Keep this state.
      break;
    case kInterArrivalStateDelay:
      if (loss_event_count_ >= kLossEventsBeforeUpgradingState) {
        // We have packet loss events. Assume fighting with TCP.
        current_state_ = kInterArrivalStateCompetingTcpFLow;
      } else {
        current_state_ = kInterArrivalStateCompetingFlow;
      }
      break;
    case kInterArrivalStateCompetingFlow:
      if (loss_event_count_ >= kLossEventsBeforeUpgradingState) {
        // We have packet loss events. Assume fighting with TCP.
        current_state_ = kInterArrivalStateCompetingTcpFLow;
      }
      break;
    case kInterArrivalStateCompetingTcpFLow:
      // Keep this state.
      break;
  }
}

void InterArrivalStateMachine::set_rtt(QuicTime::Delta rtt) {
  smoothed_rtt_ = rtt;
}

bool InterArrivalStateMachine::PacketLossEvent() {
  QuicTime current_time = clock_->ApproximateNow();
  if (current_time.Subtract(last_loss_event_) < smoothed_rtt_) {
    // Less than one RTT have passed; ignore this event.
    return false;
  }
  last_loss_event_ = current_time;
  loss_event_count_++;
  if (current_time.Subtract(last_delay_event_) >
      QuicTime::Delta::FromMilliseconds(kEventTimeoutMs)) {
    // Delay event have timed out.
    delay_event_count_ = 0;
  }
  return true;
}

bool InterArrivalStateMachine::IncreasingDelayEvent() {
  QuicTime current_time = clock_->ApproximateNow();
  if (current_time.Subtract(last_delay_event_) < smoothed_rtt_) {
    // Less than one RTT have passed; ignore this event.
    return false;
  }
  last_delay_event_ = current_time;
  delay_event_count_++;
  if (current_time.Subtract(last_loss_event_) >
      QuicTime::Delta::FromMilliseconds(kEventTimeoutMs)) {
    // Loss event have timed out.
    loss_event_count_ = 0;
  }
  return true;
}

}  // namespace net
