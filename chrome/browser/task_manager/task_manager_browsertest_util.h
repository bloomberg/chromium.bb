// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSERTEST_UTIL_H_

#include "chrome/browser/ui/browser.h"

class TaskManagerBrowserTestUtil {
 public:
  static void WaitForWebResourceChange(int target_count);

  static void ShowTaskManagerAndWaitForReady(Browser* browser);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSERTEST_UTIL_H_
