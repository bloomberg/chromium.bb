// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"

class AppApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableApps);
  }
};

// Simulates a page calling window.open on an URL, and waits for the navigation.
static void WindowOpenHelper(Browser* browser,
                             RenderViewHost* opener_host, const GURL& url) {
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      opener_host, L"",
      L"window.open('" + UTF8ToWide(url.spec()) + L"');"
      L"window.domAutomationController.send(true);",
      &result);
  ASSERT_TRUE(result);

  // Now the current tab should be the new tab.
  TabContents* newtab = browser->GetSelectedTabContents();
  if (!newtab->controller().GetLastCommittedEntry() ||
      newtab->controller().GetLastCommittedEntry()->url() != url)
    ui_test_utils::WaitForNavigation(&newtab->controller());
  EXPECT_EQ(url, newtab->controller().GetLastCommittedEntry()->url());
}

// Simulates a page navigating itself to an URL, and waits for the navigation.
static void NavigateTabHelper(TabContents* contents, const GURL& url) {
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->render_view_host(), L"",
      L"window.addEventListener('unload', function() {"
      L"    window.domAutomationController.send(true);"
      L"}, false);"
      L"window.location = '" + UTF8ToWide(url.spec()) + L"';",
      &result);
  ASSERT_TRUE(result);

  if (!contents->controller().GetLastCommittedEntry() ||
      contents->controller().GetLastCommittedEntry()->url() != url)
    ui_test_utils::WaitForNavigation(&contents->controller());
  EXPECT_EQ(url, contents->controller().GetLastCommittedEntry()->url());
}

IN_PROC_BROWSER_TEST_F(AppApiTest, AppProcess) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(StartHTTPServer());

  ASSERT_TRUE(RunExtensionTest("app_process")) << message_;

  Extension* extension = GetSingleLoadedExtension();
  ExtensionHost* host = browser()->profile()->GetExtensionProcessManager()->
      GetBackgroundHostForExtension(extension);
  ASSERT_TRUE(host);

  // The extension should have opened 3 new tabs. Including the original blank
  // tab, we now have 4 tabs. Two should be part of the extension app, and
  // grouped in the extension process.
  ASSERT_EQ(4, browser()->tab_count());
  EXPECT_EQ(host->render_process_host(),
            browser()->GetTabContentsAt(1)->render_view_host()->process());
  EXPECT_EQ(host->render_process_host(),
            browser()->GetTabContentsAt(2)->render_view_host()->process());
  EXPECT_NE(host->render_process_host(),
            browser()->GetTabContentsAt(3)->render_view_host()->process());

  // Now let's do the same using window.open. The same should happen.
  GURL base_url("http://localhost:1337/files/extensions/api_test/app_process/");
  WindowOpenHelper(browser(), host->render_view_host(),
                   base_url.Resolve("path1/empty.html"));
  WindowOpenHelper(browser(), host->render_view_host(),
                   base_url.Resolve("path2/empty.html"));
  WindowOpenHelper(browser(), host->render_view_host(),
                   base_url.Resolve("path3/empty.html"));

  ASSERT_EQ(7, browser()->tab_count());
  EXPECT_EQ(host->render_process_host(),
            browser()->GetTabContentsAt(4)->render_view_host()->process());
  EXPECT_EQ(host->render_process_host(),
            browser()->GetTabContentsAt(5)->render_view_host()->process());
  EXPECT_NE(host->render_process_host(),
            browser()->GetTabContentsAt(6)->render_view_host()->process());

  // Now let's have these pages navigate, into or out of the extension web
  // extent. They should switch processes.
  const GURL& app_url(base_url.Resolve("path1/empty.html"));
  const GURL& non_app_url(base_url.Resolve("path3/empty.html"));
  NavigateTabHelper(browser()->GetTabContentsAt(1), non_app_url);
  NavigateTabHelper(browser()->GetTabContentsAt(3), app_url);
  EXPECT_NE(host->render_process_host(),
            browser()->GetTabContentsAt(1)->render_view_host()->process());
  EXPECT_EQ(host->render_process_host(),
            browser()->GetTabContentsAt(3)->render_view_host()->process());

  // Navigate the non-app tab into the browse extent. It should not enter the
  // app process.
  // Navigate the app tab into the browse extent. It should stay in the app
  // process.
  const GURL& browse_url(base_url.Resolve("path4/empty.html"));
  NavigateTabHelper(browser()->GetTabContentsAt(1), browse_url);
  NavigateTabHelper(browser()->GetTabContentsAt(3), browse_url);
  EXPECT_NE(host->render_process_host(),
            browser()->GetTabContentsAt(1)->render_view_host()->process());
  EXPECT_EQ(host->render_process_host(),
            browser()->GetTabContentsAt(3)->render_view_host()->process());
}
