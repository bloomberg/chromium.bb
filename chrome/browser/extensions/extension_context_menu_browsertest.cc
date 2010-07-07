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

 protected:
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator) {
    // None of our commands have accelerators, so always return false.
    return false;
  }
  virtual void PlatformInit() {}
};

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ContextMenusSimple) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  FilePath extension_dir = test_data_dir_.AppendASCII("context_menus");
  ASSERT_TRUE(LoadExtension(extension_dir));

  // The extension's background page will create a context menu item and then
  // cause a navigation on success - we wait for that here.
  ASSERT_TRUE(ui_test_utils::WaitForNavigationsInCurrentTab(browser(), 1));

  // Initialize the data we need to create a context menu.
  TabContents* tab_contents = browser()->GetSelectedTabContents();
  ContextMenuParams params;
  params.media_type = WebContextMenuData::MediaTypeNone;
  params.x = 0;
  params.y = 0;
  params.is_image_blocked = false;
  params.frame_url = tab_contents->GetURL();
  params.media_flags = 0;
  params.spellcheck_enabled = false;
  params.is_editable = false;

  // Create and build our test context menu.
  TestRenderViewContextMenu menu(tab_contents, params);
  menu.Init();

  // Look for the extension item in the menu, and execute it.
  int command_id = IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST;
  ASSERT_TRUE(menu.IsCommandIdEnabled(command_id));
  menu.ExecuteCommand(command_id);

  // The onclick handler for the extension item will cause a navigation - we
  // wait for that here.
  ASSERT_TRUE(ui_test_utils::WaitForNavigationsInCurrentTab(browser(), 1));
}
