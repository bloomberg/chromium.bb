// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#import "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"

namespace {

class AppControllerPlatformAppBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerPlatformAppBrowserTest()
      : native_browser_list(BrowserList::GetInstance(
                                chrome::HOST_DESKTOP_TYPE_NATIVE)) {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kAppId,
                                    "1234");
  }

  // Mac only has the native desktop.
  const BrowserList* native_browser_list;
};

// Test that if only a platform app window is open and no browser windows are
// open then a reopen event does nothing.
IN_PROC_BROWSER_TEST_F(AppControllerPlatformAppBrowserTest,
                       PlatformAppReopenWithWindows) {
  scoped_nsobject<AppController> ac([[AppController alloc] init]);
  NSUInteger old_window_count = [[NSApp windows] count];
  EXPECT_EQ(1u, native_browser_list->size());
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:YES];

  EXPECT_TRUE(result);
  EXPECT_EQ(old_window_count, [[NSApp windows] count]);
  EXPECT_EQ(1u, native_browser_list->size());
}

class AppControllerWebAppBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerWebAppBrowserTest()
      : native_browser_list(BrowserList::GetInstance(
                                chrome::HOST_DESKTOP_TYPE_NATIVE)) {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kApp, GetAppURL());
  }

  std::string GetAppURL() const {
    return "http://example.com/";
  }

  // Mac only has the native desktop.
  const BrowserList* native_browser_list;
};

// Test that in web app mode a reopen event opens the app URL.
IN_PROC_BROWSER_TEST_F(AppControllerWebAppBrowserTest,
                       WebAppReopenWithNoWindows) {
  scoped_nsobject<AppController> ac([[AppController alloc] init]);
  EXPECT_EQ(1u, native_browser_list->size());
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];

  EXPECT_FALSE(result);
  EXPECT_EQ(2u, native_browser_list->size());

  Browser* browser = native_browser_list->get(0);
  GURL current_url =
      browser->tab_strip_model()->GetActiveWebContents()->GetURL();
  EXPECT_EQ(GetAppURL(), current_url.spec());
}

}  // namespace
