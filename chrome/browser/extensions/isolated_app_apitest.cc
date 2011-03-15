// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "net/base/mock_host_resolver.h"

namespace {

class IsolatedAppApiTest : public ExtensionApiTest {
 public:
  // Returns whether the given tab's current URL has the given cookie.
  bool WARN_UNUSED_RESULT HasCookie(TabContents* contents, std::string cookie) {
    int value_size;
    std::string actual_cookie;
    automation_util::GetCookies(contents->GetURL(), contents, &value_size,
                                &actual_cookie);
    return actual_cookie.find(cookie) != std::string::npos;
  }

  const Extension* GetInstalledApp(TabContents* contents) {
    return static_cast<BrowserRenderProcessHost*>(
        contents->render_view_host()->process())->installed_app();
  }
};

}  // namespace

// Tests that cookies set within an isolated app are not visible to normal
// pages or other apps.
IN_PROC_BROWSER_TEST_F(IsolatedAppApiTest, CookieIsolation) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalAppManifests);

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app1")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app2")));

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = test_server()->GetURL(
      "files/extensions/api_test/isolated_apps/");
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);
  base_url = base_url.ReplaceComponents(replace_host);

  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("app1/main.html"));
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("app2/main.html"));
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(),
                               base_url.Resolve("non_app/main.html"));

  // Ensure first two tabs have installed apps.
  TabContents* tab1 = browser()->GetTabContentsAt(1);
  TabContents* tab2 = browser()->GetTabContentsAt(2);
  TabContents* tab3 = browser()->GetTabContentsAt(3);
  ASSERT_TRUE(GetInstalledApp(tab1));
  ASSERT_TRUE(GetInstalledApp(tab2));
  ASSERT_TRUE(!GetInstalledApp(tab3));

  // Check that each tab sees its own cookie.
  ASSERT_TRUE(HasCookie(tab1, "app1=3"));
  ASSERT_TRUE(HasCookie(tab2, "app2=4"));
  ASSERT_TRUE(HasCookie(tab3, "normalPage=5"));

  // Check that app1 tab cannot see the other cookies.
  ASSERT_FALSE(HasCookie(tab1, "app2"));
  ASSERT_FALSE(HasCookie(tab1, "normalPage"));

  // Check that app2 tab cannot see the other cookies.
  ASSERT_FALSE(HasCookie(tab2, "app1"));
  ASSERT_FALSE(HasCookie(tab2, "normalPage"));

  // Check that normal tab cannot see the other cookies.
  ASSERT_FALSE(HasCookie(tab3, "app1"));
  ASSERT_FALSE(HasCookie(tab3, "app2"));

  // Check that the non_app iframe cookie is associated with app1 and not the
  // normal tab.  (For now, iframes are always rendered in their parent
  // process, even if they aren't in the app manifest.)
  ASSERT_TRUE(HasCookie(tab1, "nonAppFrame=6"));
  ASSERT_FALSE(HasCookie(tab3, "nonAppFrame"));
}

// Without the --enable-experimental-app-manifests flag, all the tabs
// should see each others' cookies.
IN_PROC_BROWSER_TEST_F(IsolatedAppApiTest, CookieIsolationRequiresFlag) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app1")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app2")));

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = test_server()->GetURL(
      "files/extensions/api_test/isolated_apps/");
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);
  base_url = base_url.ReplaceComponents(replace_host);

  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("app1/main.html"));
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("app2/main.html"));
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(),
                               base_url.Resolve("non_app/main.html"));

  // Check that tabs see each others' cookies.
  ASSERT_TRUE(HasCookie(browser()->GetTabContentsAt(1), "app2=4"));
  ASSERT_TRUE(HasCookie(browser()->GetTabContentsAt(1), "normalPage=5"));
  ASSERT_TRUE(HasCookie(browser()->GetTabContentsAt(1), "nonAppFrame=6"));
  ASSERT_TRUE(HasCookie(browser()->GetTabContentsAt(2), "app1=3"));
  ASSERT_TRUE(HasCookie(browser()->GetTabContentsAt(2), "normalPage=5"));
  ASSERT_TRUE(HasCookie(browser()->GetTabContentsAt(2), "nonAppFrame=6"));
  ASSERT_TRUE(HasCookie(browser()->GetTabContentsAt(3), "app1=3"));
  ASSERT_TRUE(HasCookie(browser()->GetTabContentsAt(3), "app2=4"));
  ASSERT_TRUE(HasCookie(browser()->GetTabContentsAt(3), "nonAppFrame=6"));
}
