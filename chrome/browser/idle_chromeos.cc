// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

void CalculateIdleStateNotifier(unsigned int idle_treshold,
                                IdleCallback notify,
                                int64_t idle_time_s) {
  if (idle_time_s >= (int64_t)idle_treshold) {
    notify.Run(IDLE_STATE_IDLE);
  } else if (idle_time_s < 0) {
    notify.Run(IDLE_STATE_UNKNOWN);
  } else {
    notify.Run(IDLE_STATE_ACTIVE);
  }
}

void CalculateIdleState(unsigned int idle_threshold, IdleCallback notify) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      CalculateIdleTime(base::Bind(&CalculateIdleStateNotifier, idle_threshold,
                                   notify));
}

bool CheckIdleStateIsLocked() {
  // Usually the screensaver is used to lock the screen, so we do not need to
  // check if the workstation is locked.
  // TODO(oshima): Verify if this behavior is correct.
  return false;
}
