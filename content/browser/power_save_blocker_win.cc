// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker.h"

#include <windows.h>

#include "content/browser/browser_thread.h"

// Called only from UI thread.
// static
void PowerSaveBlocker::ApplyBlock(PowerSaveBlockerType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DWORD flags = ES_CONTINUOUS;

  switch (type) {
    case kPowerSaveBlockPreventSystemSleep:
      flags |= ES_SYSTEM_REQUIRED;
      break;
    case kPowerSaveBlockPreventDisplaySleep:
      flags |= ES_DISPLAY_REQUIRED;
      break;
    case kPowerSaveBlockPreventNone:
      break;
  }

  SetThreadExecutionState(flags);
}
