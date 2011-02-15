// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/test/testing_browser_process.h"
#include "testing/platform_test.h"

class AppControllerTest : public PlatformTest {
 private:
  ScopedTestingBrowserProcess browser_process_;
};

TEST_F(AppControllerTest, DockMenu) {
  scoped_nsobject<AppController> ac([[AppController alloc] init]);
  NSMenu* menu = [ac applicationDockMenu:NSApp];
  NSMenuItem* item;

  EXPECT_TRUE(menu);
  EXPECT_NE(-1, [menu indexOfItemWithTag:IDC_NEW_WINDOW]);
  EXPECT_NE(-1, [menu indexOfItemWithTag:IDC_NEW_INCOGNITO_WINDOW]);
  for (item in [menu itemArray]) {
    EXPECT_EQ(ac.get(), [item target]);
    EXPECT_EQ(@selector(commandFromDock:), [item action]);
  }
}
