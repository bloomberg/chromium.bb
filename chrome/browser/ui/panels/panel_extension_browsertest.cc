// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
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
  scoped_ptr<NativePanelTesting> native_panel_testing(
      CreateNativePanelTesting(panel));
  EXPECT_TRUE(native_panel_testing->VerifyAppIcon());

  panel->Close();
}

// Tests that icon loading might not be completed when the panel is closed.
// (crbug.com/151484)
IN_PROC_BROWSER_TEST_F(PanelExtensionBrowserTest,
                       ClosePanelBeforeIconLoadingCompleted) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("test_extension"));
  Panel* panel = CreatePanelFromExtension(extension);

  // Close tha panel without waiting for the app icon loaded.
  panel->Close();
}

class PanelExtensionApiTest : public ExtensionApiTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePanels);
  }
};

#if defined(OS_LINUX) || defined(USE_AURA)
// Focus test fails if there is no window manager on Linux.
// Aura panels have different behavior that do not apply to this test.
#define MAYBE_FocusChangeEventOnMinimize DISABLED_FocusChangeEventOnMinimize
#else
#define MAYBE_FocusChangeEventOnMinimize FocusChangeEventOnMinimize
#endif
IN_PROC_BROWSER_TEST_F(PanelExtensionApiTest,
                       MAYBE_FocusChangeEventOnMinimize) {
  // This is needed so the subsequently created panels can be activated.
  // On a Mac, it transforms background-only test process into foreground one.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(RunExtensionTest("panels/focus_change_on_minimize")) << message_;
}
