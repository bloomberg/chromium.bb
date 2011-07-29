
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class PanelAppBrowserTest : public ExtensionBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePanels);
  }

  void LoadAndLaunchExtension(const char* name) {
    ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(name)));

    ExtensionService* service = browser()->profile()->GetExtensionService();
    const Extension* extension = service->GetExtensionById(
        last_loaded_extension_id_, false);
    ASSERT_TRUE(extension);

    Browser::OpenApplication(
        browser()->profile(),
        extension,
        extension_misc::LAUNCH_PANEL,  // Override manifest, open in panel.
        NEW_WINDOW);
  }
};

IN_PROC_BROWSER_TEST_F(PanelAppBrowserTest, OpenAppInPanel) {
  // Start with one browser, new Panel will create another.
  ASSERT_EQ(1u, BrowserList::size());

  // No Panels initially.
  PanelManager* panel_manager = PanelManager::GetInstance();
  EXPECT_EQ(0, panel_manager->num_panels()); // No panels initially.

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

  new_browser->CloseWindow();
  EXPECT_EQ(0, panel_manager->num_panels());
}
