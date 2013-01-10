// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/test/base/in_process_browser_test.h"

// Browser Test for AppListController that runs on all platforms supporting
// app_list.
class AppListControllerBrowserTest : public InProcessBrowserTest {
 public:
  AppListControllerBrowserTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListControllerBrowserTest);
};

// Disabled on Windows. Investigating in http://crbug.com/169114 .
#if defined(OS_WIN)
#define MAYBE_ShowAndShutdown DISABLED_ShowAndShutdown
#else
#define MAYBE_ShowAndShutdown ShowAndShutdown
#endif

// Test showing the app list, followed by browser close.
IN_PROC_BROWSER_TEST_F(AppListControllerBrowserTest, MAYBE_ShowAndShutdown) {
  chrome::ShowAppList();
}
