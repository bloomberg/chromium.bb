// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/communicator/auto_reconnect.h"

#include "chrome/browser/sync/notifier/base/network_status_detector_task.h"
#include "chrome/browser/sync/notifier/base/time.h"
#include "chrome/browser/sync/notifier/base/timer.h"
#include "talk/base/common.h"

namespace notifier {
const int kResetReconnectInfoDelaySec = 2;

AutoReconnect::AutoReconnect(talk_base::Task* parent,
                             NetworkStatusDetectorTask* network_status)
  : reconnect_interval_ns_(0),
    reconnect_timer_(NULL),
    delayed_reset_timer_(NULL),
    parent_(parent),
    is_idle_(false) {
  SetupReconnectInterval();
  if (network_status) {
    network_status->SignalNetworkStateDetected.connect(
        this, &AutoReconnect::OnNetworkStateDetected);
  }
}

void AutoReconnect::OnNetworkStateDetected(bool was_alive, bool is_alive) {
  if (is_retrying() && !was_alive && is_alive) {
    // Reconnect in 1 to 9 seconds (vary the time a little to try to avoid
    // spikey behavior on network hiccups).
    StartReconnectTimerWithInterval((rand() % 9 + 1) * kSecsTo100ns);
  }
}

int AutoReconnect::seconds_until() const {
  if (!is_retrying() || !reconnect_timer_->get_timeout_time()) {
    return 0;
  }
  int64 time_until_100ns =
      reconnect_timer_->get_timeout_time() - GetCurrent100NSTime();
  if (time_until_100ns < 0) {
    return 0;
  }

  // Do a ceiling on the value (to avoid returning before its time)
  return (time_until_100ns + kSecsTo100ns - 1) / kSecsTo100ns;
}

void AutoReconnect::StartReconnectTimer() {
  StartReconnectTimerWithInterval(reconnect_interval_ns_);
}

void AutoReconnect::StartReconnectTimerWithInterval(time64 interval_ns) {
  // Don't call StopReconnectTimer because we don't
  // want other classes to detect that the intermediate state of
  // the timer being stopped.  (We're avoiding the call to SignalTimerStartStop
  // while reconnect_timer_ is NULL.)
  if (reconnect_timer_) {
    reconnect_timer_->Abort();
    reconnect_timer_ = NULL;
  }
  reconnect_timer_ = new Timer(parent_,
                               static_cast<int>(interval_ns / kSecsTo100ns),
                               false);  // repeat
  reconnect_timer_->SignalTimeout.connect(this,
                                          &AutoReconnect::DoReconnect);
  SignalTimerStartStop();
}

void AutoReconnect::DoReconnect() {
  reconnect_timer_ = NULL;

  // if timed out again, double autoreconnect time up to 30 minutes
  reconnect_interval_ns_ *= 2;
  if (reconnect_interval_ns_ > 30 * kMinsTo100ns) {
    reconnect_interval_ns_ = 30 * kMinsTo100ns;
  }
  SignalStartConnection();
}

void AutoReconnect::StopReconnectTimer() {
  if (reconnect_timer_) {
    reconnect_timer_->Abort();
    reconnect_timer_ = NULL;
    SignalTimerStartStop();
  }
}

void AutoReconnect::StopDelayedResetTimer() {
  if (delayed_reset_timer_) {
    delayed_reset_timer_->Abort();
    delayed_reset_timer_ = NULL;
  }
}

void AutoReconnect::ResetState() {
  StopDelayedResetTimer();
  StopReconnectTimer();
  SetupReconnectInterval();
}

void AutoReconnect::SetupReconnectInterval() {
  if (is_idle_) {
    // If we were idle, start the timer over again (120 - 360 seconds).
    reconnect_interval_ns_ = (rand() % 240 + 120) * kSecsTo100ns;
  } else {
    // If we weren't idle, try the connection 5 - 25 seconds later.
    reconnect_interval_ns_ = (rand() % 20 + 5) * kSecsTo100ns;
  }
}

void AutoReconnect::OnPowerSuspend(bool suspended) {
  if (suspended) {
    // When the computer comes back on, ensure that the reconnect
    // happens quickly (5 - 25 seconds).
    reconnect_interval_ns_ = (rand() % 20 + 5) * kSecsTo100ns;
  }
}

void AutoReconnect::OnClientStateChange(Login::ConnectionState state) {
  // On any state change, stop the reset timer.
  StopDelayedResetTimer();
  switch (state) {
    case Login::STATE_RETRYING:
      // do nothing
      break;

    case Login::STATE_CLOSED:
      // When the user has been logged out and no auto-reconnect
      // is happening, then the autoreconnect intervals should be
      // reset.
      ResetState();
      break;

    case Login::STATE_OPENING:
      StopReconnectTimer();
      break;

    case Login::STATE_OPENED:
      // Reset autoreconnect timeout sequence after being connected
      // for a bit of time.  This helps in the case that we are
      // connecting briefly and then getting disconnect like when
      // an account hits an abuse limit.
      StopReconnectTimer();
      delayed_reset_timer_ = new Timer(parent_,
                                       kResetReconnectInfoDelaySec,
                                       false);  // repeat
      delayed_reset_timer_->SignalTimeout.connect(this,
                                                  &AutoReconnect::ResetState);
      break;
  }
}
}  // namespace notifier
