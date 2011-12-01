// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class PlaformAppBrowserTest : public ExtensionBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePlatformApps);
  }

  void LoadAndLaunchPlatformApp(const char* name) {
    EXPECT_TRUE(LoadExtension(test_data_dir_.AppendASCII(name)));

    ExtensionService* service = browser()->profile()->GetExtensionService();
    const Extension* extension = service->GetExtensionById(
        last_loaded_extension_id_, false);
    EXPECT_TRUE(extension);

    size_t browser_count = BrowserList::size();

    Browser::OpenApplication(
        browser()->profile(),
        extension,
        extension_misc::LAUNCH_SHELL,
        GURL(),
        NEW_WINDOW);

    // Now we have a new browser instance.
    EXPECT_EQ(browser_count + 1, BrowserList::size());
  }
};

IN_PROC_BROWSER_TEST_F(PlaformAppBrowserTest, OpenAppInShellContainer) {
  // Start with one browser, new platform app will create another.
  ASSERT_EQ(1u, BrowserList::size());

  LoadAndLaunchPlatformApp("platform_app");

  // The launch should have created a new browser, so there should be 2 now.
  ASSERT_EQ(2u, BrowserList::size());

  // The new browser is the last one.
  BrowserList::const_reverse_iterator reverse_iterator(BrowserList::end());
  Browser* new_browser = *(reverse_iterator++);

  ASSERT_TRUE(new_browser);
  ASSERT_TRUE(new_browser != browser());

  // Expect an app in a shell window.
  EXPECT_TRUE(new_browser->is_app());
  EXPECT_TRUE(new_browser->is_type_shell());
}
