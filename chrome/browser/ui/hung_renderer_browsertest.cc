// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents_unresponsive_state.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/ui_features.h"

class HungRendererNavigationBrowserTest : public InProcessBrowserTest {
 public:
  HungRendererNavigationBrowserTest() {}
  ~HungRendererNavigationBrowserTest() override {}

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HungRendererNavigationBrowserTest);
};

// Verify that a cross-process navigation will dismiss the hung renderer
// dialog so that we do not kill the new (responsive) process.
IN_PROC_BROWSER_TEST_F(HungRendererNavigationBrowserTest,
                       HungRendererWithCrossProcessNavigation) {
  EXPECT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title1.html"));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TabDialogs::FromWebContents(active_web_contents)
      ->ShowHungRendererDialog(content::WebContentsUnresponsiveState());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.com", "/title2.html"));
  // Expect that the dialog has been dismissed.
  EXPECT_FALSE(TabDialogs::FromWebContents(active_web_contents)
                   ->IsShowingHungRendererDialog());
}
