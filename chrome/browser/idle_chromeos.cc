// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/power_library.h"

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
  chromeos::CalculateIdleTimeCallback* callback =
      new base::Callback<void(int64_t)>(base::Bind(&CalculateIdleStateNotifier,
                                        idle_threshold,
                                        notify));
  chromeos::CrosLibrary::Get()->GetPowerLibrary()->CalculateIdleTime(callback);
}
