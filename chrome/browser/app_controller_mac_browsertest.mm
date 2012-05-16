// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/browser_list.h"
#import "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"

namespace {

class AppControllerPlatformAppBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerPlatformAppBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAppId,
                                    "1234");
  }
};

// Test that if only a platform app window is open and no browser windows are
// open then a reopen event does nothing.
IN_PROC_BROWSER_TEST_F(AppControllerPlatformAppBrowserTest,
                       PlatformAppReopenWithWindows) {
  scoped_nsobject<AppController> ac([[AppController alloc] init]);
  NSUInteger old_window_count = [[NSApp windows] count];
  EXPECT_EQ(1u, BrowserList::size());
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:YES];

  EXPECT_TRUE(result);
  EXPECT_EQ(old_window_count, [[NSApp windows] count]);
  EXPECT_EQ(1u, BrowserList::size());
}

class AppControllerWebAppBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerWebAppBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kApp,
                                    GetAppURL());
  }

  std::string GetAppURL() const {
    return "http://example.com/";
  }
};

// Test that in web app mode a reopen event opens the app URL.
IN_PROC_BROWSER_TEST_F(AppControllerWebAppBrowserTest,
                       WebAppReopenWithNoWindows) {
  scoped_nsobject<AppController> ac([[AppController alloc] init]);
  EXPECT_EQ(1u, BrowserList::size());
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];

  EXPECT_FALSE(result);
  EXPECT_EQ(2u, BrowserList::size());

  Browser* browser = *(BrowserList::begin());
  GURL current_url = browser->GetSelectedWebContents()->GetURL();
  EXPECT_EQ(GetAppURL(), current_url.spec());
}

}  // namespace
