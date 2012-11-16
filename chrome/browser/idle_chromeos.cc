// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle.h"

#include <limits.h>

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

void CalculateIdleTimeNotifier(IdleTimeCallback notify,
                               int64_t idle_time_s) {
  if (idle_time_s > INT_MAX) {
    // Just in case you are idle for > 100 years.
    idle_time_s = INT_MAX;
  }
  notify.Run(static_cast<int>(idle_time_s));
}

void CalculateIdleTime(IdleTimeCallback notify) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      CalculateIdleTime(base::Bind(&CalculateIdleTimeNotifier, notify));
}

bool CheckIdleStateIsLocked() {
  // Usually the screensaver is used to lock the screen, so we do not need to
  // check if the workstation is locked.
  // TODO(oshima): Verify if this behavior is correct.
  return false;
}
