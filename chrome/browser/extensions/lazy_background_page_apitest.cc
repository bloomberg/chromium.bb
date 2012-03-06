// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/mock_host_resolver.h"

namespace {
// Helper class to wait for a lazy background page to load and close again.
class LazyBackgroundObserver {
public:
  LazyBackgroundObserver()
      : page_created_(chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
                      content::NotificationService::AllSources()),
        page_closed_(chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                     content::NotificationService::AllSources()) {
  }
  void Wait() {
    page_created_.Wait();
    page_closed_.Wait();
  }
private:
  ui_test_utils::WindowedNotificationObserver page_created_;
  ui_test_utils::WindowedNotificationObserver page_closed_;
};
}  // namespace

class LazyBackgroundPageApiTest : public ExtensionApiTest {
public:
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  // Loads the extension, which temporarily starts the lazy background page
  // to dispatch the onInstalled event. We wait until it shuts down again.
  const Extension* LoadExtensionAndWait(const std::string& test_name) {
    LazyBackgroundObserver page_complete;
    FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
        AppendASCII(test_name);
    const Extension* extension = LoadExtension(extdir);
    if (extension)
      page_complete.Wait();
    return extension;
  }
};

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, BrowserActionCreateTab) {
  ASSERT_TRUE(LoadExtensionAndWait("browser_action_create_tab"));

  // Lazy Background Page doesn't exist yet.
  ExtensionProcessManager* pm =
      browser()->profile()->GetExtensionProcessManager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  int num_tabs_before = browser()->tab_count();

  // Observe background page being created and closed after
  // the browser action is clicked.
  LazyBackgroundObserver page_complete;
  BrowserActionTestUtil(browser()).Press(0);
  page_complete.Wait();

  // Background page created a new tab before it closed.
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  EXPECT_EQ(num_tabs_before + 1, browser()->tab_count());
  EXPECT_EQ("chrome://extensions/",
            browser()->GetSelectedWebContents()->GetURL().spec());
}

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest,
                       BrowserActionCreateTabAfterCallback) {
  ASSERT_TRUE(LoadExtensionAndWait("browser_action_with_callback"));

  // Lazy Background Page doesn't exist yet.
  ExtensionProcessManager* pm =
      browser()->profile()->GetExtensionProcessManager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  int num_tabs_before = browser()->tab_count();

  // Observe background page being created and closed after
  // the browser action is clicked.
  LazyBackgroundObserver page_complete;
  BrowserActionTestUtil(browser()).Press(0);
  page_complete.Wait();

  // Background page is closed after creating a new tab.
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  EXPECT_EQ(num_tabs_before + 1, browser()->tab_count());
}

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, BroadcastEvent) {
  ASSERT_TRUE(StartTestServer());

  const Extension* extension = LoadExtensionAndWait("broadcast_event");
  ASSERT_TRUE(extension);

  // Lazy Background Page doesn't exist yet.
  ExtensionProcessManager* pm =
      browser()->profile()->GetExtensionProcessManager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  int num_page_actions = browser()->window()->GetLocationBar()->
      GetLocationBarForTesting()->PageActionVisibleCount();

  // Open a tab to a URL that will trigger the page action to show.
  LazyBackgroundObserver page_complete;
  ui_test_utils::WindowedNotificationObserver page_action_changed(
        chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
        content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("files/extensions/test_file.html"));
  page_complete.Wait();

  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));

  // Page action is shown.
  page_action_changed.Wait();
  EXPECT_EQ(num_page_actions + 1,
            browser()->window()->GetLocationBar()->
                GetLocationBarForTesting()->PageActionVisibleCount());
}

// Tests that the lazy background page receives the onInstalled event and shuts
// down.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, OnInstalled) {
  ResultCatcher catcher;
  ASSERT_TRUE(LoadExtensionAndWait("on_installed"));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Lazy Background Page has been shut down.
  ExtensionProcessManager* pm =
      browser()->profile()->GetExtensionProcessManager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
}

// Tests that the lazy background page stays alive until all visible views are
// closed.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, WaitForView) {
  LazyBackgroundObserver page_complete;
  ResultCatcher catcher;
  FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("wait_for_view");
  const Extension* extension = LoadExtension(extdir);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // The extension should've opened a new tab to an extension page.
  EXPECT_EQ(extension->GetResourceURL("extension_page.html").spec(),
            browser()->GetSelectedWebContents()->GetURL().spec());

  // Lazy Background Page still exists, because the extension created a new tab
  // to an extension page.
  ExtensionProcessManager* pm =
      browser()->profile()->GetExtensionProcessManager();
  EXPECT_TRUE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));

  // Close the new tab.
  browser()->CloseTabContents(browser()->GetSelectedWebContents());
  page_complete.Wait();

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
}

// Tests that the lazy background page stays alive until all network requests
// are complete.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, WaitForRequest) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());

  LazyBackgroundObserver page_complete;
  ResultCatcher catcher;
  FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("wait_for_request");
  const Extension* extension = LoadExtension(extdir);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Lazy Background Page still exists, because the extension started a request.
  ExtensionProcessManager* pm =
      browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host =
      pm->GetBackgroundHostForExtension(last_loaded_extension_id_);
  ASSERT_TRUE(host);

  // Abort the request.
  bool result = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), std::wstring(), L"abortRequest()", &result));
  EXPECT_TRUE(result);
  page_complete.Wait();

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
}

// TODO: background page with timer.
// TODO: background page that interacts with popup.
// TODO: background page with menu.
