// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"

class LazyBackgroundPageApiTest : public ExtensionApiTest {
public:
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableLazyBackgroundPages);
  }
};

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, BrowserActionCreateTab) {
  ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableLazyBackgroundPages));

  FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("browser_action_create_tab");
  ASSERT_TRUE(LoadExtension(extdir));

  // Lazy Background Page doesn't exist yet.
  ExtensionProcessManager* pm =
      browser()->profile()->GetExtensionProcessManager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  int num_tabs_before = browser()->tab_count();

  // Observe background page being created and closed after
  // the browser action is clicked.
  ui_test_utils::WindowedNotificationObserver bg_pg_created(
      chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
      content::NotificationService::AllSources());
  ui_test_utils::WindowedNotificationObserver bg_pg_closed(
      chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::NotificationService::AllSources());
  BrowserActionTestUtil(browser()).Press(0);
  bg_pg_created.Wait();
  bg_pg_closed.Wait();

  // Background page created a new tab before it closed.
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  EXPECT_EQ(num_tabs_before + 1, browser()->tab_count());
  EXPECT_EQ("chrome://extensions/",
            browser()->GetSelectedWebContents()->GetURL().spec());
}

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest,
                       BrowserActionCreateTabAfterCallback) {
  ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableLazyBackgroundPages));

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
  ui_test_utils::WindowedNotificationObserver bg_pg_created(
      chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
      content::NotificationService::AllSources());
  ui_test_utils::WindowedNotificationObserver bg_pg_closed(
      chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::NotificationService::AllSources());
  BrowserActionTestUtil(browser()).Press(0);
  bg_pg_created.Wait();
  bg_pg_closed.Wait();

  // Background page is closed after creating a new tab.
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  EXPECT_EQ(num_tabs_before + 1, browser()->tab_count());
}

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest,
                       BroadcastEvent) {
  ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableLazyBackgroundPages));

  FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("broadcast_event");
  ASSERT_TRUE(LoadExtension(extdir));

  // Lazy Background Page doesn't exist yet.
  ExtensionProcessManager* pm =
      browser()->profile()->GetExtensionProcessManager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  int num_page_actions = browser()->window()->GetLocationBar()->
      GetLocationBarForTesting()->PageActionVisibleCount();

  // Open a tab to a URL that will trigger the page action to show.
  // stegosaurus.html doesn't actually exist but that doesn't seem to matter.
  GURL stego_url = GURL("stegosaurus.html");
  ui_test_utils::NavigateToURL(browser(), stego_url);

  // New page action is never shown because background page is never created.
  // TODO(tessamac): Implement! Broadcast events (like tab updates) should
  //                 cause lazy background pages to be created.
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  EXPECT_EQ(num_page_actions,   // should be + 1
            browser()->window()->GetLocationBar()->
            GetLocationBarForTesting()->PageActionVisibleCount());
}

// TODO: background page with timer.
// TODO: background page that interacts with popup.
// TODO: background page with menu.
