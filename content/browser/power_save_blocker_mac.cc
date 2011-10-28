// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker.h"

#include <IOKit/pwr_mgt/IOPMLib.h>

#include "base/bind.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Power management cannot be done on the UI thread. IOPMAssertionCreate does a
// synchronous MIG call to configd, so if it is called on the main thread the UI
// is at the mercy of another process. See http://crbug.com/79559 and
// http://www.opensource.apple.com/source/IOKitUser/IOKitUser-514.16.31/pwr_mgt.subproj/IOPMLibPrivate.c .
base::Thread* g_power_thread;
IOPMAssertionID g_power_assertion;

void CreateSleepAssertion(PowerSaveBlocker::PowerSaveBlockerType type) {
  DCHECK_EQ(base::PlatformThread::CurrentId(), g_power_thread->thread_id());
  IOReturn result;

  if (g_power_assertion != kIOPMNullAssertionID) {
    result = IOPMAssertionRelease(g_power_assertion);
    g_power_assertion = kIOPMNullAssertionID;
    LOG_IF(ERROR, result != kIOReturnSuccess)
        << "IOPMAssertionRelease: " << result;
  }

  CFStringRef level = NULL;
  // See QA1340 <http://developer.apple.com/library/mac/#qa/qa1340/> for more
  // details.
  switch (type) {
    case PowerSaveBlocker::kPowerSaveBlockPreventSystemSleep:
      level = kIOPMAssertionTypeNoIdleSleep;
      break;
    case PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep:
      level = kIOPMAssertionTypeNoDisplaySleep;
      break;
    default:
      break;
  }
  if (level) {
    result = IOPMAssertionCreate(level,
                                 kIOPMAssertionLevelOn,
                                 &g_power_assertion);
    LOG_IF(ERROR, result != kIOReturnSuccess)
        << "IOPMAssertionCreate: " << result;
  }
}

}  // namespace

// Called only from UI thread.
// static
void PowerSaveBlocker::ApplyBlock(PowerSaveBlockerType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!g_power_thread) {
    g_power_assertion = kIOPMNullAssertionID;
    g_power_thread = new base::Thread("PowerSaveBlocker");
    g_power_thread->Start();
  }

  g_power_thread->message_loop()->
      PostTask(FROM_HERE, base::Bind(CreateSleepAssertion, type));
}
