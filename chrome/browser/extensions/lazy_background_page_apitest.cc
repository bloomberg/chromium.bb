// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
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
};

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, BrowserActionCreateTab) {
  FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("browser_action_create_tab");
  ASSERT_TRUE(LoadExtension(extdir));

  // Lazy Background Page doesn't exist yet.
  // Note: We actually loaded and destroyed the page to dispatch the onInstalled
  // event. LoadExtension waits for the load to finish, after which onInstalled
  // is dispatched. Since the extension isn't listening to it, we immediately
  // tear it down again.
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
  FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("browser_action_with_callback");
  ASSERT_TRUE(LoadExtension(extdir));

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

  FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("broadcast_event");
  const Extension* extension = LoadExtension(extdir);
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

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, OnInstalled) {
  LazyBackgroundObserver page_complete;
  ResultCatcher catcher;
  FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("on_installed");
  const Extension* extension = LoadExtension(extdir);
  ASSERT_TRUE(extension);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  page_complete.Wait();

  // Lazy Background Page has been shut down.
  ExtensionProcessManager* pm =
      browser()->profile()->GetExtensionProcessManager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
}

// TODO: background page with timer.
// TODO: background page that interacts with popup.
// TODO: background page with menu.
