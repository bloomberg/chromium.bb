// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/power_library.h"
#if !defined(USE_AURA)
#include "chrome/browser/screensaver_window_finder_gtk.h"
#endif

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
  if (CheckIdleStateIsLocked()) {
    notify.Run(IDLE_STATE_LOCKED);
    return;
  }
  chromeos::CalculateIdleTimeCallback* callback =
      new base::Callback<void(int64_t)>(base::Bind(&CalculateIdleStateNotifier,
                                        idle_threshold,
                                        notify));
  chromeos::CrosLibrary::Get()->GetPowerLibrary()->CalculateIdleTime(callback);
}

bool CheckIdleStateIsLocked() {
  // Usually the screensaver is used to lock the screen, so we do not need to
  // check if the workstation is locked.
#if defined(USE_AURA)
  return false;
#else
  return ScreensaverWindowFinder::ScreensaverWindowExists();
#endif
}
