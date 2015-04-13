// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_
#define CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"

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

 private:
  friend class AfterStartupTaskTest;
  FRIEND_TEST_ALL_PREFIXES(AfterStartupTaskTest, IsStartupComplete);
  FRIEND_TEST_ALL_PREFIXES(AfterStartupTaskTest, PostTask);

  static bool IsBrowserStartupComplete();
  static void SetBrowserStartupIsComplete();
  static void UnsafeResetForTesting();

  DISALLOW_IMPLICIT_CONSTRUCTORS(AfterStartupTaskUtils);
};

#endif  // CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_
