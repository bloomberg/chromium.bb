// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "webkit/glue/context_menu.h"

using content::WebContents;

namespace {
// Non-abstract RenderViewContextMenu class.
class PlatformAppContextMenu : public RenderViewContextMenu {
 public:
  PlatformAppContextMenu(WebContents* web_contents,
                         const ContextMenuParams& params)
      : RenderViewContextMenu(web_contents, params) {}

 protected:
  // These two functions implement pure virtual methods of
  // RenderViewContextMenu.
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          ui::Accelerator* accelerator) {
    return false;
  }
  virtual void PlatformInit() {}
};

}  // namespace

class PlatformAppBrowserTest : public ExtensionBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePlatformApps);
  }

  void LoadAndLaunchPlatformApp(const char* name) {
    web_app::SetDisableShortcutCreationForTests(true);
    EXPECT_TRUE(LoadExtension(test_data_dir_.AppendASCII("platform_apps").
        AppendASCII(name)));

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

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, OpenAppInShellContainer) {
  // Start with one browser, new platform app will create another.
  ASSERT_EQ(1u, BrowserList::size());

  LoadAndLaunchPlatformApp("empty");

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

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, EmptyContextMenu) {
  // Start with one browser, new platform app will create another.
  ASSERT_EQ(1u, BrowserList::size());

  LoadAndLaunchPlatformApp("empty");

  // The launch should have created a new browser, so there should be 2 now.
  ASSERT_EQ(2u, BrowserList::size());

  // The new browser is the last one.
  BrowserList::const_reverse_iterator reverse_iterator(BrowserList::end());
  Browser* new_browser = *(reverse_iterator++);

  ASSERT_TRUE(new_browser);
  ASSERT_TRUE(new_browser != browser());

  // The empty app doesn't add any context menu items, so its menu should
  // be empty.
  WebContents* web_contents = new_browser->GetSelectedWebContents();
  WebKit::WebContextMenuData data;
  ContextMenuParams params(data);
  PlatformAppContextMenu* menu = new PlatformAppContextMenu(web_contents,
      params);
  menu->Init();
  ASSERT_FALSE(menu->menu_model().GetItemCount());
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AppWithContextMenu) {
  // Start with one browser, new platform app will create another.
  ASSERT_EQ(1u, BrowserList::size());

  ExtensionTestMessageListener listener1("created item", false);
  LoadAndLaunchPlatformApp("context_menu");

  // Wait for the extension to tell us it's created an item.
  ASSERT_TRUE(listener1.WaitUntilSatisfied());

  // The launch should have created a new browser, so there should be 2 now.
  ASSERT_EQ(2u, BrowserList::size());

  // The new browser is the last one.
  BrowserList::const_reverse_iterator reverse_iterator(BrowserList::end());
  Browser* new_browser = *(reverse_iterator++);

  ASSERT_TRUE(new_browser);
  ASSERT_TRUE(new_browser != browser());

  // The context_menu app has one context menu item. This is all that should
  // be in the menu, there should be no seperator.
  WebContents* web_contents = new_browser->GetSelectedWebContents();
  WebKit::WebContextMenuData data;
  ContextMenuParams params(data);
  PlatformAppContextMenu* menu = new PlatformAppContextMenu(web_contents,
      params);
  menu->Init();
  ASSERT_EQ(1, menu->menu_model().GetItemCount());
}
