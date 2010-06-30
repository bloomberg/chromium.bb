// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_AUTO_RECONNECT_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_AUTO_RECONNECT_H_

#include <string>

#include "base/time.h"
#include "base/timer.h"
#include "jingle/notifier/communicator/login_connection_state.h"
#include "talk/base/sigslot.h"

namespace notifier {

class AutoReconnect : public sigslot::has_slots<> {
 public:
  AutoReconnect();
  void StartReconnectTimer();
  void StopReconnectTimer();
  void OnClientStateChange(LoginConnectionState state);

  void NetworkStateChanged(bool is_alive);

  // Callback when power is suspended.
  void OnPowerSuspend(bool suspended);

  void set_idle(bool idle) {
    is_idle_ = idle;
  }

  // Returns true if the auto-retry is to be done (pending a countdown).
  bool is_retrying() const {
    return reconnect_timer_started_;
  }

  sigslot::signal0<> SignalTimerStartStop;
  sigslot::signal0<> SignalStartConnection;

 private:
  void StartReconnectTimerWithInterval(base::TimeDelta interval);
  void DoReconnect();
  void ResetState();
  void SetupReconnectInterval();
  void StopDelayedResetTimer();

  base::TimeDelta reconnect_interval_;
  bool reconnect_timer_started_;
  base::OneShotTimer<AutoReconnect> reconnect_timer_;
  base::OneShotTimer<AutoReconnect> delayed_reset_timer_;

  bool is_idle_;
  DISALLOW_COPY_AND_ASSIGN(AutoReconnect);
};

// Wait 2 seconds until after we actually connect to reset reconnect related
// items.
//
// The reason for this delay is to avoid the situation in which buzz is trying
// to block the client due to abuse and the client responses by going into
// rapid reconnect mode, which makes the problem more severe.
extern const int kResetReconnectInfoDelaySec;

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_AUTO_RECONNECT_H_
