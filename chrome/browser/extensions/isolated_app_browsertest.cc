// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/test_url_constants.h"
#include "content/public/browser/render_process_host.h"
#include "net/base/mock_host_resolver.h"

namespace {

class IsolatedAppTest : public ExtensionBrowserTest {
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
    const Extension* installed_app = NULL;
    Profile* profile = Profile::FromBrowserContext(contents->browser_context());
    ExtensionService* service = profile->GetExtensionService();
    if (service) {
      installed_app = service->GetInstalledAppForRenderer(
          contents->render_view_host()->process()->GetID());
    }
    return installed_app;
  }

 private:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

}  // namespace

// Tests that cookies set within an isolated app are not visible to normal
// pages or other apps.
IN_PROC_BROWSER_TEST_F(IsolatedAppTest, CookieIsolation) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app1")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app2")));

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = test_server()->GetURL(
      "files/extensions/isolated_apps/");
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);
  base_url = base_url.ReplaceComponents(replace_host);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("app1/main.html"),
      CURRENT_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("app2/main.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("non_app/main.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_EQ(3, browser()->tab_count());

  // Ensure first two tabs have installed apps.
  TabContents* tab1 = browser()->GetTabContentsAt(0);
  TabContents* tab2 = browser()->GetTabContentsAt(1);
  TabContents* tab3 = browser()->GetTabContentsAt(2);
  ASSERT_TRUE(GetInstalledApp(tab1));
  ASSERT_TRUE(GetInstalledApp(tab2));
  ASSERT_TRUE(!GetInstalledApp(tab3));

  // Check that each tab sees its own cookie.
  EXPECT_TRUE(HasCookie(tab1, "app1=3"));
  EXPECT_TRUE(HasCookie(tab2, "app2=4"));
  EXPECT_TRUE(HasCookie(tab3, "normalPage=5"));

  // Check that app1 tab cannot see the other cookies.
  EXPECT_FALSE(HasCookie(tab1, "app2"));
  EXPECT_FALSE(HasCookie(tab1, "normalPage"));

  // Check that app2 tab cannot see the other cookies.
  EXPECT_FALSE(HasCookie(tab2, "app1"));
  EXPECT_FALSE(HasCookie(tab2, "normalPage"));

  // Check that normal tab cannot see the other cookies.
  EXPECT_FALSE(HasCookie(tab3, "app1"));
  EXPECT_FALSE(HasCookie(tab3, "app2"));

  // Check that the non_app iframe cookie is associated with app1 and not the
  // normal tab.  (For now, iframes are always rendered in their parent
  // process, even if they aren't in the app manifest.)
  EXPECT_TRUE(HasCookie(tab1, "nonAppFrame=6"));
  EXPECT_FALSE(HasCookie(tab3, "nonAppFrame"));

  // Check that isolation persists even if the tab crashes and is reloaded.
  browser()->SelectNumberedTab(1);
  ui_test_utils::CrashTab(tab1);
  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->GetSelectedTabContentsWrapper()->controller()));
  browser()->Reload(CURRENT_TAB);
  observer.Wait();
  EXPECT_TRUE(HasCookie(tab1, "app1=3"));
  EXPECT_FALSE(HasCookie(tab1, "app2"));
  EXPECT_FALSE(HasCookie(tab1, "normalPage"));
}

// Ensure that cookies are not isolated if the isolated apps are not installed.
IN_PROC_BROWSER_TEST_F(IsolatedAppTest, NoCookieIsolationWithoutApp) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = test_server()->GetURL(
      "files/extensions/isolated_apps/");
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);
  base_url = base_url.ReplaceComponents(replace_host);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("app1/main.html"),
      CURRENT_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("app2/main.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("non_app/main.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_EQ(3, browser()->tab_count());

  // Check that tabs see each others' cookies.
  EXPECT_TRUE(HasCookie(browser()->GetTabContentsAt(0), "app2=4"));
  EXPECT_TRUE(HasCookie(browser()->GetTabContentsAt(0), "normalPage=5"));
  EXPECT_TRUE(HasCookie(browser()->GetTabContentsAt(0), "nonAppFrame=6"));
  EXPECT_TRUE(HasCookie(browser()->GetTabContentsAt(1), "app1=3"));
  EXPECT_TRUE(HasCookie(browser()->GetTabContentsAt(1), "normalPage=5"));
  EXPECT_TRUE(HasCookie(browser()->GetTabContentsAt(1), "nonAppFrame=6"));
  EXPECT_TRUE(HasCookie(browser()->GetTabContentsAt(2), "app1=3"));
  EXPECT_TRUE(HasCookie(browser()->GetTabContentsAt(2), "app2=4"));
  EXPECT_TRUE(HasCookie(browser()->GetTabContentsAt(2), "nonAppFrame=6"));
}

// Ensure that an isolated app never shares a process with WebUIs, non-isolated
// extensions, and normal webpages.  None of these should ever comingle
// RenderProcessHosts even if we hit the process limit.
IN_PROC_BROWSER_TEST_F(IsolatedAppTest, ProcessOverflow) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCountForTest(1);

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app1")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app2")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("hosted_app")));

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = test_server()->GetURL(
      "files/extensions/");
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);
  base_url = base_url.ReplaceComponents(replace_host);

  // Create a tab for each type of renderer that might exist.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("isolated_apps/app1/main.html"),
      CURRENT_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kTestNewTabURL),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("hosted_app/main.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("test_file.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("isolated_apps/app2/main.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_EQ(5, browser()->tab_count());
  content::RenderProcessHost* isolated1_host =
      browser()->GetTabContentsAt(0)->GetRenderProcessHost();
  content::RenderProcessHost* ntp_host =
      browser()->GetTabContentsAt(1)->GetRenderProcessHost();
  content::RenderProcessHost* normal_extension_host =
      browser()->GetTabContentsAt(2)->GetRenderProcessHost();
  content::RenderProcessHost* web_host =
      browser()->GetTabContentsAt(3)->GetRenderProcessHost();
  content::RenderProcessHost* isolated2_host =
      browser()->GetTabContentsAt(4)->GetRenderProcessHost();

  // Isolated apps shared with each other, but no one else.  They're clannish
  // like that.
  ASSERT_EQ(isolated1_host, isolated2_host);
  ASSERT_NE(isolated1_host, ntp_host);
  ASSERT_NE(isolated1_host, normal_extension_host);
  ASSERT_NE(isolated1_host, web_host);

  // And cause we're here, make sure everyone else is also clannish.  This could
  // technially go in another test, but not worth the extra setup overhead.
  ASSERT_NE(web_host, ntp_host);
  ASSERT_NE(web_host, normal_extension_host);
  ASSERT_NE(normal_extension_host, ntp_host);
}
