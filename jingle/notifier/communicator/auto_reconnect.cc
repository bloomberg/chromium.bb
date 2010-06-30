// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/communicator/auto_reconnect.h"

#include "base/rand_util.h"
#include "jingle/notifier/communicator/login.h"

namespace notifier {

const int kResetReconnectInfoDelaySec = 2;

AutoReconnect::AutoReconnect()
    : reconnect_timer_started_(false),
      is_idle_(false) {
  SetupReconnectInterval();
}

void AutoReconnect::NetworkStateChanged(bool is_alive) {
  if (is_retrying() && is_alive) {
    // Reconnect in 1 to 9 seconds (vary the time a little to try to avoid
    // spikey behavior on network hiccups).
    StartReconnectTimerWithInterval(
        base::TimeDelta::FromSeconds(base::RandInt(1, 9)));
  }
}

void AutoReconnect::StartReconnectTimer() {
  StartReconnectTimerWithInterval(reconnect_interval_);
}

void AutoReconnect::StartReconnectTimerWithInterval(
    base::TimeDelta interval) {
  // Don't call StopReconnectTimer because we don't want other classes to
  // detect that the intermediate state of the timer being stopped.
  // (We're avoiding the call to SignalTimerStartStop while reconnect_timer_ is
  // NULL).
  reconnect_timer_.Stop();
  reconnect_timer_.Start(interval, this, &AutoReconnect::DoReconnect);
  reconnect_timer_started_ = true;
  SignalTimerStartStop();
}

void AutoReconnect::DoReconnect() {
  reconnect_timer_started_ = false;

  // If timed out again, double autoreconnect time up to 30 minutes.
  reconnect_interval_ *= 2;
  if (reconnect_interval_ > base::TimeDelta::FromMinutes(30)) {
    reconnect_interval_ = base::TimeDelta::FromMinutes(30);
  }
  SignalStartConnection();
}

void AutoReconnect::StopReconnectTimer() {
  if (reconnect_timer_started_) {
    reconnect_timer_started_ = false;
    reconnect_timer_.Stop();
    SignalTimerStartStop();
  }
}

void AutoReconnect::StopDelayedResetTimer() {
  delayed_reset_timer_.Stop();
}

void AutoReconnect::ResetState() {
  StopDelayedResetTimer();
  StopReconnectTimer();
  SetupReconnectInterval();
}

void AutoReconnect::SetupReconnectInterval() {
  if (is_idle_) {
    // If we were idle, start the timer over again (120 - 360 seconds).
    reconnect_interval_ =
        base::TimeDelta::FromSeconds(base::RandInt(120, 360));
  } else {
    // If we weren't idle, try the connection 5 - 25 seconds later.
    reconnect_interval_ =
        base::TimeDelta::FromSeconds(base::RandInt(5, 25));
  }
}

void AutoReconnect::OnPowerSuspend(bool suspended) {
  if (suspended) {
    // When the computer comes back on, ensure that the reconnect happens
    // quickly (5 - 25 seconds).
    reconnect_interval_ =
        base::TimeDelta::FromSeconds(base::RandInt(5, 25));
  }
}

void AutoReconnect::OnClientStateChange(LoginConnectionState state) {
  // On any state change, stop the reset timer.
  StopDelayedResetTimer();
  switch (state) {
    case STATE_RETRYING:
      // Do nothing.
      break;

    case STATE_CLOSED:
      // When the user has been logged out and no auto-reconnect is happening,
      // then the autoreconnect intervals should be reset.
      ResetState();
      break;

    case STATE_OPENING:
      StopReconnectTimer();
      break;

    case STATE_OPENED:
      // Reset autoreconnect timeout sequence after being connected for a bit
      // of time. This helps in the case that we are connecting briefly and
      // then getting disconnect like when an account hits an abuse limit.
      StopReconnectTimer();
      delayed_reset_timer_.Start(
          base::TimeDelta::FromSeconds(kResetReconnectInfoDelaySec),
          this, &AutoReconnect::ResetState);
      break;
  }
}

}  // namespace notifier
