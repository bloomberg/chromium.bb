// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/content_notification_types.h"
#include "googleurl/src/gurl.h"
#include "net/base/mock_host_resolver.h"

class WebstoreInlineInstallTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    EnableDOMAutomation();

    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAppsGalleryURL, "http://cws.com");
    command_line->AppendSwitch(switches::kEnableInlineWebstoreInstall);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    host_resolver()->AddRule("cws.com", "127.0.0.1");
    host_resolver()->AddRule("app.com", "127.0.0.1");
    ASSERT_TRUE(test_server()->Start());
  }

 protected:
  GURL GetPageUrl(const std::string& page_filename) {
   GURL page_url = test_server()->GetURL(
          "files/extensions/api_test/webstore_inline_install/" + page_filename);

    GURL::Replacements replace_host;
    std::string host_str("app.com");
    replace_host.SetHostStr(host_str);
    return page_url.ReplaceComponents(replace_host);
  }
};

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallTest, Install) {
  ui_test_utils::NavigateToURL(browser(), GetPageUrl("install.html"));

  bool result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"runTest()", &result));
  EXPECT_TRUE(result);

  // The "inline" UI right now is just the store entry in a new tab.
  if (browser()->tabstrip_model()->count() == 1) {
    ui_test_utils::WaitForNewTab(browser());
  }

  TabContents* tab_contents = browser()->GetSelectedTabContents();
  EXPECT_EQ(
      GURL("http://cws.com/detail/mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm"),
      tab_contents->GetURL());
}

IN_PROC_BROWSER_TEST_F(WebstoreInlineInstallTest, FindLink) {
  ui_test_utils::NavigateToURL(browser(), GetPageUrl("find_link.html"));

  bool result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"runTest()", &result));
  EXPECT_TRUE(result);
}
