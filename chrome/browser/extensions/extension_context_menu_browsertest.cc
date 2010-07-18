// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/menus/menu_model.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui_test_utils.h"
#include "third_party/WebKit/WebKit/chromium/public/WebContextMenuData.h"
#include "webkit/glue/context_menu.h"

using menus::MenuModel;
using WebKit::WebContextMenuData;

// This test class helps us sidestep platform-specific issues with popping up a
// real context menu, while still running through the actual code in
// RenderViewContextMenu where extension items get added and executed.
class TestRenderViewContextMenu : public RenderViewContextMenu {
 public:
  TestRenderViewContextMenu(TabContents* tab_contents,
                            const ContextMenuParams& params)
      : RenderViewContextMenu(tab_contents, params) {}

  virtual ~TestRenderViewContextMenu() {}

  bool HasExtensionItemWithTitle(std::string title) {
    std::map<int, ExtensionMenuItem::Id>::iterator i;
    for (i = extension_item_map_.begin(); i != extension_item_map_.end(); ++i) {
      int id = i->first;
      ExtensionMenuItem* item = GetExtensionMenuItem(id);
      if (item && item->title() == title) {
        return true;
      }
    }
    return false;
  }

 protected:
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator) {
    // None of our commands have accelerators, so always return false.
    return false;
  }
  virtual void PlatformInit() {}
};

class ExtensionContextMenuBrowserTest : public ExtensionBrowserTest {
 public:
  // Helper to load an extension from context_menus/|subdirectory| in the
  // extensions test data dir.
  void LoadContextMenuExtension(std::string subdirectory) {
    FilePath extension_dir =
        test_data_dir_.AppendASCII("context_menus").AppendASCII(subdirectory);
    ASSERT_TRUE(LoadExtension(extension_dir));
  }

  // This creates a test menu using |params|, looks for an extension item with
  // the given |title|, and returns true if the item was found.
  bool MenuHasItemWithTitle(const ContextMenuParams& params,
                            std::string title) {
    TabContents* tab_contents = browser()->GetSelectedTabContents();
    TestRenderViewContextMenu menu(tab_contents, params);
    menu.Init();
    return menu.HasExtensionItemWithTitle(title);
  }
};

// Returns a new ContextMenuParams initialized with reasonable default values.
ContextMenuParams* CreateParams() {
  WebContextMenuData data;
  ContextMenuParams* params = new ContextMenuParams(data);
  return params;
}

IN_PROC_BROWSER_TEST_F(ExtensionContextMenuBrowserTest, Simple) {
  LoadContextMenuExtension("simple");

  // The extension's background page will create a context menu item and then
  // cause a navigation on success - we wait for that here.
  ASSERT_TRUE(ui_test_utils::WaitForNavigationsInCurrentTab(browser(), 1));

  // Initialize the data we need to create a context menu.
  TabContents* tab_contents = browser()->GetSelectedTabContents();
  scoped_ptr<ContextMenuParams> params(CreateParams());
  params->page_url = GURL("http://www.google.com");

  // Create and build our test context menu.
  TestRenderViewContextMenu menu(tab_contents, *params);
  menu.Init();

  // Look for the extension item in the menu, and execute it.
  int command_id = IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST;
  ASSERT_TRUE(menu.IsCommandIdEnabled(command_id));
  menu.ExecuteCommand(command_id);

  // The onclick handler for the extension item will cause a navigation - we
  // wait for that here.
  ASSERT_TRUE(ui_test_utils::WaitForNavigationsInCurrentTab(browser(), 1));
}

IN_PROC_BROWSER_TEST_F(ExtensionContextMenuBrowserTest, Patterns) {
  // The js test code will create two items with patterns and then navigate a
  // tab to tell us to proceed.
  LoadContextMenuExtension("patterns");
  ASSERT_TRUE(ui_test_utils::WaitForNavigationsInCurrentTab(browser(), 1));

  scoped_ptr<ContextMenuParams> params(CreateParams());

  // Check that a document url that should match the items' patterns appears.
  params->frame_url = GURL("http://www.google.com");
  ASSERT_TRUE(MenuHasItemWithTitle(*params, std::string("test_item1")));
  ASSERT_TRUE(MenuHasItemWithTitle(*params, std::string("test_item2")));

  // Now check for a non-matching url.
  params->frame_url = GURL("http://www.test.com");
  ASSERT_FALSE(MenuHasItemWithTitle(*params, std::string("test_item1")));
  ASSERT_FALSE(MenuHasItemWithTitle(*params, std::string("test_item2")));
}
