// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/dev_tools_controller.h"

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"

class DevToolsControllerTest : public InProcessBrowserTest {
 public:
  DevToolsControllerTest() : InProcessBrowserTest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    DevToolsWindow::OpenDevToolsWindowForTest(browser(), true);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsControllerTest);
};

// Verify that AllowOverlappingViews is set while the find bar is visible.
IN_PROC_BROWSER_TEST_F(DevToolsControllerTest, AllowOverlappingViews) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContents* dev_tools =
      DevToolsWindow::GetInTabWebContents(web_contents, NULL);

  // Without the find bar.
  EXPECT_TRUE(dev_tools->GetAllowOverlappingViews());

  // With the find bar.
  browser()->GetFindBarController()->find_bar()->Show(false);
  EXPECT_TRUE(dev_tools->GetAllowOverlappingViews());

  // Without the find bar.
  browser()->GetFindBarController()->find_bar()->Hide(false);
  EXPECT_TRUE(dev_tools->GetAllowOverlappingViews());
}
