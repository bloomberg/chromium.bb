// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_views.h"

#include "base/run_loop.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/views/widget/widget.h"

// Browser Test for AppListService on Views platforms.
typedef InProcessBrowserTest AppListServiceViewsBrowserTest;

// Test closing the native app list window as if via a request from the OS.
IN_PROC_BROWSER_TEST_F(AppListServiceViewsBrowserTest, NativeClose) {
  AppListService* service = test::GetAppListService();
  EXPECT_FALSE(service->GetAppListWindow());

  // Since the profile is loaded, this will create a view immediately. This is
  // important, because anything asynchronous would need an interactive_uitest
  // due to the possibility of the app list being dismissed, and
  // AppListService::GetAppListWindow returning NULL.
  service->ShowForProfile(browser()->profile());
  gfx::NativeWindow window = service->GetAppListWindow();
  EXPECT_TRUE(window);

  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  EXPECT_TRUE(widget);
  widget->Close();

  // Close is asynchronous (dismiss is not) so sink the message queue.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service->GetAppListWindow());

  // Show again to get some code coverage for possibly stale pointers.
  service->ShowForProfile(browser()->profile());
  EXPECT_TRUE(service->GetAppListWindow());
  service->DismissAppList();  // Note: in Ash, this will invalidate the window.

  // Note: no need to sink message queue.
  EXPECT_FALSE(service->GetAppListWindow());
}
