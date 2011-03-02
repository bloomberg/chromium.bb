// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "net/base/mock_host_resolver.h"

class AppApiTest : public ExtensionApiTest {
};

// Simulates a page calling window.open on an URL, and waits for the navigation.
static void WindowOpenHelper(Browser* browser,
                             RenderViewHost* opener_host,
                             const GURL& url,
                             bool newtab_process_should_equal_opener) {
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      opener_host, L"", L"window.open('" + UTF8ToWide(url.spec()) + L"');"));

  // The above window.open call is not user-initiated, it will create
  // a popup window instead of a new tab in current window.
  // Now the active tab in last active window should be the new tab.
  Browser* last_active_browser = BrowserList::GetLastActive();
  EXPECT_TRUE(last_active_browser);
  TabContents* newtab = last_active_browser->GetSelectedTabContents();
  EXPECT_TRUE(newtab);
  if (!newtab->controller().GetLastCommittedEntry() ||
      newtab->controller().GetLastCommittedEntry()->url() != url)
    ui_test_utils::WaitForNavigation(&newtab->controller());
  EXPECT_EQ(url, newtab->controller().GetLastCommittedEntry()->url());
  if (newtab_process_should_equal_opener)
    EXPECT_EQ(opener_host->process(), newtab->render_view_host()->process());
  else
    EXPECT_NE(opener_host->process(), newtab->render_view_host()->process());
}

// Simulates a page navigating itself to an URL, and waits for the navigation.
static void NavigateTabHelper(TabContents* contents, const GURL& url) {
  bool result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->render_view_host(), L"",
      L"window.addEventListener('unload', function() {"
      L"    window.domAutomationController.send(true);"
      L"}, false);"
      L"window.location = '" + UTF8ToWide(url.spec()) + L"';",
      &result));
  ASSERT_TRUE(result);

  if (!contents->controller().GetLastCommittedEntry() ||
      contents->controller().GetLastCommittedEntry()->url() != url)
    ui_test_utils::WaitForNavigation(&contents->controller());
  EXPECT_EQ(url, contents->controller().GetLastCommittedEntry()->url());
}

IN_PROC_BROWSER_TEST_F(AppApiTest, AppProcess) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app_process")));

  // Open two tabs in the app, one outside it.
  GURL base_url = test_server()->GetURL(
      "files/extensions/api_test/app_process/");

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // must stay in scope with replace_host
  replace_host.SetHostStr(host_str);
  base_url = base_url.ReplaceComponents(replace_host);

  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path2/empty.html"));
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path3/empty.html"));

  // The extension should have opened 3 new tabs. Including the original blank
  // tab, we now have 4 tabs. Two should be part of the extension app, and
  // grouped in the same process.
  ASSERT_EQ(4, browser()->tab_count());
  RenderViewHost* host = browser()->GetTabContentsAt(1)->render_view_host();

  EXPECT_EQ(host->process(),
            browser()->GetTabContentsAt(2)->render_view_host()->process());
  EXPECT_NE(host->process(),
            browser()->GetTabContentsAt(3)->render_view_host()->process());

  // Now let's do the same using window.open. The same should happen.
  ASSERT_EQ(1u, BrowserList::GetBrowserCount(browser()->profile()));
  WindowOpenHelper(browser(), host,
                   base_url.Resolve("path1/empty.html"), true);
  WindowOpenHelper(browser(), host,
                   base_url.Resolve("path2/empty.html"), true);
  // TODO(creis): This should open in a new process (i.e., false for the last
  // argument), but we temporarily avoid swapping processes away from an app
  // until we're able to restore window.opener if the page later returns to an
  // in-app URL.  See crbug.com/65953.
  WindowOpenHelper(browser(), host,
                   base_url.Resolve("path3/empty.html"), true);

  // Now let's have these pages navigate, into or out of the extension web
  // extent. They should switch processes.
  const GURL& app_url(base_url.Resolve("path1/empty.html"));
  const GURL& non_app_url(base_url.Resolve("path3/empty.html"));
  NavigateTabHelper(browser()->GetTabContentsAt(2), non_app_url);
  NavigateTabHelper(browser()->GetTabContentsAt(3), app_url);
  // TODO(creis): This should swap out of the app's process (i.e., EXPECT_NE),
  // but we temporarily avoid swapping away from an app in case it needs to
  // communicate with window.opener later.  See crbug.com/65953.
  EXPECT_EQ(host->process(),
            browser()->GetTabContentsAt(2)->render_view_host()->process());
  EXPECT_EQ(host->process(),
            browser()->GetTabContentsAt(3)->render_view_host()->process());

  // If one of the popup tabs navigates back to the app, window.opener should
  // be valid.
  NavigateTabHelper(browser()->GetTabContentsAt(6), app_url);
  EXPECT_EQ(host->process(),
            browser()->GetTabContentsAt(6)->render_view_host()->process());
  bool windowOpenerValid = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetTabContentsAt(6)->render_view_host(), L"",
      L"window.domAutomationController.send(window.opener != null)",
      &windowOpenerValid));
  ASSERT_TRUE(windowOpenerValid);
}

// Tests that app process switching works properly in the following scenario:
// 1. navigate to a page1 in the app
// 2. page1 redirects to a page2 outside the app extent (ie, "/server-redirect")
// 3. page2 redirects back to a page in the app
// The final navigation should end up in the app process.
// See http://crbug.com/61757
IN_PROC_BROWSER_TEST_F(AppApiTest, AppProcessRedirectBack) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app_process")));

  // Open two tabs in the app.
  GURL base_url = test_server()->GetURL(
      "files/extensions/api_test/app_process/");

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // must stay in scope with replace_host
  replace_host.SetHostStr(host_str);
  base_url = base_url.ReplaceComponents(replace_host);

  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  browser()->NewTab();
  // Wait until the second tab finishes its redirect train (2 hops).
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), base_url.Resolve("path1/redirect.html"), 2);

  // 3 tabs, including the initial about:blank. The last 2 should be the same
  // process.
  ASSERT_EQ(3, browser()->tab_count());
  EXPECT_EQ("/files/extensions/api_test/app_process/path1/empty.html",
            browser()->GetTabContentsAt(2)->controller().
                GetLastCommittedEntry()->url().path());
  RenderViewHost* host = browser()->GetTabContentsAt(1)->render_view_host();
  EXPECT_EQ(host->process(),
            browser()->GetTabContentsAt(2)->render_view_host()->process());
}
