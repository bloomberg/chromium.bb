// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/power_save_blocker.h"

#include <IOKit/pwr_mgt/IOPMLib.h>

#include "content/browser/browser_thread.h"

// Run on the UI thread only.
void PowerSaveBlocker::ApplyBlock(bool blocking) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Block just idle sleep; allow display sleep.
  // See QA1340 <http://developer.apple.com/library/mac/#qa/qa2004/qa1340.html>
  // for more details.

  static IOPMAssertionID sleep_assertion = kIOPMNullAssertionID;
  IOReturn result;
  if (blocking) {
    DCHECK_EQ(sleep_assertion, kIOPMNullAssertionID);
    result = IOPMAssertionCreate(
        kIOPMAssertionTypeNoIdleSleep,
        kIOPMAssertionLevelOn,
        &sleep_assertion);
  } else {
    DCHECK_NE(sleep_assertion, kIOPMNullAssertionID);
    result = IOPMAssertionRelease(sleep_assertion);
    sleep_assertion = kIOPMNullAssertionID;
  }

  if (result != kIOReturnSuccess) {
    NOTREACHED();
  }
}
