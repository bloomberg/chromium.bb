// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

// Refactor has only been done for Mac panels so far.
#if defined(OS_MACOSX)

class PanelAppBrowserTest : public ExtensionBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kBrowserlessPanels);
  }

  void LoadAndLaunchExtension(const char* name) {
#if defined(OS_MACOSX)
    // Opening panels on a Mac causes NSWindowController of the Panel window
    // to be autoreleased. We need a pool drained after it's done so the test
    // can close correctly. The NSWindowController of the Panel window controls
    // lifetime of the Panel object so we want to release it as soon as
    // possible. In real Chrome, this is done by message pump.
    // On non-Mac platform, this is an empty class.
    base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

    EXPECT_TRUE(LoadExtension(test_data_dir_.AppendASCII(name)));

    ExtensionService* service = browser()->profile()->GetExtensionService();
    const extensions::Extension* extension = service->GetExtensionById(
        last_loaded_extension_id_, false);
    EXPECT_TRUE(extension);

    PanelManager* manager = PanelManager::GetInstance();
    int panel_count = manager->num_panels();

    application_launch::OpenApplication(
        browser()->profile(),
        extension,
        // Overriding manifest to open in a panel.
        extension_misc::LAUNCH_PANEL,
        GURL(),
        NEW_WINDOW,
        NULL);

    // Now we have a new browser instance.
    EXPECT_EQ(panel_count + 1, manager->num_panels());
  }

  void ClosePanelAndWait(Panel* panel) {
    // Closing a panel window may involve several async tasks. Need to use
    // message pump and wait for the notification.
    int panel_count = PanelManager::GetInstance()->num_panels();
    ui_test_utils::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_PANEL_CLOSED,
        content::Source<Panel>(panel));
    panel->Close();
    signal.Wait();
    // Now we have one less panel instance.
    EXPECT_EQ(panel_count - 1, PanelManager::GetInstance()->num_panels());
  }
};

IN_PROC_BROWSER_TEST_F(PanelAppBrowserTest, OpenAppInPanel) {
  // Start with one browser, new Panel will NOT create another.
  ASSERT_EQ(1u, BrowserList::size());

  // No Panels initially.
  PanelManager* panel_manager = PanelManager::GetInstance();
  ASSERT_EQ(0, panel_manager->num_panels()); // No panels initially.

  LoadAndLaunchExtension("app_with_panel_container");

  // The launch should have created no new browsers.
  ASSERT_EQ(1u, BrowserList::size());

  // Now also check that PanelManager has one new Panel under management.
  EXPECT_EQ(1, panel_manager->num_panels());

  Panel* panel = panel_manager->panels()[0];
  ClosePanelAndWait(panel);

  EXPECT_EQ(0, panel_manager->num_panels());
  EXPECT_EQ(1u, BrowserList::size());
}

#endif  // OS_MACOSX
