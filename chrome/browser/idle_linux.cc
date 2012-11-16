// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle.h"

#include "base/basictypes.h"
#include "chrome/browser/idle_query_linux.h"

#if !defined(USE_AURA)
#include "chrome/browser/screensaver_window_finder_gtk.h"
#endif

void CalculateIdleTime(IdleTimeCallback notify) {
  chrome::IdleQueryLinux idle_query;
  notify.Run(idle_query.IdleTime());
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
