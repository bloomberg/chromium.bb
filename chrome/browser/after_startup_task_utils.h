// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_
#define CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"

namespace android {
class AfterStartupTaskUtilsJNI;
}

namespace base {
class TaskRunner;
}
namespace tracked_objects {
class Location;
};

class AfterStartupTaskUtils {
 public:
  // Observes startup and when complete runs tasks that have accrued.
  static void StartMonitoringStartup();

  // Used to augment the behavior of BrowserThread::PostAfterStartupTask
  // for chrome. Tasks are queued until startup is complete.
  // Note: see browser_thread.h
  static void PostTask(const tracked_objects::Location& from_here,
                       const scoped_refptr<base::TaskRunner>& task_runner,
                       const base::Closure& task);

  // Returns true if browser startup is complete. Only use this on a one-off
  // basis; If you need to poll this function constantly, use the above
  // PostTask() API instead.
  static bool IsBrowserStartupComplete();

 private:
  friend class AfterStartupTaskTest;
  friend class android::AfterStartupTaskUtilsJNI;
  friend class InProcessBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(AfterStartupTaskTest, IsStartupComplete);
  FRIEND_TEST_ALL_PREFIXES(AfterStartupTaskTest, PostTask);

  static void SetBrowserStartupIsComplete();
  static void UnsafeResetForTesting();

  DISALLOW_IMPLICIT_CONSTRUCTORS(AfterStartupTaskUtils);
};

#endif  // CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_
