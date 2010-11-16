// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui_test_utils.h"
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

#if defined(OS_WIN)
// AppProcess sometimes hangs on Windows
// http://crbug.com/58810
#define MAYBE_AppProcess DISABLED_AppProcess
#else
#define MAYBE_AppProcess AppProcess
#endif
IN_PROC_BROWSER_TEST_F(AppApiTest, MAYBE_AppProcess) {
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
  replace_host.SetHostStr("localhost");
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
  WindowOpenHelper(browser(), host,
                   base_url.Resolve("path3/empty.html"), false);

  // Now let's have these pages navigate, into or out of the extension web
  // extent. They should switch processes.
  const GURL& app_url(base_url.Resolve("path1/empty.html"));
  const GURL& non_app_url(base_url.Resolve("path3/empty.html"));
  NavigateTabHelper(browser()->GetTabContentsAt(2), non_app_url);
  NavigateTabHelper(browser()->GetTabContentsAt(3), app_url);
  EXPECT_NE(host->process(),
            browser()->GetTabContentsAt(2)->render_view_host()->process());
  EXPECT_EQ(host->process(),
            browser()->GetTabContentsAt(3)->render_view_host()->process());
}
