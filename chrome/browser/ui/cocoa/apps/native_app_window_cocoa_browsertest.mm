// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/apps/native_app_window_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "apps/app_window_registry.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

using extensions::PlatformAppBrowserTest;

namespace {

class NativeAppWindowCocoaBrowserTest : public PlatformAppBrowserTest {
 protected:
  NativeAppWindowCocoaBrowserTest() {}

  void SetUpAppWithWindows(int num_windows) {
    app_ = InstallExtension(
        test_data_dir_.AppendASCII("platform_apps").AppendASCII("minimal"), 1);
    EXPECT_TRUE(app_);

    for (int i = 0; i < num_windows; ++i) {
      content::WindowedNotificationObserver app_loaded_observer(
          content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
          content::NotificationService::AllSources());
      OpenApplication(
          AppLaunchParams(profile(),
                          app_,
                          extensions::LAUNCH_CONTAINER_NONE,
                          NEW_WINDOW));
      app_loaded_observer.Wait();
    }
  }

  const extensions::Extension* app_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeAppWindowCocoaBrowserTest);
};

}  // namespace

// Test interaction of Hide/Show() with Hide/ShowWithApp().
IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, HideShowWithApp) {
  SetUpAppWithWindows(2);
  apps::AppWindowRegistry::AppWindowList windows =
      apps::AppWindowRegistry::Get(profile())->app_windows();

  apps::AppWindow* app_window = windows.front();
  apps::NativeAppWindow* native_window = app_window->GetBaseWindow();
  NSWindow* ns_window = native_window->GetNativeWindow();

  apps::AppWindow* other_app_window = windows.back();
  apps::NativeAppWindow* other_native_window =
      other_app_window->GetBaseWindow();
  NSWindow* other_ns_window = other_native_window->GetNativeWindow();

  // Normal Hide/Show.
  app_window->Hide();
  EXPECT_FALSE([ns_window isVisible]);
  app_window->Show(apps::AppWindow::SHOW_ACTIVE);
  EXPECT_TRUE([ns_window isVisible]);

  // Normal Hide/ShowWithApp.
  native_window->HideWithApp();
  EXPECT_FALSE([ns_window isVisible]);
  native_window->ShowWithApp();
  EXPECT_TRUE([ns_window isVisible]);

  // HideWithApp, Hide, ShowWithApp does not show.
  native_window->HideWithApp();
  app_window->Hide();
  native_window->ShowWithApp();
  EXPECT_FALSE([ns_window isVisible]);

  // Hide, HideWithApp, ShowWithApp does not show.
  native_window->HideWithApp();
  native_window->ShowWithApp();
  EXPECT_FALSE([ns_window isVisible]);

  // Return to shown state.
  app_window->Show(apps::AppWindow::SHOW_ACTIVE);
  EXPECT_TRUE([ns_window isVisible]);

  // HideWithApp the other window.
  EXPECT_TRUE([other_ns_window isVisible]);
  other_native_window->HideWithApp();
  EXPECT_FALSE([other_ns_window isVisible]);

  // HideWithApp, Show shows all windows for this app.
  native_window->HideWithApp();
  EXPECT_FALSE([ns_window isVisible]);
  app_window->Show(apps::AppWindow::SHOW_ACTIVE);
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_TRUE([other_ns_window isVisible]);

  // Hide the other window.
  other_app_window->Hide();
  EXPECT_FALSE([other_ns_window isVisible]);

  // HideWithApp, ShowWithApp does not show the other window.
  native_window->HideWithApp();
  EXPECT_FALSE([ns_window isVisible]);
  native_window->ShowWithApp();
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_FALSE([other_ns_window isVisible]);
}
