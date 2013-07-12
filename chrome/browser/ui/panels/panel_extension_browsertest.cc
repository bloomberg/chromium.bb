// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

using extensions::Extension;

class PanelExtensionBrowserTest : public ExtensionBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePanels);
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
    test_data_dir_ = test_data_dir_.AppendASCII("panels");
  }

  Panel* CreatePanelFromExtension(const Extension* extension) const {
#if defined(OS_MACOSX)
    // Opening panels on a Mac causes NSWindowController of the Panel window
    // to be autoreleased. We need a pool drained after it's done so the test
    // can close correctly. The NSWindowController of the Panel window controls
    // lifetime of the Panel object so we want to release it as soon as
    // possible. In real Chrome, this is done by message pump.
    // On non-Mac platform, this is an empty class.
    base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

    Panel* panel = PanelManager::GetInstance()->CreatePanel(
        web_app::GenerateApplicationNameFromExtensionId(extension->id()),
        browser()->profile(),
        GURL(),
        gfx::Rect(),
        PanelManager::CREATE_AS_DETACHED);
    panel->ShowInactive();
    return panel;
  }

  void WaitForAppIconAvailable(Panel* panel) const {
    content::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_PANEL_APP_ICON_LOADED,
        content::Source<Panel>(panel));
    if (!panel->app_icon().IsEmpty())
      return;
    signal.Wait();
    EXPECT_FALSE(panel->app_icon().IsEmpty());
  }

  static NativePanelTesting* CreateNativePanelTesting(Panel* panel) {
    return panel->native_panel()->CreateNativePanelTesting();
  }
};

// TODO(jschuh): Hanging plugin tests. crbug.com/244653
#if !defined(OS_WIN) && !defined(ARCH_CPU_X86_64)
IN_PROC_BROWSER_TEST_F(PanelExtensionBrowserTest, PanelAppIcon) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("test_extension"));
  Panel* panel = CreatePanelFromExtension(extension);

  // Wait for the app icon gets fully loaded.
  WaitForAppIconAvailable(panel);

  // First verify on the panel level.
  gfx::ImageSkia app_icon = panel->app_icon().AsImageSkia();
  EXPECT_EQ(panel::kPanelAppIconSize, app_icon.width());
  EXPECT_EQ(panel::kPanelAppIconSize, app_icon.height());

  // Then verify on the native panel level.
#if !defined(OS_WIN) || !defined(USE_AURA)
  scoped_ptr<NativePanelTesting> native_panel_testing(
      CreateNativePanelTesting(panel));
  EXPECT_TRUE(native_panel_testing->VerifyAppIcon());
#endif

  panel->Close();
}
#endif

IN_PROC_BROWSER_TEST_F(PanelExtensionBrowserTest,
                       ClosePanelBeforeIconLoadingCompleted) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("test_extension"));
  Panel* panel = CreatePanelFromExtension(extension);

  // Close tha panel without waiting for the app icon loaded.
  panel->Close();
}

// Non-abstract RenderViewContextMenu class for testing context menus in Panels.
class PanelContextMenu : public RenderViewContextMenu {
 public:
  PanelContextMenu(content::WebContents* web_contents,
                   const content::ContextMenuParams& params)
      : RenderViewContextMenu(web_contents, params) {}

  bool HasCommandWithId(int command_id) {
    return menu_model_.GetIndexOfCommandId(command_id) != -1;
  }

 protected:
  // RenderViewContextMenu implementation.
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE {
    return false;
  }
  virtual void PlatformInit() OVERRIDE {}
  virtual void PlatformCancel() OVERRIDE {}
};

IN_PROC_BROWSER_TEST_F(PanelExtensionBrowserTest, BasicContextMenu) {
  ExtensionTestMessageListener listener("panel loaded", false);
  LoadExtension(test_data_dir_.AppendASCII("basic"));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // There should only be one panel.
  PanelManager* panel_manager = PanelManager::GetInstance();
  EXPECT_EQ(1, panel_manager->num_panels());
  Panel* panel = panel_manager->panels().front();
  content::WebContents* web_contents = panel->GetWebContents();
  ASSERT_TRUE(web_contents);

  // Verify basic menu contents. The basic extension does not add any
  // context menu items so the panel's menu should include only the
  // developer tools.
  {
    content::ContextMenuParams params;
    params.page_url = web_contents->GetURL();
    // Ensure context menu isn't swallowed by WebContentsDelegate (the panel).
    EXPECT_FALSE(web_contents->GetDelegate()->HandleContextMenu(params));

    scoped_ptr<PanelContextMenu> menu(
        new PanelContextMenu(web_contents, params));
    menu->Init();

    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_UNDO));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_PASTE));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_COPY));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_BACK));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  }

  // Verify expected menu contents for editable item.
  {
    content::ContextMenuParams params;
    params.is_editable = true;
    params.page_url = web_contents->GetURL();
    // Ensure context menu isn't swallowed by WebContentsDelegate (the panel).
    EXPECT_FALSE(web_contents->GetDelegate()->HandleContextMenu(params));

    scoped_ptr<PanelContextMenu> menu(
        new PanelContextMenu(web_contents, params));
    menu->Init();

    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
    EXPECT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_UNDO));
    EXPECT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_PASTE));
    EXPECT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_COPY));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_BACK));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  }

  // Verify expected menu contents for text selection.
  {
    content::ContextMenuParams params;
    params.page_url = web_contents->GetURL();
    params.selection_text = ASCIIToUTF16("Select me");
    // Ensure context menu isn't swallowed by WebContentsDelegate (the panel).
    EXPECT_FALSE(web_contents->GetDelegate()->HandleContextMenu(params));

    scoped_ptr<PanelContextMenu> menu(
        new PanelContextMenu(web_contents, params));
    menu->Init();

    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_UNDO));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_PASTE));
    EXPECT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_COPY));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_BACK));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  }

  // Verify expected menu contexts for a link.
  {
    content::ContextMenuParams params;
    params.page_url = web_contents->GetURL();
    params.unfiltered_link_url = GURL("http://google.com/");
    // Ensure context menu isn't swallowed by WebContentsDelegate (the panel).
    EXPECT_FALSE(web_contents->GetDelegate()->HandleContextMenu(params));

    scoped_ptr<PanelContextMenu> menu(
        new PanelContextMenu(web_contents, params));
    menu->Init();

    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_UNDO));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_PASTE));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_COPY));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_BACK));
    EXPECT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
    EXPECT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  }
}

IN_PROC_BROWSER_TEST_F(PanelExtensionBrowserTest, CustomContextMenu) {
  ExtensionTestMessageListener listener("created item", false);
  LoadExtension(test_data_dir_.AppendASCII("context_menu"));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Load a second extension that also creates a custom context menu item.
  ExtensionTestMessageListener bogey_listener("created bogey item", false);
  LoadExtension(test_data_dir_.AppendASCII("context_menu2"));
  ASSERT_TRUE(bogey_listener.WaitUntilSatisfied());

  // There should only be one panel.
  PanelManager* panel_manager = PanelManager::GetInstance();
  EXPECT_EQ(1, panel_manager->num_panels());
  Panel* panel = panel_manager->panels().front();
  content::WebContents* web_contents = panel->GetWebContents();
  ASSERT_TRUE(web_contents);

  content::ContextMenuParams params;
  params.page_url = web_contents->GetURL();

  // Ensure context menu isn't swallowed by WebContentsDelegate (the panel).
  EXPECT_FALSE(web_contents->GetDelegate()->HandleContextMenu(params));

  // Verify menu contents contains the custom item added by their own extension.
  scoped_ptr<PanelContextMenu> menu;
  menu.reset(new PanelContextMenu(web_contents, params));
  menu->Init();
  EXPECT_TRUE(menu->HasCommandWithId(IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST));
  EXPECT_FALSE(menu->HasCommandWithId(IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + 1));
  EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_UNDO));
  EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_PASTE));
  EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_COPY));
  EXPECT_FALSE(menu->HasCommandWithId(IDC_BACK));
  EXPECT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
  EXPECT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));

  // Execute the extension's custom menu item and wait for the extension's
  // script to tell us its onclick fired.
  ExtensionTestMessageListener onclick_listener("clicked", false);
  int command_id = IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST;
  ASSERT_TRUE(menu->IsCommandIdEnabled(command_id));
  menu->ExecuteCommand(command_id, 0);
  EXPECT_TRUE(onclick_listener.WaitUntilSatisfied());
}
