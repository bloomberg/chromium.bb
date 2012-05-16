// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/string_ordinal.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/test/test_navigation_observer.h"
#include "net/base/mock_host_resolver.h"

using content::NavigationController;
using content::RenderViewHost;
using content::WebContents;

// Simulates a page calling window.open on an URL, and waits for the navigation.
static void WindowOpenHelper(Browser* browser,
                             RenderViewHost* opener_host,
                             const GURL& url,
                             bool newtab_process_should_equal_opener) {
  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      opener_host, L"", L"window.open('" + UTF8ToWide(url.spec()) + L"');"));

  // The above window.open call is not user-initiated, it will create
  // a popup window instead of a new tab in current window.
  // Now the active tab in last active window should be the new tab.
  Browser* last_active_browser = BrowserList::GetLastActive();
  ASSERT_TRUE(last_active_browser);
  WebContents* newtab = last_active_browser->GetSelectedWebContents();
  ASSERT_TRUE(newtab);
  observer.Wait();
  EXPECT_EQ(url, newtab->GetController().GetLastCommittedEntry()->GetURL());
  if (newtab_process_should_equal_opener)
    EXPECT_EQ(opener_host->GetProcess(), newtab->GetRenderProcessHost());
  else
    EXPECT_NE(opener_host->GetProcess(), newtab->GetRenderProcessHost());
}

// Simulates a page navigating itself to an URL, and waits for the navigation.
static void NavigateTabHelper(WebContents* contents, const GURL& url) {
  bool result = false;
  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->GetRenderViewHost(), L"",
      L"window.addEventListener('unload', function() {"
      L"    window.domAutomationController.send(true);"
      L"}, false);"
      L"window.location = '" + UTF8ToWide(url.spec()) + L"';",
      &result));
  ASSERT_TRUE(result);
  observer.Wait();
  EXPECT_EQ(url, contents->GetController().GetLastCommittedEntry()->GetURL());
}

class AppApiTest : public ExtensionApiTest {
 protected:
  // Gets the base URL for files for a specific test, making sure that it uses
  // "localhost" as the hostname, since that is what the extent is declared
  // as in the test apps manifests.
  GURL GetTestBaseURL(std::string test_directory) {
    GURL::Replacements replace_host;
    std::string host_str("localhost");  // must stay in scope with replace_host
    replace_host.SetHostStr(host_str);
    GURL base_url = test_server()->GetURL(
        "files/extensions/api_test/" + test_directory + "/");
    return base_url.ReplaceComponents(replace_host);
  }

  // Pass flags to make testing apps easier.
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisablePopupBlocking);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAllowHTTPBackgroundPage);
  }

  // Helper function to test that independent tabs of the named app are loaded
  // into separate processes.
  void TestAppInstancesHelper(std::string app_name) {
    LOG(INFO) << "Start of test.";

    extensions::ProcessMap* process_map =
        browser()->profile()->GetExtensionService()->process_map();

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(test_server()->Start());

    ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII(app_name)));

    // Open two tabs in the app, one outside it.
    GURL base_url = GetTestBaseURL(app_name);

    // Test both opening a URL in a new tab, and opening a tab and then
    // navigating it.  Either way, app tabs should be considered extension
    // processes, but they have no elevated privileges and thus should not
    // have WebUI bindings.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), base_url.Resolve("path1/empty.html"), NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    LOG(INFO) << "Nav 1.";
    EXPECT_TRUE(process_map->Contains(
        browser()->GetWebContentsAt(1)->GetRenderProcessHost()->GetID()));
    EXPECT_FALSE(browser()->GetWebContentsAt(1)->GetWebUI());

    ui_test_utils::WindowedNotificationObserver tab_added_observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
    browser()->NewTab();
    tab_added_observer.Wait();
    LOG(INFO) << "New tab.";
    ui_test_utils::NavigateToURL(browser(),
                                 base_url.Resolve("path2/empty.html"));
    LOG(INFO) << "Nav 2.";
    EXPECT_TRUE(process_map->Contains(
        browser()->GetWebContentsAt(2)->GetRenderProcessHost()->GetID()));
    EXPECT_FALSE(browser()->GetWebContentsAt(2)->GetWebUI());

    // We should have opened 2 new extension tabs. Including the original blank
    // tab, we now have 3 tabs. The two app tabs should not be in the same
    // process, since they do not have the background permission.  (Thus, we
    // want to separate them to improve responsiveness.)
    ASSERT_EQ(3, browser()->tab_count());
    RenderViewHost* host1 = browser()->GetWebContentsAt(1)->GetRenderViewHost();
    RenderViewHost* host2 = browser()->GetWebContentsAt(2)->GetRenderViewHost();
    EXPECT_NE(host1->GetProcess(), host2->GetProcess());

    // Opening tabs with window.open should keep the page in the opener's
    // process.
    ASSERT_EQ(1u, browser::GetBrowserCount(browser()->profile()));
    WindowOpenHelper(browser(), host1,
                    base_url.Resolve("path1/empty.html"), true);
    LOG(INFO) << "WindowOpenHelper 1.";
    WindowOpenHelper(browser(), host2,
                    base_url.Resolve("path2/empty.html"), true);
    LOG(INFO) << "End of test.";
  }
};

// Tests that hosted apps with the background permission get a process-per-app
// model, since all pages need to be able to script the background page.
IN_PROC_BROWSER_TEST_F(AppApiTest, AppProcess) {
  LOG(INFO) << "Start of test.";

  extensions::ProcessMap* process_map =
      browser()->profile()->GetExtensionService()->process_map();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app_process")));

  LOG(INFO) << "Loaded extension.";

  // Open two tabs in the app, one outside it.
  GURL base_url = GetTestBaseURL("app_process");

  // Test both opening a URL in a new tab, and opening a tab and then navigating
  // it.  Either way, app tabs should be considered extension processes, but
  // they have no elevated privileges and thus should not have WebUI bindings.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("path1/empty.html"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_TRUE(process_map->Contains(
      browser()->GetWebContentsAt(1)->GetRenderProcessHost()->GetID()));
  EXPECT_FALSE(browser()->GetWebContentsAt(1)->GetWebUI());
  LOG(INFO) << "Nav 1.";

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("path2/empty.html"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_TRUE(process_map->Contains(
      browser()->GetWebContentsAt(2)->GetRenderProcessHost()->GetID()));
  EXPECT_FALSE(browser()->GetWebContentsAt(2)->GetWebUI());
  LOG(INFO) << "Nav 2.";

  ui_test_utils::WindowedNotificationObserver tab_added_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  browser()->NewTab();
  tab_added_observer.Wait();
  LOG(INFO) << "New tab.";
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path3/empty.html"));
  LOG(INFO) << "Nav 3.";
  EXPECT_FALSE(process_map->Contains(
      browser()->GetWebContentsAt(3)->GetRenderProcessHost()->GetID()));
  EXPECT_FALSE(browser()->GetWebContentsAt(3)->GetWebUI());

  // We should have opened 3 new extension tabs. Including the original blank
  // tab, we now have 4 tabs. Because the app_process app has the background
  // permission, all of its instances are in the same process.  Thus two tabs
  // should be part of the extension app and grouped in the same process.
  ASSERT_EQ(4, browser()->tab_count());
  RenderViewHost* host = browser()->GetWebContentsAt(1)->GetRenderViewHost();

  EXPECT_EQ(host->GetProcess(),
            browser()->GetWebContentsAt(2)->GetRenderProcessHost());
  EXPECT_NE(host->GetProcess(),
            browser()->GetWebContentsAt(3)->GetRenderProcessHost());

  // Now let's do the same using window.open. The same should happen.
  ASSERT_EQ(1u, browser::GetBrowserCount(browser()->profile()));
  WindowOpenHelper(browser(), host,
                   base_url.Resolve("path1/empty.html"), true);
  LOG(INFO) << "WindowOpenHelper 1.";
  WindowOpenHelper(browser(), host,
                   base_url.Resolve("path2/empty.html"), true);
  LOG(INFO) << "WindowOpenHelper 2.";
  // TODO(creis): This should open in a new process (i.e., false for the last
  // argument), but we temporarily avoid swapping processes away from an app
  // until we're able to support cross-process postMessage calls.
  // See crbug.com/59285.
  WindowOpenHelper(browser(), host,
                   base_url.Resolve("path3/empty.html"), true);
  LOG(INFO) << "WindowOpenHelper 3.";

  // Now let's have these pages navigate, into or out of the extension web
  // extent. They should switch processes.
  const GURL& app_url(base_url.Resolve("path1/empty.html"));
  const GURL& non_app_url(base_url.Resolve("path3/empty.html"));
  NavigateTabHelper(browser()->GetWebContentsAt(2), non_app_url);
  LOG(INFO) << "NavigateTabHelper 1.";
  NavigateTabHelper(browser()->GetWebContentsAt(3), app_url);
  LOG(INFO) << "NavigateTabHelper 2.";
  // TODO(creis): This should swap out of the app's process (i.e., EXPECT_NE),
  // but we temporarily avoid swapping away from an app in case the window
  // tries to send a postMessage to the app.  See crbug.com/59285.
  EXPECT_EQ(host->GetProcess(),
            browser()->GetWebContentsAt(2)->GetRenderProcessHost());
  EXPECT_EQ(host->GetProcess(),
            browser()->GetWebContentsAt(3)->GetRenderProcessHost());

  // If one of the popup tabs navigates back to the app, window.opener should
  // be valid.
  NavigateTabHelper(browser()->GetWebContentsAt(6), app_url);
  LOG(INFO) << "NavigateTabHelper 3.";
  EXPECT_EQ(host->GetProcess(),
            browser()->GetWebContentsAt(6)->GetRenderProcessHost());
  bool windowOpenerValid = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetWebContentsAt(6)->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(window.opener != null)",
      &windowOpenerValid));
  ASSERT_TRUE(windowOpenerValid);

  LOG(INFO) << "End of test.";
}

// Test that hosted apps without the background permission use a process per app
// instance model, such that separate instances are in separate processes.
IN_PROC_BROWSER_TEST_F(AppApiTest, AppProcessInstances) {
  TestAppInstancesHelper("app_process_instances");
}

// Test that hosted apps with the background permission but that set
// allow_js_access to false also use a process per app instance model.
// Separate instances should be in separate processes.
IN_PROC_BROWSER_TEST_F(AppApiTest, AppProcessBackgroundInstances) {
  TestAppInstancesHelper("app_process_background_instances");
}

// Tests that bookmark apps do not use the app process model and are treated
// like normal web pages instead.  http://crbug.com/104636.
IN_PROC_BROWSER_TEST_F(AppApiTest, BookmarkAppGetsNormalProcess) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  extensions::ProcessMap* process_map = service->process_map();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());
  GURL base_url = GetTestBaseURL("app_process");

  // Load an app as a bookmark app.
  std::string error;
  scoped_refptr<const Extension> extension(extension_file_util::LoadExtension(
      test_data_dir_.AppendASCII("app_process"),
      Extension::LOAD,
      Extension::FROM_BOOKMARK,
      &error));
  service->OnExtensionInstalled(extension, false,
                                StringOrdinal::CreateInitialOrdinal());
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->from_bookmark());

  // Test both opening a URL in a new tab, and opening a tab and then navigating
  // it.  Either way, bookmark app tabs should be considered normal processes
  // with no elevated privileges and no WebUI bindings.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("path1/empty.html"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_FALSE(process_map->Contains(
      browser()->GetWebContentsAt(1)->GetRenderProcessHost()->GetID()));
  EXPECT_FALSE(browser()->GetWebContentsAt(1)->GetWebUI());

  ui_test_utils::WindowedNotificationObserver tab_added_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  browser()->NewTab();
  tab_added_observer.Wait();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path2/empty.html"));
  EXPECT_FALSE(process_map->Contains(
      browser()->GetWebContentsAt(2)->GetRenderProcessHost()->GetID()));
  EXPECT_FALSE(browser()->GetWebContentsAt(2)->GetWebUI());

  // We should have opened 2 new bookmark app tabs. Including the original blank
  // tab, we now have 3 tabs.  Because normal pages use the
  // process-per-site-instance model, each should be in its own process.
  ASSERT_EQ(3, browser()->tab_count());
  RenderViewHost* host = browser()->GetWebContentsAt(1)->GetRenderViewHost();
  EXPECT_NE(host->GetProcess(),
            browser()->GetWebContentsAt(2)->GetRenderProcessHost());

  // Now let's do the same using window.open. The same should happen.
  ASSERT_EQ(1u, browser::GetBrowserCount(browser()->profile()));
  WindowOpenHelper(browser(), host,
                   base_url.Resolve("path1/empty.html"), true);
  WindowOpenHelper(browser(), host,
                   base_url.Resolve("path2/empty.html"), true);

  // Now let's have a tab navigate out of and back into the app's web
  // extent. Neither navigation should switch processes.
  const GURL& app_url(base_url.Resolve("path1/empty.html"));
  const GURL& non_app_url(base_url.Resolve("path3/empty.html"));
  RenderViewHost* host2 = browser()->GetWebContentsAt(2)->GetRenderViewHost();
  NavigateTabHelper(browser()->GetWebContentsAt(2), non_app_url);
  EXPECT_EQ(host2->GetProcess(),
            browser()->GetWebContentsAt(2)->GetRenderProcessHost());
  NavigateTabHelper(browser()->GetWebContentsAt(2), app_url);
  EXPECT_EQ(host2->GetProcess(),
            browser()->GetWebContentsAt(2)->GetRenderProcessHost());
}

// Tests that app process switching works properly in the following scenario:
// 1. navigate to a page1 in the app
// 2. page1 redirects to a page2 outside the app extent (ie, "/server-redirect")
// 3. page2 redirects back to a page in the app
// The final navigation should end up in the app process.
// See http://crbug.com/61757
// This test doesn't complete on WebKit Win (dbg). See crbug.com/108853.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_AppProcessRedirectBack DISABLED_AppProcessRedirectBack
#else
#define MAYBE_AppProcessRedirectBack AppProcessRedirectBack
#endif
IN_PROC_BROWSER_TEST_F(AppApiTest, MAYBE_AppProcessRedirectBack) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app_process")));

  // Open two tabs in the app.
  GURL base_url = GetTestBaseURL("app_process");

  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  browser()->NewTab();
  // Wait until the second tab finishes its redirect train (3 hops).
  // 1. We navigate to redirect.html
  // 2. Renderer navigates and finishes, counting as a load stop.
  // 3. Renderer issues the meta refresh to navigate to server-redirect.
  // 4. Renderer is now in a "provisional load", waiting for navigation to
  //    complete.
  // 5. Browser sees a redirect response from server-redirect to empty.html, and
  //    transfers that to a new navigation, using RequestTransferURL.
  // 6. We navigate to empty.html.
  // 7. Renderer is still in a provisional load to server-redirect, so that is
  //    cancelled, and counts as a load stop
  // 8. Renderer navigates to empty.html, and finishes loading, counting as the
  //    third load stop
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), base_url.Resolve("path1/redirect.html"), 3);

  // 3 tabs, including the initial about:blank. The last 2 should be the same
  // process.
  ASSERT_EQ(3, browser()->tab_count());
  EXPECT_EQ("/files/extensions/api_test/app_process/path1/empty.html",
            browser()->GetWebContentsAt(2)->GetController().
                GetLastCommittedEntry()->GetURL().path());
  EXPECT_EQ(browser()->GetWebContentsAt(1)->GetRenderProcessHost(),
            browser()->GetWebContentsAt(2)->GetRenderProcessHost());
}

// Ensure that reloading a URL after installing or uninstalling it as an app
// correctly swaps the process.  (http://crbug.com/80621)
//
// The test times out under AddressSanitizer, see http://crbug.com/103371
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ReloadIntoAppProcess DISABLED_ReloadIntoAppProcess
#else
#define MAYBE_ReloadIntoAppProcess ReloadIntoAppProcess
#endif
IN_PROC_BROWSER_TEST_F(AppApiTest, MAYBE_ReloadIntoAppProcess) {
  extensions::ProcessMap* process_map =
      browser()->profile()->GetExtensionService()->process_map();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = GetTestBaseURL("app_process");

  // Load an app URL before loading the app.
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  WebContents* contents = browser()->GetWebContentsAt(0);
  EXPECT_FALSE(process_map->Contains(
      contents->GetRenderProcessHost()->GetID()));

  // Load app and navigate to the page.
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  ASSERT_TRUE(app);
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  EXPECT_TRUE(process_map->Contains(
      contents->GetRenderProcessHost()->GetID()));

  // Disable app and navigate to the page.
  DisableExtension(app->id());
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  EXPECT_FALSE(process_map->Contains(
      contents->GetRenderProcessHost()->GetID()));

  // Enable app and reload the page.
  EnableExtension(app->id());
  ui_test_utils::WindowedNotificationObserver reload_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->GetSelectedTabContentsWrapper()->web_contents()->
              GetController()));
  browser()->Reload(CURRENT_TAB);
  reload_observer.Wait();
  EXPECT_TRUE(process_map->Contains(
      contents->GetRenderProcessHost()->GetID()));

  // Disable app and reload the page.
  DisableExtension(app->id());
  ui_test_utils::WindowedNotificationObserver reload_observer2(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->GetSelectedTabContentsWrapper()->web_contents()->
              GetController()));
  browser()->Reload(CURRENT_TAB);
  reload_observer2.Wait();
  EXPECT_FALSE(process_map->Contains(
      contents->GetRenderProcessHost()->GetID()));

  // Enable app and reload via JavaScript.
  EnableExtension(app->id());
  ui_test_utils::WindowedNotificationObserver js_reload_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->GetSelectedTabContentsWrapper()->web_contents()->
              GetController()));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(contents->GetRenderViewHost(),
                                               L"", L"location.reload();"));
  js_reload_observer.Wait();
  EXPECT_TRUE(process_map->Contains(
      contents->GetRenderProcessHost()->GetID()));

  // Disable app and reload via JavaScript.
  DisableExtension(app->id());
  ui_test_utils::WindowedNotificationObserver js_reload_observer2(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->GetSelectedTabContentsWrapper()->web_contents()->
              GetController()));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(contents->GetRenderViewHost(),
                                               L"", L"location = location;"));
  js_reload_observer2.Wait();
  EXPECT_FALSE(process_map->Contains(
      contents->GetRenderProcessHost()->GetID()));
}

// Tests that if we have a non-app process (path3/container.html) that has an
// iframe with  a URL in the app's extent (path1/iframe.html), then opening a
// link from that iframe to a new window to a URL in the app's extent (path1/
// empty.html) results in the new window being in an app process. See
// http://crbug.com/89272 for more details.
IN_PROC_BROWSER_TEST_F(AppApiTest, OpenAppFromIframe) {
  extensions::ProcessMap* process_map =
      browser()->profile()->GetExtensionService()->process_map();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL base_url = GetTestBaseURL("app_process");

  // Load app and start URL (not in the app).
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  ASSERT_TRUE(app);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      base_url.Resolve("path3/container.html"),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_FALSE(process_map->Contains(
      browser()->GetWebContentsAt(0)->GetRenderProcessHost()->GetID()));

  // Wait for popup window to appear.
  GURL app_url = base_url.Resolve("path1/empty.html");
  Browser* last_active_browser = BrowserList::GetLastActive();
  EXPECT_TRUE(last_active_browser);
  ASSERT_NE(browser(), last_active_browser);
  WebContents* newtab = last_active_browser->GetSelectedWebContents();
  EXPECT_TRUE(newtab);
  if (!newtab->GetController().GetLastCommittedEntry() ||
      newtab->GetController().GetLastCommittedEntry()->GetURL() != app_url) {
    // TODO(gbillock): This still looks racy. Need to make a custom
    // observer to intercept new window creation and then look for
    // NAV_ENTRY_COMMITTED on the new tab there.
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<NavigationController>(&(newtab->GetController())));
    observer.Wait();
  }

  // Popup window should be in the app's process.
  EXPECT_TRUE(process_map->Contains(
      last_active_browser->GetWebContentsAt(0)->GetRenderProcessHost()->
          GetID()));
}

// Tests that if an extension launches an app via chrome.tabs.create with an URL
// that's not in the app's extent but that redirects to it, we still end up with
// an app process. See http://crbug.com/99349 for more details.
IN_PROC_BROWSER_TEST_F(AppApiTest, OpenAppFromExtension) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());

  LoadExtension(test_data_dir_.AppendASCII("app_process"));
  const Extension* launcher =
      LoadExtension(test_data_dir_.AppendASCII("app_launcher"));

  // There should be three navigations by the time the app page is loaded.
  // 1. The extension launcher page.
  // 2. The URL that the extension launches, which redirects.
  // 3. The app's URL.
  TestNavigationObserver test_navigation_observer(
        content::NotificationService::AllSources(),
        NULL,
        3);

  // Load the launcher extension, which should launch the app.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      launcher->GetResourceURL("main.html"),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Wait for app tab to be created and loaded.
  test_navigation_observer.WaitForObservation(
      base::Bind(&ui_test_utils::RunMessageLoop),
      base::Bind(&MessageLoop::Quit,
                 base::Unretained(MessageLoopForUI::current())));

  // App has loaded, and chrome.app.isInstalled should be true.
  bool is_installed = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(chrome.app.isInstalled)",
      &is_installed));
  ASSERT_TRUE(is_installed);
}

// Tests that if we have an app process (path1/container.html) with a non-app
// iframe (path3/iframe.html), then opening a link from that iframe to a new
// window to a same-origin non-app URL (path3/empty.html) should keep the window
// in the app process.
// This is in contrast to OpenAppFromIframe, since here the popup will not be
// missing special permissions and should be scriptable from the iframe.
// See http://crbug.com/92669 for more details.
IN_PROC_BROWSER_TEST_F(AppApiTest, OpenWebPopupFromWebIframe) {
  extensions::ProcessMap* process_map =
      browser()->profile()->GetExtensionService()->process_map();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL base_url = GetTestBaseURL("app_process");

  // Load app and start URL (in the app).
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  ASSERT_TRUE(app);
  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      base_url.Resolve("path1/container.html"),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  content::RenderProcessHost* process =
      browser()->GetWebContentsAt(0)->GetRenderProcessHost();
  EXPECT_TRUE(process_map->Contains(process->GetID()));

  // Wait for popup window to appear.  The new Browser may not have been
  // added with SetLastActive, in which case we need to show it first.
  // This is necessary for popup windows without a cross-site transition.
  if (browser() == BrowserList::GetLastActive()) {
    // Grab the second window and show it.
    ASSERT_TRUE(BrowserList::size() == 2);
    Browser* popup_browser = *(++BrowserList::begin());
    popup_browser->window()->Show();
  }
  Browser* last_active_browser = BrowserList::GetLastActive();
  EXPECT_TRUE(last_active_browser);
  ASSERT_NE(browser(), last_active_browser);
  WebContents* newtab = last_active_browser->GetSelectedWebContents();
  EXPECT_TRUE(newtab);
  GURL non_app_url = base_url.Resolve("path3/empty.html");
  observer.Wait();

  // Popup window should be in the app's process.
  content::RenderProcessHost* popup_process =
      last_active_browser->GetWebContentsAt(0)->GetRenderProcessHost();
  EXPECT_EQ(process, popup_process);
}

// http://crbug.com/118502
#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_ReloadAppAfterCrash DISABLED_ReloadAppAfterCrash
#else
#define MAYBE_ReloadAppAfterCrash ReloadAppAfterCrash
#endif
IN_PROC_BROWSER_TEST_F(AppApiTest, MAYBE_ReloadAppAfterCrash) {
  extensions::ProcessMap* process_map =
      browser()->profile()->GetExtensionService()->process_map();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app_process")));

  GURL base_url = GetTestBaseURL("app_process");

  // Load the app, chrome.app.isInstalled should be true.
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  WebContents* contents = browser()->GetWebContentsAt(0);
  EXPECT_TRUE(process_map->Contains(
      contents->GetRenderProcessHost()->GetID()));
  bool is_installed = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(chrome.app.isInstalled)",
      &is_installed));
  ASSERT_TRUE(is_installed);

  // Crash the tab and reload it, chrome.app.isInstalled should still be true.
  ui_test_utils::CrashTab(browser()->GetSelectedWebContents());
  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->GetSelectedTabContentsWrapper()->web_contents()->
              GetController()));
  browser()->Reload(CURRENT_TAB);
  observer.Wait();
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(chrome.app.isInstalled)",
      &is_installed));
  ASSERT_TRUE(is_installed);
}
