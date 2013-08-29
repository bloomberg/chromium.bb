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
#include "content/public/browser/web_contents_view.h"

class DevToolsControllerTest : public InProcessBrowserTest {
 public:
  DevToolsControllerTest() : InProcessBrowserTest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    DevToolsWindow::ToggleDevToolsWindow(browser(),
                                         DEVTOOLS_TOGGLE_ACTION_SHOW);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsControllerTest);
};

// Verify that AllowOverlappingViews is set while the find bar is visible.
IN_PROC_BROWSER_TEST_F(DevToolsControllerTest, AllowOverlappingViews) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DevToolsWindow* dev_tools =
      DevToolsWindow::GetDockedInstanceForInspectedTab(web_contents);
  content::WebContentsView* dev_tools_view =
      dev_tools->web_contents()->GetView();

  // Without the find bar.
  EXPECT_TRUE(dev_tools_view->GetAllowOverlappingViews());

  // With the find bar.
  browser()->GetFindBarController()->find_bar()->Show(false);
  EXPECT_TRUE(dev_tools_view->GetAllowOverlappingViews());

  // Without the find bar.
  browser()->GetFindBarController()->find_bar()->Hide(false);
  EXPECT_TRUE(dev_tools_view->GetAllowOverlappingViews());
}
