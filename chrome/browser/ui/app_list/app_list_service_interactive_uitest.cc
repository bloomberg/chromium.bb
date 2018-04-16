// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"

// Interactive UI Test for AppListService that runs on all platforms supporting
// app_list. Interactive because the app list uses focus changes to dismiss
// itself, which will cause tests that check the visibility to fail flakily.
using AppListServiceInteractiveTest = InProcessBrowserTest;

// Show the app list, then dismiss it.
IN_PROC_BROWSER_TEST_F(AppListServiceInteractiveTest, ShowAndDismiss) {
  AppListService* service = AppListService::Get();
  ASSERT_FALSE(service->IsAppListVisible());
  service->Show();
  service->FlushForTesting();
  ASSERT_TRUE(service->IsAppListVisible());
  service->DismissAppList();
  service->FlushForTesting();
  ASSERT_FALSE(service->IsAppListVisible());
}
