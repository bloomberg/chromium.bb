// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle.h"

#include "base/basictypes.h"

#if defined(USE_X11)
#include "chrome/browser/idle_query_x11.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/screensaver_window_finder_x11.h"
#endif

void CalculateIdleTime(IdleTimeCallback notify) {
#if defined(USE_X11)
  chrome::IdleQueryX11 idle_query;
  notify.Run(idle_query.IdleTime());
#endif
}

bool CheckIdleStateIsLocked() {
  // Usually the screensaver is used to lock the screen, so we do not need to
  // check if the workstation is locked.
#if defined(OS_CHROMEOS)
  return false;
#elif defined(USE_OZONE)
  return false;
#else
  return ScreensaverWindowFinder::ScreensaverWindowExists();
#endif
}
