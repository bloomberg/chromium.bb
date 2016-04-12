// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

class SitePerProcessInteractiveBrowserTest : public InProcessBrowserTest {
 public:
  SitePerProcessInteractiveBrowserTest() {}
  ~SitePerProcessInteractiveBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data for cross_site_iframe_factory.html
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SitePerProcessInteractiveBrowserTest);
};

// Check that document.hasFocus() works properly with out-of-process iframes.
// The test builds a page with four cross-site frames and then focuses them one
// by one, checking the value of document.hasFocus() in all frames.  For any
// given focused frame, document.hasFocus() should return true for that frame
// and all its ancestor frames.
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest, DocumentHasFocus) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),d)"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  content::RenderFrameHost* child1 = ChildFrameAt(main_frame, 0);
  ASSERT_NE(nullptr, child1);
  content::RenderFrameHost* child2 = ChildFrameAt(main_frame, 1);
  ASSERT_NE(nullptr, child2);
  content::RenderFrameHost* grandchild = ChildFrameAt(child1, 0);
  ASSERT_NE(nullptr, grandchild);

  EXPECT_NE(main_frame->GetSiteInstance(), child1->GetSiteInstance());
  EXPECT_NE(main_frame->GetSiteInstance(), child2->GetSiteInstance());
  EXPECT_NE(child1->GetSiteInstance(), grandchild->GetSiteInstance());

  // Helper function to check document.hasFocus() for a given frame.
  auto document_has_focus = [](content::RenderFrameHost* rfh) -> bool {
    bool has_focus = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        rfh,
        "window.domAutomationController.send(document.hasFocus())",
        &has_focus));
    return has_focus;
  };

  // The main frame should be focused to start with.
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());

  EXPECT_TRUE(document_has_focus(main_frame));
  EXPECT_FALSE(document_has_focus(child1));
  EXPECT_FALSE(document_has_focus(grandchild));
  EXPECT_FALSE(document_has_focus(child2));

  EXPECT_TRUE(ExecuteScript(child1, "window.focus();"));
  EXPECT_EQ(child1, web_contents->GetFocusedFrame());

  EXPECT_TRUE(document_has_focus(main_frame));
  EXPECT_TRUE(document_has_focus(child1));
  EXPECT_FALSE(document_has_focus(grandchild));
  EXPECT_FALSE(document_has_focus(child2));

  EXPECT_TRUE(ExecuteScript(grandchild, "window.focus();"));
  EXPECT_EQ(grandchild, web_contents->GetFocusedFrame());

  EXPECT_TRUE(document_has_focus(main_frame));
  EXPECT_TRUE(document_has_focus(child1));
  EXPECT_TRUE(document_has_focus(grandchild));
  EXPECT_FALSE(document_has_focus(child2));

  EXPECT_TRUE(ExecuteScript(child2, "window.focus();"));
  EXPECT_EQ(child2, web_contents->GetFocusedFrame());

  EXPECT_TRUE(document_has_focus(main_frame));
  EXPECT_FALSE(document_has_focus(child1));
  EXPECT_FALSE(document_has_focus(grandchild));
  EXPECT_TRUE(document_has_focus(child2));
}

