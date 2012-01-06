// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

class PanelAppBrowserTest : public ExtensionBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePanels);
  }

  void LoadAndLaunchExtension(const char* name) {
#if defined(OS_MACOSX)
    // Opening panels on a Mac causes NSWindowController of the Panel window
    // to be autoreleased. We need a pool drained after it's done so the test
    // can close correctly. The NSWindowController of the Panel window controls
    // lifetime of the Browser object so we want to release it as soon as
    // possible. In real Chrome, this is done by message pump.
    // On non-Mac platform, this is an empty class.
    base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

    EXPECT_TRUE(LoadExtension(test_data_dir_.AppendASCII(name)));

    ExtensionService* service = browser()->profile()->GetExtensionService();
    const Extension* extension = service->GetExtensionById(
        last_loaded_extension_id_, false);
    EXPECT_TRUE(extension);

    size_t browser_count = BrowserList::size();

    Browser::OpenApplication(
        browser()->profile(),
        extension,
        // Overriding manifest to open in a panel.
        extension_misc::LAUNCH_PANEL,
        GURL(),
        NEW_WINDOW);

    // Now we have a new browser instance.
    EXPECT_EQ(browser_count + 1, BrowserList::size());
  }

  void CloseWindowAndWait(Browser* browser) {
    // Closing a browser window may involve several async tasks. Need to use
    // message pump and wait for the notification.
    size_t browser_count = BrowserList::size();
    ui_test_utils::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::Source<Browser>(browser));
    browser->CloseWindow();
    signal.Wait();
    // Now we have one less browser instance.
    EXPECT_EQ(browser_count - 1, BrowserList::size());
  }
};

IN_PROC_BROWSER_TEST_F(PanelAppBrowserTest, OpenAppInPanel) {
  // Start with one browser, new Panel will create another.
  ASSERT_EQ(1u, BrowserList::size());

  // No Panels initially.
  PanelManager* panel_manager = PanelManager::GetInstance();
  ASSERT_EQ(0, panel_manager->num_panels()); // No panels initially.

  LoadAndLaunchExtension("app_with_panel_container");

  // The launch should have created a new browser, so there should be 2 now.
  ASSERT_EQ(2u, BrowserList::size());

  // The new browser is the last one.
  BrowserList::const_reverse_iterator reverse_iterator(BrowserList::end());
  Browser* new_browser = *(reverse_iterator++);

  ASSERT_TRUE(new_browser);
  ASSERT_TRUE(new_browser != browser());

  // Expect an app in a panel window.
  EXPECT_TRUE(new_browser->is_app());
  EXPECT_TRUE(new_browser->is_type_panel());

  // Now also check that PanelManager has one new Panel under management.
  EXPECT_EQ(1, panel_manager->num_panels());

  CloseWindowAndWait(new_browser);

  EXPECT_EQ(0, panel_manager->num_panels());
  EXPECT_EQ(1u, BrowserList::size());
}
