// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/power_save_blocker.h"

#include <IOKit/pwr_mgt/IOPMLib.h>

#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "content/browser/browser_thread.h"

namespace {

// Power management cannot be done on the UI thread. IOPMAssertionCreate does a
// synchronous MIG call to configd, so if it is called on the main thread the UI
// is at the mercy of another process. See http://crbug.com/79559 and
// http://www.opensource.apple.com/source/IOKitUser/IOKitUser-514.16.31/pwr_mgt.subproj/IOPMLibPrivate.c .
base::Thread* g_power_thread;
IOPMAssertionID g_power_assertion;

void CreateSleepAssertion() {
  DCHECK_EQ(base::PlatformThread::CurrentId(), g_power_thread->thread_id());
  IOReturn result;
  DCHECK_EQ(g_power_assertion, kIOPMNullAssertionID);

  // Block just idle sleep; allow display sleep.
  // See QA1340 <http://developer.apple.com/library/mac/#qa/qa2004/qa1340.html>
  // for more details.
  result = IOPMAssertionCreate(kIOPMAssertionTypeNoIdleSleep,
                               kIOPMAssertionLevelOn,
                               &g_power_assertion);
  LOG_IF(ERROR, result != kIOReturnSuccess)
      << "IOPMAssertionCreate: " << result;
}

void ReleaseSleepAssertion() {
  DCHECK_EQ(base::PlatformThread::CurrentId(), g_power_thread->thread_id());
  IOReturn result;
  DCHECK_NE(g_power_assertion, kIOPMNullAssertionID);
  result = IOPMAssertionRelease(g_power_assertion);
  g_power_assertion = kIOPMNullAssertionID;
  LOG_IF(ERROR, result != kIOReturnSuccess)
      << "IOPMAssertionRelease: " << result;
}

}  // namespace

// Called only from UI thread.
void PowerSaveBlocker::ApplyBlock(bool blocking) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!g_power_thread) {
    g_power_assertion = kIOPMNullAssertionID;
    g_power_thread = new base::Thread("PowerSaveBlocker");
    g_power_thread->Start();
  }

  MessageLoop* loop = g_power_thread->message_loop();
  if (blocking)
    loop->PostTask(FROM_HERE, NewRunnableFunction(CreateSleepAssertion));
  else
    loop->PostTask(FROM_HERE, NewRunnableFunction(ReleaseSleepAssertion));
}
