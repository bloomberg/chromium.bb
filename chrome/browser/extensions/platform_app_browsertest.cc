// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/ui_test_utils.h"

using content::WebContents;
using extensions::Extension;

namespace {
// Non-abstract RenderViewContextMenu class.
class PlatformAppContextMenu : public RenderViewContextMenu {
 public:
  PlatformAppContextMenu(WebContents* web_contents,
                         const content::ContextMenuParams& params)
      : RenderViewContextMenu(web_contents, params) {}

  bool HasCommandWithId(int command_id) {
    return menu_model_.GetIndexOfCommandId(command_id) != -1;
  }

 protected:
  // These two functions implement pure virtual methods of
  // RenderViewContextMenu.
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          ui::Accelerator* accelerator) {
    return false;
  }
  virtual void PlatformInit() {}
  virtual void PlatformCancel() {}
};

}  // namespace

// Tests that platform apps received the "launch" event when launched.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, OnLaunchedEvent) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch")) << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, EmptyContextMenu) {
  ExtensionTestMessageListener launched_listener("Launched", false);
  LoadAndLaunchPlatformApp("minimal");

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  // The empty app doesn't add any context menu items, so its menu should
  // only include the developer tools.
  WebContents* web_contents = GetFirstShellWindowWebContents();
  ASSERT_TRUE(web_contents);
  WebKit::WebContextMenuData data;
  content::ContextMenuParams params(data);
  PlatformAppContextMenu* menu = new PlatformAppContextMenu(web_contents,
      params);
  menu->Init();
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_RELOAD));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_BACK));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AppWithContextMenu) {
  ExtensionTestMessageListener launched_listener("Launched", false);
  LoadAndLaunchPlatformApp("context_menu");

  // Wait for the extension to tell us it's initialized its context menus and
  // launched a window.
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  // The context_menu app has one context menu item. This, along with a
  // separator and the developer tools, is all that should be in the menu.
  WebContents* web_contents = GetFirstShellWindowWebContents();
  ASSERT_TRUE(web_contents);
  WebKit::WebContextMenuData data;
  content::ContextMenuParams params(data);
  PlatformAppContextMenu* menu = new PlatformAppContextMenu(web_contents,
      params);
  menu->Init();
  ASSERT_TRUE(menu->HasCommandWithId(IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_RELOAD));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_BACK));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, DisallowNavigation) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/navigation")) << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, Iframes) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/iframes")) << message_;
}

// Tests that localStorage and WebSQL are disabled for platform apps.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, DisallowStorage) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/storage")) << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, Restrictions) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/restrictions")) << message_;
}

// Tests that platform apps can use the chrome.appWindow.* API.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, WindowsApi) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/windows_api")) << message_;
}

// Tests that platform apps have isolated storage by default.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, Isolation) {
  ASSERT_TRUE(StartTestServer());

  // Load a (non-app) page under the "localhost" origin that sets a cookie.
  GURL set_cookie_url = test_server()->GetURL(
      "files/extensions/platform_apps/isolation/set_cookie.html");
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);
  set_cookie_url = set_cookie_url.ReplaceComponents(replace_host);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), set_cookie_url,
      CURRENT_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Make sure the cookie is set.
  int cookie_size;
  std::string cookie_value;
  automation_util::GetCookies(
      set_cookie_url,
      browser()->GetWebContentsAt(0),
      &cookie_size,
      &cookie_value);
  ASSERT_EQ("testCookie=1", cookie_value);

  // Let the platform app request the same URL, and make sure that it doesn't
  // see the cookie.
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/isolation")) << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, ExtensionWindowingApis) {
  // Initially there should be just the one browser window visible to the
  // extensions API.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("common/background_page"));
  ASSERT_EQ(1U, RunGetWindowsFunctionForExtension(extension));

  // And no shell windows.
  ASSERT_EQ(0U, GetShellWindowCount());

  // Launch a platform app that shows a window.
  ExtensionTestMessageListener launched_listener("Launched", false);
  LoadAndLaunchPlatformApp("minimal");
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  ASSERT_EQ(1U, GetShellWindowCount());
  ShellWindowRegistry::ShellWindowSet shell_windows =
      ShellWindowRegistry::Get(browser()->profile())->shell_windows();
  int shell_window_id = (*shell_windows.begin())->session_id().id();

  // But it's not visible to the extensions API, it still thinks there's just
  // one browser window.
  ASSERT_EQ(1U, RunGetWindowsFunctionForExtension(extension));
  // It can't look it up by ID either
  ASSERT_FALSE(RunGetWindowFunctionForExtension(shell_window_id, extension));

  // The app can also only see one window (its own).
  // TODO(jeremya): add an extension function to get a shell window by ID, and
  // to get a list of all the shell windows, so we can test this.

  // Launch another platform app that also shows a window.
  ExtensionTestMessageListener launched_listener2("Launched", false);
  LoadAndLaunchPlatformApp("context_menu");
  ASSERT_TRUE(launched_listener2.WaitUntilSatisfied());

  // There are two total shell windows, but each app can only see its own.
  ASSERT_EQ(2U, GetShellWindowCount());
  // TODO(jeremya): as above, this requires more extension functions.
}

// TODO(benwells): fix these tests for ChromeOS.
#if !defined(OS_CHROMEOS)
// Tests that command line parameters get passed through to platform apps
// via launchData correctly when launching with a file.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithFile) {
  SetCommandLineArg( "platform_apps/launch_files/test.txt");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_file"))
      << message_;
}

// Tests that no launch data is sent through if the platform app provides
// an intent with the wrong action.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithWrongIntent) {
  SetCommandLineArg("platform_apps/launch_files/test.txt");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_wrong_intent"))
      << message_;
}

// Tests that no launch data is sent through if the file is of the wrong MIME
// type.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithWrongType) {
  SetCommandLineArg("platform_apps/launch_files/test.txt");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_wrong_type"))
      << message_;
}

// Tests that no launch data is sent through if the platform app does not
// provide an intent.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithNoIntent) {
  SetCommandLineArg("platform_apps/launch_files/test.txt");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_no_intent"))
      << message_;
}

// Tests that no launch data is sent through if the file MIME type cannot
// be read.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchNoType) {
  SetCommandLineArg("platform_apps/launch_files/test.unknownextension");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_invalid"))
      << message_;
}

// Tests that no launch data is sent through if the file does not exist.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchNoFile) {
  SetCommandLineArg("platform_apps/launch_files/doesnotexist.txt");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_invalid"))
      << message_;
}

// Tests that no launch data is sent through if the argument is a directory.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithDirectory) {
  SetCommandLineArg("platform_apps/launch_files");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_invalid"))
      << message_;
}

// Tests that no launch data is sent through if there are no arguments passed
// on the command line
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithNothing) {
  ClearCommandLineArgs();
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_nothing"))
      << message_;
}

// Test that platform apps can use the chrome.fileSystem.getDisplayPath
// function to get the native file system path of a file they are launched with.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, GetDisplayPath) {
  SetCommandLineArg("platform_apps/launch_files/test.txt");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/get_display_path"))
      << message_;
}

#endif  // defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, OpenLink) {
  ASSERT_TRUE(StartTestServer());
  ui_test_utils::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::Source<content::WebContentsDelegate>(browser()));
  LoadAndLaunchPlatformApp("open_link");
  observer.Wait();
  ASSERT_EQ(2, browser()->tab_count());
}
