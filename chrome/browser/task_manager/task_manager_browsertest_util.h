// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSERTEST_UTIL_H_
#pragma once

#include "base/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/ui_test_utils.h"
#include "content/common/notification_source.h"

class TaskManagerBrowserTestUtil {
 public:
  static void WaitForResourceChange(int target_count);

  static void ShowTaskManagerAndWaitForReady(Browser* browser);

  // Wait for any pending BackgroundContents to finish starting up.
  static void WaitForBackgroundContents(Browser* browser);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSERTEST_UTIL_H_
