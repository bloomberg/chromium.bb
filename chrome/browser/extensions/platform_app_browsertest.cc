// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_host.h"
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

 protected:
  void LoadAndLaunchPlatformApp(const char* name) {
    web_app::SetDisableShortcutCreationForTests(true);
    EXPECT_TRUE(LoadExtension(test_data_dir_.AppendASCII("platform_apps").
        AppendASCII(name)));

    ExtensionService* service = browser()->profile()->GetExtensionService();
    const Extension* extension = service->GetExtensionById(
        last_loaded_extension_id_, false);
    EXPECT_TRUE(extension);

    size_t platform_app_count = GetPlatformAppCount();

    Browser::OpenApplication(
        browser()->profile(),
        extension,
        extension_misc::LAUNCH_SHELL,
        GURL(),
        NEW_WINDOW);

    // Now we have a new platform app running.
    EXPECT_EQ(platform_app_count + 1, GetPlatformAppCount());
  }

  // Gets the number of platform apps that are running.
  size_t GetPlatformAppCount() {
    int count = 0;
    ExtensionProcessManager* process_manager =
        browser()->profile()->GetExtensionProcessManager();
    ExtensionProcessManager::const_iterator iter;
    for (iter = process_manager->begin(); iter != process_manager->end();
         ++iter) {
      ExtensionHost* host = *iter;
      if (host->extension() && host->extension()->is_platform_app())
        count++;
    }

    return count;
  }

  // Gets the WebContents associated with the ExtensionHost of the first
  // platform app that is found (most tests only deal with one platform
  // app, so this is good enough).
  WebContents* GetFirstPlatformAppWebContents() {
    ExtensionProcessManager* process_manager =
        browser()->profile()->GetExtensionProcessManager();
    ExtensionProcessManager::const_iterator iter;
    for (iter = process_manager->begin(); iter != process_manager->end();
         ++iter) {
      ExtensionHost* host = *iter;
      if (host->extension() && host->extension()->is_platform_app())
        return host->host_contents();
    }

    return NULL;
  }
};

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, OpenAppInShellContainer) {
  ASSERT_EQ(0u, GetPlatformAppCount());
  LoadAndLaunchPlatformApp("empty");
  ASSERT_EQ(1u, GetPlatformAppCount());
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, EmptyContextMenu) {
  LoadAndLaunchPlatformApp("empty");

  // The empty app doesn't add any context menu items, so its menu should
  // be empty.
  WebContents* web_contents = GetFirstPlatformAppWebContents();
  ASSERT_TRUE(web_contents);
  WebKit::WebContextMenuData data;
  ContextMenuParams params(data);
  PlatformAppContextMenu* menu = new PlatformAppContextMenu(web_contents,
      params);
  menu->Init();
  ASSERT_FALSE(menu->menu_model().GetItemCount());
}

// Disabled until shell windows are implemented for views.
#if defined(TOOLKIT_VIEWS)
#define MAYBE_AppWithContextMenu DISABLED_AppWithContextMenu
#else
#define MAYBE_AppWithContextMenu AppWithContextMenu
#endif
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_AppWithContextMenu) {
  ExtensionTestMessageListener listener1("created item", false);
  LoadAndLaunchPlatformApp("context_menu");

  // Wait for the extension to tell us it's created an item.
  ASSERT_TRUE(listener1.WaitUntilSatisfied());

  // The context_menu app has one context menu item. This is all that should
  // be in the menu, there should be no seperator.
  WebContents* web_contents = GetFirstPlatformAppWebContents();
  ASSERT_TRUE(web_contents);
  WebKit::WebContextMenuData data;
  ContextMenuParams params(data);
  PlatformAppContextMenu* menu = new PlatformAppContextMenu(web_contents,
      params);
  menu->Init();
  ASSERT_EQ(1, menu->menu_model().GetItemCount());
}
