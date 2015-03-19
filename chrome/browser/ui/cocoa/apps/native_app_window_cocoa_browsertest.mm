// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests whichever implementation of NativeAppWindow is used.
// I.e. it could be NativeAppWindowCocoa or ChromeNativeAppWindowViewsMac.
#include "extensions/browser/app_window/native_app_window.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/constants.h"

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
          AppLaunchParams(profile(), app_, extensions::LAUNCH_CONTAINER_NONE,
                          NEW_WINDOW, extensions::SOURCE_TEST));
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
  extensions::AppWindowRegistry::AppWindowList windows =
      extensions::AppWindowRegistry::Get(profile())->app_windows();

  extensions::AppWindow* app_window = windows.front();
  extensions::NativeAppWindow* native_window = app_window->GetBaseWindow();
  NSWindow* ns_window = native_window->GetNativeWindow();

  extensions::AppWindow* other_app_window = windows.back();
  extensions::NativeAppWindow* other_native_window =
      other_app_window->GetBaseWindow();
  NSWindow* other_ns_window = other_native_window->GetNativeWindow();

  // Normal Hide/Show.
  app_window->Hide();
  EXPECT_FALSE([ns_window isVisible]);
  app_window->Show(extensions::AppWindow::SHOW_ACTIVE);
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
  app_window->Show(extensions::AppWindow::SHOW_ACTIVE);
  EXPECT_TRUE([ns_window isVisible]);

  // HideWithApp the other window.
  EXPECT_TRUE([other_ns_window isVisible]);
  other_native_window->HideWithApp();
  EXPECT_FALSE([other_ns_window isVisible]);

  // HideWithApp, Show shows all windows for this app.
  native_window->HideWithApp();
  EXPECT_FALSE([ns_window isVisible]);
  app_window->Show(extensions::AppWindow::SHOW_ACTIVE);
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

@interface ScopedNotificationWatcher : NSObject {
 @private
  BOOL received_;
}
- (id)initWithNotification:(NSString*)notification
                 andObject:(NSObject*)object;
- (void)onNotification:(NSString*)notification;
- (void)waitForNotification;
@end

@implementation ScopedNotificationWatcher

- (id)initWithNotification:(NSString*)notification
                 andObject:(NSObject*)object {
  if ((self = [super init])) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onNotification:)
               name:notification
             object:object];
  }
  return self;
}

- (void)onNotification:(NSString*)notification {
  received_ = YES;
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)waitForNotification {
  while (!received_)
    content::RunAllPendingInMessageLoop();
}

@end

// Test that NativeAppWindow and AppWindow fullscreen state is updated when
// the window is fullscreened natively.
IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, Fullscreen) {
  if (!base::mac::IsOSLionOrLater())
    return;

  SetUpAppWithWindows(1);
  extensions::AppWindow* app_window = GetFirstAppWindow();
  extensions::NativeAppWindow* window = app_window->GetBaseWindow();
  NSWindow* ns_window = app_window->GetNativeWindow();
  base::scoped_nsobject<ScopedNotificationWatcher> watcher;

  EXPECT_EQ(extensions::AppWindow::FULLSCREEN_TYPE_NONE,
            app_window->fullscreen_types_for_test());
  EXPECT_FALSE(window->IsFullscreen());
  EXPECT_FALSE([ns_window styleMask] & NSFullScreenWindowMask);

  watcher.reset([[ScopedNotificationWatcher alloc]
      initWithNotification:NSWindowDidEnterFullScreenNotification
                 andObject:ns_window]);
  [ns_window toggleFullScreen:nil];
  [watcher waitForNotification];
  EXPECT_TRUE(app_window->fullscreen_types_for_test() &
      extensions::AppWindow::FULLSCREEN_TYPE_OS);
  EXPECT_TRUE(window->IsFullscreen());
  EXPECT_TRUE([ns_window styleMask] & NSFullScreenWindowMask);

  watcher.reset([[ScopedNotificationWatcher alloc]
      initWithNotification:NSWindowDidExitFullScreenNotification
                 andObject:ns_window]);
  app_window->Restore();
  EXPECT_FALSE(window->IsFullscreenOrPending());
  [watcher waitForNotification];
  EXPECT_EQ(extensions::AppWindow::FULLSCREEN_TYPE_NONE,
            app_window->fullscreen_types_for_test());
  EXPECT_FALSE(window->IsFullscreen());
  EXPECT_FALSE([ns_window styleMask] & NSFullScreenWindowMask);

  watcher.reset([[ScopedNotificationWatcher alloc]
      initWithNotification:NSWindowDidEnterFullScreenNotification
                 andObject:ns_window]);
  app_window->Fullscreen();
  EXPECT_TRUE(window->IsFullscreenOrPending());
  [watcher waitForNotification];
  EXPECT_TRUE(app_window->fullscreen_types_for_test() &
      extensions::AppWindow::FULLSCREEN_TYPE_WINDOW_API);
  EXPECT_TRUE(window->IsFullscreen());
  EXPECT_TRUE([ns_window styleMask] & NSFullScreenWindowMask);

  watcher.reset([[ScopedNotificationWatcher alloc]
      initWithNotification:NSWindowDidExitFullScreenNotification
                 andObject:ns_window]);
  [ns_window toggleFullScreen:nil];
  [watcher waitForNotification];
  EXPECT_EQ(extensions::AppWindow::FULLSCREEN_TYPE_NONE,
            app_window->fullscreen_types_for_test());
  EXPECT_FALSE(window->IsFullscreen());
  EXPECT_FALSE([ns_window styleMask] & NSFullScreenWindowMask);
}

// Test that, in frameless windows, the web contents has the same size as the
// window.
IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, Frameless) {
  extensions::AppWindow* app_window =
      CreateTestAppWindow("{\"frame\": \"none\"}");
  NSWindow* ns_window = app_window->GetNativeWindow();
  NSView* web_contents = app_window->web_contents()->GetNativeView();
  EXPECT_TRUE(NSEqualSizes(NSMakeSize(512, 384), [web_contents frame].size));
  // Move and resize the window.
  NSRect new_frame = NSMakeRect(50, 50, 200, 200);
  [ns_window setFrame:new_frame display:YES];
  EXPECT_TRUE(NSEqualSizes(new_frame.size, [web_contents frame].size));

  // Windows created with NSBorderlessWindowMask by default don't have shadow,
  // but packaged apps should always have one.
  EXPECT_TRUE([ns_window hasShadow]);

  // Since the window has no constraints, it should have all of the following
  // style mask bits.
  NSUInteger style_mask = NSTitledWindowMask | NSClosableWindowMask |
                          NSMiniaturizableWindowMask | NSResizableWindowMask |
                          NSTexturedBackgroundWindowMask;
  EXPECT_EQ(style_mask, [ns_window styleMask]);

  CloseAppWindow(app_window);
}
