// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/power_save_blocker.h"

#include <windows.h>

#include "content/browser/browser_thread.h"

// Runs on UI thread only.
void PowerSaveBlocker::ApplyBlock(bool blocking) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DWORD flags = ES_CONTINUOUS;

  if (blocking)
    flags |= ES_SYSTEM_REQUIRED;

  SetThreadExecutionState(flags);
}
