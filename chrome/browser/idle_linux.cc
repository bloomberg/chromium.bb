// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle.h"

#include "base/basictypes.h"
#include "chrome/browser/idle_query_linux.h"
#include "chrome/browser/screensaver_window_finder_linux.h"

void CalculateIdleState(unsigned int idle_threshold, IdleCallback notify) {
  if (CheckIdleStateIsLocked()) {
    notify.Run(IDLE_STATE_LOCKED);
    return;
  }
  browser::IdleQueryLinux idle_query;
  unsigned int idle_time = idle_query.IdleTime();
  if (idle_time >= idle_threshold)
    notify.Run(IDLE_STATE_IDLE);
  else
    notify.Run(IDLE_STATE_ACTIVE);
}

bool CheckIdleStateIsLocked() {
  // Usually the screensaver is used to lock the screen, so we do not need to
  // check if the workstation is locked.
  return ScreensaverWindowFinder::ScreensaverWindowExists();
}
