// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_process_policy.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::NavigationController;
using content::WebContents;

namespace {

class ProcessManagementTest : public ExtensionBrowserTest {
 private:
  // This is needed for testing isolated apps, which are still experimental.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        extensions::switches::kEnableExperimentalExtensionApis);
  }
};

class ChromeWebStoreProcessTest : public ExtensionBrowserTest {
 public:
  const GURL& gallery_url() { return gallery_url_; }

 private:
  // Overrides location of Chrome Web Store gallery to a test controlled URL.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);

    ASSERT_TRUE(embedded_test_server()->Start());
    gallery_url_ =
        embedded_test_server()->GetURL("chrome.webstore.test.com", "/");
    command_line->AppendSwitchASCII(switches::kAppsGalleryURL,
                                    gallery_url_.spec());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  GURL gallery_url_;
};

}  // namespace


// TODO(nasko): crbug.com/173137
#if defined(OS_WIN)
#define MAYBE_ProcessOverflow DISABLED_ProcessOverflow
#else
#define MAYBE_ProcessOverflow ProcessOverflow
#endif

// Ensure that an isolated app never shares a process with WebUIs, non-isolated
// extensions, and normal webpages.  None of these should ever comingle
// RenderProcessHosts even if we hit the process limit.
IN_PROC_BROWSER_TEST_F(ProcessManagementTest, MAYBE_ProcessOverflow) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app1")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app2")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("hosted_app")));
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("api_test/app_process")));

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = embedded_test_server()->GetURL(
      "/extensions/");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  base_url = base_url.ReplaceComponents(replace_host);

  // Load an extension before adding tabs.
  const extensions::Extension* extension1 = LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/basics"));
  ASSERT_TRUE(extension1);
  GURL extension1_url = extension1->url();

  // Create multiple tabs for each type of renderer that might exist.
  ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("isolated_apps/app1/main.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("hosted_app/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("test_file.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("isolated_apps/app2/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("api_test/app_process/path1/empty.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("test_file_with_body.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Load another copy of isolated app 1.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("isolated_apps/app1/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Load another extension.
  const extensions::Extension* extension2 = LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/close_background"));
  ASSERT_TRUE(extension2);
  GURL extension2_url = extension2->url();

  // Get tab processes.
  ASSERT_EQ(9, browser()->tab_strip_model()->count());
  content::RenderProcessHost* isolated1_host =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetRenderProcessHost();
  content::RenderProcessHost* ntp1_host =
      browser()->tab_strip_model()->GetWebContentsAt(1)->GetRenderProcessHost();
  content::RenderProcessHost* hosted1_host =
      browser()->tab_strip_model()->GetWebContentsAt(2)->GetRenderProcessHost();
  content::RenderProcessHost* web1_host =
      browser()->tab_strip_model()->GetWebContentsAt(3)->GetRenderProcessHost();

  content::RenderProcessHost* isolated2_host =
      browser()->tab_strip_model()->GetWebContentsAt(4)->GetRenderProcessHost();
  content::RenderProcessHost* ntp2_host =
      browser()->tab_strip_model()->GetWebContentsAt(5)->GetRenderProcessHost();
  content::RenderProcessHost* hosted2_host =
      browser()->tab_strip_model()->GetWebContentsAt(6)->GetRenderProcessHost();
  content::RenderProcessHost* web2_host =
      browser()->tab_strip_model()->GetWebContentsAt(7)->GetRenderProcessHost();

  content::RenderProcessHost* second_isolated1_host =
      browser()->tab_strip_model()->GetWebContentsAt(8)->GetRenderProcessHost();

  // Get extension processes.
  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(browser()->profile());
  content::RenderProcessHost* extension1_host =
      process_manager->GetSiteInstanceForURL(extension1_url)->GetProcess();
  content::RenderProcessHost* extension2_host =
      process_manager->GetSiteInstanceForURL(extension2_url)->GetProcess();

  // An isolated app only shares with other instances of itself, not other
  // isolated apps or anything else.
  EXPECT_EQ(isolated1_host, second_isolated1_host);
  EXPECT_NE(isolated1_host, isolated2_host);
  EXPECT_NE(isolated1_host, ntp1_host);
  EXPECT_NE(isolated1_host, hosted1_host);
  EXPECT_NE(isolated1_host, web1_host);
  EXPECT_NE(isolated1_host, extension1_host);
  EXPECT_NE(isolated2_host, ntp1_host);
  EXPECT_NE(isolated2_host, hosted1_host);
  EXPECT_NE(isolated2_host, web1_host);
  EXPECT_NE(isolated2_host, extension1_host);

  // Everything else is clannish.  WebUI only shares with other WebUI.
  EXPECT_EQ(ntp1_host, ntp2_host);
  EXPECT_NE(ntp1_host, hosted1_host);
  EXPECT_NE(ntp1_host, web1_host);
  EXPECT_NE(ntp1_host, extension1_host);

  // Hosted apps only share with each other.
  // Note that hosted2_host's app has the background permission and will use
  // process-per-site mode, but it should still share with hosted1_host's app.
  EXPECT_EQ(hosted1_host, hosted2_host);
  EXPECT_NE(hosted1_host, web1_host);
  EXPECT_NE(hosted1_host, extension1_host);

  // Web pages only share with each other.
  EXPECT_EQ(web1_host, web2_host);
  EXPECT_NE(web1_host, extension1_host);

  // Extensions only share with each other.
  EXPECT_EQ(extension1_host, extension2_host);
}

// See
#if defined(OS_WIN)
#define MAYBE_ExtensionProcessBalancing DISABLED_ExtensionProcessBalancing
#else
#define MAYBE_ExtensionProcessBalancing ExtensionProcessBalancing
#endif
// Test to verify that the policy of maximum share of extension processes is
// properly enforced.
IN_PROC_BROWSER_TEST_F(ProcessManagementTest, MAYBE_ExtensionProcessBalancing) {
  // Set max renderers to 6 so we can expect 2 extension processes to be
  // allocated.
  content::RenderProcessHost::SetMaxRendererProcessCount(6);

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = embedded_test_server()->GetURL(
      "/extensions/");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  base_url = base_url.ReplaceComponents(replace_host);

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/none")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/basics")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/remove_popup")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/add_popup")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/no_icon")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("isolated_apps/app1")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/management/test")));

  ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("isolated_apps/app1/main.html"));

  ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("api_test/management/test/basics.html"));

  std::set<int> process_ids;
  Profile* profile = browser()->profile();
  extensions::ProcessManager* epm = extensions::ProcessManager::Get(profile);
  for (extensions::ExtensionHost* host : epm->background_hosts())
    process_ids.insert(host->render_process_host()->GetID());

  // We've loaded 5 extensions with background pages, 1 extension without
  // background page, and one isolated app. We expect only 2 unique processes
  // hosting those extensions.
  extensions::ProcessMap* process_map = extensions::ProcessMap::Get(profile);

  EXPECT_GE((size_t) 6, process_map->size());
  EXPECT_EQ((size_t) 2, process_ids.size());
}

IN_PROC_BROWSER_TEST_F(ProcessManagementTest,
                       NavigateExtensionTabToWebViaPost) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an extension.
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/popup_with_form"));
  ASSERT_TRUE(extension);

  // Navigate a tab to an extension page.
  GURL extension_url = extension->GetResourceURL("popup.html");
  ui_test_utils::NavigateToURL(browser(), extension_url);
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(extension_url, web_contents->GetLastCommittedURL());
  content::RenderProcessHost* old_process_host =
      web_contents->GetMainFrame()->GetProcess();

  // Note that the |setTimeout| call below is needed to make sure
  // ExecuteScriptAndExtractBool returns *after* a scheduled navigation has
  // already started.
  GURL web_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  std::string navigation_starting_script =
      "var form = document.getElementById('form');\n"
      "form.action = '" + web_url.spec() + "';\n"
      "form.submit();\n"
      "setTimeout(\n"
      "    function() { window.domAutomationController.send(true); },\n"
      "    0);\n";

  // Try to trigger navigation to a webpage from within the tab.
  bool ignored_script_result = false;
  content::TestNavigationObserver nav_observer(web_contents, 1);
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, navigation_starting_script, &ignored_script_result));

  // Verify that the navigation succeeded.
  nav_observer.Wait();
  EXPECT_EQ(web_url, web_contents->GetLastCommittedURL());

  // Verify that the navigation transferred the contents to another renderer
  // process.
  if (extensions::IsIsolateExtensionsEnabled()) {
    content::RenderProcessHost* new_process_host =
        web_contents->GetMainFrame()->GetProcess();
    EXPECT_NE(old_process_host, new_process_host);
  }
}

IN_PROC_BROWSER_TEST_F(ChromeWebStoreProcessTest,
                       NavigateWebTabToChromeWebStoreViaPost) {
  // Navigate a tab to a web page with a form.
  GURL web_url = embedded_test_server()->GetURL("foo.com", "/form.html");
  ui_test_utils::NavigateToURL(browser(), web_url);
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(web_url, web_contents->GetLastCommittedURL());
  content::RenderProcessHost* old_process_host =
      web_contents->GetMainFrame()->GetProcess();

  // Calculate an URL that is 1) relative to the fake (i.e. test-controlled)
  // Chrome Web Store gallery URL and 2) resolves to something that
  // embedded_test_server can actually serve (e.g. title1.html test file).
  GURL::Replacements replace_path;
  replace_path.SetPathStr("/title1.html");
  GURL cws_web_url = gallery_url().ReplaceComponents(replace_path);

  // Note that the |setTimeout| call below is needed to make sure
  // ExecuteScriptAndExtractBool returns *after* a scheduled navigation has
  // already started.
  std::string navigation_starting_script =
      "var form = document.getElementById('form');\n"
      "form.action = '" + cws_web_url.spec() + "';\n"
      "form.submit();\n"
      "setTimeout(\n"
      "    function() { window.domAutomationController.send(true); },\n"
      "    0);\n";

  // Trigger a renderer-initiated POST navigation (via the form) to a Chrome Web
  // Store gallery URL (which will commit into a chrome-extension://cws-app-id).
  bool ignored_script_result = false;
  content::TestNavigationObserver nav_observer(web_contents, 1);
  content::RenderProcessHostWatcher crash_observer(
      web_contents->GetMainFrame()->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, navigation_starting_script, &ignored_script_result));

  // When --isolate-extensions is enabled, the expectation is that the store
  // will be properly put in its own process, otherwise the renderer process
  // is going to be terminated.
  if (!extensions::IsIsolateExtensionsEnabled()) {
    crash_observer.Wait();
    return;
  }

  // Verify that the navigation succeeded.
  nav_observer.Wait();
  EXPECT_EQ(cws_web_url, web_contents->GetLastCommittedURL());

  // Verify that we really have the Chrome Web Store app loaded in the Web
  // Contents.
  content::RenderProcessHost* new_process_host =
      web_contents->GetMainFrame()->GetProcess();
  EXPECT_TRUE(extensions::ProcessMap::Get(profile())->Contains(
      extensions::kWebStoreAppId, new_process_host->GetID()));

  // Verify that Chrome Web Store is isolated in a separate renderer process.
  EXPECT_NE(old_process_host, new_process_host);
}
