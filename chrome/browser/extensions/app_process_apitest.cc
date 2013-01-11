// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/mock_host_resolver.h"
#include "sync/api/string_ordinal.h"

using content::NavigationController;
using content::RenderViewHost;
using content::WebContents;
using extensions::Extension;

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

    extensions::ProcessMap* process_map = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service()->process_map();

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
        browser()->tab_strip_model()->GetWebContentsAt(1)->
            GetRenderProcessHost()->GetID()));
    EXPECT_FALSE(browser()->tab_strip_model()->GetWebContentsAt(1)->GetWebUI());

    content::WindowedNotificationObserver tab_added_observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
    chrome::NewTab(browser());
    tab_added_observer.Wait();
    LOG(INFO) << "New tab.";
    ui_test_utils::NavigateToURL(browser(),
                                 base_url.Resolve("path2/empty.html"));
    LOG(INFO) << "Nav 2.";
    EXPECT_TRUE(process_map->Contains(
        browser()->tab_strip_model()->GetWebContentsAt(2)->
            GetRenderProcessHost()->GetID()));
    EXPECT_FALSE(browser()->tab_strip_model()->GetWebContentsAt(2)->GetWebUI());

    // We should have opened 2 new extension tabs. Including the original blank
    // tab, we now have 3 tabs. The two app tabs should not be in the same
    // process, since they do not have the background permission.  (Thus, we
    // want to separate them to improve responsiveness.)
    ASSERT_EQ(3, browser()->tab_strip_model()->count());
    WebContents* tab1 = browser()->tab_strip_model()->GetWebContentsAt(1);
    WebContents* tab2 = browser()->tab_strip_model()->GetWebContentsAt(2);
    EXPECT_NE(tab1->GetRenderProcessHost(), tab2->GetRenderProcessHost());

    // Opening tabs with window.open should keep the page in the opener's
    // process.
    ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
    OpenWindow(tab1, base_url.Resolve("path1/empty.html"), true, NULL);
    LOG(INFO) << "WindowOpenHelper 1.";
    OpenWindow(tab2, base_url.Resolve("path2/empty.html"), true, NULL);
    LOG(INFO) << "End of test.";
  }
};

// Omits the disable-popup-blocking flag so we can cover that case.
class BlockedAppApiTest : public AppApiTest {
 protected:
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAllowHTTPBackgroundPage);
  }
};

// Tests that hosted apps with the background permission get a process-per-app
// model, since all pages need to be able to script the background page.
IN_PROC_BROWSER_TEST_F(AppApiTest, AppProcess) {
  LOG(INFO) << "Start of test.";

  extensions::ProcessMap* process_map = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service()->process_map();

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
      browser()->tab_strip_model()->GetWebContentsAt(1)->
          GetRenderProcessHost()->GetID()));
  EXPECT_FALSE(browser()->tab_strip_model()->GetWebContentsAt(1)->GetWebUI());
  LOG(INFO) << "Nav 1.";

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("path2/empty.html"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_TRUE(process_map->Contains(
      browser()->tab_strip_model()->GetWebContentsAt(2)->
          GetRenderProcessHost()->GetID()));
  EXPECT_FALSE(browser()->tab_strip_model()->GetWebContentsAt(2)->GetWebUI());
  LOG(INFO) << "Nav 2.";

  content::WindowedNotificationObserver tab_added_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  chrome::NewTab(browser());
  tab_added_observer.Wait();
  LOG(INFO) << "New tab.";
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path3/empty.html"));
  LOG(INFO) << "Nav 3.";
  EXPECT_FALSE(process_map->Contains(
      browser()->tab_strip_model()->GetWebContentsAt(3)->
          GetRenderProcessHost()->GetID()));
  EXPECT_FALSE(browser()->tab_strip_model()->GetWebContentsAt(3)->GetWebUI());

  // We should have opened 3 new extension tabs. Including the original blank
  // tab, we now have 4 tabs. Because the app_process app has the background
  // permission, all of its instances are in the same process.  Thus two tabs
  // should be part of the extension app and grouped in the same process.
  ASSERT_EQ(4, browser()->tab_strip_model()->count());
  WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(1);

  EXPECT_EQ(tab->GetRenderProcessHost(),
            browser()->tab_strip_model()->GetWebContentsAt(2)->
                GetRenderProcessHost());
  EXPECT_NE(tab->GetRenderProcessHost(),
            browser()->tab_strip_model()->GetWebContentsAt(3)->
                GetRenderProcessHost());

  // Now let's do the same using window.open. The same should happen.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  OpenWindow(tab, base_url.Resolve("path1/empty.html"), true, NULL);
  LOG(INFO) << "WindowOpenHelper 1.";
  OpenWindow(tab, base_url.Resolve("path2/empty.html"), true, NULL);
  LOG(INFO) << "WindowOpenHelper 2.";
  // TODO(creis): This should open in a new process (i.e., false for the last
  // argument), but we temporarily avoid swapping processes away from a hosted
  // app if it has an opener, because some OAuth providers make script calls
  // between non-app popups and non-app iframes in the app process.
  // See crbug.com/59285.
  OpenWindow(tab, base_url.Resolve("path3/empty.html"), true, NULL);
  LOG(INFO) << "WindowOpenHelper 3.";

  // Now let's have these pages navigate, into or out of the extension web
  // extent. They should switch processes.
  const GURL& app_url(base_url.Resolve("path1/empty.html"));
  const GURL& non_app_url(base_url.Resolve("path3/empty.html"));
  NavigateInRenderer(browser()->tab_strip_model()->GetWebContentsAt(2),
                     non_app_url);
  LOG(INFO) << "NavigateTabHelper 1.";
  NavigateInRenderer(browser()->tab_strip_model()->GetWebContentsAt(3),
                     app_url);
  LOG(INFO) << "NavigateTabHelper 2.";
  EXPECT_NE(tab->GetRenderProcessHost(),
            browser()->tab_strip_model()->GetWebContentsAt(2)->
                GetRenderProcessHost());
  EXPECT_EQ(tab->GetRenderProcessHost(),
            browser()->tab_strip_model()->GetWebContentsAt(3)->
                GetRenderProcessHost());

  // If one of the popup tabs navigates back to the app, window.opener should
  // be valid.
  NavigateInRenderer(browser()->tab_strip_model()->GetWebContentsAt(6),
                     app_url);
  LOG(INFO) << "NavigateTabHelper 3.";
  EXPECT_EQ(tab->GetRenderProcessHost(),
            browser()->tab_strip_model()->GetWebContentsAt(6)->
                GetRenderProcessHost());
  bool windowOpenerValid = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetWebContentsAt(6),
      "window.domAutomationController.send(window.opener != null)",
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
// Flaky on XP: http://crbug.com/165834
#if defined(OS_WIN)
#define MAYBE_AppProcessBackgroundInstances \
    DISABLED_AppProcessBackgroundInstances
#else
#define MAYBE_AppProcessBackgroundInstances AppProcessBackgroundInstances
#endif
IN_PROC_BROWSER_TEST_F(AppApiTest, MAYBE_AppProcessBackgroundInstances) {
  TestAppInstancesHelper("app_process_background_instances");
}

// Tests that bookmark apps do not use the app process model and are treated
// like normal web pages instead.  http://crbug.com/104636.
IN_PROC_BROWSER_TEST_F(AppApiTest, BookmarkAppGetsNormalProcess) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
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
  service->OnExtensionInstalled(extension,
                                syncer::StringOrdinal::CreateInitialOrdinal(),
                                false /* no requirement errors */,
                                false /* don't wait for idle */);
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->from_bookmark());

  // Test both opening a URL in a new tab, and opening a tab and then navigating
  // it.  Either way, bookmark app tabs should be considered normal processes
  // with no elevated privileges and no WebUI bindings.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("path1/empty.html"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_FALSE(process_map->Contains(
      browser()->tab_strip_model()->GetWebContentsAt(1)->
          GetRenderProcessHost()->GetID()));
  EXPECT_FALSE(browser()->tab_strip_model()->GetWebContentsAt(1)->GetWebUI());

  content::WindowedNotificationObserver tab_added_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  chrome::NewTab(browser());
  tab_added_observer.Wait();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path2/empty.html"));
  EXPECT_FALSE(process_map->Contains(
      browser()->tab_strip_model()->GetWebContentsAt(2)->
          GetRenderProcessHost()->GetID()));
  EXPECT_FALSE(browser()->tab_strip_model()->GetWebContentsAt(2)->GetWebUI());

  // We should have opened 2 new bookmark app tabs. Including the original blank
  // tab, we now have 3 tabs.  Because normal pages use the
  // process-per-site-instance model, each should be in its own process.
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_NE(tab->GetRenderProcessHost(),
            browser()->tab_strip_model()->GetWebContentsAt(2)->
                GetRenderProcessHost());

  // Now let's do the same using window.open. The same should happen.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  OpenWindow(tab, base_url.Resolve("path1/empty.html"), true, NULL);
  OpenWindow(tab, base_url.Resolve("path2/empty.html"), true, NULL);

  // Now let's have a tab navigate out of and back into the app's web
  // extent. Neither navigation should switch processes.
  const GURL& app_url(base_url.Resolve("path1/empty.html"));
  const GURL& non_app_url(base_url.Resolve("path3/empty.html"));
  RenderViewHost* host2 =
      browser()->tab_strip_model()->GetWebContentsAt(2)->GetRenderViewHost();
  NavigateInRenderer(browser()->tab_strip_model()->GetWebContentsAt(2),
                     non_app_url);
  EXPECT_EQ(host2->GetProcess(),
            browser()->tab_strip_model()->GetWebContentsAt(2)->
                GetRenderProcessHost());
  NavigateInRenderer(browser()->tab_strip_model()->GetWebContentsAt(2),
                     app_url);
  EXPECT_EQ(host2->GetProcess(),
            browser()->tab_strip_model()->GetWebContentsAt(2)->
                GetRenderProcessHost());
}

// Tests that app process switching works properly in the following scenario:
// 1. navigate to a page1 in the app
// 2. page1 redirects to a page2 outside the app extent (ie, "/server-redirect")
// 3. page2 redirects back to a page in the app
// The final navigation should end up in the app process.
// See http://crbug.com/61757
IN_PROC_BROWSER_TEST_F(AppApiTest, AppProcessRedirectBack) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app_process")));

  // Open two tabs in the app.
  GURL base_url = GetTestBaseURL("app_process");

  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  chrome::NewTab(browser());
  // Wait until the second tab finishes its redirect train (2 hops).
  // 1. We navigate to redirect.html
  // 2. Renderer navigates and finishes, counting as a load stop.
  // 3. Renderer issues the meta refresh to navigate to server-redirect.
  // 4. Renderer is now in a "provisional load", waiting for navigation to
  //    complete.
  // 5. Browser sees a redirect response from server-redirect to empty.html, and
  //    transfers that to a new navigation, using RequestTransferURL.
  // 6. Renderer navigates to empty.html, and finishes loading, counting as the
  //    second load stop
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), base_url.Resolve("path1/redirect.html"), 2);

  // 3 tabs, including the initial about:blank. The last 2 should be the same
  // process.
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ("/files/extensions/api_test/app_process/path1/empty.html",
            browser()->tab_strip_model()->GetWebContentsAt(2)->
                GetController().GetLastCommittedEntry()->GetURL().path());
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1)->
                GetRenderProcessHost(),
            browser()->tab_strip_model()->GetWebContentsAt(2)->
                GetRenderProcessHost());
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
  extensions::ProcessMap* process_map = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service()->process_map();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = GetTestBaseURL("app_process");

  // Load an app URL before loading the app.
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  WebContents* contents = browser()->tab_strip_model()->GetWebContentsAt(0);
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
  content::WindowedNotificationObserver reload_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  chrome::Reload(browser(), CURRENT_TAB);
  reload_observer.Wait();
  EXPECT_TRUE(process_map->Contains(
      contents->GetRenderProcessHost()->GetID()));

  // Disable app and reload the page.
  DisableExtension(app->id());
  content::WindowedNotificationObserver reload_observer2(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  chrome::Reload(browser(), CURRENT_TAB);
  reload_observer2.Wait();
  EXPECT_FALSE(process_map->Contains(
      contents->GetRenderProcessHost()->GetID()));

  // Enable app and reload via JavaScript.
  EnableExtension(app->id());
  content::WindowedNotificationObserver js_reload_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  ASSERT_TRUE(content::ExecuteScript(contents, "location.reload();"));
  js_reload_observer.Wait();
  EXPECT_TRUE(process_map->Contains(
      contents->GetRenderProcessHost()->GetID()));

  // Disable app and reload via JavaScript.
  DisableExtension(app->id());
  content::WindowedNotificationObserver js_reload_observer2(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  ASSERT_TRUE(content::ExecuteScript(contents, "location = location;"));
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
  extensions::ProcessMap* process_map = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service()->process_map();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL base_url = GetTestBaseURL("app_process");

  // Load app and start URL (not in the app).
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  ASSERT_TRUE(app);

  content::WindowedNotificationObserver popup_observer(
      content::NOTIFICATION_RENDER_VIEW_HOST_CREATED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(),
                               base_url.Resolve("path3/container.html"));
  EXPECT_FALSE(process_map->Contains(
      browser()->tab_strip_model()->GetWebContentsAt(0)->
          GetRenderProcessHost()->GetID()));
  popup_observer.Wait();

  // Popup window should be in the app's process.
  RenderViewHost* popup_host =
      content::Source<RenderViewHost>(popup_observer.source()).ptr();
  EXPECT_TRUE(process_map->Contains(popup_host->GetProcess()->GetID()));
}

// Similar to the previous test, but ensure that popup blocking bypass
// isn't granted to the iframe.  See crbug.com/117446.
#if defined(OS_CHROMEOS)
// http://crbug.com/153513
#define MAYBE_OpenAppFromIframe DISABLED_OpenAppFromIframe
#else
#define MAYBE_OpenAppFromIframe OpenAppFromIframe
#endif
IN_PROC_BROWSER_TEST_F(BlockedAppApiTest, MAYBE_OpenAppFromIframe) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  // Load app and start URL (not in the app).
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  ASSERT_TRUE(app);

  content::WindowedNotificationObserver blocker_observer(
      chrome::NOTIFICATION_CONTENT_BLOCKED_STATE_CHANGED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(
      browser(), GetTestBaseURL("app_process").Resolve("path3/container.html"));

  blocker_observer.Wait();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  BlockedContentTabHelper* blocked_content_tab_helper =
      BlockedContentTabHelper::FromWebContents(tab);
  std::vector<WebContents*> blocked_contents;
  blocked_content_tab_helper->GetBlockedContents(&blocked_contents);
  EXPECT_EQ(blocked_contents.size(), 1u);
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
  content::TestNavigationObserver test_navigation_observer(
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
      base::Bind(&content::RunMessageLoop),
      base::Bind(&MessageLoop::Quit,
                 base::Unretained(MessageLoopForUI::current())));

  // App has loaded, and chrome.app.isInstalled should be true.
  bool is_installed = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(chrome.app.isInstalled)",
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
  extensions::ProcessMap* process_map = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service()->process_map();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL base_url = GetTestBaseURL("app_process");

  // Load app and start URL (in the app).
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  ASSERT_TRUE(app);

  content::WindowedNotificationObserver popup_observer(
        content::NOTIFICATION_RENDER_VIEW_HOST_CREATED,
        content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(),
                               base_url.Resolve("path1/container.html"));
  content::RenderProcessHost* process =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetRenderProcessHost();
  EXPECT_TRUE(process_map->Contains(process->GetID()));

  // Wait for popup window to appear.
  popup_observer.Wait();

  // Popup window should be in the app's process.
  RenderViewHost* popup_host =
      content::Source<RenderViewHost>(popup_observer.source()).ptr();
  EXPECT_EQ(process, popup_host->GetProcess());
}

// http://crbug.com/118502
#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_ReloadAppAfterCrash DISABLED_ReloadAppAfterCrash
#else
#define MAYBE_ReloadAppAfterCrash ReloadAppAfterCrash
#endif
IN_PROC_BROWSER_TEST_F(AppApiTest, MAYBE_ReloadAppAfterCrash) {
  extensions::ProcessMap* process_map = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service()->process_map();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app_process")));

  GURL base_url = GetTestBaseURL("app_process");

  // Load the app, chrome.app.isInstalled should be true.
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  WebContents* contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_TRUE(process_map->Contains(
      contents->GetRenderProcessHost()->GetID()));
  bool is_installed = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      "window.domAutomationController.send(chrome.app.isInstalled)",
      &is_installed));
  ASSERT_TRUE(is_installed);

  // Crash the tab and reload it, chrome.app.isInstalled should still be true.
  content::CrashTab(browser()->tab_strip_model()->GetActiveWebContents());
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  chrome::Reload(browser(), CURRENT_TAB);
  observer.Wait();
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      "window.domAutomationController.send(chrome.app.isInstalled)",
      &is_installed));
  ASSERT_TRUE(is_installed);
}
