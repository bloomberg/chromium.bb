// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hung_renderer_view.h"

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

class HungRendererDialogViewBrowserTest : public DialogBrowserTest {
 public:
  HungRendererDialogViewBrowserTest() {}

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    TabDialogs::FromWebContents(web_contents)
        ->ShowHungRendererDialog(content::WebContentsUnresponsiveState());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HungRendererDialogViewBrowserTest);
};

class HungRendererViewNavigationBrowserTest : public InProcessBrowserTest {
 public:
  HungRendererViewNavigationBrowserTest() {}
  ~HungRendererViewNavigationBrowserTest() override {}

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HungRendererViewNavigationBrowserTest);
};

// Invokes the hung renderer (aka page unresponsive) dialog. See
// test_browser_dialog.h.
// TODO(tapted): The framework sometimes doesn't pick up the spawned dialog and
// the ASSERT_EQ in TestBrowserDialog::RunDialog() fails. This seems to only
// happen on the bots. So the test is disabled for now.
IN_PROC_BROWSER_TEST_F(HungRendererDialogViewBrowserTest,
                       DISABLED_InvokeDialog_default) {
  RunDialog();
}

// Verify that a cross-process navigation will dismiss the hung renderer
// dialog so that we do not kill the new (responsive) process.
IN_PROC_BROWSER_TEST_F(HungRendererViewNavigationBrowserTest,
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
  EXPECT_FALSE(HungRendererDialogView::GetInstance());
}
